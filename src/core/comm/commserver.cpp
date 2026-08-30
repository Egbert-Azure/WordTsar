//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @class cCommServer
 *
 * @brief MQTT broker (server) implementation for inter-process communication.
 *
 * Implements the cCommServer class, which provides a near-complete MQTT 3.1.1
 * broker that listens on a TCP port (fixed or randomly assigned) and accepts
 * multiple client connections. Manages the full broker lifecycle: socket
 * binding, client acceptance via a dedicated thread, CONNECT/CONNACK
 * handshake, SUBSCRIBE/UNSUBSCRIBE with topic filter matching (including
 * wildcards), PUBLISH fan-out to subscribed clients with QoS 0/1/2 flow
 * control, retained messages, PINGREQ/PINGRESP keep-alive, and periodic
 * retransmission of unacknowledged QoS packets.
 *
 * @section commserver_lifecycle Broker Lifecycle
 * - Start(): binds a TCP socket, starts an acceptance thread that listens
 *   for incoming client connections, and spawns per-client handler threads
 * - Stop(): sends graceful shutdown to all connected clients, joins threads,
 *   and closes all sockets
 * - Port selection: supports both fixed port assignment and random port
 *   allocation for testing
 *
 * @section commserver_client_mgmt Client Management
 * - Each connected client is tracked via an sClientInfo structure containing
 *   socket, client ID, subscriptions, and pending QoS acknowledgments
 * - CONNECT/CONNACK handshake validates protocol version and assigns client
 * - Per-client handler threads process incoming packets independently
 *
 * @section commserver_routing Message Routing
 * - PUBLISH fan-out: messages are forwarded to all clients whose subscriptions
 *   match the topic (including '+' and '#' wildcards)
 * - Retained messages: stored per-topic and delivered to new subscribers
 * - QoS flow control: PUBACK (QoS 1), PUBREC/PUBREL/PUBCOMP (QoS 2)
 * - Periodic retransmission of unacknowledged QoS packets
 *
 * @section commserver_limitations Intentional MQTT 3.1.1 Deviations
 * This broker is designed for short-lived local plugin communication and
 * intentionally omits offline message queueing (Section 3.1.2.4). When a
 * persistent-session client (cleanSession=false) disconnects, its
 * subscriptions are preserved for reconnection, but any QoS 1/2 messages
 * published while the client is offline are silently dropped rather than
 * queued. All session state is in-memory only and does not survive broker
 * restarts.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCommServer MQTT broker class
 * @see cCommClient MQTT client counterpart
 * @see CommUtils Shared utility functions and packet builders
 * @see sClientInfo Per-client connection state
 * @see sRetainedMessage Stored retained message data
 */

#include "commserver.h"
#include "comm_utils.h"
#include "commdefs.h"

#ifndef _WIN32
    #include <sys/time.h>
#endif

