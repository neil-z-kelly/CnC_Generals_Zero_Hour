/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// Transport.h ///////////////////////////////////////////////////////////////
// Transport layer - a thin layer around a UDP socket, with queues.
// Author: Matthew D. Campbell, July 2001

#pragma once

#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_

#include "GameNetwork/udp.h"
#include "GameNetwork/NetworkDefs.h"

/**
 * The transport layer handles the UDP socket for the game, and will packetize and
 * de-packetize multiple ACK/CommandPacket/etc packets into larger aggregates.
 */
// we only ever allocate one of there, and it is quite large, so we really DON'T want
// it to be a MemoryPoolObject (srj)
class Transport //: public MemoryPoolObject
{
	//MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(Transport, "Transport")		
public:

	Transport();
	~Transport();

	Bool init( AsciiString ip, UnsignedShort port );
	Bool init( UnsignedInt ip, UnsignedShort port );
	void reset( void );
	Bool update( void );									///< Call this once a GameEngine tick, regardless of whether the frame advances.

	Bool doRecv( void );		///< call this to service the receive packets
	Bool doSend( void );		///< call this to service the send queue.

	Bool queueSend(UnsignedInt addr, UnsignedShort port, const UnsignedByte *buf, Int len /*,
		NetMessageFlags flags, Int id */);				///< Queue a packet for sending to the specified address and port.  This will be sent on the next update() call.

	inline Bool allowBroadcasts(Bool val) { if (!m_udpsock) return false; return (m_udpsock->AllowBroadcasts(val))?true:false; }

	/**
	 * Packet authentication.  While a session key is set, every packet carries a
	 * keyed MAC over the packet contents, and packets that do not carry a valid
	 * MAC for that key are discarded before they reach the parsers.  With no key
	 * set (lobby/LAN discovery/NAT negotiation, which happen before a key can be
	 * agreed on) the tag degenerates to the legacy unkeyed checksum, which only
	 * detects corruption and authenticates nothing.
	 */
	void setSessionKey( const UnsignedByte *key, Int keyLen );
	void clearSessionKey( void );
	Bool hasSessionKey( void ) const { return m_hasSessionKey; }

	/**
	 * Known peer addresses.  While the list is non-empty, datagrams from any other
	 * source address are dropped without being parsed.
	 */
	void clearAllowedPeers( void );
	void addAllowedPeer( UnsignedInt addr );

	// Latency insertion and packet loss
	void setLatency( Bool val ) { m_useLatency = val; }
	void setPacketLoss( Bool val ) { m_usePacketLoss = val; }

	// Bandwidth metrics
	Real getIncomingBytesPerSecond( void );
	Real getIncomingPacketsPerSecond( void );
	Real getOutgoingBytesPerSecond( void );
	Real getOutgoingPacketsPerSecond( void );
	Real getUnknownBytesPerSecond( void );
	Real getUnknownPacketsPerSecond( void );

	TransportMessage m_outBuffer[MAX_MESSAGES];
	TransportMessage m_inBuffer[MAX_MESSAGES];

#if defined(_DEBUG) || defined(_INTERNAL)
	DelayedTransportMessage m_delayedInBuffer[MAX_MESSAGES];
#endif

	UnsignedShort m_port;
private:
	Bool m_winsockInit;
	UDP *m_udpsock;

	// Latency insertion and packet loss
	Bool m_useLatency;
	Bool m_usePacketLoss;

	// Bandwidth metrics
	UnsignedInt m_incomingBytes[MAX_TRANSPORT_STATISTICS_SECONDS];
	UnsignedInt m_unknownBytes[MAX_TRANSPORT_STATISTICS_SECONDS];
	UnsignedInt m_outgoingBytes[MAX_TRANSPORT_STATISTICS_SECONDS];
	UnsignedInt m_incomingPackets[MAX_TRANSPORT_STATISTICS_SECONDS];
	UnsignedInt m_unknownPackets[MAX_TRANSPORT_STATISTICS_SECONDS];
	UnsignedInt m_outgoingPackets[MAX_TRANSPORT_STATISTICS_SECONDS];
	Int m_statisticsSlot;
	UnsignedInt m_lastSecond;

	// Packet authentication state
	UnsignedByte m_sessionKey[TRANSPORT_SESSION_KEY_LEN];
	Bool m_hasSessionKey;
	UnsignedInt m_allowedPeers[MAX_SLOTS];
	Int m_numAllowedPeers;

	Bool isAllowedPeer( UnsignedInt addr ) const;
	void computeMessageMac( const TransportMessage *msg, Int dataLen, UnsignedByte *mac ) const;
	Bool isGeneralsPacket( TransportMessage *msg );
};

#endif // _TRANSPORT_H_
