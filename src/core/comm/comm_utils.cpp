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
 * @file comm_utils.cpp
 *
 * @brief Shared utility functions and data types for the MQTT communication layer.
 *
 * Implements the CommUtils namespace and supporting structures used by both
 * cCommClient and cCommServer. Provides MQTT Variable Byte Integer encoding
 * and decoding, simple control packet construction (PUBACK, PUBREC, PUBREL,
 * PUBCOMP, UNSUBACK), cross-platform socket read/write wrappers, and the
 * sClientInfo / sRetainedMessage data types.
 *
 * @section commutils_encoding Packet Encoding
 * - encodeRemainingLength(): encodes integers as MQTT Variable Byte Integers
 *   (1-4 bytes, 7 bits per byte with continuation bit)
 * - decodeRemainingLength(): decodes Variable Byte Integers from a socket stream
 * - buildSimplePacket(): constructs fixed-header MQTT control packets (PUBACK,
 *   PUBREC, PUBREL, PUBCOMP, UNSUBACK) from packet type and packet identifier
 *
 * @section commutils_sockets Cross-Platform Socket I/O
 * - socket_read(): platform-abstracted socket read (recv on Windows, read on POSIX)
 * - socket_write(): platform-abstracted socket write (send on Windows, write on POSIX)
 * - socket_close(): platform-abstracted socket close (closesocket on Windows,
 *   close on POSIX)
 *
 * @section commutils_winsock Windows Socket Initialization
 * On Windows, a static WinsockInit singleton calls WSAStartup/WSACleanup to
 * manage Winsock lifecycle for the entire process.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCommClient MQTT client using these utilities
 * @see cCommServer MQTT broker using these utilities
 * @see sClientInfo Per-client connection state structure
 * @see sRetainedMessage Stored retained message structure
 */

#include "comm_utils.h"
#include <iostream>
#include <cerrno>