#include <cstring>
#include <iostream>
#include <vector>
#include <cerrno>
#include <thread>
#include <mutex>
#include <algorithm>


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock  [in] socket file descriptor
/// @param  writeResult [in] return value from socket_write
/// @param  expectedSize [in] number of bytes expected to write
///
/// @return bool - true if connection should be closed, false if error can be retried
///
/// @brief
/// Handle write errors in MQTT communication. Checks errno to determine
/// whether a connection is definitively broken or if a retry is possible.
///
/////////////////////////////////////////////////////////////////////////////
static bool handleWriteError(int clientSock, ssize_t writeResult, size_t expectedSize)
{
    if (writeResult == static_cast<ssize_t>(expectedSize))
    {
        return false;
    }

    if (writeResult == -1)
    {
        // Check errno to determine if connection is broken
        if (errno == EPIPE || errno == ECONNRESET || errno == EBADF)
        {
            std::cerr << "Client " << clientSock << " connection broken (errno=" << errno << ")\n";
            return true;
        }
        else
        {
            // Temporary error (EAGAIN, EINTR, etc.) - let retry mechanism handle it
            std::cerr << "Client " << clientSock << " write error (errno=" << errno << "), will retry\n";
            return false;
        }
    }
    else
    {
        // Partial write
        std::cerr << "Client " << clientSock << " partial write (" << writeResult << "/" << expectedSize << ")\n";
        return false;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  port [in] TCP port to listen on (0 to let the kernel pick a
///                   free ephemeral port at bind() time)
///
/// @return nothing
///
/// @brief
/// Construct a new cCommServer instance. With port == 0, the actual
/// port is chosen by the kernel during start() and read back via
/// getsockname(); getPort() then returns the bound value.
///
/////////////////////////////////////////////////////////////////////////////
cCommServer::cCommServer(int port)
    : mPort(port)
#ifdef _WIN32
    , mServerSock(static_cast<int>(INVALID_SOCKET))
#else
    , mServerSock(-1)
#endif
    , mRunning(false)
{
    // mPort == 0 means "let the kernel pick a free ephemeral port at
    // bind() time"; start() will read the actual port back via
    // getsockname() so getPort() returns the bound value.

#ifdef _WIN32
    CommUtils::getWinsockInit();
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destroy the cCommServer and stop listening
///
/////////////////////////////////////////////////////////////////////////////
cCommServer::~cCommServer(void)
{
    stop();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int - the TCP port number the broker is listening on
///
/// @brief
/// Get the port number this server is bound to
///
/////////////////////////////////////////////////////////////////////////////
int cCommServer::getPort(void) const
{
    return mPort;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true on success, false on failure
///
/// @brief
/// Start the MQTT broker. Creates a listening socket, binds to the port,
/// and launches the accept and retransmit threads. If the initial port
/// was 0, the kernel picks a free ephemeral port at bind() time and the
/// chosen port is read back via getsockname().
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::start(void)
{
    mServerSock = ::socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (mServerSock == INVALID_SOCKET)
#else
    if (mServerSock < 0)
#endif
    {
        perror("socket");
        return false;
    }
    socket_set_nosigpipe(mServerSock);

    // SO_REUSEADDR allows binding to a port still in TIME_WAIT state
    // from a previous process, avoiding "Address already in use" errors
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(mServerSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0)
#else
    if (setsockopt(mServerSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
#endif
    {
        perror("setsockopt");
        ::close(mServerSock);
        mServerSock = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    // INADDR_LOOPBACK (127.0.0.1) restricts connections to the local
    // machine only, preventing remote access to the broker
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // mPort == 0 -> kernel assigns a free ephemeral port and we read it
    // back via getsockname() after bind. Otherwise bind to the caller's
    // fixed port and fail-fast if it's in use.
    addr.sin_port = htons(static_cast<uint16_t>(mPort));

    if (bind(mServerSock,
             reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) != 0)
    {
        perror("bind");
        ::close(mServerSock);
        mServerSock = -1;
        return false;
    }

    if (mPort == 0)
    {
        sockaddr_in boundAddr{};
#ifdef _WIN32
        int boundLen = sizeof(boundAddr);
#else
        socklen_t boundLen = sizeof(boundAddr);
#endif
        if (getsockname(mServerSock,
                        reinterpret_cast<sockaddr*>(&boundAddr),
                        &boundLen) != 0)
        {
            perror("getsockname");
            ::close(mServerSock);
            mServerSock = -1;
            return false;
        }
        mPort = ntohs(boundAddr.sin_port);
    }

    // SOMAXCONN uses the system's maximum backlog queue size for
    // pending connections waiting to be accepted
    if (listen(mServerSock, SOMAXCONN) < 0)
    {
        perror("listen");
        ::close(mServerSock);
        mServerSock = -1;
        return false;
    }

    mRunning = true;

    try
    {
        mAcceptThread = std::thread(&cCommServer::acceptClients, this);
        mRetransmitThread = std::thread(&cCommServer::retransmitLoop, this);
    }
    catch (const std::system_error& e)
    {
        std::cerr << "Failed to start accept thread: "
                  << e.what()
                  << std::endl;
        ::close(mServerSock);
        mServerSock = -1;
        mRunning = false;
        return false;
    }
/*
    std::cout << "Broker listening on port "
              << mPort
              << std::endl;
*/
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stop the MQTT broker, close the server socket, and join all threads.
///
/////////////////////////////////////////////////////////////////////////////
void cCommServer::stop(void)
{
    if (!mRunning)
    {
        return;
    }

    mRunning = false;

    // Check socket validity before shutdown/close
#ifdef _WIN32
    if (mServerSock != static_cast<int>(INVALID_SOCKET))
#else
    if (mServerSock != -1)
#endif
    {
        ::shutdown(mServerSock, SHUT_RDWR);
        ::close(mServerSock);
        mServerSock = -1;
    }

    if (mAcceptThread.joinable())
    {
        mAcceptThread.join();
    }
    if (mRetransmitThread.joinable())
    {
        mRetransmitThread.join();
    }

    // Shutdown all client sockets to unblock handler threads
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (auto& [sock, client] : mClients)
        {
            ::shutdown(sock, SHUT_RDWR);
        }
    }

    // Join all client handler threads
    {
        std::lock_guard<std::mutex> lock(mThreadsMutex);
        for (auto& t : mClientThreads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
        mClientThreads.clear();
    }
/*
    std::cout << "Broker stopped"
              << std::endl;
*/
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket file descriptor
///
/// @return uint16_t - next available packet ID for this client
///
/// @brief
/// Allocate a packet ID from the client's per-connection sequence
///
/////////////////////////////////////////////////////////////////////////////
uint16_t cCommServer::nextPacketId(int clientSock)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mClients.find(clientSock);
    if (it != mClients.end())
    {
        return it->second.allocatePacketId();
    }
    return 1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Accept incoming client connections in a loop. Each accepted client
/// is handled in its own detached thread.
///
/////////////////////////////////////////////////////////////////////////////
void cCommServer::acceptClients(void)
{
    while (mRunning)
    {
        sockaddr_in clientAddr;
        socklen_t   len = sizeof(clientAddr);

        int clientSock = ::accept(mServerSock,
                                reinterpret_cast<sockaddr*>(&clientAddr),
                                &len);
#ifdef _WIN32
        if (clientSock == INVALID_SOCKET)
#else
        if (clientSock < 0)
#endif
        {
            // shutdown() / close() called: exit quietly
            if (!mRunning)
            {
                break;
            }

            // EINTR (interrupted by signal) and EAGAIN (non-blocking, no
            // pending connection) are normal transient conditions, not real
            // errors -- retry silently without logging
            if (errno != EINTR && errno != EAGAIN)
            {
                perror("accept");
            }
            continue;
        }
        socket_set_nosigpipe(clientSock);

        // Each accepted client gets its own thread for independent
        // packet processing, allowing concurrent handling of multiple clients.
        // Threads are stored and joined in stop() for clean shutdown.
        {
            std::lock_guard<std::mutex> lock(mThreadsMutex);
            mClientThreads.emplace_back(&cCommServer::handleClient, this, clientSock);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
///
/// @return uint8_t - CONNACK return code (0 = accepted)
///
/// @brief
/// Parse an incoming CONNECT packet, including protocol name/level,
/// connect flags, keep-alive, client ID, optional Will, username,
/// and password fields.
///
/////////////////////////////////////////////////////////////////////////////
uint8_t cCommServer::parseConnect(int clientSock)
{
    // 1) Read and verify CONNECT packet header
    uint8_t header;
    if (socket_read(clientSock, &header, 1) != 1)
    {
        return CONNACK_RC_SERVER_UNAVAILABLE;
    }

    // 2) Decode Remaining Length
    size_t remLen;
    if (!CommUtils::decodeRemainingLength(clientSock, remLen))
    {
        return CONNACK_RC_SERVER_UNAVAILABLE;
    }
    // Cap overall CONNECT body.
    if (remLen > MAX_CONNECT_REM_LEN)
    {
        return CONNACK_RC_SERVER_UNAVAILABLE;
    }

    size_t bytesRead = 0;
    uint8_t buf2[2];

    // 3) Protocol Name
    if (CommUtils::readFully(clientSock, buf2, 2) != 2)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }
    uint16_t protoNameLen = (uint16_t(buf2[0]) << 8) | buf2[1];
    bytesRead += 2;

    // Reject oversized protocol name.
    if (protoNameLen > MAX_PROTO_NAME_LEN)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }

    std::string protoName(protoNameLen, '\0');
    if (CommUtils::readFully(clientSock, &protoName[0], protoNameLen)
        != static_cast<ssize_t>(protoNameLen))
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }
    bytesRead += protoNameLen;
    if (protoName != "MQTT")
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }

    // 4) Protocol Level
    uint8_t protoLevel;
    if (socket_read(clientSock, &protoLevel, 1) != 1)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }
    bytesRead += 1;
    // Protocol Level 4 = MQTT 3.1.1 version identifier
    if (protoLevel != 4)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }

    // 5) Connect Flags
    uint8_t connFlags;
    if (socket_read(clientSock, &connFlags, 1) != 1)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }
    bytesRead += 1;

    // Connect Flags byte bit layout (MQTT 3.1.1 section 3.1.2-3):
    //   bit 0: Reserved (must be 0)
    //   bit 1: Clean Session -- start fresh or resume existing session
    //   bit 2: Will Flag -- client has a Will message
    //   bits 3-4: Will QoS -- quality of service for the Will message (0-2)
    //   bit 5: Will Retain -- retain the Will message on the broker
    //   bit 6: Password Flag -- payload contains a password
    //   bit 7: Username Flag -- payload contains a username
    bool cleanSession = (connFlags & 0x02) != 0;
    bool willFlag     = (connFlags & 0x04) != 0;
    uint8_t willQoS   = (connFlags >> 3) & 0x03;
    bool willRetain   = (connFlags & 0x20) != 0;
    bool userFlag     = (connFlags & 0x80) != 0;
    bool passFlag     = (connFlags & 0x40) != 0;

    // MQTT 3.1.1 section 3.1.2: Reserved bit 0 must be 0
    if (connFlags & 0x01)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }
    // If Will Flag is 0, Will QoS and Will Retain must also be 0
    if (!willFlag && (willQoS != 0 || willRetain))
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }
    // Password Flag requires Username Flag
    if (!userFlag && passFlag)
    {
        return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
    }

    // 6) Keep-Alive
    if (CommUtils::readFully(clientSock, buf2, 2) != 2)
    {
        return CONNACK_RC_SERVER_UNAVAILABLE;
    }
    bytesRead += 2;
    // Keep-alive is a 2-byte big-endian value in seconds; value 0 means
    // the client does not want keep-alive monitoring
    uint16_t keepAliveSecs = (uint16_t(buf2[0]) << 8) | buf2[1];

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mClients[clientSock].cleanSession = cleanSession;
        if (keepAliveSecs > 0)
        {
            mClients[clientSock].keepAlive = keepAliveSecs;
        }
        else
        {
            mClients[clientSock].keepAlive = 60;
        }
    }

    // 7) Client Identifier
    if (CommUtils::readFully(clientSock, buf2, 2) != 2)
    {
        return CONNACK_RC_IDENTIFIER_REJECTED;
    }
    uint16_t cidLen = (uint16_t(buf2[0]) << 8) | buf2[1];
    bytesRead += 2;

    // Reject oversized client id.
    if (cidLen > MAX_CLIENT_ID_LEN)
    {
        return CONNACK_RC_IDENTIFIER_REJECTED;
    }

    std::string clientId(cidLen, '\0');
    if (CommUtils::readFully(clientSock, &clientId[0], cidLen)
        != static_cast<ssize_t>(cidLen))
    {
        return CONNACK_RC_IDENTIFIER_REJECTED;
    }
    bytesRead += cidLen;

    // Empty Client ID with CleanSession=false is invalid per MQTT 3.1.1
    // section 3.1.3-7: the broker must reject it with Identifier Rejected
    if (clientId.empty() && !cleanSession)
    {
        return CONNACK_RC_IDENTIFIER_REJECTED;
    }

    // MQTT 3.1.1 section 3.1.3-7: empty client ID with cleanSession=true
    // requires the server to assign a unique identifier
    if (clientId.empty())
    {
        clientId = "auto-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mClients[clientSock].id = clientId;
    }

    // Prepare Will storage
    std::string willTopic;
    std::vector<uint8_t> willPayload;

    // 8) Will Message (if willFlag)
    if (willFlag)
    {
        // Will Topic
        if (CommUtils::readFully(clientSock, buf2, 2) != 2)
        {
            return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
        }
        uint16_t wtLen = (uint16_t(buf2[0]) << 8) | buf2[1];
        bytesRead += 2;

        // Reject oversized will topic.
        if (wtLen > MAX_WILL_TOPIC_LEN)
        {
            return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
        }

        willTopic.resize(wtLen);
        if (CommUtils::readFully(clientSock, &willTopic[0], wtLen)
            != static_cast<ssize_t>(wtLen))
        {
            return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
        }
        bytesRead += wtLen;

        // Will Payload
        if (CommUtils::readFully(clientSock, buf2, 2) != 2)
        {
            return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
        }
        uint16_t wmLen = (uint16_t(buf2[0]) << 8) | buf2[1];
        bytesRead += 2;

        // Reject oversized will payload.
        if (wmLen > MAX_WILL_PAYLOAD_LEN)
        {
            return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
        }

        willPayload.resize(wmLen);
        if (CommUtils::readFully(clientSock, willPayload.data(), wmLen)
            != static_cast<ssize_t>(wmLen))
        {
            return CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION;
        }
        bytesRead += wmLen;

        // Store the Will
        std::lock_guard<std::mutex> lock(mMutex);
        mClients[clientSock].will.topic   = std::move(willTopic);
        mClients[clientSock].will.payload = std::move(willPayload);
        mClients[clientSock].will.qos     = willQoS;
        mClients[clientSock].will.retain  = willRetain;
        mClients[clientSock].hasWill      = true;
    }
    else
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mClients[clientSock].hasWill = false;
    }

    // 9) Username (if present)
    if (userFlag)
    {
        if (CommUtils::readFully(clientSock, buf2, 2) != 2)
        {
            return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
        }
        uint16_t uLen = (uint16_t(buf2[0]) << 8) | buf2[1];
        bytesRead += 2;

        // Reject oversized username.
        if (uLen > MAX_USERNAME_LEN)
        {
            return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
        }

        std::string username(uLen, '\0');
        if (CommUtils::readFully(clientSock, &username[0], uLen)
            != static_cast<ssize_t>(uLen))
        {
            return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
        }
        bytesRead += uLen;

        // TODO: if auth fails: return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
    }

    // 10) Password (if present)
    if (passFlag)
    {
        if (CommUtils::readFully(clientSock, buf2, 2) != 2)
        {
            return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
        }
        uint16_t pLen = (uint16_t(buf2[0]) << 8) | buf2[1];
        bytesRead += 2;

        // Reject oversized password.
        if (pLen > MAX_PASSWORD_LEN)
        {
            return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
        }

        std::string password(pLen, '\0');
        if (CommUtils::readFully(clientSock, &password[0], pLen)
            != static_cast<ssize_t>(pLen))
        {
            return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
        }
        bytesRead += pLen;

        // TODO: if auth fails: return CONNACK_RC_BAD_USERNAME_OR_PASSWORD;
    }

    // 11) Skip any leftover bytes
    if (bytesRead < remLen)
    {
        size_t toSkip = remLen - bytesRead;
        std::vector<uint8_t> skipBuf(toSkip);
        if (CommUtils::readFully(clientSock, skipBuf.data(), toSkip)
            != static_cast<ssize_t>(toSkip))
        {
            return CONNACK_RC_SERVER_UNAVAILABLE;
        }
    }

    return CONNACK_RC_ACCEPTED;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock     [in] client socket descriptor
/// @param  returnCode     [in] CONNACK return code
/// @param  sessionPresent [in] whether a session already exists
///
/// @return bool - true on successful CONNACK send
///
/// @brief
/// Send a CONNACK response to a client
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendConnAck(int clientSock, uint8_t returnCode, bool sessionPresent)
{
    std::vector<uint8_t> packet;
    packet.push_back(CTRL_CONNACK);
    auto rem = CommUtils::encodeRemainingLength(2);
    packet.insert(packet.end(), rem.begin(), rem.end());

    // Session Present per MQTT 3.1.1 section 3.2.2-2
    if (sessionPresent)
    {
        packet.push_back(CONNACK_FLAG_SESSION_PRESENT);
    }
    else
    {
        packet.push_back(0x00);
    }
    packet.push_back(returnCode);

    return (socket_write(clientSock, packet.data(), packet.size())
            == static_cast<ssize_t>(packet.size()));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
///
/// @return nothing
///
/// @brief
/// Handle the full lifecycle for a single connected client: CONNECT handshake,
/// session restore, keep-alive, packet dispatch (PUBLISH, SUBSCRIBE,
/// UNSUBSCRIBE, PING, DISCONNECT), Will publication, and cleanup.
///
/////////////////////////////////////////////////////////////////////////////
void cCommServer::handleClient(int clientSock)
{
    bool gracefulDisconnect = false;

    // 1) CONNECT -> CONNACK
    uint8_t rc = parseConnect(clientSock);

    // Session presence check: if CleanSession is false AND we have stored
    // subscriptions for this client ID, set sessionPresent=true in CONNACK
    // to tell the client its previous session state was preserved
    bool cleanSession;
    bool hadSession;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        cleanSession = mClients[clientSock].cleanSession;
        std::string clientId = mClients[clientSock].id;   // copy by value -- no dangling reference
        hadSession = (mPersistentSubscriptions.count(clientId) > 0);
    }
    bool sessionPresent = (!cleanSession && hadSession);

    sendConnAck(clientSock, rc, sessionPresent);
    if (rc != CONNACK_RC_ACCEPTED)
    {
        ::close(clientSock);
        return;
    }

    // Client-ID collision detection and session takeover per MQTT 3.1.1
    // section 3.1.4: if another socket already has the same client ID,
    // disconnect the old connection so the new client takes over.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto &cid = mClients[clientSock].id;

        // Find any existing client with the same ID
        for (auto& [sock, client] : mClients)
        {
            if (sock != clientSock && client.id == cid)
            {
                ::shutdown(sock, SHUT_RDWR);
                break;
            }
        }
    }

    // 2) Restore persistent subscriptions if CleanSession==false.
    // NOTE: MQTT 3.1.1 Section 3.1.2.4 requires queuing QoS 1/2 messages
    // for offline persistent-session clients. This broker intentionally
    // omits offline message queueing -- only subscriptions are restored,
    // not missed messages. This is acceptable for local plugin comms.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mClients[clientSock].cleanSession)
        {
            const auto &cid = mClients[clientSock].id;
            auto it = mPersistentSubscriptions.find(cid);
            if (it != mPersistentSubscriptions.end())
            {
                for (auto &p : it->second)
                {
                    mTopicSubscriptions[p.first].push_back({clientSock, p.second});
                }
            }
        }
    }

    // 3) Configure socket receive timeout to 1.5x the client's Keep-Alive.
    // If no packet arrives within this window, the client is considered
    // disconnected (the read will return EAGAIN/EWOULDBLOCK).
    //
    // Per MQTT 3.1.1 section 3.1.2.10, Keep-Alive value 0 means the
    // server must not disconnect for inactivity -- in that case we
    // leave the socket with no read timeout (read blocks forever).
    uint16_t kaSecs;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        kaSecs = mClients[clientSock].keepAlive;
    }
    if (kaSecs != 0)
    {
#ifdef _WIN32
        // Windows SO_RCVTIMEO takes milliseconds as a DWORD
        DWORD timeout = (kaSecs + (kaSecs / 2)) * 1000;
        if (setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0)
        {
            std::cerr << "Warning: failed to set SO_RCVTIMEO for client " << clientSock << std::endl;
        }
#else
        // POSIX SO_RCVTIMEO takes a timeval struct in seconds + microseconds
        struct timeval tv{ kaSecs + (kaSecs / 2), 0 };
        if (setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            std::cerr << "Warning: failed to set SO_RCVTIMEO for client " << clientSock << std::endl;
        }
#endif
    }

    // 4) Main packet-processing loop
    bool clientRunning = true;
    while (clientRunning && mRunning)
    {
        uint8_t hdr;
        ssize_t r = socket_read(clientSock, &hdr, 1);
        if (r <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cout << "Client " << clientSock << " keep-alive timeout\n";
            }
            break;
        }

        // Fixed header byte: high nibble (bits 4-7) = packet type,
        // low nibble (bits 0-3) = flags. For PUBLISH packets:
        // bits 1-2 = QoS level (0/1/2), bit 0 = retain flag
        uint8_t packetType = hdr >> 4;
        uint8_t flags      = hdr & 0x0F;
        uint8_t qos        = (flags & 0x06) >> 1;
        bool    retain     = (flags & 0x01) != 0;

        // MQTT 3.1.1: reserved bits in the fixed-header low nibble must
        // match the per-packet-type fixed value, otherwise the packet
        // is malformed and the server must close the connection
        // (sections 3.6.1-1, 3.8.1-1, 3.10.1-1, etc.). PUBLISH is the
        // only packet that legitimately varies its low nibble.
        uint8_t expectedFlags = 0x00;
        bool flagsCheck = true;
        switch (packetType)
        {
            case PKT_PUBREL:
            case PKT_SUBSCRIBE:
            case PKT_UNSUBSCRIBE:
                expectedFlags = 0x02;
                break;
            case PKT_PUBLISH:
                flagsCheck = false;
                break;
            default:
                expectedFlags = 0x00;
                break;
        }
        if (flagsCheck && flags != expectedFlags)
        {
            std::cerr << "Malformed fixed-header flags 0x"
                      << std::hex << int(flags) << std::dec
                      << " for packet type " << int(packetType)
                      << " from client " << clientSock
                      << " -- disconnecting\n";
            break;
        }

        size_t remLen;
        if (!CommUtils::decodeRemainingLength(clientSock, remLen))
        {
            break;
        }

        switch (packetType)
        {
            case PKT_PUBLISH:
            {
                // Read Topic
                uint8_t buf[2];
                if (CommUtils::readFully(clientSock, buf, 2) != 2)
                {
                    clientRunning = false;
                    break;
                }
                uint16_t tlen = (uint16_t(buf[0]) << 8) | buf[1];
                std::vector<char> topicBuf(tlen);
                if (CommUtils::readFully(clientSock, topicBuf.data(), tlen) != static_cast<ssize_t>(tlen))
                {
                    clientRunning = false;
                    break;
                }
                std::string topic(topicBuf.begin(), topicBuf.end());

                // MQTT 3.1.1: PUBLISH topic must be non-empty with no wildcards
                if (topic.empty() ||
                    topic.find('\0') != std::string::npos ||
                    topic.find('+') != std::string::npos ||
                    topic.find('#') != std::string::npos)
                {
                    clientRunning = false;
                    break;
                }

                // Read Packet ID if QoS>0
                uint16_t packetId = 0;
                if (qos > 0)
                {
                    if (CommUtils::readFully(clientSock, buf, 2) != 2)
                    {
                        clientRunning = false;
                        break;
                    }
                    packetId = (uint16_t(buf[0]) << 8) | buf[1];
                    // MQTT 3.1.1 section 2.3.1: Packet ID must not be 0
                    if (packetId == 0)
                    {
                        clientRunning = false;
                        break;
                    }
                }

                // Read Payload: payload size = remaining length minus the
                // variable header (2-byte topic length field + topic string
                // + optional 2-byte packet ID for QoS > 0)
                size_t headerSz   = 2 + tlen + (qos > 0 ? 2 : 0);
                size_t payloadLen = remLen > headerSz ? remLen - headerSz : 0;

                // DoS protection: reject oversized payloads
                if (payloadLen > MAX_PAYLOAD_SIZE)
                {
                    clientRunning = false;
                    break;
                }

                std::vector<uint8_t> payload(payloadLen);
                if (payloadLen > 0)
                {
                    if (CommUtils::readFully(clientSock, payload.data(), payloadLen)
                        != static_cast<ssize_t>(payloadLen))
                    {
                        clientRunning = false;
                        break;
                    }
                }

                // Retain logic: non-empty payload stores/overwrites the
                // retained message for this topic; empty payload deletes
                // any existing retained message for this topic
                if (retain)
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (!payload.empty())
                    {
                        // Only store if we already have this topic or have room
                        if (mRetained.count(topic) > 0 ||
                            mRetained.size() < MAX_RETAINED_MESSAGES)
                        {
                            mRetained[topic] = CommUtils::sRetainedMessage{payload, qos};
                        }
                    }
                    else
                    {
                        mRetained.erase(topic);
                    }
                }

                // Fan-out: copy the recipient list under lock, then send
                // outside lock to avoid blocking the broker on slow clients.
                // Effective QoS = min(publish QoS, subscriber granted QoS)
                std::vector<std::pair<int, uint8_t>> recipients;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    for (auto& [filter, subs] : mTopicSubscriptions)
                    {
                        if (filterMatches(filter, topic))
                        {
                            for (auto& [sock, subQos] : subs)
                            {
                                uint8_t effectiveQos = std::min(qos, subQos);
                                recipients.push_back({sock, effectiveQos});
                            }
                        }
                    }
                }
                for (auto& [sock, effQos] : recipients)
                {
                    if (!sendPublish(sock, topic, payload, effQos, false))
                    {
                        std::cerr << "Broker dropped publish to client "
                                  << sock << " on topic '" << topic
                                  << "' (inflight overflow or write failed)\n";
                    }
                }

                // Broker-side QoS acks to the publishing client:
                // QoS 1 -> PUBACK (single ack completes delivery)
                // QoS 2 -> PUBREC (starts the 4-step handshake:
                //          PUBLISH -> PUBREC -> PUBREL -> PUBCOMP)
                if (qos == QOS1)
                {
                    sendPubAck(clientSock, packetId);
                }
                else if (qos == QOS2)
                {
                    sendPubRec(clientSock, packetId);
                }
                break;
            }

            case PKT_PUBREL:
            {
                // QoS2: client PUBREL -> server PUBCOMP
                uint8_t buf[2];
                if (CommUtils::readFully(clientSock, buf, 2) != 2)
                {
                    clientRunning = false;
                    break;
                }
                uint16_t pid = (uint16_t(buf[0]) << 8) | buf[1];
                sendPubComp(clientSock, pid);
                break;
            }

            case PKT_PUBACK:
            {
                // Subscriber ack for a broker->subscriber QoS 1 delivery.
                // Completes the handshake -- drop the inflight entry so
                // the retransmit thread stops retrying it.
                uint8_t buf[2];
                if (CommUtils::readFully(clientSock, buf, 2) != 2)
                {
                    clientRunning = false;
                    break;
                }
                uint16_t pid = (uint16_t(buf[0]) << 8) | buf[1];

                std::lock_guard<std::mutex> lock(mMutex);
                auto clientIt = mClients.find(clientSock);
                if (clientIt != mClients.end())
                {
                    if (clientIt->second.inflightMessages.erase(pid) == 0)
                    {
                        std::cerr << "Broker received PUBACK for unknown "
                                     "packet id " << pid << " from client "
                                  << clientSock << "\n";
                    }
                }
                break;
            }

            case PKT_PUBREC:
            {
                // Subscriber ack for a broker->subscriber QoS 2 delivery,
                // step 2. Transition the inflight entry so the retransmit
                // thread re-sends PUBREL (not the original PUBLISH) if
                // PUBCOMP is delayed, then send PUBREL.
                uint8_t buf[2];
                if (CommUtils::readFully(clientSock, buf, 2) != 2)
                {
                    clientRunning = false;
                    break;
                }
                uint16_t pid = (uint16_t(buf[0]) << 8) | buf[1];

                bool found = false;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    auto clientIt = mClients.find(clientSock);
                    if (clientIt != mClients.end())
                    {
                        auto entryIt = clientIt->second.inflightMessages.find(pid);
                        if (entryIt != clientIt->second.inflightMessages.end())
                        {
                            entryIt->second.packet =
                                CommUtils::buildControlPacket(CTRL_PUBREL, pid);
                            entryIt->second.state =
                                CommUtils::eInflightState::WAIT_PUBREL;
                            entryIt->second.timestamp =
                                std::chrono::steady_clock::now();
                            entryIt->second.retries = 0;
                            found = true;
                        }
                    }
                }

                if (found)
                {
                    CommUtils::sendControlPacket(clientSock, CTRL_PUBREL, pid);
                }
                else
                {
                    std::cerr << "Broker received PUBREC for unknown "
                                 "packet id " << pid << " from client "
                              << clientSock << "\n";
                }
                break;
            }

            case PKT_PUBCOMP:
            {
                // Subscriber ack for a broker->subscriber QoS 2 delivery,
                // step 4 (final). Drop the inflight entry.
                uint8_t buf[2];
                if (CommUtils::readFully(clientSock, buf, 2) != 2)
                {
                    clientRunning = false;
                    break;
                }
                uint16_t pid = (uint16_t(buf[0]) << 8) | buf[1];

                std::lock_guard<std::mutex> lock(mMutex);
                auto clientIt = mClients.find(clientSock);
                if (clientIt != mClients.end())
                {
                    if (clientIt->second.inflightMessages.erase(pid) == 0)
                    {
                        std::cerr << "Broker received PUBCOMP for unknown "
                                     "packet id " << pid << " from client "
                                  << clientSock << "\n";
                    }
                }
                break;
            }

            case PKT_SUBSCRIBE:
            {
                uint16_t packetId;
                std::vector<std::pair<std::string,uint8_t>> filters;
                if (!parseSubscribe(clientSock, packetId, remLen, filters))
                {
                    clientRunning = false;
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    // Register live subscriptions with granted QoS, skipping
                    // any filter parseSubscribe marked invalid (QoS == 0x80
                    // per MQTT 3.1.1 section 3.9.3).
                    for (auto& p : filters)
                    {
                        if (p.second == 0x80)
                        {
                            continue;
                        }
                        mTopicSubscriptions[p.first].push_back({clientSock, p.second});
                    }
                    // Persist only the accepted subset.
                    if (!mClients[clientSock].cleanSession)
                    {
                        std::vector<std::pair<std::string, uint8_t>> accepted;
                        accepted.reserve(filters.size());
                        for (auto& p : filters)
                        {
                            if (p.second != 0x80)
                            {
                                accepted.push_back(p);
                            }
                        }
                        mPersistentSubscriptions[mClients[clientSock].id] =
                            std::move(accepted);
                    }
                }

                // SUBACK: per-filter return codes. 0x00/0x01/0x02 grant the
                // corresponding QoS; 0x80 signals filter rejection without
                // closing the connection.
                std::vector<uint8_t> granted;
                granted.reserve(filters.size());
                for (auto& f : filters)
                {
                    granted.push_back(f.second);
                }
                if (!sendSubAck(clientSock, packetId, granted))
                {
                    clientRunning = false;
                    break;
                }

                // Deliver retained messages: copy data under lock, send outside
                struct sRetainedDelivery
                {
                    std::string topic;
                    std::vector<uint8_t> payload;
                    uint8_t qos;
                };
                std::vector<sRetainedDelivery> retainedToSend;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    for (auto& [filter, subQos] : filters)
                    {
                        for (auto& [rTopic, rMsg] : mRetained)
                        {
                            if (filterMatches(filter, rTopic))
                            {
                                // Effective QoS = min(retained msg QoS, subscription QoS)
                                uint8_t effQos = std::min(rMsg.qos, subQos);
                                retainedToSend.push_back(
                                    {rTopic, rMsg.payload, effQos});
                            }
                        }
                    }
                }
                for (auto& rd : retainedToSend)
                {
                    sendPublish(clientSock, rd.topic, rd.payload,
                                rd.qos, true);
                }
                break;
            }

            case PKT_UNSUBSCRIBE:
            {
                uint16_t packetId;
                std::vector<std::string> topics;
                if (!parseUnsubscribe(clientSock, packetId, remLen, topics))
                {
                    clientRunning = false;
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    auto &saved = mPersistentSubscriptions[mClients[clientSock].id];
                    for (auto &t : topics)
                    {
                        // Live subscriptions: remove entries matching this socket
                        auto &vec = mTopicSubscriptions[t];
                        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                      [&](auto &p){ return p.first == clientSock; }),
                                  vec.end());
                        if (vec.empty())
                        {
                            mTopicSubscriptions.erase(t);
                        }
                        // Persistent subscriptions
                        if (!mClients[clientSock].cleanSession)
                        {
                            saved.erase(std::remove_if(
                                           saved.begin(), saved.end(),
                                           [&](auto &pp){ return pp.first == t; }),
                                        saved.end());
                        }
                    }
                    if (!mClients[clientSock].cleanSession && saved.empty())
                    {
                        mPersistentSubscriptions.erase(mClients[clientSock].id);
                    }
                }

                if (!sendUnsubAck(clientSock, packetId))
                {
                    clientRunning = false;
                    break;
                }
                break;
            }

            case PKT_PINGREQ:
            {
                sendPingResp(clientSock);
                break;
            }

            case PKT_DISCONNECT:
            {
                gracefulDisconnect = true;
                parseDisconnect(clientSock);
                clientRunning = false;
                break;
            }

            default:
            {
                // Skip unknown packet types
                std::vector<uint8_t> skip(remLen);
                if (CommUtils::readFully(clientSock, skip.data(), remLen)
                    != static_cast<ssize_t>(remLen))
                {
                    clientRunning = false;
                }
                break;
            }
        }
    }

    // 5) Will message: on unexpected disconnect (no DISCONNECT packet
    // received), publish the client's Will message to all subscribers
    // whose topic filters match the Will topic
    if (!gracefulDisconnect)
    {
        CommUtils::sClientInfo::sWill w;
        bool publishWill = false;
        {
            // Copy the Will and clear it under lock to prevent double-publish.
            // Use find() (not operator[]) so an entry already removed by another
            // thread is not resurrected as a default sClientInfo.
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mClients.find(clientSock);
            if (it != mClients.end() && it->second.hasWill)
            {
                w = it->second.will;
                it->second.hasWill = false;
                publishWill = true;
            }
        }
        if (publishWill)
        {
            // Fan-out the Will message: copy recipients under lock, send outside
            // Effective QoS = min(will QoS, subscriber granted QoS)
            std::vector<std::pair<int, uint8_t>> willRecipients;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (auto& [filter, subs] : mTopicSubscriptions)
                {
                    if (filterMatches(filter, w.topic))
                    {
                        for (auto& [sock, subQos] : subs)
                        {
                            uint8_t effectiveQos = std::min(w.qos, subQos);
                            willRecipients.push_back({sock, effectiveQos});
                        }
                    }
                }
            }
            for (auto& [sock, effQos] : willRecipients)
            {
                sendPublish(sock, w.topic, w.payload, effQos, w.retain);
            }
        }
    }

    // 6) Cleanup: remove client from mClients map, remove this socket
    // from all topic subscription lists, and close the socket. If
    // CleanSession was set, also discard persistent subscriptions.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        // Use find() (not operator[]) so a possibly-already-erased entry is not
        // resurrected just to be erased again.
        auto it = mClients.find(clientSock);
        if (it != mClients.end())
        {
            if (it->second.cleanSession)
            {
                mPersistentSubscriptions.erase(it->second.id);
            }
            mClients.erase(it);
        }
        // Remove this socket from every topic's subscriber list
        for (auto& kv : mTopicSubscriptions)
        {
            kv.second.erase(std::remove_if(
                                kv.second.begin(),
                                kv.second.end(),
                                [&](auto &p){ return p.first == clientSock; }),
                            kv.second.end());
        }
    }
    ::close(clientSock);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
