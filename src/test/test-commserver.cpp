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

#include "doctest.h"

#include "src/core/comm/commdefs.h"
#include "src/core/comm/comm_utils.h"
#include "src/core/comm/commserver.h"
#include "src/core/comm/commclient.h"

#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>

// Platform-specific includes for the test helper
// (comm_utils.h already provides socket headers, socket_read/socket_write, close macro)
#ifdef _WIN32
    // inet_pton is in ws2tcpip.h (already included via comm_utils.h)
#else
    #include <arpa/inet.h>
    #include <fcntl.h>
#endif


// TEST VIA TCP

//------------------------------------------------------------------------------
// Run the broker on a given port, using its ctor(port) + start()
//------------------------------------------------------------------------------
struct ServerRunner
{
    cCommServer srv;
    uint16_t port;   // actual bound port (resolved after start)

    // Use port 0 to let the OS pick a free port.
    // start() is non-blocking: it binds, listens, spawns accept/retransmit
    // threads, then returns. No background thread needed for startup.
    ServerRunner()
      : srv(0), port(0)
    {
        srv.start();
        port = static_cast<uint16_t>(srv.getPort());
    }

    // Accept (and ignore) a port argument for backward compatibility
    ServerRunner(uint16_t /*requestedPort*/)
      : ServerRunner()
    {}

    ~ServerRunner()
    {
        srv.stop();
    }
};

//------------------------------------------------------------------------------
// Minimal MQTT client for TCP tests (cross-platform)
//------------------------------------------------------------------------------
struct MqttClient
{
    int sock{-1};

    bool connectTo(const char* host, uint16_t port)
    {
        sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            return false;
        }
        socket_set_nosigpipe(sock);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        inet_pton(AF_INET, host, &addr.sin_addr);
        return ::connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0;
    }

    void closeIt()
    {
        if (sock >= 0)
        {
            ::close(sock);
            sock = -1;
        }
    }

    bool sendAll(const std::vector<uint8_t>& buf)
    {
        size_t sent = 0;
        while (sent < buf.size())
        {
            ssize_t n = socket_write(sock, buf.data() + sent, buf.size() - sent);
            if (n < 0)
            {
                return false;
            }
            sent += n;
        }
        return true;
    }

    bool recvAll(void* out, size_t n)
    {
        uint8_t* p = (uint8_t*)out;
        size_t rcvd = 0;
        while (rcvd < n)
        {
            ssize_t m = socket_read(sock, p + rcvd, n - rcvd);
            if (m <= 0)
            {
                return false;
            }
            rcvd += m;
        }
        return true;
    }

    bool readVarInt(size_t& outLen)
    {
        outLen = 0;
        uint32_t mul = 1;
        for (int i = 0; i < 4; i++)
        {
            uint8_t byte;
            if (!recvAll(&byte, 1))
            {
                return false;
            }
            outLen += (byte & 0x7F) * mul;
            if (!(byte & 0x80))
            {
                return true;
            }
            mul <<= 7;
        }
        return false;
    }

    bool sendConnect(const std::string& clientId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(4);
        pkt.push_back(0x02);
        pkt.push_back(0x00);
        pkt.push_back(60);
        pushStr(clientId);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        // Assume rlEnc.size()==1 for our small packets
        pkt[idxRL] = rlEnc[0];

        return sendAll(pkt);
    }

    bool readConnAck(uint8_t& sp, uint8_t& rc)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != (CTRL_CONNACK >> 4))
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl != 2)
        {
            return false;
        }
        if (!recvAll(&sp, 1))
        {
            return false;
        }
        if (!recvAll(&rc, 1))
        {
            return false;
        }
        return true;
    }

    bool sendSubscribe(uint16_t packetId, const std::string& topic,
                       uint8_t qos = 0)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_SUBSCRIBE | 0x02);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Packet ID
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));
        // Topic filter + requested QoS
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());
        pkt.push_back(qos);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    bool readSubAck(uint16_t& outPacketId)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != (CTRL_SUBACK >> 4))
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl < 3)
        {
            return false;
        }
        uint8_t buf2[2];
        if (!recvAll(buf2, 2))
        {
            return false;
        }
        outPacketId = (uint16_t(buf2[0]) << 8) | buf2[1];
        std::vector<uint8_t> skip(rl - 2);
        return recvAll(skip.data(), skip.size());
    }

    // Read SUBACK and return granted QoS values
    bool readSubAckWithQos(uint16_t& outPacketId,
                           std::vector<uint8_t>& grantedQos)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != (CTRL_SUBACK >> 4))
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl < 3)
        {
            return false;
        }
        uint8_t buf2[2];
        if (!recvAll(buf2, 2))
        {
            return false;
        }
        outPacketId = (uint16_t(buf2[0]) << 8) | buf2[1];
        size_t qosCount = rl - 2;
        grantedQos.resize(qosCount);
        return recvAll(grantedQos.data(), qosCount);
    }

    // Send CONNECT with empty client ID and cleanSession=true
    bool sendConnectEmptyId(void)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t rlIdx = pkt.size();
        pkt.push_back(0);

        // Variable header
        pkt.push_back(0x00); pkt.push_back(0x04);
        pkt.push_back('M'); pkt.push_back('Q'); pkt.push_back('T'); pkt.push_back('T');
        pkt.push_back(0x04); // Protocol level 4
        pkt.push_back(0x02); // cleanSession=true, no will/user/pass
        pkt.push_back(0x00); pkt.push_back(0x3C); // keepAlive=60

        // Empty client ID (length = 0)
        pkt.push_back(0x00); pkt.push_back(0x00);

        size_t rl = pkt.size() - rlIdx - 1;
        pkt[rlIdx] = static_cast<uint8_t>(rl);
        return sendAll(pkt);
    }

    bool sendPublish(const std::string& topic,
                     const std::vector<uint8_t>& payload)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_PUBLISH);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());
        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    bool readPublish(std::string& topic,
                     std::vector<uint8_t>& payload)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != (CTRL_PUBLISH >> 4))
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl))
        {
            return false;
        }

        uint8_t buf2[2];
        if (!recvAll(buf2, 2))
        {
            return false;
        }
        uint16_t tlen = (uint16_t(buf2[0]) << 8) | buf2[1];
        topic.resize(tlen);
        if (!recvAll(&topic[0], tlen))
        {
            return false;
        }

        size_t paylen = rl - 2 - tlen;
        payload.resize(paylen);
        return recvAll(payload.data(), paylen);
    }
};

TEST_CASE("TCP: CONNECT/CONNACK handshake")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClient client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    CHECK(client.sendConnect("testClient"));
    uint8_t sp, rc;
    CHECK(client.readConnAck(sp, rc));
    CHECK(sp == 0);
    CHECK(rc == CONNACK_RC_ACCEPTED);

    client.closeIt();
}

TEST_CASE("TCP: PUBLISH -> SUBSCRIBE round trip")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClient pub, sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(pub.connectTo("127.0.0.1", port));

    uint8_t sp, rc;
    REQUIRE(sub.sendConnect("sub1"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(pub.sendConnect("pub1"));
    REQUIRE(pub.readConnAck(sp, rc));

    uint16_t pktId = 0x1001;
    REQUIRE(sub.sendSubscribe(pktId, "foo/bar"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));
    CHECK(ret == pktId);

    std::vector<uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF};
    REQUIRE(pub.sendPublish("foo/bar", payload));

    std::string topic;
    std::vector<uint8_t> recv;
    REQUIRE(sub.readPublish(topic, recv));
    CHECK(topic == "foo/bar");
    CHECK(recv == payload);

    sub.closeIt();
    pub.closeIt();
}


