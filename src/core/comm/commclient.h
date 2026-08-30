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

#ifndef COMMCLIENT_H
#define COMMCLIENT_H

#include "commdefs.h"
#include "comm_utils.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <chrono>

/// @class cCommClient
/// @brief MQTT 3.1.1 client for plugin-side communication with the WordTsar broker.
///        Supports QoS 0, 1, and 2 publish/subscribe operations.
class cCommClient
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cCommClient(const std::string& host, int port, const std::string& clientId);
    ~cCommClient(void);

    bool connect(void);
    void disconnect(void);

    // Publish with selectable QoS (0, 1, or 2)
    bool publish(const std::string& topic,
                 const std::vector<uint8_t>& payload,
                 uint8_t qos = QOS0);

    // Subscribe with requested QoS and message callback
    bool subscribe(const std::string& topic,
                   uint8_t qos,
                   std::function<void(const std::string&,
                                      const std::vector<uint8_t>&)> callback);

private:
    // Internal helpers
    void                          receiveLoop(void);
    bool                          parseConnAck(void);
    static std::vector<uint8_t>   encodeRemainingLength(size_t length);

    // QoS 1 & 2 handshake methods
    bool sendPubAck(uint16_t packetId);
    bool sendPubRec(uint16_t packetId);
    bool sendPubRel(uint16_t packetId);
    bool sendPubComp(uint16_t packetId);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    // Connection parameters
    std::string mHost;
    int         mPort;
    std::string mClientId;
    int                   mSock{-1};
    std::atomic<bool>     mConnected{false};

    // Receiver thread & callback
    std::thread mRecvThread;
    std::mutex  mCallbackMutex;
    std::function<void(const std::string&, const std::vector<uint8_t>&)> mMsgCallback;

    // For QoS-1/2 publication tracking
    std::mutex                                 mPubMutex;
    std::condition_variable                    mPubCond;
    std::unordered_map<uint16_t, bool>         mPendingPubs;

    // Packet ID generators (instance members, not static)
    uint16_t mNextPacketId{1};
    uint16_t mNextSubscribeId{1};

    // Keep-alive: timestamp of last packet sent
    std::chrono::steady_clock::time_point mLastPacketSent;
};

#endif // COMMCLIENT_H
