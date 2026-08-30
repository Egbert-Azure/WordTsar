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

#include <cstdint>

// Other constants
static constexpr uint16_t DEFAULT_KEEP_ALIVE_SECONDS = 60;

// MQTT Control Packet Types
static constexpr uint8_t CTRL_CONNECT     = 0x10;
static constexpr uint8_t CTRL_CONNACK     = 0x20;
static constexpr uint8_t CTRL_PUBLISH     = 0x30;
static constexpr uint8_t CTRL_PUBACK      = 0x40;
static constexpr uint8_t CTRL_PUBREC      = 0x50;
static constexpr uint8_t CTRL_PUBREL      = 0x62;
static constexpr uint8_t CTRL_PUBCOMP     = 0x70;
static constexpr uint8_t CTRL_SUBSCRIBE   = 0x82;
static constexpr uint8_t CTRL_SUBACK      = 0x90;
static constexpr uint8_t CTRL_UNSUBSCRIBE = 0xA2;
static constexpr uint8_t CTRL_UNSUBACK    = 0xB0;
static constexpr uint8_t CTRL_PINGREQ     = 0xC0;
static constexpr uint8_t CTRL_PINGRESP    = 0xD0;
static constexpr uint8_t CTRL_DISCONNECT  = 0xE0;

// CONNACK Return Codes (MQTT 3.1.1 section 3.2.2-2)
static constexpr uint8_t CONNACK_RC_ACCEPTED                       = 0x00;
static constexpr uint8_t CONNACK_RC_UNACCEPTABLE_PROTOCOL_VERSION = 0x01;
static constexpr uint8_t CONNACK_RC_IDENTIFIER_REJECTED           = 0x02;
static constexpr uint8_t CONNACK_RC_SERVER_UNAVAILABLE            = 0x03;
static constexpr uint8_t CONNACK_RC_BAD_USERNAME_OR_PASSWORD      = 0x04;
static constexpr uint8_t CONNACK_RC_NOT_AUTHORIZED                = 0x05;

// CONNACK Flags
static constexpr uint8_t CONNACK_FLAG_SESSION_PRESENT = 0x01;

// Packet types for easier comparison (shifted down from control byte)
static constexpr uint8_t PKT_CONNECT     = CTRL_CONNECT >> 4;
static constexpr uint8_t PKT_CONNACK     = CTRL_CONNACK >> 4;
static constexpr uint8_t PKT_PUBLISH     = CTRL_PUBLISH >> 4;
static constexpr uint8_t PKT_PUBACK      = CTRL_PUBACK >> 4;
static constexpr uint8_t PKT_PUBREC      = CTRL_PUBREC >> 4;
static constexpr uint8_t PKT_PUBREL      = CTRL_PUBREL >> 4;
static constexpr uint8_t PKT_PUBCOMP     = CTRL_PUBCOMP >> 4;
static constexpr uint8_t PKT_SUBSCRIBE   = CTRL_SUBSCRIBE >> 4;
static constexpr uint8_t PKT_SUBACK      = CTRL_SUBACK >> 4;
static constexpr uint8_t PKT_UNSUBSCRIBE = CTRL_UNSUBSCRIBE >> 4;
static constexpr uint8_t PKT_UNSUBACK    = CTRL_UNSUBACK >> 4;
static constexpr uint8_t PKT_PINGREQ     = CTRL_PINGREQ >> 4;
static constexpr uint8_t PKT_PINGRESP    = CTRL_PINGRESP >> 4;
static constexpr uint8_t PKT_DISCONNECT  = CTRL_DISCONNECT >> 4;

// Protocol name and level
static constexpr uint16_t PROTO_NAME_LEN       = 4;
static constexpr const char* PROTO_NAME        = "MQTT";
static constexpr uint8_t  PROTO_LEVEL          = 4;
static constexpr uint8_t  CONNECT_FLAG_CLEAN_S = 0x02;
static constexpr uint16_t KEEP_ALIVE_SECONDS   = DEFAULT_KEEP_ALIVE_SECONDS;

// Quality of Service levels
static constexpr uint8_t QOS0 = 0x00;
static constexpr uint8_t QOS1 = 0x01;
static constexpr uint8_t QOS2 = 0x02;
