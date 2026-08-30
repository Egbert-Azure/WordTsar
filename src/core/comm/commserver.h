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

#ifndef COMMSERVER_H
#define COMMSERVER_H

#include "commdefs.h"
#include "comm_utils.h"

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <cstdint>
#include <chrono>

// DoS protection limits
static constexpr size_t MAX_PAYLOAD_SIZE          = 1 * 1024 * 1024;  // 1 MB
static constexpr size_t MAX_FILTERS_PER_SUBSCRIBE = 100;
static constexpr size_t MAX_RETAINED_MESSAGES     = 1000;
static constexpr size_t MAX_INFLIGHT_PER_CLIENT   = 1000;

// CONNECT field length caps (reject before allocation)
static constexpr uint16_t MAX_PROTO_NAME_LEN   = 8;
static constexpr uint16_t MAX_CLIENT_ID_LEN    = 256;
static constexpr uint16_t MAX_WILL_TOPIC_LEN   = 1024;
static constexpr uint16_t MAX_WILL_PAYLOAD_LEN = 8192;
static constexpr uint16_t MAX_USERNAME_LEN     = 1024;
static constexpr uint16_t MAX_PASSWORD_LEN     = 1024;
static constexpr size_t   MAX_CONNECT_REM_LEN  = 65536;

class cCommServer
{
    // =================================================================
    //  METHODS
    // =================================================================

public:
    explicit cCommServer(int port);
    ~cCommServer(void);

    bool start(void);
    void stop(void);

    int getPort(void) const;

private:
    // Client handling
    void acceptClients(void);
    void handleClient(int clientSock);

    // Connection setup
    uint8_t parseConnect(int clientSock);
    bool sendConnAck(int clientSock, uint8_t returnCode, bool sessionPresent);

    // Subscribe: parse N filters from the remaining length
    bool parseSubscribe(int clientSock,
        uint16_t& packetId,
        size_t remLen,
        std::vector<std::pair<std::string,uint8_t>>& filters);

    // Unsubscribe: parse N topic filters from the remaining length
    bool parseUnsubscribe(int clientSock,
        uint16_t& packetId,
        size_t remLen,
        std::vector<std::string>& topics);

    bool sendSubAck(int clientSock,
                    uint16_t packetId,
                    const std::vector<uint8_t> &qosResults);

    // PUBLISH with optional QoS and retain flags
    bool sendPublish(int clientSock,
                     const std::string &topic,
                     const std::vector<uint8_t> &payload,
                     uint8_t qos    = QOS0,
                     bool    retain = false);

    bool sendUnsubAck(int clientSock,
                      uint16_t packetId);

    // QoS 1 & 2 handshake
    bool sendPubAck(int clientSock, uint16_t packetId);
    bool sendPubRec(int clientSock, uint16_t packetId);
    bool sendPubComp(int clientSock, uint16_t packetId);

    // Keep-alive / Disconnect
    bool sendPingResp(int clientSock);
    bool parseDisconnect(int clientSock);

    // Helper to allocate a new packetId for a client
    uint16_t nextPacketId(int clientSock);

    // Retransmission
    void retransmitLoop(void);

    // Topic filter matching
    std::vector<std::string> splitTopic(const std::string& t);
    bool filterMatches(const std::string& filter, const std::string& topic);

    // =================================================================
    //  MEMBER VARIABLES
    // =================================================================

private:
    int                                      mPort;
    int                                      mServerSock;
    bool                                     mRunning;
    std::thread                              mAcceptThread;
    std::mutex                               mMutex;

    // Unified client registry
    std::map<int, CommUtils::sClientInfo>     mClients;

    // Topic subscriptions: topic -> list of (socket, granted QoS) pairs
    std::map<std::string, std::vector<std::pair<int, uint8_t>>>  mTopicSubscriptions;

    // Persistent subscriptions for clients with cleanSession=false (in-memory only)
    std::map<std::string, std::vector<std::pair<std::string,uint8_t>>> mPersistentSubscriptions;

    // Retained messages
    std::map<std::string, CommUtils::sRetainedMessage> mRetained;

    // Client handler threads (joined in stop())
    std::vector<std::thread>                 mClientThreads;
    std::mutex                               mThreadsMutex;

    // Retransmission thread
    std::thread mRetransmitThread;
};

#endif // COMMSERVER_H