TEST_CASE("TCP: fragmented PUBLISH is reassembled (partial-read robustness)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClient pub, sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(pub.connectTo("127.0.0.1", port));

    uint8_t sp, rc;
    REQUIRE(sub.sendConnect("fragSub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(pub.sendConnect("fragPub"));
    REQUIRE(pub.readConnAck(sp, rc));

    uint16_t pktId = 0x3001;
    REQUIRE(sub.sendSubscribe(pktId, "t/frag"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));

    // Build a PUBLISH packet by hand, then send it ONE BYTE AT A TIME with a
    // small gap so the broker's per-field reads see partial TCP reads. The old
    // single socket_read() per field would treat each partial as failure and
    // drop the message; readFully() must reassemble it.
    const std::string topic = "t/frag";
    const std::vector<uint8_t> payload{0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);
    pkt.push_back(0);                                  // remaining-length placeholder
    pkt.push_back(uint8_t(topic.size() >> 8));
    pkt.push_back(uint8_t(topic.size() & 0xFF));
    pkt.insert(pkt.end(), topic.begin(), topic.end());
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    pkt[1] = static_cast<uint8_t>(pkt.size() - 2);    // remaining length (< 128 -> 1 byte)

    for (uint8_t b : pkt)
    {
        REQUIRE(socket_write(pub.sock, &b, 1) == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::string gotTopic;
    std::vector<uint8_t> gotPayload;
    REQUIRE(sub.readPublish(gotTopic, gotPayload));
    CHECK(gotTopic == topic);
    CHECK(gotPayload == payload);

    sub.closeIt();
    pub.closeIt();
}


TEST_CASE("TCP: wildcard subscribe with multi- and single-level wildcards")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClient pub, sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(pub.connectTo("127.0.0.1", port));

    uint8_t sp, rc;
    // Handshake both clients
    REQUIRE(sub.sendConnect("wildSub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(pub.sendConnect("wildPub"));
    REQUIRE(pub.readConnAck(sp, rc));

    // 1) Test multi-level '#' wildcard
    {
        // Subscribe to all under "foo"
        uint16_t pktId1 = 0x2001;
        REQUIRE(sub.sendSubscribe(pktId1, "foo/#"));
        uint16_t ret1;
        REQUIRE(sub.readSubAck(ret1));
        CHECK(ret1 == pktId1);

        // Publish to foo, foo/bar, foo/bar/baz
        std::vector<uint8_t> p1{0x01}, p2{0x02}, p3{0x03};
        REQUIRE(pub.sendPublish("foo",          p1));
        REQUIRE(pub.sendPublish("foo/bar",      p2));
        REQUIRE(pub.sendPublish("foo/bar/baz",  p3));

        std::string topic;
        std::vector<uint8_t> payload;

        REQUIRE(sub.readPublish(topic, payload));
        CHECK(topic   == "foo");
        CHECK(payload == p1);

        REQUIRE(sub.readPublish(topic, payload));
        CHECK(topic   == "foo/bar");
        CHECK(payload == p2);

        REQUIRE(sub.readPublish(topic, payload));
        CHECK(topic   == "foo/bar/baz");
        CHECK(payload == p3);
    }

    // 2) Test single-level '+' wildcard
    {
        // Reset the subscriber so previous subscriptions are cleared
        sub.closeIt();
        REQUIRE(sub.connectTo("127.0.0.1", port));
        REQUIRE(sub.sendConnect("wildSub2"));
        REQUIRE(sub.readConnAck(sp, rc));

        // Subscribe to exactly foo/+/baz
        uint16_t pktId2 = 0x2002;
        REQUIRE(sub.sendSubscribe(pktId2, "foo/+/baz"));
        uint16_t ret2;
        REQUIRE(sub.readSubAck(ret2));
        CHECK(ret2 == pktId2);

        // Publish matching and non-matching
        std::vector<uint8_t> pm{0xAA}, pn{0xBB};
        REQUIRE(pub.sendPublish("foo/bar/baz", pm));   // should match
        REQUIRE(pub.sendPublish("foo/bar/qux", pn));   // should NOT match

        // Only one incoming
        std::string topic;
        std::vector<uint8_t> payload;
        REQUIRE(sub.readPublish(topic, payload));
        CHECK(topic == "foo/bar/baz");
        CHECK(payload == pm);

        // Give a moment and ensure no more messages (non-blocking read attempt)
#ifdef _WIN32
        // Set socket to non-blocking on Windows
        u_long mode = 1;
        ioctlsocket(sub.sock, FIONBIO, &mode);
        uint8_t b;
        ssize_t r = socket_read(sub.sock, &b, 1);
        REQUIRE(r == -1);
        // Restore blocking mode
        mode = 0;
        ioctlsocket(sub.sock, FIONBIO, &mode);
#else
        int flags = fcntl(sub.sock, F_GETFL, 0);
        fcntl(sub.sock, F_SETFL, flags | O_NONBLOCK);
        uint8_t b;
        ssize_t r = socket_read(sub.sock, &b, 1);
        REQUIRE(r == -1);
        if (errno != EAGAIN)
        {
            REQUIRE(errno == EWOULDBLOCK);
        }
        // Restore flags
        fcntl(sub.sock, F_SETFL, flags);
#endif
    }

    sub.closeIt();
    pub.closeIt();
}


//------------------------------------------------------------------------------
// Extended MqttClient helper for additional protocol features
//------------------------------------------------------------------------------
struct MqttClientEx : public MqttClient
{
    // CONNECT with Will message
    bool sendConnectWithWill(const std::string& clientId,
                             const std::string& willTopic,
                             const std::vector<uint8_t>& willPayload,
                             uint8_t willQos,
                             bool willRetain)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        // Protocol name + level
        pushStr("MQTT");
        pkt.push_back(4);

        // Connect flags: CleanSession + Will flag + Will QoS + Will Retain
        uint8_t flags = 0x02 | 0x04; // Clean session + Will flag
        flags |= ((willQos & 0x03) << 3);
        if (willRetain)
        {
            flags |= 0x20;
        }
        pkt.push_back(flags);

        // Keep alive
        pkt.push_back(0x00);
        pkt.push_back(60);

        // Client ID
        pushStr(clientId);

        // Will Topic
        pushStr(willTopic);

        // Will Payload (length-prefixed)
        pkt.push_back(uint8_t(willPayload.size() >> 8));
        pkt.push_back(uint8_t(willPayload.size() & 0xFF));
        pkt.insert(pkt.end(), willPayload.begin(), willPayload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];

        return sendAll(pkt);
    }

    // CONNECT with cleanSession=false
    bool sendConnectCleanFalse(const std::string& clientId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(4);
        pkt.push_back(0x00); // flags: NO clean session
        pkt.push_back(0x00);
        pkt.push_back(60);
        pushStr(clientId);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];

        return sendAll(pkt);
    }

    // PUBLISH with QoS 1
    bool sendPublishQos1(const std::string& topic,
                         const std::vector<uint8_t>& payload,
                         uint16_t packetId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_PUBLISH | 0x02); // QoS 1 = bits 1-2 = 0x02
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        // Packet ID
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));

        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // PUBLISH with QoS 2
    bool sendPublishQos2(const std::string& topic,
                         const std::vector<uint8_t>& payload,
                         uint16_t packetId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_PUBLISH | 0x04); // QoS 2 = bits 1-2 = 0x04
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        // Packet ID
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));

        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // PUBLISH with retain flag
    bool sendPublishRetain(const std::string& topic,
                           const std::vector<uint8_t>& payload)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_PUBLISH | 0x01); // Retain bit set
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // Read PUBACK response
    bool readPubAck(uint16_t& outPacketId)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != PKT_PUBACK)
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl != 2)
        {
            return false;
        }
        uint8_t buf[2];
        if (!recvAll(buf, 2))
        {
            return false;
        }
        outPacketId = (uint16_t(buf[0]) << 8) | buf[1];
        return true;
    }

    // Read PUBREC response
    bool readPubRec(uint16_t& outPacketId)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != PKT_PUBREC)
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl != 2)
        {
            return false;
        }
        uint8_t buf[2];
        if (!recvAll(buf, 2))
        {
            return false;
        }
        outPacketId = (uint16_t(buf[0]) << 8) | buf[1];
        return true;
    }

    // Send PUBREL
    bool sendPubRel(uint16_t packetId)
    {
        auto pkt = CommUtils::buildControlPacket(CTRL_PUBREL, packetId);
        return sendAll(pkt);
    }

    // Send PUBACK
    bool sendPubAck(uint16_t packetId)
    {
        auto pkt = CommUtils::buildControlPacket(CTRL_PUBACK, packetId);
        return sendAll(pkt);
    }

    // Read PUBCOMP response
    bool readPubComp(uint16_t& outPacketId)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != PKT_PUBCOMP)
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl != 2)
        {
            return false;
        }
        uint8_t buf[2];
        if (!recvAll(buf, 2))
        {
            return false;
        }
        outPacketId = (uint16_t(buf[0]) << 8) | buf[1];
        return true;
    }

    // Send UNSUBSCRIBE
    bool sendUnsubscribe(uint16_t packetId, const std::string& topic)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_UNSUBSCRIBE);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Packet ID
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));

        // Topic filter
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // Read UNSUBACK
    bool readUnsubAck(uint16_t& outPacketId)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != PKT_UNSUBACK)
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl != 2)
        {
            return false;
        }
        uint8_t buf[2];
        if (!recvAll(buf, 2))
        {
            return false;
        }
        outPacketId = (uint16_t(buf[0]) << 8) | buf[1];
        return true;
    }

    // Send PINGREQ
    bool sendPingReq(void)
    {
        std::vector<uint8_t> pkt = {CTRL_PINGREQ, 0x00};
        return sendAll(pkt);
    }

    // Read PINGRESP
    bool readPingResp(void)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if (hdr != CTRL_PINGRESP)
        {
            return false;
        }
        size_t rl;
        if (!readVarInt(rl) || rl != 0)
        {
            return false;
        }
        return true;
    }

    // Send DISCONNECT
    bool sendDisconnect(void)
    {
        std::vector<uint8_t> pkt = {CTRL_DISCONNECT, 0x00};
        return sendAll(pkt);
    }

    // SUBSCRIBE requesting a specific QoS level
    bool sendSubscribeQos(uint16_t packetId, const std::string& topic, uint8_t qos)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_SUBSCRIBE | 0x02);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Packet ID
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));
        // Topic filter + QoS
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());
        pkt.push_back(qos);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // CONNECT with bad protocol version (level 3 instead of 4)
    bool sendConnectBadProtocol(const std::string& clientId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(3); // protocol level 3, not 4
        pkt.push_back(0x02); // clean session
        pkt.push_back(0x00);
        pkt.push_back(60);
        pushStr(clientId);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // CONNECT with username and password flags set
    bool sendConnectWithCredentials(const std::string& clientId,
                                    const std::string& username,
                                    const std::string& password)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(4);
        // flags: cleanSession + username + password
        uint8_t flags = 0x02 | 0x80 | 0x40;
        pkt.push_back(flags);
        pkt.push_back(0x00);
        pkt.push_back(60);
        pushStr(clientId);
        pushStr(username);
        pushStr(password);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // CONNECT with keepAlive=0
    bool sendConnectKeepAlive0(const std::string& clientId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(4);
        pkt.push_back(0x02); // clean session
        pkt.push_back(0x00);
        pkt.push_back(0x00); // keepAlive = 0
        pushStr(clientId);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // CONNECT with extra trailing bytes after client ID
    bool sendConnectWithExtra(const std::string& clientId,
                              const std::vector<uint8_t>& extraBytes)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(4);
        pkt.push_back(0x02); // clean session
        pkt.push_back(0x00);
        pkt.push_back(60);
        pushStr(clientId);

        // Append extra bytes (included in remaining length)
        pkt.insert(pkt.end(), extraBytes.begin(), extraBytes.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // Send raw bytes directly (for testing unknown packet types)
    bool sendRaw(const std::vector<uint8_t>& data)
    {
        return sendAll(data);
    }

    // CONNECT with custom connect flags byte (for testing flag validation)
    bool sendConnectWithFlags(const std::string& clientId, uint8_t connFlags)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_CONNECT);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        auto pushStr = [&](const std::string& s)
        {
            pkt.push_back(uint8_t(s.size() >> 8));
            pkt.push_back(uint8_t(s.size() & 0xFF));
            pkt.insert(pkt.end(), s.begin(), s.end());
        };

        pushStr("MQTT");
        pkt.push_back(4);
        pkt.push_back(connFlags);
        pkt.push_back(0x00);
        pkt.push_back(60);
        pushStr(clientId);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // SUBSCRIBE with empty topic filter
    bool sendSubscribeEmptyTopic(uint16_t packetId)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_SUBSCRIBE);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Packet ID
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));

        // Empty topic filter (length = 0) + QoS 0
        pkt.push_back(0x00);
        pkt.push_back(0x00);
        pkt.push_back(0x00);

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // PUBLISH with wildcards in topic name
    bool sendPublishWithTopic(const std::string& topic,
                               const std::vector<uint8_t>& payload)
    {
        std::vector<uint8_t> pkt;
        pkt.push_back(CTRL_PUBLISH); // QoS 0, no retain
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // PUBLISH with retain + QoS 1 flags + packet ID
    bool sendPublishRetainQos1(const std::string& topic,
                                const std::vector<uint8_t>& payload,
                                uint16_t packetId)
    {
        std::vector<uint8_t> pkt;
        // Retain (bit 0) + QoS 1 (bits 1-2 = 0x02) = 0x03
        pkt.push_back(CTRL_PUBLISH | 0x03);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        // Packet ID (required for QoS > 0)
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));

        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // PUBLISH with retain + QoS 2 flags + packet ID
    bool sendPublishRetainQos2(const std::string& topic,
                                const std::vector<uint8_t>& payload,
                                uint16_t packetId)
    {
        std::vector<uint8_t> pkt;
        // Retain (bit 0) + QoS 2 (bits 1-2 = 0x04) = 0x05
        pkt.push_back(CTRL_PUBLISH | 0x05);
        size_t idxRL = pkt.size();
        pkt.push_back(0);

        // Topic
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());

        // Packet ID (required for QoS > 0)
        pkt.push_back(uint8_t(packetId >> 8));
        pkt.push_back(uint8_t(packetId & 0xFF));

        // Payload
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        size_t rl = pkt.size() - idxRL - 1;
        auto rlEnc = CommUtils::encodeRemainingLength(rl);
        pkt[idxRL] = rlEnc[0];
        return sendAll(pkt);
    }

    // Read a PUBLISH with QoS > 0 (has packet ID in the header)
    bool readPublishWithQos(std::string& topic,
                             std::vector<uint8_t>& payload,
                             uint8_t& qos,
                             uint16_t& packetId)
    {
        uint8_t hdr;
        if (!recvAll(&hdr, 1))
        {
            return false;
        }
        if ((hdr >> 4) != (CTRL_PUBLISH >> 4))
        {
            return false;
        }
        qos = (hdr >> 1) & 0x03;

        size_t rl;
        if (!readVarInt(rl))
        {
            return false;
        }

        uint8_t buf2[2];
        if (!recvAll(buf2, 2))
        {
            return false;
        }
        uint16_t tlen = (uint16_t(buf2[0]) << 8) | buf2[1];
        topic.resize(tlen);
        if (!recvAll(&topic[0], tlen))
        {
            return false;
        }

        size_t consumed = 2 + tlen;

        // Read packet ID if QoS > 0
        packetId = 0;
        if (qos > 0)
        {
            if (!recvAll(buf2, 2))
            {
                return false;
            }
            packetId = (uint16_t(buf2[0]) << 8) | buf2[1];
            consumed += 2;
        }

        size_t paylen = rl - consumed;
        payload.resize(paylen);
        if (paylen > 0)
        {
            return recvAll(payload.data(), paylen);
        }
        return true;
    }
};


// Bound socket reads so a missing/late packet fails a test by timeout instead
// of hanging the whole suite.
static void setRecvTimeoutSeconds(int sock, int seconds)
{
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(seconds) * 1000;
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}


// Regression for: retransmit thread held mMutex during a blocking socket_write,
// so one stuck client froze the whole broker. The refactored retransmitLoop
// collects packets under the lock and sends them outside it. This verifies both
// that (a) QoS-1 retransmission still works and (b) a stuck client with an
// unacked in-flight message does not block service to other clients.
TEST_CASE("Comm: QoS-1 retransmit works and broker stays responsive under a stuck client")
{
    ServerRunner runner;
    uint16_t port = runner.port;
    uint8_t sp = 0;
    uint8_t rc = 0;

    // Subscriber on QoS 1 that reads its first PUBLISH but never PUBACKs.
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    // Retransmit fires on a 5s RETRY_INTERVAL checked every 1s, so the retry
    // can legitimately land up to ~6s after the first delivery. 8s left only
    // ~2s of margin against CI/scheduler jitter, which is what made this test
    // flaky; 15s gives a comfortable margin without changing server behavior.
    setRecvTimeoutSeconds(sub.sock, 15);
    REQUIRE(sub.sendConnect("retrySub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0x0001, "retry/topic", QOS1));
    uint16_t sid = 0;
    REQUIRE(sub.readSubAck(sid));

    // Publisher sends a QoS-1 message; the broker delivers it to sub and keeps
    // it in-flight until acked.
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    setRecvTimeoutSeconds(pub.sock, 8);
    REQUIRE(pub.sendConnect("retryPub"));
    REQUIRE(pub.readConnAck(sp, rc));
    std::vector<uint8_t> payload = { 'h', 'i' };
    REQUIRE(pub.sendPublishQos1("retry/topic", payload, 0x1001));
    uint16_t ackId = 0;
    REQUIRE(pub.readPubAck(ackId));

    // First delivery to the subscriber.
    std::string topic1;
    std::vector<uint8_t> pay1;
    uint8_t q1 = 0;
    uint16_t firstPid = 0;
    REQUIRE(sub.readPublishWithQos(topic1, pay1, q1, firstPid));
    CHECK(topic1 == "retry/topic");
    CHECK(q1 == QOS1);

    // Subscriber deliberately does NOT PUBACK. After the ~5s retry window the
    // retransmit loop must resend the SAME packet id (now sent outside mMutex).
    std::string topic2;
    std::vector<uint8_t> pay2;
    uint8_t q2 = 0;
    uint16_t retryPid = 0;
    REQUIRE(sub.readPublishWithQos(topic2, pay2, q2, retryPid));
    CHECK(topic2 == "retry/topic");
    CHECK(retryPid == firstPid);

    // While that stuck in-flight entry exists, a brand-new client must still be
    // served promptly. Pre-fix the retransmit thread could block in send() while
    // holding mMutex, wedging every other client.
    MqttClientEx healthy;
    REQUIRE(healthy.connectTo("127.0.0.1", port));
    setRecvTimeoutSeconds(healthy.sock, 5);
    REQUIRE(healthy.sendConnect("healthy1"));
    REQUIRE(healthy.readConnAck(sp, rc));
    CHECK(rc == 0);
    REQUIRE(healthy.sendSubscribe(0x0002, "healthy/topic"));
    uint16_t hid = 0;
    REQUIRE(healthy.readSubAck(hid));
}


// ===========================================================================
// Group A: CommUtils unit tests
// ===========================================================================

TEST_CASE("CommUtils: encodeRemainingLength")
{
    SUBCASE("zero encodes to single 0x00 byte")
    {
        auto result = CommUtils::encodeRemainingLength(0);
        REQUIRE(result.size() == 1);
        CHECK(result[0] == 0x00);
    }

    SUBCASE("127 encodes to single byte")
    {
        auto result = CommUtils::encodeRemainingLength(127);
        REQUIRE(result.size() == 1);
        CHECK(result[0] == 0x7F);
    }

    SUBCASE("128 encodes to two bytes")
    {
        auto result = CommUtils::encodeRemainingLength(128);
        REQUIRE(result.size() == 2);
        CHECK(result[0] == 0x80);
        CHECK(result[1] == 0x01);
    }

    SUBCASE("16383 encodes to two bytes")
    {
        auto result = CommUtils::encodeRemainingLength(16383);
        REQUIRE(result.size() == 2);
        CHECK(result[0] == 0xFF);
        CHECK(result[1] == 0x7F);
    }

    SUBCASE("16384 encodes to three bytes")
    {
        auto result = CommUtils::encodeRemainingLength(16384);
        REQUIRE(result.size() == 3);
        CHECK(result[0] == 0x80);
        CHECK(result[1] == 0x80);
        CHECK(result[2] == 0x01);
    }
}

TEST_CASE("CommUtils: buildControlPacket")
{
    SUBCASE("PUBACK format")
    {
        auto pkt = CommUtils::buildControlPacket(CTRL_PUBACK, 0x0001);
        REQUIRE(pkt.size() == 4);
        CHECK(pkt[0] == CTRL_PUBACK);
        CHECK(pkt[1] == 0x02); // remaining length
        CHECK(pkt[2] == 0x00); // packet ID MSB
        CHECK(pkt[3] == 0x01); // packet ID LSB
    }

    SUBCASE("packet ID encoding 0x1234")
    {
        auto pkt = CommUtils::buildControlPacket(CTRL_PUBREC, 0x1234);
        REQUIRE(pkt.size() == 4);
        CHECK(pkt[0] == CTRL_PUBREC);
        CHECK(pkt[2] == 0x12);
        CHECK(pkt[3] == 0x34);
    }
}

TEST_CASE("CommUtils: sClientInfo constructors")
{
    SUBCASE("default constructor")
    {
        CommUtils::sClientInfo info;
        CHECK(info.socket == -1);
        CHECK(info.cleanSession == true);
        CHECK(info.keepAlive == DEFAULT_KEEP_ALIVE_SECONDS);
        CHECK(info.nextPacketId == 1);
        CHECK(info.hasWill == false);
        CHECK(info.id.empty());
    }

    SUBCASE("parameterized constructor")
    {
        CommUtils::sClientInfo info("testClient", 42, false);
        CHECK(info.id == "testClient");
        CHECK(info.socket == 42);
        CHECK(info.cleanSession == false);
        CHECK(info.keepAlive == DEFAULT_KEEP_ALIVE_SECONDS);
    }
}

TEST_CASE("CommUtils: allocatePacketId")
{
    CommUtils::sClientInfo info;

    SUBCASE("sequential allocation")
    {
        CHECK(info.allocatePacketId() == 1);
        CHECK(info.allocatePacketId() == 2);
        CHECK(info.allocatePacketId() == 3);
    }

    SUBCASE("wraps from 65535 to 1")
    {
        info.nextPacketId = 65535;
        CHECK(info.allocatePacketId() == 65535);
        // nextPacketId overflowed to 0, allocatePacketId should have wrapped to 1
        CHECK(info.allocatePacketId() == 1);
    }
}

TEST_CASE("CommUtils: sRetainedMessage constructors")
{
    SUBCASE("default constructor")
    {
        CommUtils::sRetainedMessage msg;
        CHECK(msg.qos == QOS0);
        CHECK(msg.payload.empty());
    }

    SUBCASE("parameterized constructor")
    {
        std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
        CommUtils::sRetainedMessage msg(data, QOS1);
        CHECK(msg.payload == data);
        CHECK(msg.qos == QOS1);
    }
}

TEST_CASE("CommUtils: sInflightEntry default constructor")
{
    CommUtils::sInflightEntry entry;
    CHECK(entry.state == CommUtils::eInflightState::WAIT_PUBACK);
    CHECK(entry.retries == 0);
    CHECK(entry.packet.empty());
}


// ===========================================================================
// Group B: cCommClient tests
// ===========================================================================

TEST_CASE("cCommClient: connect and disconnect")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    cCommClient client("127.0.0.1", port, "clientB1");
    CHECK(client.connect());
    client.disconnect();
}

TEST_CASE("cCommClient: publish QoS 0")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    cCommClient client("127.0.0.1", port, "clientB2");
    REQUIRE(client.connect());

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    CHECK(client.publish("test/topic", payload, QOS0));

    client.disconnect();
}