namespace CommUtils
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  buf [in] pointer to 2-byte big-endian value
///
/// @return uint16_t - decoded value
///
/// @brief
/// Extract a 16-bit unsigned integer from a big-endian byte buffer
///
/////////////////////////////////////////////////////////////////////////////
uint16_t readUint16BE(const uint8_t* buf)
{
    return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  vec [in,out] byte vector to append to
/// @param  val [in] 16-bit value to append in big-endian format
///
/// @return nothing
///
/// @brief
/// Append a 16-bit unsigned integer in big-endian format to a byte vector
///
/////////////////////////////////////////////////////////////////////////////
void appendUint16BE(std::vector<uint8_t>& vec, uint16_t val)
{
    vec.push_back(static_cast<uint8_t>(val >> 8));
    vec.push_back(static_cast<uint8_t>(val & 0xFF));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  fd [in] socket file descriptor
/// @param  buf [in] destination buffer
/// @param  count [in] number of bytes to read
///
/// @return count on full success; a short count (< count) on EOF; -1 on error
///         with no bytes read
///
/// @brief
/// Read exactly count bytes from a socket, looping over partial reads (a single
/// socket_read may return fewer bytes than requested). Retries on EINTR. Does
/// NOT retry on EAGAIN/EWOULDBLOCK: a mid-packet read timeout returns the bytes
/// read so far (a short count), so the caller's "!= count" check disconnects
/// rather than spinning. count == 0 returns 0.
///
/////////////////////////////////////////////////////////////////////////////
ssize_t readFully(int fd, void* buf, size_t count)
{
    size_t total = 0;
    char* dest = static_cast<char*>(buf);
    while (total < count)
    {
        ssize_t r = socket_read(fd, dest + total, count - total);
        if (r > 0)
        {
            total += static_cast<size_t>(r);
            continue;
        }
        if (r == 0)
        {
            return static_cast<ssize_t>(total);   // EOF: peer closed
        }
        if (errno == EINTR)
        {
            continue;   // interrupted syscall: retry
        }
        // EAGAIN/EWOULDBLOCK (timeout) or hard error.
        return (total == 0) ? -1 : static_cast<ssize_t>(total);
    }
    return static_cast<ssize_t>(total);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  vec [in,out] byte vector to append to
/// @param  str [in] string to append with MQTT length prefix
///
/// @return nothing
///
/// @brief
/// Append an MQTT length-prefixed UTF-8 string to a byte vector
///
/////////////////////////////////////////////////////////////////////////////
void appendMqttString(std::vector<uint8_t>& vec, const std::string& str)
{
    appendUint16BE(vec, static_cast<uint16_t>(str.size()));
    vec.insert(vec.end(), str.begin(), str.end());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sock [in] socket to read from
/// @param  out  [out] decoded string
///
/// @return bool - true on success
///
/// @brief
/// Read an MQTT length-prefixed UTF-8 string from a socket
///
/////////////////////////////////////////////////////////////////////////////
bool readMqttString(int sock, std::string& out)
{
    uint8_t buf[2];
    if (readFully(sock, buf, 2) != 2)
    {
        return false;
    }
    uint16_t len = readUint16BE(buf);
    out.resize(len);
    if (len > 0)
    {
        if (readFully(sock, &out[0], len) != static_cast<ssize_t>(len))
        {
            return false;
        }
    }
    return true;
}

#ifdef _WIN32
/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Initialize Windows socket subsystem (WSAStartup)
///
/////////////////////////////////////////////////////////////////////////////
WinsockInit::WinsockInit(void)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clean up Windows socket subsystem (WSACleanup)
///
/////////////////////////////////////////////////////////////////////////////
WinsockInit::~WinsockInit(void)
{
    WSACleanup();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return WinsockInit& - singleton instance
///
/// @brief
/// Get the singleton WinsockInit instance to ensure WSAStartup is called
///
/////////////////////////////////////////////////////////////////////////////
WinsockInit& getWinsockInit()
{
    static WinsockInit instance;
    return instance;
}
#endif

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Default constructor for sClientInfo
///
/////////////////////////////////////////////////////////////////////////////
sClientInfo::sClientInfo(void)
    : socket(-1)
    , cleanSession(true)
    , keepAlive(DEFAULT_KEEP_ALIVE_SECONDS)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  clientId [in] client identifier string
/// @param  sock     [in] socket file descriptor
/// @param  clean    [in] whether to use clean session (default true)
///
/// @return nothing
///
/// @brief
/// Construct a sClientInfo with the given client ID and socket
///
/////////////////////////////////////////////////////////////////////////////
sClientInfo::sClientInfo(const std::string& clientId, int sock, bool clean)
    : id(clientId)
    , socket(sock)
    , cleanSession(clean)
    , keepAlive(DEFAULT_KEEP_ALIVE_SECONDS)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return uint16_t - next available packet identifier (1-65535, wraps)
///
/// @brief
/// Allocate a unique packet ID for this client, wrapping around zero
///
/////////////////////////////////////////////////////////////////////////////
uint16_t sClientInfo::allocatePacketId(void)
{
    uint16_t packetId = nextPacketId++;

    // MQTT spec reserves packet ID 0; valid IDs are 1-65535.
    // When uint16_t wraps from 65535 to 0, skip back to 1.
    if (nextPacketId == 0)
    {
        nextPacketId = 1;
    }
    return packetId;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Default constructor for sRetainedMessage
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Default constructor for sInflightEntry
///
/////////////////////////////////////////////////////////////////////////////
sInflightEntry::sInflightEntry(void)
    : state(eInflightState::WAIT_PUBACK)
    , retries(0)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Default constructor for sRetainedMessage
///
/////////////////////////////////////////////////////////////////////////////
sRetainedMessage::sRetainedMessage(void)
    : qos(QOS0)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  data [in] payload bytes
/// @param  q    [in] QoS level
///
/// @return nothing
///
/// @brief
/// Construct a sRetainedMessage with the given payload and QoS
///
/////////////////////////////////////////////////////////////////////////////
sRetainedMessage::sRetainedMessage(const std::vector<uint8_t>& data, uint8_t q)
    : payload(data)
    , qos(q)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  length [in] the remaining length value to encode
///
/// @return std::vector<uint8_t> - encoded bytes per MQTT variable-length integer
///
/// @brief
/// Encode a remaining length value as an MQTT Variable Byte Integer.
/// Values up to 268,435,455 are supported per the MQTT 3.1.1 spec.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<uint8_t> encodeRemainingLength(size_t length)
{
    std::vector<uint8_t> result;

    // Encode as MQTT Variable Byte Integer: each byte carries 7 data bits
    // (bits 0-6), with bit 7 as a continuation flag indicating more bytes follow.
    while (length > 0)
    {
        // Extract the lowest 7 bits of the remaining length
        uint8_t byte = length & 0x7F;

        // Shift right by 7 to discard the bits just encoded
        length >>= 7;

        // If more bits remain, set the continuation bit (bit 7) to signal
        // the decoder that additional bytes follow
        if (length > 0)
        {
            byte |= 0x80;
        }
        result.push_back(byte);
    }

    // Special case: length=0 produces no loop iterations, so emit a single
    // zero byte to represent it
    if (result.empty())
    {
        result.push_back(0);
    }
    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sock   [in]  socket file descriptor to read from
/// @param  outLen [out] decoded remaining length value
///
/// @return bool - true on success, false on malformed data or read error
///
/// @brief
/// Decode an MQTT Variable Byte Integer remaining length from the socket.
/// Reads up to 4 bytes as specified by MQTT 3.1.1.
///
/////////////////////////////////////////////////////////////////////////////
bool decodeRemainingLength(int sock, size_t& outLen)
{
    outLen = 0;

    // Position weight for the current byte: 1, 128, 16384, 2097152 (128^0 through 128^3).
    // Each successive byte contributes its 7 data bits at a higher power of 128.
    size_t multiplier = 1;

    // MQTT Variable Byte Integer uses at most 4 bytes, encoding up to 268,435,455
    for (int i = 0; i < 4; ++i)
    {
        uint8_t byte;
        int bytesRead = socket_read(sock, reinterpret_cast<char*>(&byte), 1);
        if (bytesRead != 1)
        {
            std::cerr << "Error reading remaining length byte\n";
            return false;
        }

        // Accumulate the lower 7 bits (mask 0x7F) scaled by the current position weight
        outLen += (byte & 0x7F) * multiplier;

        // Bit 7 clear (0x80 not set) means this is the final byte of the encoding
        if ((byte & 0x80) == 0)
        {
            if (outLen > MAX_REMAINING_LENGTH)
            {
                std::cerr << "Remaining length exceeds policy cap\n";
                return false;
            }
            return true;
        }

        // Advance to the next 128x position weight for the next byte
        multiplier *= 128;

        // Overflow guard: multiplier beyond 128^3 means a 5th byte would be needed,
        // which exceeds the MQTT spec maximum of 4 bytes
        if (multiplier > 128 * 128 * 128)
        {
            std::cerr << "Malformed remaining length\n";
            return false;
        }
    }

    std::cerr << "Remaining length exceeds maximum\n";
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  controlType [in] MQTT control packet type byte
/// @param  packetId    [in] packet identifier
///
/// @return std::vector<uint8_t> - complete control packet bytes
///
/// @brief
/// Build a simple MQTT control packet with a 2-byte packet identifier payload
///
/////////////////////////////////////////////////////////////////////////////
std::vector<uint8_t> buildControlPacket(uint8_t controlType, uint16_t packetId)
{
    std::vector<uint8_t> packet;

    // Fixed header byte identifies the MQTT packet type (e.g., PUBACK, PUBREL)
    packet.push_back(controlType);

    // Remaining length is always 2 for simple control packets: the payload
    // consists solely of a 2-byte packet identifier
    auto remLen = encodeRemainingLength(2);
    packet.insert(packet.end(), remLen.begin(), remLen.end());

    // Encode the 16-bit packet ID in big-endian (network byte order): MSB first
    packet.push_back(static_cast<uint8_t>(packetId >> 8));
    packet.push_back(static_cast<uint8_t>(packetId & 0xFF));

    return packet;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  socket      [in] socket file descriptor to write to
/// @param  controlType [in] MQTT control packet type byte
/// @param  packetId    [in] packet identifier
///
/// @return bool - true if all bytes were sent successfully
///
/// @brief
/// Build and send a simple MQTT control packet over the socket
///
/////////////////////////////////////////////////////////////////////////////
bool sendControlPacket(int socket, uint8_t controlType, uint16_t packetId)
{
    auto packet = buildControlPacket(controlType, packetId);

    // All-or-nothing write: the entire packet must be sent in one call.
    // A partial write is treated as failure since MQTT packets are atomic.
    ssize_t sent = socket_write(socket, packet.data(), packet.size());
    if (sent != static_cast<ssize_t>(packet.size()))
    {
        return false;
    }

    return true;
}

}
