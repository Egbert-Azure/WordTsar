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

#pragma once

#include "commdefs.h"
#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include <chrono>

// Platform-specific socket includes and abstractions.
// These inline wrappers must remain in the header so all translation units
// share the same platform-appropriate I/O calls.
#ifdef _WIN32
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>

    #ifndef _SSIZE_T_DEFINED
        #include "../include/config.h"
    #endif

    #define close closesocket
    typedef int socklen_t;

    #define SHUT_RDWR SD_BOTH

    #undef errno
    #define errno WSAGetLastError()

    inline ssize_t socket_read(int fd, void* buf, size_t count)
    {
        return recv(fd, reinterpret_cast<char*>(buf), static_cast<int>(count), 0);
    }

    inline ssize_t socket_write(int fd, const void* buf, size_t count)
    {
        return send(fd, reinterpret_cast<const char*>(buf), static_cast<int>(count), 0);
    }

    inline void socket_set_nosigpipe(int /*fd*/)
    {
    }
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>

    inline ssize_t socket_read(int fd, void* buf, size_t count)
    {
        return ::read(fd, buf, count);
    }

    inline ssize_t socket_write(int fd, const void* buf, size_t count)
    {
        return ::write(fd, buf, count);
    }

    inline void socket_set_nosigpipe(int fd)
    {
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    }
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>

    inline ssize_t socket_read(int fd, void* buf, size_t count)
    {
        return ::read(fd, buf, count);
    }

    // MSG_NOSIGNAL prevents the kernel from raising SIGPIPE when the peer has
    // closed; the call returns -1 with errno=EPIPE instead.
    inline ssize_t socket_write(int fd, const void* buf, size_t count)
    {
        return ::send(fd, buf, count, MSG_NOSIGNAL);
    }

    inline void socket_set_nosigpipe(int /*fd*/)
    {
    }
#endif

namespace CommUtils
{

#ifdef _WIN32
    /// @brief RAII wrapper for Windows socket initialization
    class WinsockInit
    {
    public:
        WinsockInit(void);
        ~WinsockInit(void);
    };

    WinsockInit& getWinsockInit(void);
#endif

    // Big-endian uint16 helpers
    uint16_t readUint16BE(const uint8_t* buf);
    void appendUint16BE(std::vector<uint8_t>& vec, uint16_t val);

    // Read exactly count bytes, looping over partial reads. Returns count on
    // full success, a short count (< count) on EOF, or -1 on error with nothing
    // read. Retries on EINTR; does NOT retry on EAGAIN/EWOULDBLOCK (a mid-packet
    // timeout returns short so the caller disconnects). Used for multi-byte field
    // reads; single-byte/keepalive reads keep using socket_read directly.
    ssize_t readFully(int fd, void* buf, size_t count);

    // MQTT length-prefixed string helpers
    void appendMqttString(std::vector<uint8_t>& vec, const std::string& str);
    bool readMqttString(int sock, std::string& out);

    std::vector<uint8_t> encodeRemainingLength(size_t length);

    static constexpr size_t MAX_REMAINING_LENGTH = 2 * 1024 * 1024;

    bool decodeRemainingLength(int sock, size_t& outLen);

    std::vector<uint8_t> buildControlPacket(uint8_t controlType, uint16_t packetId);

    bool sendControlPacket(int socket, uint8_t controlType, uint16_t packetId);

    /// @enum eInflightState
    /// @brief QoS message delivery states for in-flight tracking
    enum class eInflightState
    {
        WAIT_PUBACK,
        WAIT_PUBREC,
        WAIT_PUBREL
    };

    /// @struct sInflightEntry
    /// @brief QoS in-flight message state tracking for retransmission
    struct sInflightEntry
    {
        std::vector<uint8_t> packet;
        eInflightState state;
        std::chrono::steady_clock::time_point timestamp;
        int retries;

        sInflightEntry(void);
    };

    /// @struct sClientInfo
    /// @brief Connected client state including Will message, subscriptions, and in-flight tracking
    struct sClientInfo
    {
        std::string id;
        int socket;
        std::vector<std::pair<std::string, uint8_t>> subscriptions;
        bool cleanSession;
        uint16_t keepAlive;

        /// @struct sWill
        /// @brief MQTT Last Will and Testament message published on unexpected disconnect
        struct sWill
        {
            std::string topic;
            std::vector<uint8_t> payload;
            uint8_t qos;
            bool retain;
        };
        sWill will;
        bool hasWill = false;

        std::map<uint16_t, sInflightEntry> inflightMessages;
        uint16_t nextPacketId = 1;

        sClientInfo(void);
        sClientInfo(const std::string& clientId, int sock, bool clean = true);

        uint16_t allocatePacketId(void);
    };

    /// @struct sRetainedMessage
    /// @brief Topic-retained message delivered to new subscribers
    struct sRetainedMessage
    {
        std::vector<uint8_t> payload;
        uint8_t qos;

        sRetainedMessage(void);
        sRetainedMessage(const std::vector<uint8_t>& data, uint8_t q);
    };

}