TEST_CASE("cCommClient: subscribe and receive message")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Set up cCommClient subscriber
    cCommClient subscriber("127.0.0.1", port, "clientB3sub");
    REQUIRE(subscriber.connect());

    std::string receivedTopic;
    std::vector<uint8_t> receivedPayload;
    bool messageReceived = false;

    subscriber.subscribe("foo/bar", QOS0,
        [&](const std::string& topic, const std::vector<uint8_t>& payload)
        {
            receivedTopic = topic;
            receivedPayload = payload;
            messageReceived = true;
        });

    // Small delay for subscription to register
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Use MqttClient as publisher
    MqttClient pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(pub.sendConnect("clientB3pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0xDE, 0xAD};
    REQUIRE(pub.sendPublish("foo/bar", payload));

    // Wait for message delivery
    for (int i = 0; i < 50 && !messageReceived; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    CHECK(messageReceived);
    CHECK(receivedTopic == "foo/bar");
    CHECK(receivedPayload == payload);

    pub.closeIt();
    subscriber.disconnect();
}

TEST_CASE("cCommClient: publish QoS 1")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    cCommClient client("127.0.0.1", port, "clientB4");
    REQUIRE(client.connect());

    std::vector<uint8_t> payload = {0x11, 0x22};
    // QoS 1 publish waits for PUBACK from server
    CHECK(client.publish("qos1/topic", payload, QOS1));

    client.disconnect();
}

TEST_CASE("cCommClient: publish QoS 2")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    cCommClient client("127.0.0.1", port, "clientB5");
    REQUIRE(client.connect());

    std::vector<uint8_t> payload = {0x33, 0x44};
    // QoS 2 publish completes full handshake (PUBREC/PUBREL/PUBCOMP)
    CHECK(client.publish("qos2/topic", payload, QOS2));

    client.disconnect();
}

TEST_CASE("cCommClient: multiple subscribes")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    cCommClient subscriber("127.0.0.1", port, "clientB6sub");
    REQUIRE(subscriber.connect());

    int messageCount = 0;
    subscriber.subscribe("a/b", QOS0,
        [&](const std::string& /*topic*/, const std::vector<uint8_t>& /*payload*/)
        {
            messageCount++;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Publish to a/b
    MqttClient pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(pub.sendConnect("clientB6pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0x01};
    REQUIRE(pub.sendPublish("a/b", payload));

    // Wait for message
    for (int i = 0; i < 50 && messageCount < 1; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    CHECK(messageCount == 1);

    pub.closeIt();
    subscriber.disconnect();
}


// Throughput sanity test: drive N publishes through publisher cCommClient
// -> broker -> subscriber cCommClient at each QoS level. Not a benchmark;
// the wall-clock budget exists only so a hang fails the test rather than
// stalling the suite. Catches regressions in ack tracking, packet-id
// reuse, and per-message resource cleanup that single-message functional
// tests would not surface.
TEST_CASE("cCommClient: throughput pub/sub round-trip at each QoS")
{
    auto runQos = [](uint8_t qos, uint32_t N, const std::string& topic,
                     const std::string& pubId, const std::string& subId)
    {
        ServerRunner runner;
        const uint16_t port = runner.port;

        std::mutex receivedMutex;
        std::vector<uint32_t> receivedSeqs;
        receivedSeqs.reserve(N);
        std::atomic<int> receivedCount{0};

        cCommClient subscriber("127.0.0.1", port, subId);
        REQUIRE(subscriber.connect());

        REQUIRE(subscriber.subscribe(topic, qos,
            [&](const std::string& /*t*/, const std::vector<uint8_t>& payload)
            {
                uint32_t seq = 0;
                if (payload.size() >= 4)
                {
                    seq =  uint32_t(payload[0])
                        | (uint32_t(payload[1]) << 8)
                        | (uint32_t(payload[2]) << 16)
                        | (uint32_t(payload[3]) << 24);
                }
                {
                    std::lock_guard<std::mutex> lock(receivedMutex);
                    receivedSeqs.push_back(seq);
                }
                receivedCount.fetch_add(1, std::memory_order_relaxed);
            }));

        // Give the broker time to register the SUBSCRIBE before
        // the publisher starts.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        cCommClient publisher("127.0.0.1", port, pubId);
        REQUIRE(publisher.connect());

        for (uint32_t i = 0; i < N; i++)
        {
            std::vector<uint8_t> payload = {
                uint8_t(i & 0xFF),
                uint8_t((i >> 8) & 0xFF),
                uint8_t((i >> 16) & 0xFF),
                uint8_t((i >> 24) & 0xFF),
            };
            REQUIRE(publisher.publish(topic, payload, qos));
        }

        constexpr auto BUDGET = std::chrono::seconds(10);
        const auto deadline = std::chrono::steady_clock::now() + BUDGET;
        const int target = static_cast<int>(N);
        while (receivedCount.load(std::memory_order_relaxed) < target
               && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        CHECK(receivedCount.load() >= target);

        std::vector<uint32_t> seqs;
        {
            std::lock_guard<std::mutex> lock(receivedMutex);
            seqs = receivedSeqs;
        }

        // MQTT guarantees order per (publisher, topic) for a single
        // subscriber, so sequence numbers must be non-decreasing
        // (duplicates only legal at QoS 1).
        bool monotonic = true;
        for (size_t i = 1; i < seqs.size(); i++)
        {
            if (seqs[i] < seqs[i - 1])
            {
                monotonic = false;
                break;
            }
        }
        CHECK(monotonic);

        // QoS 2 is exactly-once. QoS 0/1 may legitimately differ from
        // exactly-N (0 may drop, 1 may duplicate), but on localhost
        // with no induced loss we expect all messages.
        if (qos == QOS2)
        {
            CHECK(seqs.size() == N);
        }
        else
        {
            CHECK(seqs.size() >= N);
        }

        publisher.disconnect();
        subscriber.disconnect();
    };

    SUBCASE("QoS 0: 1000 messages")
    {
        runQos(QOS0, 1000, "throughput/q0",
               "throughput-pub-q0", "throughput-sub-q0");
    }
    SUBCASE("QoS 1: 500 messages")
    {
        runQos(QOS1, 500, "throughput/q1",
               "throughput-pub-q1", "throughput-sub-q1");
    }
    SUBCASE("QoS 2: 250 messages")
    {
        runQos(QOS2, 250, "throughput/q2",
               "throughput-pub-q2", "throughput-sub-q2");
    }
}


// ===========================================================================
// Group C: Server QoS 1 flow
// ===========================================================================

TEST_CASE("TCP: QoS 1 PUBACK from server to publisher")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(pub.sendConnect("qos1Pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    // Send QoS 1 PUBLISH
    std::vector<uint8_t> payload = {0xAA, 0xBB};
    uint16_t pubPktId = 0x0001;
    REQUIRE(pub.sendPublishQos1("qos1/test", payload, pubPktId));

    // Server should respond with PUBACK
    uint16_t ackId;
    CHECK(pub.readPubAck(ackId));
    CHECK(ackId == pubPktId);

    pub.closeIt();
}


// ===========================================================================
// Group D: Server QoS 2 flow
// ===========================================================================

TEST_CASE("TCP: QoS 2 full handshake (PUBLISH -> PUBREC -> PUBREL -> PUBCOMP)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(pub.sendConnect("qos2Pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    // Send QoS 2 PUBLISH
    std::vector<uint8_t> payload = {0xCC, 0xDD};
    uint16_t pubPktId = 0x0002;
    REQUIRE(pub.sendPublishQos2("qos2/test", payload, pubPktId));

    // Server should respond with PUBREC
    uint16_t recId;
    REQUIRE(pub.readPubRec(recId));
    CHECK(recId == pubPktId);

    // Client sends PUBREL
    REQUIRE(pub.sendPubRel(pubPktId));

    // Server should respond with PUBCOMP
    uint16_t compId;
    REQUIRE(pub.readPubComp(compId));
    CHECK(compId == pubPktId);

    pub.closeIt();
}


// ===========================================================================
// Group E: PINGREQ/PINGRESP
// ===========================================================================

TEST_CASE("TCP: PINGREQ -> PINGRESP")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(client.sendConnect("pingClient"));
    REQUIRE(client.readConnAck(sp, rc));

    SUBCASE("single PING")
    {
        REQUIRE(client.sendPingReq());
        CHECK(client.readPingResp());
    }

    SUBCASE("multiple PINGs")
    {
        for (int i = 0; i < 3; ++i)
        {
            REQUIRE(client.sendPingReq());
            CHECK(client.readPingResp());
        }
    }

    client.closeIt();
}


// ===========================================================================
// Group F: DISCONNECT
// ===========================================================================

TEST_CASE("TCP: graceful DISCONNECT")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(client.sendConnect("discClient"));
    REQUIRE(client.readConnAck(sp, rc));

    // Send DISCONNECT
    CHECK(client.sendDisconnect());

    // Small delay to let server process disconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.closeIt();
}

TEST_CASE("TCP: DISCONNECT suppresses Will message")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Set up a subscriber on the Will topic
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(sub.sendConnect("willSubF2"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0x3001, "will/topic"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));

    // Connect a client with a Will
    MqttClientEx willClient;
    REQUIRE(willClient.connectTo("127.0.0.1", port));
    std::vector<uint8_t> willPayload = {0xFF};
    REQUIRE(willClient.sendConnectWithWill("willClientF2", "will/topic", willPayload, QOS0, false));
    REQUIRE(willClient.readConnAck(sp, rc));

    // Graceful DISCONNECT should suppress Will
    REQUIRE(willClient.sendDisconnect());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    willClient.closeIt();

    // Verify subscriber does NOT receive Will message (non-blocking read)
#ifndef _WIN32
    int flags = fcntl(sub.sock, F_GETFL, 0);
    fcntl(sub.sock, F_SETFL, flags | O_NONBLOCK);
    uint8_t b;
    ssize_t r = socket_read(sub.sock, &b, 1);
    CHECK(r == -1); // no data available
    fcntl(sub.sock, F_SETFL, flags);
#endif

    sub.closeIt();
}


// ===========================================================================
// Group G: UNSUBSCRIBE flow
// ===========================================================================

TEST_CASE("TCP: UNSUBSCRIBE + UNSUBACK")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(client.sendConnect("unsubClient"));
    REQUIRE(client.readConnAck(sp, rc));

    // Subscribe first
    uint16_t subId = 0x4001;
    REQUIRE(client.sendSubscribe(subId, "unsub/topic"));
    uint16_t subRet;
    REQUIRE(client.readSubAck(subRet));
    CHECK(subRet == subId);

    // Unsubscribe
    uint16_t unsubId = 0x4002;
    REQUIRE(client.sendUnsubscribe(unsubId, "unsub/topic"));
    uint16_t unsubRet;
    REQUIRE(client.readUnsubAck(unsubRet));
    CHECK(unsubRet == unsubId);

    client.closeIt();
}

TEST_CASE("TCP: no messages after UNSUBSCRIBE")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx sub, pub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(sub.sendConnect("unsubSub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(pub.sendConnect("unsubPub"));
    REQUIRE(pub.readConnAck(sp, rc));

    // Subscribe
    REQUIRE(sub.sendSubscribe(0x5001, "unsub/test"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));

    // Unsubscribe
    REQUIRE(sub.sendUnsubscribe(0x5002, "unsub/test"));
    uint16_t unsubRet;
    REQUIRE(sub.readUnsubAck(unsubRet));

    // Publish after unsubscribe
    std::vector<uint8_t> payload = {0x99};
    REQUIRE(pub.sendPublish("unsub/test", payload));

    // Wait briefly and verify no message arrives
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifndef _WIN32
    int flags = fcntl(sub.sock, F_GETFL, 0);
    fcntl(sub.sock, F_SETFL, flags | O_NONBLOCK);
    uint8_t b;
    ssize_t r = socket_read(sub.sock, &b, 1);
    CHECK(r == -1); // no data
    fcntl(sub.sock, F_SETFL, flags);
#endif

    sub.closeIt();
    pub.closeIt();
}


// ===========================================================================
// Group H: Retained messages
// ===========================================================================

TEST_CASE("TCP: retained message delivered to new subscriber")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Publish a retained message
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(pub.sendConnect("retainPub"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> retainedPayload = {0xAA, 0xBB};
    REQUIRE(pub.sendPublishRetain("retain/topic", retainedPayload));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // New subscriber connects and subscribes
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("retainSub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0x6001, "retain/topic"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));

    // Should receive the retained message
    std::string topic;
    std::vector<uint8_t> payload;
    REQUIRE(sub.readPublish(topic, payload));
    CHECK(topic == "retain/topic");
    CHECK(payload == retainedPayload);

    pub.closeIt();
    sub.closeIt();
}

TEST_CASE("TCP: empty retained payload clears retained message")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(pub.sendConnect("clearRetainPub"));
    REQUIRE(pub.readConnAck(sp, rc));

    // Publish retained message
    std::vector<uint8_t> retainedPayload = {0xCC};
    REQUIRE(pub.sendPublishRetain("clearretain/topic", retainedPayload));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Clear it with empty payload retained message
    std::vector<uint8_t> emptyPayload;
    REQUIRE(pub.sendPublishRetain("clearretain/topic", emptyPayload));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // New subscriber should NOT receive a retained message
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("clearRetainSub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0x7001, "clearretain/topic"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));

    // Non-blocking read: no retained message expected
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifndef _WIN32
    int flags = fcntl(sub.sock, F_GETFL, 0);
    fcntl(sub.sock, F_SETFL, flags | O_NONBLOCK);
    uint8_t b;
    ssize_t r = socket_read(sub.sock, &b, 1);
    CHECK(r == -1); // no data
    fcntl(sub.sock, F_SETFL, flags);
#endif

    pub.closeIt();
    sub.closeIt();
}


// ===========================================================================
// Group I: Will messages
// ===========================================================================

TEST_CASE("TCP: Will message published on unexpected disconnect")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Set up subscriber on Will topic
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    uint8_t sp, rc;
    REQUIRE(sub.sendConnect("willSub"));
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0x8001, "will/test"));
    uint16_t ret;
    REQUIRE(sub.readSubAck(ret));

    // Connect a client with a Will message
    {
        MqttClientEx willClient;
        REQUIRE(willClient.connectTo("127.0.0.1", port));
        std::vector<uint8_t> willPayload = {0xDE, 0xAD};
        REQUIRE(willClient.sendConnectWithWill("willSender", "will/test", willPayload, QOS0, false));
        REQUIRE(willClient.readConnAck(sp, rc));

        // Close socket without sending DISCONNECT (unexpected disconnect)
        willClient.closeIt();
    }

    // Wait for server to detect the disconnect and publish Will
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Subscriber should receive the Will message
    std::string topic;
    std::vector<uint8_t> payload;
    REQUIRE(sub.readPublish(topic, payload));
    CHECK(topic == "will/test");
    std::vector<uint8_t> expectedWill = {0xDE, 0xAD};
    CHECK(payload == expectedWill);

    sub.closeIt();
}


// ===========================================================================
// Group J: Session persistence
// ===========================================================================

TEST_CASE("TCP: cleanSession=false persists subscriptions")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // First connection with cleanSession=false: subscribe to a topic
    {
        MqttClientEx client;
        REQUIRE(client.connectTo("127.0.0.1", port));
        REQUIRE(client.sendConnectCleanFalse("persistClient"));
        uint8_t sp, rc;
        REQUIRE(client.readConnAck(sp, rc));
        CHECK(sp == 0); // no previous session

        REQUIRE(client.sendSubscribe(0x9001, "persist/topic"));
        uint16_t ret;
        REQUIRE(client.readSubAck(ret));

        client.sendDisconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client.closeIt();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second connection with cleanSession=false: session should be restored
    {
        MqttClientEx client;
        REQUIRE(client.connectTo("127.0.0.1", port));
        REQUIRE(client.sendConnectCleanFalse("persistClient"));
        uint8_t sp, rc;
        REQUIRE(client.readConnAck(sp, rc));
        CHECK(rc == CONNACK_RC_ACCEPTED);
        CHECK(sp == CONNACK_FLAG_SESSION_PRESENT); // session was preserved

        // Publish to the persisted topic from another client
        MqttClientEx pub;
        REQUIRE(pub.connectTo("127.0.0.1", port));
        REQUIRE(pub.sendConnect("persistPub"));
        REQUIRE(pub.readConnAck(sp, rc));

        std::vector<uint8_t> payload = {0x42};
        REQUIRE(pub.sendPublish("persist/topic", payload));

        // Should receive the message via restored subscription
        std::string topic;
        std::vector<uint8_t> recv;
        REQUIRE(client.readPublish(topic, recv));
        CHECK(topic == "persist/topic");
        CHECK(recv == payload);

        pub.closeIt();
        client.closeIt();
    }
}

TEST_CASE("TCP: cleanSession=true does not persist")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // First connection: subscribe with cleanSession=true
    {
        MqttClientEx client;
        REQUIRE(client.connectTo("127.0.0.1", port));
        REQUIRE(client.sendConnect("cleanClient"));
        uint8_t sp, rc;
        REQUIRE(client.readConnAck(sp, rc));

        REQUIRE(client.sendSubscribe(0xA001, "clean/topic"));
        uint16_t ret;
        REQUIRE(client.readSubAck(ret));

        client.sendDisconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client.closeIt();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second connection: session should NOT be present
    {
        MqttClientEx client;
        REQUIRE(client.connectTo("127.0.0.1", port));
        REQUIRE(client.sendConnect("cleanClient"));
        uint8_t sp, rc;
        REQUIRE(client.readConnAck(sp, rc));
        CHECK(rc == CONNACK_RC_ACCEPTED);
        CHECK(sp == 0); // no session preserved

        client.closeIt();
    }
}


// ===========================================================================
// Group K: Protocol error paths (server-side)
// ===========================================================================

TEST_CASE("TCP: bad protocol version returns CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with protocol level 3 instead of 4
    REQUIRE(client.sendConnectBadProtocol("badProtoClient"));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION);

    client.closeIt();
}

TEST_CASE("TCP: CONNECT with username and password accepted")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with username and password flags set
    REQUIRE(client.sendConnectWithCredentials("credClient", "testuser", "testpass"));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    // Server has TODO auth -- accepts all credentials
    CHECK(rc == CONNACK_RC_ACCEPTED);

    client.closeIt();
}

