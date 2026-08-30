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
 * @class cCommClient
 *
 * @brief MQTT client implementation for inter-process communication.
 *
 * Implements the cCommClient class, which provides a lightweight MQTT 3.1.1
 * client for connecting to a broker over TCP. Handles the full client
 * lifecycle: address resolution, socket creation, CONNECT/CONNACK handshake,
 * PUBLISH with QoS 0/1/2, SUBSCRIBE, and graceful disconnect. A background
 * receive thread processes incoming packets (PUBACK, SUBACK, PUBLISH, PINGRESP)
 * and dispatches messages to registered per-topic callbacks.
 *
 * @section commclient_lifecycle Connection Lifecycle
 * - Connect(): resolves hostname, creates TCP socket, sends CONNECT packet,
 *   waits for CONNACK, and starts the background receive thread
 * - Disconnect(): sends DISCONNECT packet, stops the receive thread, and
 *   closes the socket
 * - Background thread: continuously reads from socket, parses MQTT packets,
 *   and dispatches to appropriate handlers
 *
 * @section commclient_messaging Messaging
 * - Publish(): sends PUBLISH packets at the specified QoS level (0, 1, or 2)
 * - Subscribe(): sends SUBSCRIBE packet and registers a per-topic callback
 * - QoS 1/2 flow: handles PUBACK, PUBREC, PUBREL, PUBCOMP handshakes
 *
 * @section commclient_threading Threading Model
 * The receive thread runs in the background and processes incoming packets
 * asynchronously. Topic callbacks are invoked from the receive thread context.
 * A mutex protects shared state (subscription map, pending acknowledgments).
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCommClient MQTT client class
 * @see cCommServer MQTT broker counterpart
 * @see CommUtils Shared utility functions and packet builders
 */

#include "commclient.h"
#include "comm_utils.h"
#include "commdefs.h"

#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>