///
/// @return bool - true on successful PINGRESP send
///
/// @brief
/// Send a PINGRESP packet in response to a client PINGREQ
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendPingResp(int clientSock)
{
    uint8_t packet[2] = { CTRL_PINGRESP, 0x00 };
    ssize_t sent = socket_write(clientSock, packet, sizeof(packet));
    return sent == static_cast<ssize_t>(sizeof(packet));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
///
/// @return bool - true if DISCONNECT parsed successfully
///
/// @brief
/// Parse an incoming DISCONNECT packet (no payload). Uses MSG_PEEK
/// to verify the packet type before consuming.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::parseDisconnect(int clientSock)
{
    uint8_t header;
#ifdef _WIN32
    if (recv(clientSock, reinterpret_cast<char*>(&header), 1, MSG_PEEK) != 1 ||
#else
    if (::recv(clientSock, &header, 1, MSG_PEEK) != 1 ||
#endif
        (header >> 4) != PKT_DISCONNECT)
    {
        return false;
    }
    // Consume fixed header (control byte + remaining length)
    uint8_t buf[2];
    CommUtils::readFully(clientSock, buf, 2);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [out] packet identifier from the UNSUBSCRIBE header
/// @param  remLen     [in] remaining length of the packet
/// @param  topics     [out] list of topic filters to unsubscribe from
///
/// @return bool - true on successful parse
///
/// @brief
/// Parse an UNSUBSCRIBE packet. Reads the packet ID and all topic filters.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::parseUnsubscribe(
    int clientSock,
    uint16_t& packetId,
    size_t remLen,
    std::vector<std::string>& topics)
{
    if (remLen < 2)
    {
        return false;
    }
    uint8_t buf[2];

    // 1) Packet ID
    if (CommUtils::readFully(clientSock, buf, 2) != 2)
    {
        return false;
    }
    packetId = (static_cast<uint16_t>(buf[0]) << 8)
             | static_cast<uint16_t>(buf[1]);

    size_t bytesRead = 2;
    topics.clear();

    // 2) Loop until we've consumed remLen bytes
    while (bytesRead < remLen)
    {
        // Topic length
        if (CommUtils::readFully(clientSock, buf, 2) != 2)
        {
            return false;
        }
        uint16_t tlen = (static_cast<uint16_t>(buf[0]) << 8)
                      | static_cast<uint16_t>(buf[1]);
        bytesRead += 2;

        if (bytesRead + tlen > remLen)
        {
            return false;
        }

        // Topic filter string
        std::vector<char> tbuf(tlen);
        if (CommUtils::readFully(clientSock, tbuf.data(), tlen) != static_cast<ssize_t>(tlen))
        {
            return false;
        }
        topics.emplace_back(tbuf.begin(), tbuf.end());
        bytesRead += tlen;
    }

    return (bytesRead == remLen);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  topic      [in] topic string to publish to
/// @param  payload    [in] message payload bytes
/// @param  qos        [in] quality of service level (0, 1, or 2)
/// @param  retain     [in] whether the message should be retained
///
/// @return bool - true on successful send
///
/// @brief
/// Send a PUBLISH packet to a client. For QoS>0, allocates a packet ID,
/// stores the packet for retransmission, and sets up in-flight tracking.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendPublish(
    int clientSock,
    const std::string& topic,
    const std::vector<uint8_t>& payload,
    uint8_t qos,
    bool retain)
{
    // 1) Allocate a Packet Identifier if QoS>0
    uint16_t packetId = 0;
    if (qos > 0)
    {
        packetId = nextPacketId(clientSock);
    }

    // 2) Fixed header: PUBLISH type + flags byte
    // QoS bits go into bits 1-2 of the flags nibble; retain flag goes
    // into bit 0
    uint8_t flags = (qos << 1) & 0x06;
    if (retain)
    {
        flags |= 0x01;
    }

    std::vector<uint8_t> packet;
    packet.push_back(CTRL_PUBLISH | flags);

    // 3) Remaining Length = 2 (topic length field) + topic bytes
    //    + 2 (packet ID, only if QoS > 0) + payload bytes
    uint16_t tlen   = static_cast<uint16_t>(topic.size());
    size_t remLen   = 2 + tlen + (qos > 0 ? 2 : 0) + payload.size();

    // 4) Encode Remaining Length as MQTT VarInt
    auto rem = CommUtils::encodeRemainingLength(remLen);
    packet.insert(packet.end(), rem.begin(), rem.end());

    // 5) Topic name
    packet.push_back(static_cast<uint8_t>(tlen >> 8));
    packet.push_back(static_cast<uint8_t>(tlen & 0xFF));
    packet.insert(packet.end(), topic.begin(), topic.end());

    // 6) Packet Identifier for QoS 1 or 2
    if (qos > 0)
    {
        packet.push_back(static_cast<uint8_t>(packetId >> 8));
        packet.push_back(static_cast<uint8_t>(packetId & 0xFF));
    }

    // 7) Payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    // 8) For QoS 1/2, store the full packet for retransmission in
    // case the subscriber does not acknowledge in time.
    // QoS 1 waits for PUBACK; QoS 2 waits for PUBREC (first step).
    if (qos == 1 || qos == 2)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto clientIt = mClients.find(clientSock);
        if (clientIt != mClients.end())
        {
            // DoS protection: limit in-flight messages per client
            if (clientIt->second.inflightMessages.size() >= MAX_INFLIGHT_PER_CLIENT)
            {
                return false;
            }
            CommUtils::sInflightEntry entry;
            entry.packet    = packet;
            if (qos == 1)
            {
                entry.state = CommUtils::eInflightState::WAIT_PUBACK;
            }
            else
            {
                entry.state = CommUtils::eInflightState::WAIT_PUBREC;
            }
            entry.timestamp = std::chrono::steady_clock::now();
            entry.retries   = 0;

            clientIt->second.inflightMessages[packetId] = std::move(entry);
        }
    }

    // 9) Send it on the wire
    ssize_t sent = socket_write(clientSock, packet.data(), packet.size());
    if (sent != static_cast<ssize_t>(packet.size()))
    {
        if (sent < 0)
        {
            perror("sendPublish write");
        }
        else
        {
            std::cerr << "sendPublish: partial write\n";
        }
        return false;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [out] packet identifier from the SUBSCRIBE header
/// @param  remLen     [in] remaining length of the packet
/// @param  filters    [out] list of topic filter + QoS pairs
///
/// @return bool - true if SUBSCRIBE parsed successfully
///
/// @brief
/// Parse an incoming SUBSCRIBE packet. Reads the packet ID and all
/// topic filter + QoS byte pairs from the remaining length.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::parseSubscribe(
    int clientSock,
    uint16_t& packetId,
    size_t remLen,
    std::vector<std::pair<std::string,uint8_t>>& filters)
{
    if (remLen < 2)
    {
        return false;
    }
    uint8_t buf[2];

    // 1) Packet ID
    if (CommUtils::readFully(clientSock, buf, 2) != 2)
    {
        return false;
    }
    packetId = (static_cast<uint16_t>(buf[0]) << 8)
             | static_cast<uint16_t>(buf[1]);

    size_t bytesRead = 2;
    filters.clear();

    // 2) Loop until we've consumed remLen bytes
    while (bytesRead < remLen)
    {
        // Topic length
        if (CommUtils::readFully(clientSock, buf, 2) != 2)
        {
            return false;
        }
        uint16_t tlen = (static_cast<uint16_t>(buf[0]) << 8)
                      | static_cast<uint16_t>(buf[1]);
        bytesRead += 2;

        // Safety check: topic length must fit
        if (bytesRead + tlen + 1 > remLen)
        {
            return false;
        }

        // Topic filter string
        std::vector<char> tbuf(tlen);
        if (CommUtils::readFully(clientSock, tbuf.data(), tlen) != static_cast<ssize_t>(tlen))
        {
            return false;
        }
        std::string topic(tbuf.begin(), tbuf.end());
        bytesRead += tlen;

        // Per MQTT 3.1.1 section 3.9.3, individual filter validation
        // failures are signalled with a 0x80 entry in the SUBACK return
        // codes -- not a connection close. Only wire-framing errors
        // (short reads, length mismatches) close the connection.
        bool filterValid = true;

        // MQTT 3.1.1 section 3.8.3: topic filter must not be empty
        if (topic.empty())
        {
            filterValid = false;
        }

        // MQTT 3.1.1 section 4.7.1.2: '#' must be the last character
        // and preceded by '/' (or be the entire filter by itself)
        if (filterValid)
        {
            size_t hashPos = topic.find('#');
            if (hashPos != std::string::npos)
            {
                if (hashPos != topic.size() - 1)
                {
                    filterValid = false;
                }
                else if (hashPos > 0 && topic[hashPos - 1] != '/')
                {
                    filterValid = false;
                }
            }
        }

        // MQTT 3.1.1 section 4.7.1.3: '+' must occupy an entire level
        // between '/' separators (or be at start/end of filter)
        if (filterValid)
        {
            size_t plusPos = 0;
            while ((plusPos = topic.find('+', plusPos)) != std::string::npos)
            {
                if (plusPos > 0 && topic[plusPos - 1] != '/')
                {
                    filterValid = false;
                    break;
                }
                if (plusPos + 1 < topic.size() && topic[plusPos + 1] != '/')
                {
                    filterValid = false;
                    break;
                }
                ++plusPos;
            }
        }

        // Requested QoS
        uint8_t reqQos;
        if (socket_read(clientSock, &reqQos, 1) != 1)
        {
            return false;
        }
        bytesRead += 1;

        // Validate QoS: 0/1/2 only; anything else is a filter-level
        // failure, not a wire-framing error.
        if (reqQos > QOS2)
        {
            filterValid = false;
        }

        // 0x80 in the QoS slot signals "rejected" to the caller, which
        // will skip registering this filter and emit 0x80 in SUBACK.
        filters.emplace_back(std::move(topic),
                             filterValid ? reqQos : uint8_t(0x80));

        // DoS protection: limit number of filters per SUBSCRIBE
        if (filters.size() > MAX_FILTERS_PER_SUBSCRIBE)
        {
            return false;
        }
    }

    // Must have consumed exactly remLen bytes
    return (bytesRead == remLen);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [in] packet identifier from SUBSCRIBE
/// @param  qosResults [in] granted QoS levels for each subscription
///
/// @return bool - true on successful SUBACK send
///
/// @brief
/// Send a SUBACK response to a client
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendSubAck(
    int clientSock,
    uint16_t packetId,
    const std::vector<uint8_t>& qosResults)
{
    std::vector<uint8_t> packet;
    packet.push_back(CTRL_SUBACK);

    // Remaining Length = 2 bytes for Packet Identifier + 1 byte per granted QoS
    auto rem = CommUtils::encodeRemainingLength(2 + qosResults.size());
    packet.insert(packet.end(), rem.begin(), rem.end());

    // Packet Identifier MSB, LSB
    packet.push_back(static_cast<uint8_t>(packetId >> 8));
    packet.push_back(static_cast<uint8_t>(packetId & 0xFF));

    // Payload: one return code per subscription filter
    packet.insert(packet.end(), qosResults.begin(), qosResults.end());

    ssize_t sent = socket_write(clientSock, packet.data(), packet.size());
    return (sent == static_cast<ssize_t>(packet.size()));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [in] packet identifier
///
/// @return bool - true on successful PUBACK send
///
/// @brief
/// Send PUBACK (QoS 1 acknowledgement)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendPubAck(int clientSock, uint16_t packetId)
{
    return CommUtils::sendControlPacket(clientSock, CTRL_PUBACK, packetId);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [in] packet identifier
///
/// @return bool - true on successful PUBREC send
///
/// @brief
/// Send PUBREC (QoS 2 step 1)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendPubRec(int clientSock, uint16_t packetId)
{
    return CommUtils::sendControlPacket(clientSock, CTRL_PUBREC, packetId);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [in] packet identifier
///
/// @return bool - true on successful PUBCOMP send
///
/// @brief
/// Send PUBCOMP (QoS 2 step 3: finalize delivery)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendPubComp(int clientSock, uint16_t packetId)
{
    return CommUtils::sendControlPacket(clientSock, CTRL_PUBCOMP, packetId);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientSock [in] client socket descriptor
/// @param  packetId   [in] packet identifier
///
/// @return bool - true on successful UNSUBACK send
///
/// @brief
/// Send an UNSUBACK packet in response to an UNSUBSCRIBE request
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::sendUnsubAck(int clientSock, uint16_t packetId)
{
    std::vector<uint8_t> packet;
    packet.push_back(CTRL_UNSUBACK);

    // Remaining Length = 2 (packetId)
    auto rem = CommUtils::encodeRemainingLength(2);
    packet.insert(packet.end(), rem.begin(), rem.end());

    // Packet ID
    packet.push_back(static_cast<uint8_t>(packetId >> 8));
    packet.push_back(static_cast<uint8_t>(packetId & 0xFF));

    ssize_t sent = socket_write(clientSock, packet.data(), packet.size());
    return (sent == static_cast<ssize_t>(packet.size()));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Background thread that periodically checks for un-acked QoS packets
/// and retransmits them. Disconnects clients after MAX_RETRIES failures.
///
/////////////////////////////////////////////////////////////////////////////
void cCommServer::retransmitLoop(void)
{
    constexpr auto RETRY_INTERVAL = std::chrono::seconds(5);
    constexpr int  MAX_RETRIES    = 3;

    while (mRunning)
    {
        // Wake every 1 second to check each in-flight packet's age
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();

        // Packets to retransmit and clients to disconnect, collected under the
        // lock and acted on after releasing it. socket_write is a BLOCKING send
        // (no SO_SNDTIMEO, sockets are blocking); doing it under mMutex would let
        // one stuck client freeze the whole broker. Mirror the PUBLISH fan-out
        // pattern: copy under lock, send unlocked.
        std::vector<std::pair<int, std::vector<uint8_t>>> toSend;
        std::vector<int> toDisconnect;

        // Phase 1: scan in-flight packets under the lock, bump retry bookkeeping,
        // and queue copies to send. No socket I/O happens here.
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto& [clientSock, client] : mClients)
            {
                // Check each in-flight packet against the 5-second retry interval
                for (auto& [packetId, entry] : client.inflightMessages)
                {
                    if (now - entry.timestamp > RETRY_INTERVAL)
                    {
                        // If a packet has exceeded MAX_RETRIES (3), disconnect
                        // the client as unresponsive and stop scanning it.
                        if (entry.retries >= MAX_RETRIES)
                        {
                            toDisconnect.push_back(clientSock);
                            break;
                        }

                        // Set DUP flag (bit 3) per MQTT 3.1.1 section 3.3.1-1
                        // before retransmitting QoS 1/2 PUBLISH packets
                        entry.packet[0] |= 0x08;

                        // Advance the retry window now (while locked) and queue a
                        // copy of the packet to send after the lock is released.
                        entry.timestamp = now;
                        entry.retries += 1;
                        toSend.push_back({clientSock, entry.packet});
                    }
                }
            }
        }

        // Phase 2: send queued packets WITHOUT holding the lock, so a blocking
        // write to a stuck client only stalls this thread, not the broker.
        for (auto& [clientSock, packet] : toSend)
        {
            ssize_t writeResult = socket_write(clientSock, packet.data(), packet.size());
            if (handleWriteError(clientSock, writeResult, packet.size()))
            {
                // Connection is broken, schedule disconnect
                toDisconnect.push_back(clientSock);
            }
        }

        // Phase 3: re-acquire the lock and disconnect failed/unresponsive clients
        // by key. A client may already have been removed by handleClient while
        // the lock was released, so look it up rather than reuse a stale iterator
        // (and skip the close if it is gone, to avoid double-closing the fd).
        if (!toDisconnect.empty())
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (int clientSock : toDisconnect)
            {
                auto it = mClients.find(clientSock);
                if (it != mClients.end())
                {
                    // Single-owner-of-close: only shut the socket down here so the
                    // victim's own handleClient thread wakes from its blocked read
                    // and performs the one authoritative close + erase. Closing or
                    // erasing here would race that thread (double-close / fd reuse).
                    ::shutdown(clientSock, SHUT_RDWR);
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  t [in] topic string to split
///
/// @return std::vector<std::string> - topic segments split on '/'
///
/// @brief
/// Split a topic string into path segments by '/' delimiter
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cCommServer::splitTopic(const std::string& t)
{
    // Split a topic like "a/b/c" into segments ["a", "b", "c"] by
    // scanning for '/' delimiters and extracting substrings between them
    std::vector<std::string> segs;
    size_t start = 0;
    size_t pos;
    while ((pos = t.find('/', start)) != std::string::npos)
    {
        segs.push_back(t.substr(start, pos - start));
        start = pos + 1;
    }
    // Append the final segment after the last '/' (or the entire string
    // if no '/' was found)
    segs.push_back(t.substr(start));
    return segs;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  filter [in] subscription filter (may contain + and # wildcards)
/// @param  topic  [in] published topic to match against
///
/// @return bool - true if the filter matches the topic
///
/// @brief
/// Check if an MQTT subscription filter matches a published topic.
/// Supports single-level (+) and multi-level (#) wildcards per MQTT 3.1.1.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommServer::filterMatches(const std::string& filter,
                                const std::string& topic)
{
    // MQTT 3.1.1 section 4.7.2: topics starting with $ must not match
    // wildcard subscriptions that do not also start with $
    if (!topic.empty() && topic[0] == '$' &&
        !filter.empty() && filter[0] != '$')
    {
        return false;
    }

    // Walk filter and topic segments in parallel to check for a match
    auto f = splitTopic(filter);
    auto x = splitTopic(topic);
    for (size_t i = 0; i < f.size(); ++i)
    {
        // '#' (multi-level wildcard) matches all remaining segments
        // at once, so return true immediately
        if (f[i] == "#")
        {
            return true;
        }
        // '+' (single-level wildcard) matches exactly one segment
        // at the current position; fail if there is no topic segment
        if (f[i] == "+")
        {
            if (i >= x.size())
            {
                return false;
            }
            continue;
        }
        // Literal segment: must match exactly at this position
        if (i >= x.size() || f[i] != x[i])
        {
            return false;
        }
    }
    // Filter and topic must have the same number of segments to match
    // (unless '#' already returned true above)
    return f.size() == x.size();
}