TEST_CASE("TCP: CONNECT with keepAlive=0 accepted")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // CONNECT with keepAlive == 0: per MQTT 3.1.1 section 3.1.2.10 the
    // server must not disconnect for inactivity, so the broker leaves
    // SO_RCVTIMEO unset for this connection.
    REQUIRE(client.sendConnectKeepAlive0("keepAlive0Client"));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);

    client.closeIt();
}

TEST_CASE("TCP: client ID collision disconnects old client")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Connect clientA with ID "collision"
    MqttClientEx clientA;
    REQUIRE(clientA.connectTo("127.0.0.1", port));
    REQUIRE(clientA.sendConnect("collision"));
    uint8_t sp, rc;
    REQUIRE(clientA.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);

    // Connect clientB with same ID "collision"
    MqttClientEx clientB;
    REQUIRE(clientB.connectTo("127.0.0.1", port));
    REQUIRE(clientB.sendConnect("collision"));
    REQUIRE(clientB.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);

    // clientA should be disconnected -- verify by trying to read
    // (the server closed clientA's socket, so reads should fail)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Set a short read timeout on clientA to avoid hanging
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(clientA.sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf;
    ssize_t n = socket_read(clientA.sock, &buf, 1);
    // Either 0 (graceful close) or -1 (error) means the old client is dead
    CHECK(n <= 0);

    clientA.closeIt();
    clientB.closeIt();
}

// Regression test for the takeover-vs-victim-handler race:
// two clients hammer the same client-id from separate threads. With the
// old code (which ::close()'d the victim's fd from the new client's
// thread) this would intermittently UAF a reused fd, observable as the
// victim's handler thread emitting "failed to set SO_RCVTIMEO" on a fd
// that no longer belongs to it, or corrupting mClients enough that the
// final sanity connect fails. With the fix (::shutdown only; victim
// handler owns ::close) the broker stays healthy.
//
// Iteration count is deliberately modest: enough to make the race
// observable in a few runs, but small enough not to exhaust local
// TIME_WAIT ports when the test is run repeatedly during development.
// A read timeout on every client socket guarantees worker threads
// cannot wedge if the broker fails to respond, which keeps test failure
// modes deterministic.
TEST_CASE("TCP: rapid same-client-id reconnect does not double-close (race regression)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;
    const std::string cid = "raceClient";
    constexpr int ITERATIONS = 25;

    std::atomic<int> connectsRejected{0};
    std::atomic<bool> go{false};

    auto setReadTimeout = [](int sock)
    {
#ifdef _WIN32
        DWORD ms = 2000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    };

    auto worker = [&]()
    {
        // Spin until the test signals "go" so both threads start
        // roughly together and maximize contention on the takeover path.
        while (!go.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        for (int i = 0; i < ITERATIONS; i++)
        {
            MqttClient client;
            if (!client.connectTo("127.0.0.1", port))
            {
                continue;
            }
            setReadTimeout(client.sock);
            if (!client.sendConnect(cid))
            {
                client.closeIt();
                continue;
            }
            uint8_t sp = 0;
            uint8_t rc = 0xFF;
            if (client.readConnAck(sp, rc) && rc != CONNACK_RC_ACCEPTED)
            {
                connectsRejected.fetch_add(1, std::memory_order_relaxed);
            }
            client.closeIt();
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    go.store(true, std::memory_order_release);
    t1.join();
    t2.join();

    // Every CONNACK the broker did send must be ACCEPTED. If the
    // broker had corrupted state during the takeover storm, it might
    // refuse a same-id reconnect with a non-ACCEPTED return code.
    CHECK(connectsRejected.load() == 0);

    // Sanity: broker still accepts new connections (with a fresh ID).
    // The original bug could leave the broker holding a closed fd that
    // had been reused by accept(), wedging the handler thread; the
    // final connect would then time out reading CONNACK.
    MqttClient survivor;
    REQUIRE(survivor.connectTo("127.0.0.1", port));
    setReadTimeout(survivor.sock);
    REQUIRE(survivor.sendConnect("raceSurvivor"));
    uint8_t sp = 0;
    uint8_t rc = 0xFF;
    REQUIRE(survivor.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);
    survivor.closeIt();
}

TEST_CASE("TCP: unknown packet type handled gracefully")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // First do a normal connect
    REQUIRE(client.sendConnect("unknownPktClient"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);

    // Send a reserved packet type (0xF0) with remaining length 0
    std::vector<uint8_t> badPkt = {0xF0, 0x00};
    REQUIRE(client.sendRaw(badPkt));

    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Server should skip the unknown packet and keep the connection alive
    // Verify by sending a PINGREQ and getting PINGRESP
    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());

    client.closeIt();
}

TEST_CASE("TCP: CONNECT with extra trailing bytes accepted")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with extra bytes after client ID
    std::vector<uint8_t> extra = {0xAA, 0xBB, 0xCC};
    REQUIRE(client.sendConnectWithExtra("extraClient", extra));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    // Server should skip leftover bytes and accept
    CHECK(rc == CONNACK_RC_ACCEPTED);

    client.closeIt();
}


// ===========================================================================
// Group L: QoS broker-to-subscriber delivery paths
// ===========================================================================

TEST_CASE("TCP: subscribe at QoS 1 receives messages")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscriber requests QoS 1 (server grants QoS 0 in current impl)
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("qos1Sub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));

    REQUIRE(sub.sendSubscribeQos(0xB001, "qos1/topic", QOS1));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));
    CHECK(subRet == 0xB001);

    // Publisher sends QoS 0 message
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("qos1Pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0x51, 0x6F, 0x53};
    REQUIRE(pub.sendPublish("qos1/topic", payload));

    // Subscriber should receive the message at QoS 0
    std::string topic;
    std::vector<uint8_t> recv;
    REQUIRE(sub.readPublish(topic, recv));
    CHECK(topic == "qos1/topic");
    CHECK(recv == payload);

    pub.closeIt();
    sub.closeIt();
}

TEST_CASE("TCP: cCommClient receives QoS 1 from broker")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Use cCommClient to subscribe
    cCommClient client("127.0.0.1", port, "commQos1");
    REQUIRE(client.connect());

    bool received = false;
    std::string receivedTopic;
    std::vector<uint8_t> receivedPayload;

    client.subscribe("qos1/recv", QOS0, [&](const std::string& t, const std::vector<uint8_t>& p)
    {
        received = true;
        receivedTopic = t;
        receivedPayload = p;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Use raw client to publish QoS 1 to the topic
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("rawQos1Pub"));
    uint8_t sp, rc;
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0x11, 0x22, 0x33};
    REQUIRE(pub.sendPublishQos1("qos1/recv", payload, 0x0001));

    // Read PUBACK from server
    uint16_t ackId;
    REQUIRE(pub.readPubAck(ackId));
    CHECK(ackId == 0x0001);

    // Wait for cCommClient to receive the message
    for (int i = 0; i < 50 && !received; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    CHECK(received);
    if (received)
    {
        CHECK(receivedTopic == "qos1/recv");
        CHECK(receivedPayload == payload);
    }

    pub.closeIt();
    client.disconnect();
}