/////////////////////////////////////////////////////////////////////////////
///
/// @param  length [in] remaining length to encode
///
/// @return std::vector<uint8_t> - encoded bytes per MQTT VarInt spec
///
/// @brief
/// Encode Remaining Length as a Variable Byte Integer (delegates to CommUtils)
///
/////////////////////////////////////////////////////////////////////////////
std::vector<uint8_t> cCommClient::encodeRemainingLength(size_t length)
{
    return CommUtils::encodeRemainingLength(length);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  host     [in] broker hostname or IP address
/// @param  port     [in] broker TCP port
/// @param  clientId [in] UTF-8 client identifier
///
/// @return nothing
///
/// @brief
/// Construct a new cCommClient instance for connecting to an MQTT broker
///
/////////////////////////////////////////////////////////////////////////////
cCommClient::cCommClient(const std::string& host,
                         int port,
                         const std::string& clientId)
    : mHost(host)
    , mPort(port)
    , mClientId(clientId)
#ifdef _WIN32
    , mSock(static_cast<int>(INVALID_SOCKET))
#else
    , mSock(-1)
#endif
    , mConnected(false)
{
#ifdef _WIN32
    CommUtils::getWinsockInit();
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destroy the cCommClient and clean up resources
///
/////////////////////////////////////////////////////////////////////////////
cCommClient::~cCommClient(void)
{
    disconnect();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true on successful connection and CONNACK
///
/// @brief
/// Connect to the MQTT broker. Resolves the address, creates a TCP socket,
/// sends a CONNECT packet, parses CONNACK, and starts the receive loop.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::connect(void)
{
    // Resolve broker address
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    int err = getaddrinfo(mHost.c_str(),
                          std::to_string(mPort).c_str(),
                          &hints,
                          &res);
    if (err != 0)
    {
        std::cerr << "getaddrinfo: " << gai_strerror(err) << std::endl;
        return false;
    }

    // Create socket
    mSock = ::socket(res->ai_family,
                      res->ai_socktype,
                      res->ai_protocol);
#ifdef _WIN32
    if (mSock == INVALID_SOCKET)
#else
    if (mSock < 0)
#endif
    {
        perror("socket");
        freeaddrinfo(res);
        return false;
    }
    socket_set_nosigpipe(mSock);

    // Connect
    if (::connect(mSock,
                  res->ai_addr,
                  res->ai_addrlen) < 0)
    {
        perror("connect");
        ::close(mSock);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    // Build CONNECT variable header + payload per MQTT 3.1.1 section 3.1
    std::vector<uint8_t> payload;

    // Protocol Name: length-prefixed UTF-8 string "MQTT"
    // 2-byte big-endian length (0x00, 0x04) followed by 4 ASCII bytes
    payload.push_back(static_cast<uint8_t>(PROTO_NAME_LEN >> 8));
    payload.push_back(static_cast<uint8_t>(PROTO_NAME_LEN & 0xFF));
    payload.insert(payload.end(),
                   PROTO_NAME,
                   PROTO_NAME + PROTO_NAME_LEN);

    // Protocol Level 4 = MQTT 3.1.1 (distinguishes from 3.1 which uses level 3)
    payload.push_back(PROTO_LEVEL);
    // Connect Flags byte: bit 1 (0x02) = Clean Session, requests a fresh
    // session with no stored subscriptions or queued messages from the broker
    payload.push_back(CONNECT_FLAG_CLEAN_S);

    // Keep-Alive interval: 2-byte big-endian value in seconds
    // Broker disconnects if no packet received within 1.5x this interval
    payload.push_back(static_cast<uint8_t>(KEEP_ALIVE_SECONDS >> 8));
    payload.push_back(static_cast<uint8_t>(KEEP_ALIVE_SECONDS & 0xFF));

    // Client Identifier: length-prefixed UTF-8 string (same encoding as protocol name)
    // 2-byte big-endian length followed by the UTF-8 client ID bytes
    uint16_t idLen = static_cast<uint16_t>(mClientId.size());
    payload.push_back(static_cast<uint8_t>(idLen >> 8));
    payload.push_back(static_cast<uint8_t>(idLen & 0xFF));
    payload.insert(payload.end(),
                   mClientId.begin(),
                   mClientId.end());

    // Assemble the full CONNECT packet:
    //   byte 0:    fixed header type byte (0x10 = CONNECT)
    //   bytes 1-N: VarInt-encoded remaining length (covers everything after this field)
    //   rest:      variable header (protocol name, level, flags, keep-alive) + payload (client ID)
    std::vector<uint8_t> packet;
    packet.push_back(CTRL_CONNECT);
    auto remConn = encodeRemainingLength(payload.size());
    packet.insert(packet.end(), remConn.begin(), remConn.end());
    packet.insert(packet.end(), payload.begin(), payload.end());

    // Send CONNECT
    ssize_t sent = socket_write(mSock, packet.data(), packet.size());
    if (sent != static_cast<ssize_t>(packet.size()))
    {
        perror("CONNECT write");
        ::close(mSock);
        return false;
    }

    // Parse CONNACK
    if (!parseConnAck())
    {
        ::close(mSock);
        return false;
    }

    // Start receive loop
    mConnected = true;
    mRecvThread = std::thread(&cCommClient::receiveLoop, this);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Disconnect from the broker and stop the receive thread
///
/////////////////////////////////////////////////////////////////////////////
void cCommClient::disconnect(void)
{
    if (mConnected)
    {
        ::shutdown(mSock, SHUT_RDWR);
        ::close(mSock);
        mConnected = false;
    }
    if (mRecvThread.joinable())
    {
        mRecvThread.join();
    }

    // Clear stale pending publish entries to prevent future publish() hangs
    {
        std::lock_guard<std::mutex> lk(mPubMutex);
        mPendingPubs.clear();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  topic   [in] topic to publish to
/// @param  payload [in] message payload bytes
/// @param  qos     [in] QoS level (0, 1, or 2)
///
/// @return bool - true on successful PUBLISH
///
/// @brief
/// Send a PUBLISH packet with the given QoS. For QoS>0, allocates a
/// packet ID using instance-level counter (thread-safe) and waits
/// for acknowledgement with a 10-second timeout.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::publish(const std::string& topic,
                          const std::vector<uint8_t>& payload,
                          uint8_t qos)
{
    if (!mConnected)
    {
        return false;
    }

    // MQTT 3.1.1: topic must be non-empty, no wildcards, no null bytes
    if (topic.empty() ||
        topic.find('\0') != std::string::npos ||
        topic.find('+') != std::string::npos ||
        topic.find('#') != std::string::npos)
    {
        return false;
    }

    // PUBLISH fixed header byte: high nibble = packet type (0x30),
    // bits 1-2 encode QoS level (mask 0x03 then shift left 1 into position)
    // Result: QoS0 = 0x30, QoS1 = 0x32, QoS2 = 0x34
    uint8_t header = CTRL_PUBLISH | ((qos & 0x03) << 1);
    std::vector<uint8_t> packet;
    packet.push_back(header);

    // Remaining length = 2 (topic length field) + topic bytes
    //                   + 2 (packet ID, only present when QoS > 0)
    //                   + payload bytes
    uint16_t tlen = static_cast<uint16_t>(topic.size());
    size_t   remLen = 2 + tlen + (qos > 0 ? 2 : 0) + payload.size();
    auto remPub = encodeRemainingLength(remLen);
    packet.insert(packet.end(), remPub.begin(), remPub.end());

    // Topic name: 2-byte big-endian length followed by UTF-8 string bytes
    packet.push_back(static_cast<uint8_t>(tlen >> 8));
    packet.push_back(static_cast<uint8_t>(tlen & 0xFF));
    packet.insert(packet.end(), topic.begin(), topic.end());

    // Packet Identifier: only present for QoS 1 and QoS 2 messages
    // Allocated under mutex from instance counter; wraps from 0 back to 1
    // because packet ID 0 is reserved (MQTT 3.1.1 section 2.3.1)
    // Stored in mPendingPubs as false (unacknowledged) until broker responds
    uint16_t pid = 0;
    if (qos > 0)
    {
        std::lock_guard<std::mutex> lk(mPubMutex);
        pid = mNextPacketId++;
        if (mNextPacketId == 0)
        {
            mNextPacketId = 1;
        }
        // Append 2-byte big-endian packet ID to the PUBLISH packet
        packet.push_back(static_cast<uint8_t>(pid >> 8));
        packet.push_back(static_cast<uint8_t>(pid & 0xFF));
        // Track this packet ID as pending (false = not yet acknowledged)
        mPendingPubs[pid] = false;
    }

    // Payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    // Send
    ssize_t sent = socket_write(mSock, packet.data(), packet.size());
    if (sent != static_cast<ssize_t>(packet.size()))
    {
        perror("publish write");
        return false;
    }
    mLastPacketSent = std::chrono::steady_clock::now();

    // Wait for broker acknowledgement if QoS > 0
    // Predicate checks only THIS packet ID, not all pending entries.
    // 10-second timeout prevents infinite blocking if the broker never responds.
    if (qos > 0)
    {
        std::unique_lock<std::mutex> lk(mPubMutex);
        bool completed = mPubCond.wait_for(lk, std::chrono::seconds(10), [&]
        {
            auto it = mPendingPubs.find(pid);
            if (it != mPendingPubs.end() && !it->second)
            {
                return false;
            }
            return true;
        });
        // Clean up: erase the entry whether it completed or timed out
        mPendingPubs.erase(pid);
        if (!completed)
        {
            std::cerr << "publish: timed out waiting for QoS acknowledgement\n";
            return false;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  topic    [in] topic filter to subscribe to
/// @param  qos      [in] requested QoS level
/// @param  callback [in] message callback for received messages
///
/// @return bool - true on successful SUBSCRIBE send
///
/// @brief
/// Send a SUBSCRIBE packet and register a message callback. Uses
/// instance-level subscribe ID counter (thread-safe).
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::subscribe(const std::string& topic,
                            uint8_t qos,
                            std::function<void(const std::string&,
                                               const std::vector<uint8_t>&)> callback)
{
    if (!mConnected)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(mCallbackMutex);
        mMsgCallback = std::move(callback);
    }

    // Build SUBSCRIBE variable header + payload per MQTT 3.1.1 section 3.8
    std::vector<uint8_t> payload;
    uint16_t pid;
    {
        std::lock_guard<std::mutex> lk(mPubMutex);
        pid = mNextSubscribeId++;
        if (mNextSubscribeId == 0)
        {
            mNextSubscribeId = 1;
        }
    }
    // Packet ID: 2-byte big-endian, required for SUBSCRIBE per MQTT spec
    // Broker echoes this in SUBACK so client can match response to request
    payload.push_back(static_cast<uint8_t>(pid >> 8));
    payload.push_back(static_cast<uint8_t>(pid & 0xFF));

    // Each subscription entry: 2-byte big-endian topic length + topic UTF-8
    // string bytes + 1-byte requested QoS level
    uint16_t tlen = static_cast<uint16_t>(topic.size());
    payload.push_back(static_cast<uint8_t>(tlen >> 8));
    payload.push_back(static_cast<uint8_t>(tlen & 0xFF));
    payload.insert(payload.end(), topic.begin(), topic.end());
    payload.push_back(qos);

    // Assemble the full SUBSCRIBE packet
    std::vector<uint8_t> packet;
    // SUBSCRIBE fixed header byte 0x82: high nibble 0x80 = type 8 (SUBSCRIBE),
    // low nibble 0x02 = bit 1 set, which is required by MQTT 3.1.1 spec
    packet.push_back(CTRL_SUBSCRIBE);
    auto remSub = encodeRemainingLength(payload.size());
    packet.insert(packet.end(), remSub.begin(), remSub.end());
    packet.insert(packet.end(), payload.begin(), payload.end());

    // Send
    ssize_t sent = socket_write(mSock, packet.data(), packet.size());
    if (sent != static_cast<ssize_t>(packet.size()))
    {
        perror("subscribe write");
        return false;
    }
    mLastPacketSent = std::chrono::steady_clock::now();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true on successful CONNACK parse (return code 0)
///
/// @brief
/// Read and parse a CONNACK packet after sending CONNECT. Verifies
/// the packet type, remaining length, and return code.
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::parseConnAck(void)
{
    // Read fixed header byte and verify it is CONNACK (0x20)
    uint8_t hdr;
    if (socket_read(mSock, &hdr, 1) != 1)
    {
        return false;
    }
    if (hdr != CTRL_CONNACK)
    {
        return false;
    }

    // Remaining length must be exactly 2 per MQTT 3.1.1 spec (section 3.2):
    // the CONNACK variable header is always 2 bytes, no payload
    size_t remLen;
    if (!CommUtils::decodeRemainingLength(mSock, remLen) || remLen != 2)
    {
        return false;
    }

    // Read the 2-byte CONNACK variable header:
    //   data[0] = Connect Acknowledge Flags (bit 0 = Session Present flag)
    //   data[1] = Connect Return Code (0x00 = accepted, non-zero = error)
    uint8_t data[2];
    if (CommUtils::readFully(mSock, data, 2) != 2)
    {
        return false;
    }
    // Return code 0 means connection accepted by the broker
    return (data[1] == 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Receive loop thread entry point. Reads incoming packets from the broker
/// and handles PUBLISH messages (invoking the callback) and QoS ACKs
/// (PUBACK, PUBCOMP). Exits on socket error or disconnect.
///
/////////////////////////////////////////////////////////////////////////////
void cCommClient::receiveLoop(void)
{
    mLastPacketSent = std::chrono::steady_clock::now();

    while (mConnected)
    {
        // Use select() with a timeout to allow periodic PINGREQ sending.
        // Timeout is half the keep-alive interval (30 seconds for 60s keepAlive).
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(mSock, &readfds);
        struct timeval tv;
        tv.tv_sec = KEEP_ALIVE_SECONDS / 2;
        tv.tv_usec = 0;

        int selectResult = ::select(mSock + 1, &readfds, nullptr, nullptr, &tv);
        if (selectResult < 0)
        {
            // select error, exit loop
            break;
        }
        if (selectResult == 0)
        {
            // Timeout: check if we need to send PINGREQ
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - mLastPacketSent).count();
            if (elapsed >= KEEP_ALIVE_SECONDS / 2)
            {
                // Send PINGREQ to keep the connection alive
                uint8_t pingReq[2] = { CTRL_PINGREQ, 0x00 };
                ssize_t sent = socket_write(mSock, pingReq, 2);
                if (sent != 2)
                {
                    break;
                }
                mLastPacketSent = std::chrono::steady_clock::now();
            }
            continue;
        }

        uint8_t hdr;
        if (socket_read(mSock, &hdr, 1) != 1)
        {
            break;
        }

        // Extract packet type from the high nibble (bits 7-4) of the fixed header
        // e.g., 0x30 >> 4 = 3 (PUBLISH), 0x40 >> 4 = 4 (PUBACK)
        uint8_t packetType = hdr >> 4;

        // Decode the VarInt remaining length (covers everything after this field)
        size_t remLen;
        if (!CommUtils::decodeRemainingLength(mSock, remLen))
        {
            break;
        }

        if (packetType == PKT_PUBLISH)
        {
            // Extract PUBLISH flags from the low nibble (bits 3-0) of the fixed header:
            //   bit 3 (0x08) = DUP flag (re-delivery of an earlier attempt)
            //   bits 2-1 (0x06) = QoS level (0, 1, or 2), shifted right 1
            //   bit 0 (0x01) = RETAIN flag (broker stores as last known good)
            uint8_t flags = hdr & 0x0F;
            [[maybe_unused]] const bool dup    = flags & 0x08;
            const uint8_t qos = (flags & 0x06) >> 1;
            [[maybe_unused]] const bool retain = flags & 0x01;

            // Read topic name: 2-byte big-endian length prefix
            uint8_t lenBuf[2];
            if (CommUtils::readFully(mSock, lenBuf, 2) != 2)
            {
                break;
            }
            // Reconstruct 16-bit topic length from big-endian bytes
            uint16_t topicLen = (uint16_t(lenBuf[0]) << 8) | lenBuf[1];

            // MQTT 3.1.1 section 3.3.2.1: a PUBLISH topic name must not be empty.
            // Reject before indexing &topic[0] (empty-string indexing is invalid).
            if (topicLen == 0)
            {
                break;
            }

            // Read topic string bytes
            std::string topic;
            topic.resize(topicLen);
            if (CommUtils::readFully(mSock, &topic[0], topicLen) != static_cast<ssize_t>(topicLen))
            {
                break;
            }

            // Packet Identifier: 2-byte big-endian, only present when QoS > 0
            // Used to correlate acknowledgement handshakes (PUBACK/PUBREC/PUBCOMP)
            uint16_t packetId = 0;
            if (qos > 0)
            {
                if (CommUtils::readFully(mSock, lenBuf, 2) != 2)
                {
                    break;
                }
                packetId = (uint16_t(lenBuf[0]) << 8) | lenBuf[1];
            }

            // Calculate payload size by subtracting the variable header from remaining length:
            //   headerSize = 2 (topic length field) + topicLen + 2 (packet ID, if QoS > 0)
            //   payload is whatever bytes remain after the variable header
            size_t headerSize = 2 + topicLen + (qos > 0 ? 2 : 0);
            size_t payloadLen = remLen - headerSize;
            std::vector<uint8_t> payload(payloadLen);
            if (payloadLen > 0)
            {
                if (CommUtils::readFully(mSock, payload.data(), payloadLen)
                    != static_cast<ssize_t>(payloadLen))
                {
                    break;
                }
            }

            // Send QoS acknowledgement back to the broker:
            //   QoS 1: PUBACK completes the delivery (2-step handshake)
            //   QoS 2: PUBREC starts the 4-step handshake (PUBREC -> PUBREL -> PUBCOMP)
            if (qos == QOS1)
            {
                if (!sendPubAck(packetId))
                {
                    break;
                }
            }
            else if (qos == QOS2)
            {
                if (!sendPubRec(packetId))
                {
                    break;
                }
            }

            // Invoke user callback with the received topic and payload
            {
                std::lock_guard<std::mutex> lk(mCallbackMutex);
                if (mMsgCallback)
                {
                    mMsgCallback(topic, payload);
                }
            }

            continue;
        }
        else if (packetType == PKT_PUBREC)
        {
            // QoS 2 step 2: server acknowledged our PUBLISH with PUBREC.
            // Read the 2-byte packet ID and send PUBREL to continue the handshake.
            uint8_t buf[2];
            if (CommUtils::readFully(mSock, buf, 2) != 2)
            {
                break;
            }
            uint16_t pid = (uint16_t(buf[0]) << 8) | buf[1];
            if (!sendPubRel(pid))
            {
                break;
            }
        }
        else if (packetType == PKT_PUBACK || packetType == PKT_PUBCOMP)
        {
            // PUBACK (QoS 1 completion) and PUBCOMP (QoS 2 final step) both signal
            // that the broker has finished processing our published message
            // Both carry a 2-byte big-endian packet ID matching our original PUBLISH
            uint8_t buf[2];
            if (CommUtils::readFully(mSock, buf, 2) != 2)
            {
                break;
            }
            uint16_t pid = (static_cast<uint16_t>(buf[0]) << 8)
                         | buf[1];
            {
                // Mark this packet ID as acknowledged in the pending map
                std::lock_guard<std::mutex> lk(mPubMutex);
                mPendingPubs[pid] = true;
            }
            // Wake the publish() thread so it can check if all pending acks are complete
            mPubCond.notify_all();
        }
        else if (packetType == PKT_SUBACK)
        {
            // SUBACK: read packet ID (2 bytes) + granted QoS bytes
            // Consume the remaining length to stay in sync
            std::vector<uint8_t> subackData(remLen);
            if (remLen > 0)
            {
                if (CommUtils::readFully(mSock, subackData.data(), remLen)
                    != static_cast<ssize_t>(remLen))
                {
                    break;
                }
            }
        }
        else if (packetType == PKT_PINGRESP)
        {
            // PINGRESP has no payload (remaining length = 0)
            // Nothing to read, just continue
        }
        else
        {
            // Unknown or unhandled packet type
            // Read and discard the remaining bytes to stay in sync with the byte stream
            std::vector<uint8_t> skip(remLen);
            if (remLen > 0)
            {
                if (CommUtils::readFully(mSock, skip.data(), remLen)
                    != static_cast<ssize_t>(remLen))
                {
                    break;
                }
            }
        }
    }

    mConnected = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  packetId [in] packet identifier to ACK
///
/// @return bool - true on successful PUBACK send
///
/// @brief
/// Send PUBACK (QoS 1 acknowledgement)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::sendPubAck(uint16_t packetId)
{
    return CommUtils::sendControlPacket(mSock, CTRL_PUBACK, packetId);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  packetId [in] packet identifier to ACK
///
/// @return bool - true on successful PUBREC send
///
/// @brief
/// Send PUBREC (QoS 2 step 1)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::sendPubRec(uint16_t packetId)
{
    return CommUtils::sendControlPacket(mSock, CTRL_PUBREC, packetId);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  packetId [in] packet identifier to release
///
/// @return bool - true on successful PUBREL send
///
/// @brief
/// Send PUBREL (QoS 2 step 2)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::sendPubRel(uint16_t packetId)
{
    return CommUtils::sendControlPacket(mSock, CTRL_PUBREL, packetId);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  packetId [in] packet identifier to complete
///
/// @return bool - true on successful PUBCOMP send
///
/// @brief
/// Send PUBCOMP (QoS 2 step 3)
///
/////////////////////////////////////////////////////////////////////////////
bool cCommClient::sendPubComp(uint16_t packetId)
{
    return CommUtils::sendControlPacket(mSock, CTRL_PUBCOMP, packetId);
}