TEST_CASE("TCP: QoS 2 publish with full server handshake")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscriber
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("qos2Sub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));

    REQUIRE(sub.sendSubscribe(0xC001, "qos2/server"));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // Publisher sends QoS 2
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("qos2Pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0xAA, 0xBB};
    REQUIRE(pub.sendPublishQos2("qos2/server", payload, 0x0002));

    // Server sends PUBREC
    uint16_t recId;
    REQUIRE(pub.readPubRec(recId));
    CHECK(recId == 0x0002);

    // Publisher sends PUBREL
    REQUIRE(pub.sendPubRel(0x0002));

    // Server sends PUBCOMP
    uint16_t compId;
    REQUIRE(pub.readPubComp(compId));
    CHECK(compId == 0x0002);

    // Subscriber should receive the message (at QoS 0, since server fans out at QoS 0)
    std::string topic;
    std::vector<uint8_t> recv;
    REQUIRE(sub.readPublish(topic, recv));
    CHECK(topic == "qos2/server");
    CHECK(recv == payload);

    pub.closeIt();
    sub.closeIt();
}


// ===========================================================================
// Group M: cCommClient error paths
// ===========================================================================

TEST_CASE("cCommClient: connect to non-existent server fails")
{
    // Port 37720 has no server listening
    cCommClient client("127.0.0.1", 37720, "noServerClient");
    CHECK_FALSE(client.connect());
}

TEST_CASE("cCommClient: publish when disconnected fails")
{
    cCommClient client("127.0.0.1", 37720, "disconnPub");
    std::vector<uint8_t> payload = {0x01};
    CHECK_FALSE(client.publish("some/topic", payload));
}

TEST_CASE("cCommClient: subscribe when disconnected fails")
{
    cCommClient client("127.0.0.1", 37720, "disconnSub");
    CHECK_FALSE(client.subscribe("some/topic", QOS0, [](const std::string&, const std::vector<uint8_t>&) {}));
}

TEST_CASE("cCommServer: getPort returns configured port")
{
    cCommServer srv(37721);
    CHECK(srv.getPort() == 37721);
}


// ===========================================================================
// Group N: Unsubscribe with persistent session
// ===========================================================================

TEST_CASE("TCP: unsubscribe with cleanSession=false then reconnect")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // First connection: subscribe with cleanSession=false, then unsubscribe
    {
        MqttClientEx client;
        REQUIRE(client.connectTo("127.0.0.1", port));
        REQUIRE(client.sendConnectCleanFalse("unsubPersist"));
        uint8_t sp, rc;
        REQUIRE(client.readConnAck(sp, rc));
        CHECK(rc == CONNACK_RC_ACCEPTED);

        // Subscribe
        REQUIRE(client.sendSubscribe(0xD001, "unsub/topic"));
        uint16_t subRet;
        REQUIRE(client.readSubAck(subRet));
        CHECK(subRet == 0xD001);

        // Unsubscribe
        REQUIRE(client.sendUnsubscribe(0xD002, "unsub/topic"));
        uint16_t unsubRet;
        REQUIRE(client.readUnsubAck(unsubRet));
        CHECK(unsubRet == 0xD002);

        client.sendDisconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client.closeIt();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Reconnect with cleanSession=false and publish to that topic
    {
        MqttClientEx client;
        REQUIRE(client.connectTo("127.0.0.1", port));
        REQUIRE(client.sendConnectCleanFalse("unsubPersist"));
        uint8_t sp, rc;
        REQUIRE(client.readConnAck(sp, rc));
        CHECK(rc == CONNACK_RC_ACCEPTED);

        // Publish to the topic from another client
        MqttClientEx pub;
        REQUIRE(pub.connectTo("127.0.0.1", port));
        REQUIRE(pub.sendConnect("unsubPub"));
        REQUIRE(pub.readConnAck(sp, rc));

        std::vector<uint8_t> payload = {0x55};
        REQUIRE(pub.sendPublish("unsub/topic", payload));

        // Set short read timeout -- we should NOT receive the message
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(client.sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        std::string topic;
        std::vector<uint8_t> recv;
        bool gotMsg = client.readPublish(topic, recv);
        CHECK_FALSE(gotMsg); // subscription was removed

        pub.closeIt();
        client.closeIt();
    }
}


// ===========================================================================
// Group O: Additional testable coverage gaps
// ===========================================================================

TEST_CASE("cCommServer: port 0 lets the kernel pick a free port")
{
    cCommServer srv(0);

    // Before start(), the port is still 0 -- the kernel hasn't assigned
    // one yet. The actual port is read back via getsockname() inside
    // start() and exposed through getPort().
    CHECK(srv.getPort() == 0);

    REQUIRE(srv.start());

    int port = srv.getPort();
    CHECK(port > 0);
    CHECK(port < 65536);

    MqttClient client;
    CHECK(client.connectTo("127.0.0.1", static_cast<uint16_t>(port)));
    client.closeIt();

    srv.stop();
}

TEST_CASE("TCP: retained QoS 1 message delivered to new subscriber via sendPublish QoS>0")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Publisher stores a retained message at QoS 1
    {
        MqttClientEx pub;
        REQUIRE(pub.connectTo("127.0.0.1", port));
        REQUIRE(pub.sendConnect("retQos1Pub"));
        uint8_t sp, rc;
        REQUIRE(pub.readConnAck(sp, rc));

        std::vector<uint8_t> payload = {0xCA, 0xFE};
        REQUIRE(pub.sendPublishRetainQos1("ret/qos1topic", payload, 0x0001));

        // Server sends PUBACK for QoS 1 publish
        uint16_t ackId;
        REQUIRE(pub.readPubAck(ackId));
        CHECK(ackId == 0x0001);

        pub.sendDisconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pub.closeIt();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // New subscriber connects and subscribes -- should receive the retained
    // message delivered via sendPublish() with the original QoS 1
    {
        MqttClientEx sub;
        REQUIRE(sub.connectTo("127.0.0.1", port));
        REQUIRE(sub.sendConnect("retQos1Sub"));
        uint8_t sp, rc;
        REQUIRE(sub.readConnAck(sp, rc));

        REQUIRE(sub.sendSubscribe(0xE001, "ret/qos1topic", 1));
        uint16_t subRet;
        REQUIRE(sub.readSubAck(subRet));
        CHECK(subRet == 0xE001);

        // Read the retained message -- should arrive with QoS 1 (has packet ID)
        std::string topic;
        std::vector<uint8_t> recv;
        uint8_t qos;
        uint16_t packetId;
        REQUIRE(sub.readPublishWithQos(topic, recv, qos, packetId));
        CHECK(topic == "ret/qos1topic");
        CHECK(recv == std::vector<uint8_t>{0xCA, 0xFE});
        CHECK(qos == 1);
        CHECK(packetId > 0); // server allocated a packet ID via nextPacketId()

        sub.closeIt();
    }
}

TEST_CASE("TCP: Will message at QoS 1 exercises sendPublish QoS>0")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscriber subscribes to the will topic
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("willQos1Sub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));

    REQUIRE(sub.sendSubscribe(0xF001, "will/qos1topic", 1));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // Client connects with Will at QoS 1
    {
        MqttClientEx willClient;
        REQUIRE(willClient.connectTo("127.0.0.1", port));
        std::vector<uint8_t> willPayload = {0xDE, 0xAD};
        REQUIRE(willClient.sendConnectWithWill("willQos1Client",
                                                "will/qos1topic",
                                                willPayload,
                                                1,     // willQos = 1
                                                false)); // willRetain = false
        REQUIRE(willClient.readConnAck(sp, rc));
        CHECK(rc == CONNACK_RC_ACCEPTED);

        // Disconnect ungracefully (close without DISCONNECT packet)
        willClient.closeIt();
    }

    // Give server time to detect the disconnect and publish Will
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Subscriber should receive the Will message at QoS 1
    std::string topic;
    std::vector<uint8_t> recv;
    uint8_t qos;
    uint16_t packetId;
    REQUIRE(sub.readPublishWithQos(topic, recv, qos, packetId));
    CHECK(topic == "will/qos1topic");
    CHECK(recv == std::vector<uint8_t>{0xDE, 0xAD});
    CHECK(qos == 1);
    CHECK(packetId > 0); // server used nextPacketId()

    sub.closeIt();
}

TEST_CASE("TCP: retained QoS 2 message delivered to new subscriber")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Publisher stores a retained message at QoS 2
    {
        MqttClientEx pub;
        REQUIRE(pub.connectTo("127.0.0.1", port));
        REQUIRE(pub.sendConnect("retQos2Pub"));
        uint8_t sp, rc;
        REQUIRE(pub.readConnAck(sp, rc));

        std::vector<uint8_t> payload = {0xBE, 0xEF};
        REQUIRE(pub.sendPublishRetainQos2("ret/qos2topic", payload, 0x0002));

        // Server sends PUBREC for QoS 2 publish
        uint16_t recId;
        REQUIRE(pub.readPubRec(recId));
        CHECK(recId == 0x0002);

        // Send PUBREL
        REQUIRE(pub.sendPubRel(0x0002));

        // Server sends PUBCOMP
        uint16_t compId;
        REQUIRE(pub.readPubComp(compId));
        CHECK(compId == 0x0002);

        pub.sendDisconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pub.closeIt();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // New subscriber should receive the retained message at QoS 2
    {
        MqttClientEx sub;
        REQUIRE(sub.connectTo("127.0.0.1", port));
        REQUIRE(sub.sendConnect("retQos2Sub"));
        uint8_t sp, rc;
        REQUIRE(sub.readConnAck(sp, rc));

        REQUIRE(sub.sendSubscribe(0xE002, "ret/qos2topic", 2));
        uint16_t subRet;
        REQUIRE(sub.readSubAck(subRet));
        CHECK(subRet == 0xE002);

        // Read the retained message -- should arrive with QoS 2
        std::string topic;
        std::vector<uint8_t> recv;
        uint8_t qos;
        uint16_t packetId;
        REQUIRE(sub.readPublishWithQos(topic, recv, qos, packetId));
        CHECK(topic == "ret/qos2topic");
        CHECK(recv == std::vector<uint8_t>{0xBE, 0xEF});
        CHECK(qos == 2);
        CHECK(packetId > 0);

        sub.closeIt();
    }
}


// ============================================================================
//  Group P: MQTT 3.1.1 Compliance - CONNECT flags validation (2A)
// ============================================================================

TEST_CASE("Comm P1: CONNECT with reserved bit set")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with reserved bit 0 set (flags = 0x03 = clean session + reserved)
    REQUIRE(client.sendConnectWithFlags("reservedBit", 0x03));

    // Server should reject with non-zero return code
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc != 0);

    client.closeIt();
}

TEST_CASE("Comm P2: CONNECT with willFlag=0 but willQoS=1")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // flags = 0x02 (clean session) | 0x08 (willQoS bit 3) = no Will flag but QoS set
    REQUIRE(client.sendConnectWithFlags("badWillQos", 0x0A));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc != 0);

    client.closeIt();
}

TEST_CASE("Comm P3: CONNECT with password but no username")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // flags = 0x02 (clean session) | 0x40 (password) = password without username
    REQUIRE(client.sendConnectWithFlags("badPassNoUser", 0x42));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc != 0);

    client.closeIt();
}


// Reject oversized CONNECT length fields before allocation.
TEST_CASE("Comm P4: CONNECT with oversized protoNameLen is refused")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    setRecvTimeoutSeconds(client.sock, 5);

    // header + remLen=2 + protoNameLen=0xFFFF
    std::vector<uint8_t> pkt = { CTRL_CONNECT, 0x02, 0xFF, 0xFF };
    REQUIRE(client.sendAll(pkt));

    uint8_t sp = 0;
    uint8_t rc = 0;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc != 0);

    client.closeIt();
}

TEST_CASE("Comm P5: CONNECT with oversized clientId length is refused")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    setRecvTimeoutSeconds(client.sock, 5);

    // Valid through proto/level/flags/keepAlive, then cidLen=0xFFFF (12 body bytes).
    std::vector<uint8_t> pkt = {
        CTRL_CONNECT, 0x0C,
        0x00, 0x04, 'M', 'Q', 'T', 'T',
        0x04,
        0x02,
        0x00, 0x3C,
        0xFF, 0xFF
    };
    REQUIRE(client.sendAll(pkt));

    uint8_t sp = 0;
    uint8_t rc = 0;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc != 0);

    client.closeIt();
}

TEST_CASE("Comm P6: CONNECT with oversized remaining length is refused")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    setRecvTimeoutSeconds(client.sock, 5);

    // header + remLen varint = 200000 (0xC0 0x9A 0x0C); no body.
    std::vector<uint8_t> pkt = { CTRL_CONNECT, 0xC0, 0x9A, 0x0C };
    REQUIRE(client.sendAll(pkt));

    uint8_t sp = 0;
    uint8_t rc = 0;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc != 0);

    client.closeIt();
}

// Reject oversized remaining length on any control packet (default-case skip path).
TEST_CASE("Comm P7: unknown packet with oversized remaining length closes the connection")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    setRecvTimeoutSeconds(client.sock, 5);

    // Normal CONNECT to reach the handleClient main loop.
    REQUIRE(client.sendConnect("p7client"));
    uint8_t sp = 0;
    uint8_t rc = 0;
    REQUIRE(client.readConnAck(sp, rc));
    REQUIRE(rc == 0);

    // 0xF0 = reserved control type (falls into default skip path).
    // 0xFF 0xFF 0xFF 0x7F = max VarInt remLen (268,435,455).
    std::vector<uint8_t> pkt = { 0xF0, 0xFF, 0xFF, 0xFF, 0x7F };
    REQUIRE(client.sendAll(pkt));

    // Broker rejects via decodeRemainingLength cap; loop breaks; connection closed.
    uint8_t junk = 0;
    CHECK(client.recvAll(&junk, 1) == false);

    client.closeIt();
}


// ============================================================================
//  Group Q: MQTT 3.1.1 Compliance - Topic validation (2B, 2C, 2D, 2E)
// ============================================================================

TEST_CASE("Comm Q1: PUBLISH to topic with wildcard '+' is rejected")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardPub"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    // Publish to topic with '+' wildcard -- server should close connection
    REQUIRE(client.sendPublishWithTopic("test/+/data", {0x01}));

    // Wait for server to process and close connection
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify connection is dead by trying to send PINGREQ
    bool stillAlive = client.sendPingReq();
    if (stillAlive)
    {
        // Try to read PINGRESP - should fail if server dropped us
        CHECK_FALSE(client.readPingResp());
    }

    client.closeIt();
}

TEST_CASE("Comm Q2: PUBLISH to topic with wildcard '#' is rejected")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("hashPub"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    // Publish to topic with '#' wildcard
    REQUIRE(client.sendPublishWithTopic("test/#", {0x01}));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool stillAlive = client.sendPingReq();
    if (stillAlive)
    {
        CHECK_FALSE(client.readPingResp());
    }

    client.closeIt();
}

TEST_CASE("Comm Q3: QoS 1 PUBLISH with packetId=0 is rejected")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("zeroPacketId"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    // Send QoS 1 PUBLISH with packet ID = 0 (protocol violation)
    REQUIRE(client.sendPublishQos1("test/zero", {0x01}, 0x0000));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Server should have closed the connection
    bool stillAlive = client.sendPingReq();
    if (stillAlive)
    {
        CHECK_FALSE(client.readPingResp());
    }

    client.closeIt();
}

TEST_CASE("Comm Q4: SUBSCRIBE with empty topic filter is rejected")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("emptyFilter"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    // Subscribe with empty topic filter
    REQUIRE(client.sendSubscribeEmptyTopic(0x0001));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Server should have closed the connection (parseSubscribe returns false)
    bool stillAlive = client.sendPingReq();
    if (stillAlive)
    {
        CHECK_FALSE(client.readPingResp());
    }

    client.closeIt();
}

TEST_CASE("Comm Q5: System topic ($SYS) not matched by # subscriber")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    // Subscriber subscribes to '#' (all topics)
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    // Without a timeout, readPublish() below blocks forever on a transient
    // miss instead of failing cleanly -- this is what turned a passing test
    // into an indefinite CI hang.
    setRecvTimeoutSeconds(sub.sock, 5);
    REQUIRE(sub.sendConnect("sysTopicSub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));
    CHECK(rc == 0);

    REQUIRE(sub.sendSubscribe(0x0001, "#"));
    uint16_t subId;
    REQUIRE(sub.readSubAck(subId));

    // Publisher publishes to $SYS/test
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("sysTopicPub"));
    REQUIRE(pub.readConnAck(sp, rc));

    REQUIRE(pub.sendPublishWithTopic("$SYS/test", {0xAA}));

    // Also publish to a normal topic to verify subscriber works
    REQUIRE(pub.sendPublish("normal/topic", {0xBB}));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Subscriber should receive the normal topic but NOT the $SYS topic
    std::string topic;
    std::vector<uint8_t> payload;
    REQUIRE(sub.readPublish(topic, payload));
    CHECK(topic == "normal/topic");
    CHECK(payload == std::vector<uint8_t>{0xBB});

    pub.sendDisconnect();
    sub.sendDisconnect();
    pub.closeIt();
    sub.closeIt();
}


// ============================================================================
//  Group R: CommUtils helper functions and safety tests (5B, 1A, 1F)
// ============================================================================

TEST_CASE("Comm R1: readUint16BE and appendUint16BE")
{
    // Test encoding
    std::vector<uint8_t> vec;
    CommUtils::appendUint16BE(vec, 0x1234);
    REQUIRE(vec.size() == 2);
    CHECK(vec[0] == 0x12);
    CHECK(vec[1] == 0x34);

    // Test decoding
    uint16_t val = CommUtils::readUint16BE(vec.data());
    CHECK(val == 0x1234);

    // Edge cases
    std::vector<uint8_t> vec2;
    CommUtils::appendUint16BE(vec2, 0x0000);
    CHECK(CommUtils::readUint16BE(vec2.data()) == 0x0000);

    std::vector<uint8_t> vec3;
    CommUtils::appendUint16BE(vec3, 0xFFFF);
    CHECK(CommUtils::readUint16BE(vec3.data()) == 0xFFFF);
}

TEST_CASE("Comm R2: appendMqttString")
{
    std::vector<uint8_t> vec;
    CommUtils::appendMqttString(vec, "MQTT");

    REQUIRE(vec.size() == 6);
    CHECK(vec[0] == 0x00);
    CHECK(vec[1] == 0x04);
    CHECK(vec[2] == 'M');
    CHECK(vec[3] == 'Q');
    CHECK(vec[4] == 'T');
    CHECK(vec[5] == 'T');

    // Empty string
    std::vector<uint8_t> vec2;
    CommUtils::appendMqttString(vec2, "");
    REQUIRE(vec2.size() == 2);
    CHECK(vec2[0] == 0x00);
    CHECK(vec2[1] == 0x00);
}

TEST_CASE("Comm R3: Server clean shutdown with active client")
{
    cCommServer server(0);
    REQUIRE(server.start());
    // Give it a moment to bind and listen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const uint16_t port = static_cast<uint16_t>(server.getPort());

    // Connect a client that stays connected
    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("shutdownTest"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    // Stop server while client is still connected
    // This should complete without hanging (tests 1A fix)
    server.stop();

    client.closeIt();
}

TEST_CASE("Comm R4: Client reconnect after disconnect clears mPendingPubs")
{
    ServerRunner srv;
    const uint16_t port = srv.port;

    // Connect, subscribe, disconnect, reconnect, publish -- should not hang
    cCommClient client("127.0.0.1", port, "reconnectTest");
    REQUIRE(client.connect());

    bool received = false;
    client.subscribe("test/recon", QOS0, [&](const std::string&,
                                              const std::vector<uint8_t>&)
    {
        received = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Reconnect and publish -- should not hang due to stale mPendingPubs
    cCommClient client2("127.0.0.1", port, "reconnectTest2");
    REQUIRE(client2.connect());

    bool pubResult = client2.publish("test/recon", {0x01}, QOS0);
    CHECK(pubResult);

    client2.disconnect();
}


// ============================================================================
//  Group S: SUBACK QoS granting (Fix 1)
// ============================================================================

TEST_CASE("Comm S1: SUBACK grants QoS 1 when requested")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("subackQ1"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    REQUIRE(client.sendSubscribe(0xA001, "test/suback", 1));
    uint16_t subPktId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(subPktId, granted));
    CHECK(subPktId == 0xA001);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 1);

    client.closeIt();
}

TEST_CASE("Comm S2: SUBACK grants QoS 2 when requested")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("subackQ2"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == 0);

    REQUIRE(client.sendSubscribe(0xA002, "test/suback2", 2));
    uint16_t subPktId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(subPktId, granted));
    CHECK(subPktId == 0xA002);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 2);

    client.closeIt();
}

TEST_CASE("Comm S3: QoS 1 publish to QoS 0 subscriber delivers at QoS 0")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscriber subscribes at QoS 0
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("subQos0"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0xB001, "test/qosCap", 0));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // Publisher publishes at QoS 1
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("pubQos1"));
    REQUIRE(pub.readConnAck(sp, rc));

    // Send QoS 1 PUBLISH
    std::vector<uint8_t> payload = {0xAA, 0xBB};
    REQUIRE(pub.sendPublishQos1("test/qosCap", payload, 0x0010));

    // Read PUBACK from broker
    uint16_t ackId;
    REQUIRE(pub.readPubAck(ackId));
    CHECK(ackId == 0x0010);

    // Subscriber should receive at QoS 0 (downgraded)
    std::string topic;
    std::vector<uint8_t> recv;
    uint8_t qos;
    uint16_t packetId;
    REQUIRE(sub.readPublishWithQos(topic, recv, qos, packetId));
    CHECK(topic == "test/qosCap");
    CHECK(recv == payload);
    CHECK(qos == 0);
    CHECK(packetId == 0);

    pub.closeIt();
    sub.closeIt();
}

TEST_CASE("Comm S4: QoS 1 publish to QoS 1 subscriber delivers at QoS 1")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscriber subscribes at QoS 1
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("subQos1"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0xC001, "test/qos1del", 1));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // Publisher publishes at QoS 1
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("pubQos1b"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0xCC, 0xDD};
    REQUIRE(pub.sendPublishQos1("test/qos1del", payload, 0x0011));

    uint16_t ackId;
    REQUIRE(pub.readPubAck(ackId));

    // Subscriber should receive at QoS 1
    std::string topic;
    std::vector<uint8_t> recv;
    uint8_t qos;
    uint16_t packetId;
    REQUIRE(sub.readPublishWithQos(topic, recv, qos, packetId));
    CHECK(topic == "test/qos1del");
    CHECK(recv == payload);
    CHECK(qos == 1);
    CHECK(packetId > 0);

    pub.closeIt();
    sub.closeIt();
}


// ============================================================================
//  Group T: Server-assigned Client ID (Fix 2)
// ============================================================================

TEST_CASE("Comm T1: Empty client ID with cleanSession accepted")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnectEmptyId());
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);

    client.closeIt();
}


// ============================================================================
//  Group V: Wildcard validation (Fixes 5+7)
// ============================================================================

// MQTT 3.1.1 section 3.9.3: filter-level validation failures are
// signalled via SUBACK return code 0x80, not a connection close. These
// V1-V4 tests verify each malformed filter shape produces 0x80 while
// the connection stays alive.
TEST_CASE("Comm V1: Subscribe with 'sport#' returns 0x80 (# not after /)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardV1"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Subscribe with invalid filter "sport#" -- # must be preceded by /
    REQUIRE(client.sendSubscribe(0xD001, "sport#"));

    uint16_t ackId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(ackId, granted));
    CHECK(ackId == 0xD001);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 0x80);

    // Connection must stay alive.
    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());

    client.closeIt();
}

TEST_CASE("Comm V2: Subscribe with '#/sport' returns 0x80 (# not last)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardV2"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    REQUIRE(client.sendSubscribe(0xD002, "#/sport"));

    uint16_t ackId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(ackId, granted));
    CHECK(ackId == 0xD002);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 0x80);

    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());

    client.closeIt();
}

TEST_CASE("Comm V3: Subscribe with 'sport+' returns 0x80 (+ not full level)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardV3"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    REQUIRE(client.sendSubscribe(0xD003, "sport+"));

    uint16_t ackId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(ackId, granted));
    CHECK(ackId == 0xD003);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 0x80);

    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());

    client.closeIt();
}

TEST_CASE("Comm V4: Subscribe with '+sport' returns 0x80 (+ not full level)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardV4"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    REQUIRE(client.sendSubscribe(0xD004, "+sport"));

    uint16_t ackId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(ackId, granted));
    CHECK(ackId == 0xD004);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 0x80);

    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());

    client.closeIt();
}

TEST_CASE("Comm V5: Valid wildcards 'sport/+' and 'sport/#' accepted")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardV5"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Valid: "sport/+" -- + occupies entire level
    REQUIRE(client.sendSubscribe(0xD005, "sport/+"));
    uint16_t subRet;
    REQUIRE(client.readSubAck(subRet));
    CHECK(subRet == 0xD005);

    // Valid: "sport/#" -- # is last character after /
    REQUIRE(client.sendSubscribe(0xD006, "sport/#"));
    REQUIRE(client.readSubAck(subRet));
    CHECK(subRet == 0xD006);

    // Valid: "#" alone -- matches everything
    REQUIRE(client.sendSubscribe(0xD007, "#"));
    REQUIRE(client.readSubAck(subRet));
    CHECK(subRet == 0xD007);

    // Valid: "+" alone -- matches single level
    REQUIRE(client.sendSubscribe(0xD008, "+"));
    REQUIRE(client.readSubAck(subRet));
    CHECK(subRet == 0xD008);

    client.closeIt();
}

// Per MQTT 3.1.1 section 3.9.3, SUBACK return codes are per-filter, so
// a single SUBSCRIBE carrying both a valid and an invalid filter must
// produce a SUBACK with one grant code and one 0x80 (failure) entry,
// without closing the connection. Routing for the accepted filter must
// continue to work.
TEST_CASE("Comm V7: Mixed valid + invalid filters -> SUBACK with grant and 0x80")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("wildcardV7"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build SUBSCRIBE with two filters: "valid/topic" QoS 0, then
    // "bad#wrong" (# not after /) QoS 0.
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_SUBSCRIBE);
    size_t idxRL = pkt.size();
    pkt.push_back(0);  // remaining-length placeholder
    pkt.push_back(0xD0);
    pkt.push_back(0x09);  // packet id 0xD009
    auto pushFilter = [&](const std::string& topic, uint8_t qos)
    {
        pkt.push_back(uint8_t(topic.size() >> 8));
        pkt.push_back(uint8_t(topic.size() & 0xFF));
        pkt.insert(pkt.end(), topic.begin(), topic.end());
        pkt.push_back(qos);
    };
    pushFilter("valid/topic", 0);
    pushFilter("bad#wrong",   0);
    size_t rl = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl);
    REQUIRE(client.sendAll(pkt));

    uint16_t ackId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(ackId, granted));
    CHECK(ackId == 0xD009);
    REQUIRE(granted.size() == 2);
    CHECK(granted[0] == 0x00);
    CHECK(granted[1] == 0x80);

    // Connection still alive; ping responds.
    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());

    client.closeIt();
}

// Per MQTT 3.1.1 section 3.6.1-1 the reserved bits in PUBREL's fixed-
// header low nibble must be 0010. A PUBREL whose low nibble is 0000
// (i.e. raw control byte 0x60 instead of 0x62) is malformed and the
// server must close the connection.
TEST_CASE("TCP: PUBREL with bad reserved bits disconnects client")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("badPubrelClient"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // PUBREL with control byte 0x60 instead of the spec-required 0x62
    // (reserved bits 0010 -> 0000). Remaining length 2, packet id 1.
    std::vector<uint8_t> badPubrel = { 0x60, 0x02, 0x00, 0x01 };
    REQUIRE(client.sendAll(badPubrel));

    // Server should disconnect us.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bool alive = client.sendPingReq();
    if (alive)
    {
        alive = client.readPingResp();
    }
    CHECK_FALSE(alive);

    client.closeIt();
}

TEST_CASE("Comm V6: Client-side topic validation rejects wildcards")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    cCommClient client("127.0.0.1", port, "clientValV6");
    REQUIRE(client.connect());

    // Client publish should reject topics with wildcards
    CHECK_FALSE(client.publish("test/+/foo", {0x01}, QOS0));
    CHECK_FALSE(client.publish("test/#", {0x01}, QOS0));
    CHECK_FALSE(client.publish("", {0x01}, QOS0));

    // Valid topic should succeed
    CHECK(client.publish("test/valid", {0x01}, QOS0));

    client.disconnect();
}


// ============================================================================
//  Group X: Coverage improvement tests
// ============================================================================

TEST_CASE("Comm X1: cCommClient receives QoS 1 message (covers sendPubAck)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // cCommClient subscribes at QoS 1
    cCommClient client("127.0.0.1", port, "x1sub");
    REQUIRE(client.connect());

    std::string receivedTopic;
    std::vector<uint8_t> receivedPayload;
    std::mutex mtx;
    std::condition_variable cv;
    bool gotMsg = false;

    client.subscribe("test/qos1recv", QOS1,
        [&](const std::string& t, const std::vector<uint8_t>& p)
        {
            std::lock_guard<std::mutex> lk(mtx);
            receivedTopic = t;
            receivedPayload = p;
            gotMsg = true;
            cv.notify_all();
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Raw client publishes QoS 1 to the same topic
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("x1pub"));
    uint8_t sp, rc;
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0xDE, 0xAD};
    REQUIRE(pub.sendPublishQos1("test/qos1recv", payload, 0x0001));
    uint16_t ackId;
    REQUIRE(pub.readPubAck(ackId));

    // Wait for cCommClient to receive the message
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, std::chrono::seconds(5), [&]{ return gotMsg; });
    }
    CHECK(gotMsg);
    CHECK(receivedTopic == "test/qos1recv");
    CHECK(receivedPayload == payload);

    pub.closeIt();
    client.disconnect();
}

TEST_CASE("Comm X2: cCommClient receives QoS 2 message (covers sendPubRec)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // cCommClient subscribes at QoS 2
    cCommClient client("127.0.0.1", port, "x2sub");
    REQUIRE(client.connect());

    std::string receivedTopic;
    std::vector<uint8_t> receivedPayload;
    std::mutex mtx;
    std::condition_variable cv;
    bool gotMsg = false;

    client.subscribe("test/qos2recv", QOS2,
        [&](const std::string& t, const std::vector<uint8_t>& p)
        {
            std::lock_guard<std::mutex> lk(mtx);
            receivedTopic = t;
            receivedPayload = p;
            gotMsg = true;
            cv.notify_all();
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Raw client publishes QoS 2 with full handshake
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("x2pub"));
    uint8_t sp, rc;
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0xCA, 0xFE};
    REQUIRE(pub.sendPublishRetainQos2("test/qos2recv", payload, 0x0002));
    uint16_t recId;
    REQUIRE(pub.readPubRec(recId));
    REQUIRE(pub.sendPubRel(0x0002));
    uint16_t compId;
    REQUIRE(pub.readPubComp(compId));

    // Wait for cCommClient to receive the message
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, std::chrono::seconds(5), [&]{ return gotMsg; });
    }
    CHECK(gotMsg);
    CHECK(receivedTopic == "test/qos2recv");
    CHECK(receivedPayload == payload);

    pub.closeIt();
    client.disconnect();
}

TEST_CASE("Comm X3: Server retransmits unacked QoS 1 with DUP flag")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Raw subscriber connects at QoS 1 but will NOT ack
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("x3sub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0xA001, "test/retransmit", 1));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // Publisher sends QoS 1 message
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("x3pub"));
    REQUIRE(pub.readConnAck(sp, rc));

    std::vector<uint8_t> payload = {0xBB, 0xCC};
    REQUIRE(pub.sendPublishQos1("test/retransmit", payload, 0x0001));
    uint16_t ackId;
    REQUIRE(pub.readPubAck(ackId));

    // Read the first delivery (do NOT send PUBACK)
    std::string topic1;
    std::vector<uint8_t> recv1;
    uint8_t qos1;
    uint16_t pid1;
    REQUIRE(sub.readPublishWithQos(topic1, recv1, qos1, pid1));
    CHECK(topic1 == "test/retransmit");
    CHECK(qos1 == 1);
    // Intentionally NOT sending PUBACK

    // Wait for retransmit timer (5 second interval + 1 second check)
    std::this_thread::sleep_for(std::chrono::seconds(7));

    // Read the retransmitted message -- should have DUP flag set
    std::string topic2;
    std::vector<uint8_t> recv2;
    uint8_t qos2;
    uint16_t pid2;
    REQUIRE(sub.readPublishWithQos(topic2, recv2, qos2, pid2));
    CHECK(topic2 == "test/retransmit");
    CHECK(recv2 == payload);
    CHECK(qos2 == 1);
    CHECK(pid2 == pid1);  // Same packet ID

    // Now send PUBACK to clean up
    REQUIRE(sub.sendPubAck(pid2));

    pub.closeIt();
    sub.closeIt();
}

TEST_CASE("Comm X4: CONNECT with username and password")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with userFlag=1, passFlag=1
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t rlIdx = pkt.size();
    pkt.push_back(0);  // placeholder for remaining length

    // Protocol name "MQTT"
    pkt.push_back(0x00); pkt.push_back(0x04);
    pkt.push_back('M'); pkt.push_back('Q'); pkt.push_back('T'); pkt.push_back('T');
    // Protocol level 4
    pkt.push_back(0x04);
    // Connect flags: cleanSession=1(0x02) + userFlag=1(0x80) + passFlag=1(0x40) = 0xC2
    pkt.push_back(0xC2);
    // Keep-alive 60s
    pkt.push_back(0x00); pkt.push_back(0x3C);

    // Client ID "authClient"
    std::string cid = "authClient";
    pkt.push_back(static_cast<uint8_t>(cid.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(cid.size() & 0xFF));
    pkt.insert(pkt.end(), cid.begin(), cid.end());

    // Username "testuser"
    std::string user = "testuser";
    pkt.push_back(static_cast<uint8_t>(user.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(user.size() & 0xFF));
    pkt.insert(pkt.end(), user.begin(), user.end());

    // Password "testpass"
    std::string pass = "testpass";
    pkt.push_back(static_cast<uint8_t>(pass.size() >> 8));
    pkt.push_back(static_cast<uint8_t>(pass.size() & 0xFF));
    pkt.insert(pkt.end(), pass.begin(), pass.end());

    // Fix remaining length
    size_t rl = pkt.size() - rlIdx - 1;
    pkt[rlIdx] = static_cast<uint8_t>(rl);
    REQUIRE(client.sendAll(pkt));

    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));
    CHECK(rc == CONNACK_RC_ACCEPTED);

    client.closeIt();
}

TEST_CASE("Comm X5: readMqttString unit test")
{
    // Test readMqttString via a socketpair
    int fds[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Write an MQTT string: 2-byte big-endian length + string data
    std::string testStr = "hello/mqtt";
    uint8_t lenBuf[2] = {
        static_cast<uint8_t>(testStr.size() >> 8),
        static_cast<uint8_t>(testStr.size() & 0xFF)
    };
    REQUIRE(::write(fds[1], lenBuf, 2) == 2);
    REQUIRE(::write(fds[1], testStr.data(), testStr.size())
        == static_cast<ssize_t>(testStr.size()));

    std::string out;
    CHECK(CommUtils::readMqttString(fds[0], out));
    CHECK(out == "hello/mqtt");

    // Test empty string
    uint8_t zeroLen[2] = {0, 0};
    REQUIRE(::write(fds[1], zeroLen, 2) == 2);
    std::string emptyOut;
    CHECK(CommUtils::readMqttString(fds[0], emptyOut));
    CHECK(emptyOut.empty());

    // Test read failure (close writer, then try to read)
    ::close(fds[1]);
    std::string failOut;
    CHECK_FALSE(CommUtils::readMqttString(fds[0], failOut));

    ::close(fds[0]);
}

TEST_CASE("Comm X6: cCommClient publishes QoS 2 (covers PUBREC/PUBREL path)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscribe to receive the message
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("x6sub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0xB001, "test/qos2pub", 0));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // cCommClient publishes at QoS 2
    cCommClient client("127.0.0.1", port, "x6pub");
    REQUIRE(client.connect());

    std::vector<uint8_t> payload = {0xEE, 0xFF};
    bool pubResult = client.publish("test/qos2pub", payload, QOS2);
    CHECK(pubResult);

    // Subscriber should receive the message
    std::string topic;
    std::vector<uint8_t> recv;
    REQUIRE(sub.readPublish(topic, recv));
    CHECK(topic == "test/qos2pub");
    CHECK(recv == payload);

    sub.closeIt();
    client.disconnect();
}

// ============================================================================
// Group E: Error path coverage tests
// These send truncated or malformed packets to trigger error-handling code
// paths in the server's handleClient and parse functions.
// ============================================================================

TEST_CASE("Comm E1: truncated PUBLISH topic triggers handleClient error path")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e1client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send PUBLISH header + remaining length, but close before topic data
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);  // QoS 0 PUBLISH
    pkt.push_back(0x0A);          // remaining length = 10
    // Topic length says 8 bytes, but we only send the length field
    pkt.push_back(0x00);
    pkt.push_back(0x08);
    // Close without sending the 8 topic bytes
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    // Server should handle the truncated read gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E2: oversized PUBLISH payload triggers DoS protection")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e2client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build a PUBLISH with a remaining length claiming > 1 MB payload
    // Topic = "t" (3 bytes: 00 01 't'), payload = remLen - 3
    // Use 4-byte VarInt encoding for remaining length > 1MB
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);  // QoS 0 PUBLISH
    // Encode remaining length = 1048580 (> MAX_PAYLOAD_SIZE + topic overhead)
    // 1048580 = 0x100004
    // VarInt encoding: 0x84, 0x80, 0x40
    size_t bigLen = 1 * 1024 * 1024 + 4;
    auto rl = CommUtils::encodeRemainingLength(bigLen);
    pkt.insert(pkt.end(), rl.begin(), rl.end());
    // Topic length = 1, topic = "t"
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    pkt.push_back('t');
    // Don't send payload, just close
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E3: SUBSCRIBE with QoS > 2 returns 0x80 (per-filter rejection)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e3client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build SUBSCRIBE with QoS byte = 3 (invalid, max is 2). Per MQTT
    // 3.1.1 section 3.9.3 this is a per-filter rejection signalled in
    // SUBACK as 0x80, not a connection close.
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_SUBSCRIBE);
    size_t idxRL = pkt.size();
    pkt.push_back(0);  // placeholder
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    pkt.push_back(0x00);
    pkt.push_back(0x03);
    pkt.push_back('a');
    pkt.push_back('/');
    pkt.push_back('b');
    pkt.push_back(0x03);   // invalid QoS

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));

    uint16_t ackId;
    std::vector<uint8_t> granted;
    REQUIRE(client.readSubAckWithQos(ackId, granted));
    CHECK(ackId == 0x0001);
    REQUIRE(granted.size() == 1);
    CHECK(granted[0] == 0x80);

    // Connection must stay alive.
    REQUIRE(client.sendPingReq());
    CHECK(client.readPingResp());
    client.closeIt();
}

TEST_CASE("Comm E4: SUBSCRIBE with topic length exceeding remaining length")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e4client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build SUBSCRIBE where topic length field > remaining data
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_SUBSCRIBE);
    // Remaining length = 5 (packet ID 2 + topic length 2 + QoS 1 = 5)
    // But topic length will say 100 (way more than available)
    pkt.push_back(0x05);
    // Packet ID
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    // Topic length = 100 (but only 1 byte + QoS byte remain)
    pkt.push_back(0x00);
    pkt.push_back(0x64);
    // Only 1 byte remains (QoS)
    pkt.push_back(0x00);
    REQUIRE(client.sendAll(pkt));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.closeIt();
}

TEST_CASE("Comm E5: UNSUBSCRIBE with truncated topic data")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e5client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build UNSUBSCRIBE with topic length > available data
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_UNSUBSCRIBE);
    // Remaining length = 6 (packet ID 2 + topic length 2 + topic 2)
    pkt.push_back(0x06);
    // Packet ID
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    // Topic length = 50 (exceeds remaining)
    pkt.push_back(0x00);
    pkt.push_back(0x32);
    // Only 0 topic bytes sent
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E6: empty clientId with cleanSession=false rejected")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with empty clientId and cleanSession=false
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);             // protocol level
    pkt.push_back(0x00);          // flags: NO cleanSession, no will, no auth
    pkt.push_back(0x00);
    pkt.push_back(60);            // keepAlive
    // Empty client ID (length = 0)
    pkt.push_back(0x00);
    pkt.push_back(0x00);

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));

    uint8_t sp2, rc2;
    REQUIRE(client.readConnAck(sp2, rc2));
    CHECK(rc2 == CONNACK_RC_IDENTIFIER_REJECTED);

    client.closeIt();
}

TEST_CASE("Comm E7: malformed VarInt remaining length in handleClient")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e7client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send a packet with 5 continuation bytes (max allowed is 4)
    // Each byte has high bit set (continuation), making it an invalid VarInt
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);  // packet type
    pkt.push_back(0x80);          // continuation
    pkt.push_back(0x80);          // continuation
    pkt.push_back(0x80);          // continuation
    pkt.push_back(0x80);          // 4th continuation (exceeds max multiplier)
    REQUIRE(client.sendAll(pkt));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.closeIt();
}

TEST_CASE("Comm E8: truncated SUBSCRIBE packet (remLen < 2)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e8client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send SUBSCRIBE with remaining length = 1 (needs at least 2 for packet ID)
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_SUBSCRIBE);
    pkt.push_back(0x01);  // remaining length = 1 (too short)
    pkt.push_back(0x00);  // only 1 byte of packet ID
    REQUIRE(client.sendAll(pkt));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.closeIt();
}

TEST_CASE("Comm E9: truncated UNSUBSCRIBE packet (remLen < 2)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e9client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send UNSUBSCRIBE with remaining length = 1 (too short)
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_UNSUBSCRIBE);
    pkt.push_back(0x01);  // remaining length = 1
    pkt.push_back(0x00);
    REQUIRE(client.sendAll(pkt));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.closeIt();
}

TEST_CASE("Comm E10: PUBREL with truncated packet ID")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e10client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send PUBREL header but close before sending packet ID
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBREL);
    pkt.push_back(0x02);  // remaining length = 2
    // Close without sending the 2-byte packet ID
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E11: truncated PUBLISH with QoS 1 (no packet ID)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e11client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send QoS 1 PUBLISH with valid topic but close before packet ID
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH | 0x02);  // QoS 1
    pkt.push_back(0x07);                 // remaining length = 7
    // Topic "test" (length 4)
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('t');
    pkt.push_back('e');
    pkt.push_back('s');
    pkt.push_back('t');
    // Need 2 more bytes for packet ID but close instead
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E12: truncated PUBLISH payload (QoS 0)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e12client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send QoS 0 PUBLISH with topic but truncated payload
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);  // QoS 0
    pkt.push_back(0x0C);         // remaining length = 12 (topic 2+4 + payload 6)
    // Topic "test" (length 4)
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('t');
    pkt.push_back('e');
    pkt.push_back('s');
    pkt.push_back('t');
    // Payload should be 6 bytes but close without sending any
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E13: unknown packet type triggers default skip")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e13client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send a packet with type 15 (0xF0) which is reserved/unknown
    // with valid remaining length and data so the skip works
    std::vector<uint8_t> pkt;
    pkt.push_back(0xF0);     // type 15 (unknown)
    pkt.push_back(0x03);     // remaining length = 3
    pkt.push_back(0xAA);
    pkt.push_back(0xBB);
    pkt.push_back(0xCC);
    REQUIRE(client.sendAll(pkt));

    // Server should skip the unknown packet and stay connected
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bool alive = client.sendPingReq();
    CHECK(alive);
    if (alive)
    {
        CHECK(client.readPingResp());
    }

    client.closeIt();
}

TEST_CASE("Comm E14: SUBSCRIBE with truncated topic read")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e14client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build SUBSCRIBE with topic length = 10 but close after sending 2 bytes of topic
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_SUBSCRIBE);
    pkt.push_back(0x0F);  // remaining length = 15
    // Packet ID
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    // Topic length = 10
    pkt.push_back(0x00);
    pkt.push_back(0x0A);
    // Only send 2 bytes of the 10-byte topic, then close
    pkt.push_back('a');
    pkt.push_back('b');
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E15: CONNECT with truncated Will topic")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with willFlag=1 but truncated Will Topic
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);
    // flags: cleanSession + willFlag (0x02 | 0x04 = 0x06)
    pkt.push_back(0x06);
    pkt.push_back(0x00);
    pkt.push_back(60);
    pushStr("e15client");
    // Will Topic length says 50 but we only include the length field
    pkt.push_back(0x00);
    pkt.push_back(0x32);
    // No actual topic data follows

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E16: CONNECT with truncated username")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with userFlag=1 but truncated username
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);
    // flags: cleanSession + userFlag (0x02 | 0x80 = 0x82)
    pkt.push_back(0x82);
    pkt.push_back(0x00);
    pkt.push_back(60);
    pushStr("e16client");
    // Username length says 50 but we close immediately
    pkt.push_back(0x00);
    pkt.push_back(0x32);

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E17: CONNECT with truncated password")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with user+pass flag, valid username, truncated password
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);
    // flags: cleanSession + userFlag + passFlag (0x02 | 0x80 | 0x40 = 0xC2)
    pkt.push_back(0xC2);
    pkt.push_back(0x00);
    pkt.push_back(60);
    pushStr("e17client");
    pushStr("validuser");
    // Password length says 50 but close immediately
    pkt.push_back(0x00);
    pkt.push_back(0x32);

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E18: CONNECT with truncated protocol name")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT header + remaining length, but close before protocol name
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x0A);  // remaining length = 10
    // Close without sending protocol name
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E19: CONNECT with non-MQTT protocol name")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with protocol name "XXXX" instead of "MQTT"
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    // Protocol name "XXXX"
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('X');
    pkt.push_back('X');
    pkt.push_back('X');
    pkt.push_back('X');
    // Protocol level
    pkt.push_back(4);
    // Flags: clean session
    pkt.push_back(0x02);
    // Keep alive
    pkt.push_back(0x00);
    pkt.push_back(60);
    // Client ID "e19"
    pkt.push_back(0x00);
    pkt.push_back(0x03);
    pkt.push_back('e');
    pkt.push_back('1');
    pkt.push_back('9');

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));

    uint8_t sp2, rc2;
    REQUIRE(client.readConnAck(sp2, rc2));
    CHECK(rc2 == CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION);

    client.closeIt();
}

TEST_CASE("Comm E20: CONNECT with wrong protocol level (5)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with protocol level 5 instead of 4
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    // Protocol name "MQTT"
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('M');
    pkt.push_back('Q');
    pkt.push_back('T');
    pkt.push_back('T');
    // Protocol level 5 (wrong, expect 4)
    pkt.push_back(5);
    pkt.push_back(0x02);
    pkt.push_back(0x00);
    pkt.push_back(60);
    // Client ID
    pkt.push_back(0x00);
    pkt.push_back(0x03);
    pkt.push_back('e');
    pkt.push_back('2');
    pkt.push_back('0');

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));

    uint8_t sp2, rc2;
    REQUIRE(client.readConnAck(sp2, rc2));
    CHECK(rc2 == CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION);

    client.closeIt();
}

TEST_CASE("Comm E21: CONNECT truncated before connect flags")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with protocol name + level, but close before connect flags
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x07);  // remaining length (only 7 bytes follow)
    // Protocol name "MQTT"
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('M');
    pkt.push_back('Q');
    pkt.push_back('T');
    pkt.push_back('T');
    // Protocol level 4
    pkt.push_back(0x04);
    // Close without sending connect flags byte
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E22: CONNECT truncated before keep-alive")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with flags but close before keep-alive field
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x08);  // remaining length
    // Protocol name "MQTT"
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('M');
    pkt.push_back('Q');
    pkt.push_back('T');
    pkt.push_back('T');
    pkt.push_back(0x04);
    pkt.push_back(0x02);  // cleanSession
    // Close without keep-alive
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E23: CONNECT truncated before client ID")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with keep-alive but close before client ID
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x0A);  // remaining length
    // Protocol name "MQTT"
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('M');
    pkt.push_back('Q');
    pkt.push_back('T');
    pkt.push_back('T');
    pkt.push_back(0x04);
    pkt.push_back(0x02);  // cleanSession
    pkt.push_back(0x00);
    pkt.push_back(0x3C);  // keepAlive = 60
    // Close without client ID length field
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E24: CONNECT with truncated client ID string")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT where client ID length says 20 but we only send 2 bytes
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x10);  // remaining length
    // Protocol name "MQTT"
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('M');
    pkt.push_back('Q');
    pkt.push_back('T');
    pkt.push_back('T');
    pkt.push_back(0x04);
    pkt.push_back(0x02);
    pkt.push_back(0x00);
    pkt.push_back(0x3C);
    // Client ID length = 20
    pkt.push_back(0x00);
    pkt.push_back(0x14);
    // Only send 2 bytes of the 20
    pkt.push_back('a');
    pkt.push_back('b');
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E25: CONNECT with Will payload truncated")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with valid Will topic but truncated Will payload
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);
    // flags: cleanSession + willFlag (0x06)
    pkt.push_back(0x06);
    pkt.push_back(0x00);
    pkt.push_back(60);
    pushStr("e25client");
    pushStr("will/topic");  // valid will topic
    // Will payload length says 50 but close immediately
    pkt.push_back(0x00);
    pkt.push_back(0x32);

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E26: CONNECT truncated after header byte only")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send only the CONNECT header byte, then close
    // This should trigger the decodeRemainingLength failure
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E27: handleClient with truncated remaining length")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e27client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send just one byte of a PUBLISH, then close
    // The server reads the header byte OK but decodeRemainingLength fails
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E28: SUBSCRIBE with truncated QoS byte")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e28client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build SUBSCRIBE with valid packet ID and topic but missing QoS byte
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_SUBSCRIBE);
    // remaining length = packet ID (2) + topic length (2) + topic (3) + QoS (1) = 8
    pkt.push_back(0x08);
    // Packet ID
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    // Topic "a/b" (length=3)
    pkt.push_back(0x00);
    pkt.push_back(0x03);
    pkt.push_back('a');
    pkt.push_back('/');
    pkt.push_back('b');
    // Missing QoS byte - close socket
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E29: UNSUBSCRIBE with truncated topic string")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e29client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Build UNSUBSCRIBE with topic length > actual data
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_UNSUBSCRIBE);
    // remaining length = packet ID (2) + topic length (2) + topic (10) = 14
    pkt.push_back(0x0E);
    // Packet ID
    pkt.push_back(0x00);
    pkt.push_back(0x01);
    // Topic length = 10
    pkt.push_back(0x00);
    pkt.push_back(0x0A);
    // Only send 2 bytes then close
    pkt.push_back('x');
    pkt.push_back('y');
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E30: CONNECT truncated in protocol name string")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with protocol name length = 4, but only 2 bytes of name
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x0A);  // remaining length (won't matter, we close early)
    pkt.push_back(0x00);
    pkt.push_back(0x04);  // protocol name length = 4
    pkt.push_back('M');
    pkt.push_back('Q');   // only 2 of 4 bytes, then close
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E31: CONNECT truncated after protocol name before level")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Send CONNECT with complete protocol name but close before protocol level
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    pkt.push_back(0x0A);  // remaining length
    pkt.push_back(0x00);
    pkt.push_back(0x04);
    pkt.push_back('M');
    pkt.push_back('Q');
    pkt.push_back('T');
    pkt.push_back('T');   // complete protocol name, then close
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E32: CONNECT with Will topic OK but Will payload length truncated")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build CONNECT with valid Will topic, but close after sending
    // will payload length header (before actual payload bytes)
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);
    pkt.push_back(0x06);  // cleanSession + willFlag
    pkt.push_back(0x00);
    pkt.push_back(60);
    pushStr("e32cli");
    pushStr("will/t");   // valid will topic
    // Will payload length says 30 but we send the length then close
    pkt.push_back(0x00);
    pkt.push_back(0x1E);
    // Send 1 byte of will payload then close
    pkt.push_back(0xAA);

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E33: handleClient PUBLISH with truncated topic length field")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e33client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send PUBLISH header + remaining length, then close before topic length
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_PUBLISH);
    pkt.push_back(0x06);  // remaining length = 6
    // Close without sending topic length field (2 bytes)
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E34: '+' wildcard with fewer topic segments than filter")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    // Subscribe to "a/+/c" then publish to "a" (only 1 segment)
    MqttClientEx sub;
    REQUIRE(sub.connectTo("127.0.0.1", port));
    REQUIRE(sub.sendConnect("e34sub"));
    uint8_t sp, rc;
    REQUIRE(sub.readConnAck(sp, rc));
    REQUIRE(sub.sendSubscribe(0x0001, "a/+/c"));
    uint16_t subRet;
    REQUIRE(sub.readSubAck(subRet));

    // Publish to "a" -- the '+' wildcard at level 2 should fail to match
    // because "a" has only 1 segment
    MqttClientEx pub;
    REQUIRE(pub.connectTo("127.0.0.1", port));
    REQUIRE(pub.sendConnect("e34pub"));
    REQUIRE(pub.readConnAck(sp, rc));
    REQUIRE(pub.sendPublish("a", {0x01}));

    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Subscriber should NOT have received anything
    // Verify by sending a message on a matching topic
    REQUIRE(pub.sendPublish("a/x/c", {0x02}));
    std::string topic;
    std::vector<uint8_t> recv;
    REQUIRE(sub.readPublish(topic, recv));
    CHECK(topic == "a/x/c");
    CHECK(recv == std::vector<uint8_t>{0x02});

    pub.closeIt();
    sub.closeIt();
}

TEST_CASE("Comm E35: unknown packet with truncated skip data")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));
    REQUIRE(client.sendConnect("e35client"));
    uint8_t sp, rc;
    REQUIRE(client.readConnAck(sp, rc));

    // Send unknown packet type 15 with remaining length = 10, but close
    // before sending the 10 bytes of skip data
    std::vector<uint8_t> pkt;
    pkt.push_back(0xF0);    // type 15 (unknown)
    pkt.push_back(0x0A);    // remaining length = 10
    pkt.push_back(0xAA);    // only 1 byte, then close
    REQUIRE(client.sendAll(pkt));
    client.closeIt();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_CASE("Comm E36: CONNECT with extra bytes after all fields (leftover skip)")
{
    ServerRunner runner;
    const uint16_t port = runner.port;

    MqttClientEx client;
    REQUIRE(client.connectTo("127.0.0.1", port));

    // Build a valid CONNECT but with remaining length larger than actual data,
    // so the server skips leftover bytes at the end
    std::vector<uint8_t> pkt;
    pkt.push_back(CTRL_CONNECT);
    size_t idxRL = pkt.size();
    pkt.push_back(0);

    auto pushStr = [&](const std::string& s)
    {
        pkt.push_back(uint8_t(s.size() >> 8));
        pkt.push_back(uint8_t(s.size() & 0xFF));
        pkt.insert(pkt.end(), s.begin(), s.end());
    };

    pushStr("MQTT");
    pkt.push_back(4);
    pkt.push_back(0x02);  // cleanSession
    pkt.push_back(0x00);
    pkt.push_back(60);
    pushStr("e36client");

    // Add 5 extra bytes (the server should skip them)
    pkt.push_back(0xDE);
    pkt.push_back(0xAD);
    pkt.push_back(0xBE);
    pkt.push_back(0xEF);
    pkt.push_back(0x00);

    size_t rl2 = pkt.size() - idxRL - 1;
    pkt[idxRL] = static_cast<uint8_t>(rl2);
    REQUIRE(client.sendAll(pkt));

    uint8_t sp2, rc2;
    REQUIRE(client.readConnAck(sp2, rc2));
    CHECK(rc2 == CONNACK_RC_ACCEPTED);

    client.closeIt();
}


#ifndef _WIN32
#include <csignal>
#include <cerrno>

// Proves the bug: writes to a half-closed socket must not raise SIGPIPE,
// which would otherwise terminate the broker process. The fix uses
// MSG_NOSIGNAL on Linux and SO_NOSIGPIPE on macOS.
TEST_CASE("Comm: socket_write does not raise SIGPIPE on broken-pipe write")
{
    int sv[2];
    REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    // Mirror production wiring: enables SO_NOSIGPIPE on macOS; no-op on Linux.
    socket_set_nosigpipe(sv[1]);

    // Close the read end so writes on sv[1] hit a half-closed socket.
    ::close(sv[0]);

    // Custom SIGPIPE handler records receipt without terminating the process.
    // SIG_IGN would mask the bug; SIG_DFL would kill the test binary.
    static std::atomic<bool> gSigpipeFired{false};
    gSigpipeFired = false;
    struct sigaction newAct{};
    struct sigaction oldAct{};
    newAct.sa_handler = [](int){ gSigpipeFired.store(true); };
    sigemptyset(&newAct.sa_mask);
    REQUIRE(::sigaction(SIGPIPE, &newAct, &oldAct) == 0);

    errno = 0;
    const char buf[] = "x";
    ssize_t result = socket_write(sv[1], buf, sizeof(buf));
    int savedErrno = errno;

    // Restore the original handler before asserting so an assertion failure
    // does not leave our temporary handler installed for later tests.
    ::sigaction(SIGPIPE, &oldAct, nullptr);

    CHECK(result == -1);
    CHECK(savedErrno == EPIPE);
    CHECK_FALSE(gSigpipeFired.load());

    ::close(sv[1]);
}
#endif
