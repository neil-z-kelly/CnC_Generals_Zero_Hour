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

/*
Ok, how this should have been done is to make each of the command types
have a bitmask telling which command message header information
each command type required.  That would make finding out the size of
a particular command easier to find, without so much repetitious code.
We would still need to have a separate function for each command type
for the data, but at least that wouldn't be repeating code, that would
be specialized code.
*/



#pragma once

#ifndef __NETPACKET_H
#define __NETPACKET_H

#include "NetworkDefs.h"
#include "GameNetwork/NetCommandList.h"
#include "Common/MessageStream.h"
#include "Common/GameMemory.h"

class NetPacket;

typedef std::list<NetPacket *> NetPacketList;
typedef std::list<NetPacket *>::iterator NetPacketListIter;

// Upper bounds applied when deserializing untrusted packet data.
static const Int MAX_GAME_MESSAGE_ARGUMENTS = 255;								///< GameMessage stores its argument count in an UnsignedByte.
static const Int MAX_CHAT_TEXT_LENGTH = 255;											///< Chat text length is sent as an UnsignedByte.
static const UnsignedInt MAX_WRAPPED_COMMAND_DATA_LENGTH = 16 * 1024 * 1024;	///< Largest reassembled wrapper (chunked) command accepted.

/**
 * Bounds-checked sequential reader over a received packet buffer.
 * A read that would run past the end of the buffer fails and puts the reader in a
 * failed state; every subsequent read also fails, so a sequence of reads can be
 * validated with a single hasFailed() check.
 */
class NetPacketReader
{
public:
	NetPacketReader(UnsignedByte *data, Int length);

	Bool read(void *dest, UnsignedInt size);						///< Copy size bytes into dest and advance.
	UnsignedByte * readInPlace(UnsignedInt size);				///< Return a pointer to the next size bytes and advance, or NULL.
	Bool readString(char *dest, Int destSize);					///< Read a NUL-terminated string that fits (with terminator) in destSize bytes.

	Bool hasBytes(UnsignedInt size) const { return !m_failed && size <= (UnsignedInt)(m_length - m_position); }
	Bool isAtEnd() const { return m_failed || m_position >= m_length; }
	Bool hasFailed() const { return m_failed; }
	void fail() { m_failed = TRUE; }
	Int getPosition() const { return m_position; }

protected:
	UnsignedByte *m_data;
	Int m_length;
	Int m_position;
	Bool m_failed;
};

class NetPacket : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(NetPacket, "NetPacket")		
public:
	NetPacket();
	NetPacket(TransportMessage *msg);
	//virtual ~NetPacket();

	void init();
	void reset();
	void setAddress(Int addr, Int port);
	Bool addCommand(NetCommandRef *msg);
	Int getNumCommands();

	NetCommandList *getCommandList();

	static NetCommandRef * ConstructNetCommandMsgFromRawData(UnsignedByte *data, UnsignedInt dataLength);
	static NetPacketList ConstructBigCommandPacketList(NetCommandRef *ref);

	UnsignedByte *getData();
	Int getLength();
	UnsignedInt getAddr();
	UnsignedShort getPort();

protected:
	static UnsignedInt GetBufferSizeNeededForCommand(NetCommandMsg *msg);
	static void FillBufferWithCommand(UnsignedByte *buffer, NetCommandRef *msg);

	// These functions return the size of the command without any compression, repetition, etc.
	// i.e. All of the required fields are taken into account when returning the size.
	static UnsignedInt GetGameCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetAckCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetFrameCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetPlayerLeaveCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetRunAheadMetricsCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetRunAheadCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDestroyPlayerCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetKeepAliveCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDisconnectKeepAliveCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDisconnectPlayerCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetPacketRouterQueryCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetPacketRouterAckCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDisconnectChatCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDisconnectVoteCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetChatCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetProgressMessageSize(NetCommandMsg *msg);
	static UnsignedInt GetLoadCompleteMessageSize(NetCommandMsg *msg);
	static UnsignedInt GetTimeOutGameStartMessageSize(NetCommandMsg *msg);
	static UnsignedInt GetWrapperCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetFileCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetFileAnnounceCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetFileProgressCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDisconnectFrameCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetDisconnectScreenOffCommandSize(NetCommandMsg *msg);
	static UnsignedInt GetFrameResendRequestCommandSize(NetCommandMsg *msg);

	static void FillBufferWithGameCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithAckCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithFrameCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithPlayerLeaveCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithRunAheadMetricsCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithRunAheadCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDestroyPlayerCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithKeepAliveCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDisconnectKeepAliveCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDisconnectPlayerCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithPacketRouterQueryCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithPacketRouterAckCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDisconnectChatCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDisconnectVoteCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithChatCommand(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithProgressMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithLoadCompleteMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithTimeOutGameStartMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithFileMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithFileProgressMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithFileAnnounceMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDisconnectFrameMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithDisconnectScreenOffMessage(UnsignedByte *buffer, NetCommandRef *msg);
	static void FillBufferWithFrameResendRequestMessage(UnsignedByte *buffer, NetCommandRef *msg);

	Bool addFrameCommand(NetCommandRef *msg);
	Bool isRoomForFrameMessage(NetCommandRef *msg);
	Bool addAckCommand(NetCommandRef *msg, UnsignedShort commandID, UnsignedByte originalPlayerID);
	Bool addAckStage1Command(NetCommandRef *msg);
	Bool addAckStage2Command(NetCommandRef *msg);
	Bool addAckBothCommand(NetCommandRef *msg);
	Bool isRoomForAckMessage(NetCommandRef *msg);
	Bool addGameCommand(NetCommandRef *msg);
	Bool isRoomForGameMessage(NetCommandRef *msg, GameMessage *gmsg);
	Bool addPlayerLeaveCommand(NetCommandRef *msg);
	Bool isRoomForPlayerLeaveMessage(NetCommandRef *msg);
	Bool addRunAheadMetricsCommand(NetCommandRef *msg);
	Bool isRoomForRunAheadMetricsMessage(NetCommandRef *msg);
	Bool addRunAheadCommand(NetCommandRef *msg);
	Bool isRoomForRunAheadMessage(NetCommandRef *msg);
	Bool addDestroyPlayerCommand(NetCommandRef *msg);
	Bool isRoomForDestroyPlayerMessage(NetCommandRef *msg);
	Bool addKeepAliveCommand(NetCommandRef *msg);
	Bool isRoomForKeepAliveMessage(NetCommandRef *msg);
	Bool addDisconnectKeepAliveCommand(NetCommandRef *msg);
	Bool isRoomForDisconnectKeepAliveMessage(NetCommandRef *msg);
	Bool addDisconnectPlayerCommand(NetCommandRef *msg);
	Bool isRoomForDisconnectPlayerMessage(NetCommandRef *msg);
	Bool addPacketRouterQueryCommand(NetCommandRef *msg);
	Bool isRoomForPacketRouterQueryMessage(NetCommandRef *msg);
	Bool addPacketRouterAckCommand(NetCommandRef *msg);
	Bool isRoomForPacketRouterAckMessage(NetCommandRef *msg);
	Bool addDisconnectChatCommand(NetCommandRef *msg);
	Bool isRoomForDisconnectChatMessage(NetCommandRef *msg);
	Bool addChatCommand(NetCommandRef *msg);
	Bool isRoomForChatMessage(NetCommandRef *msg);
	Bool addDisconnectVoteCommand(NetCommandRef *msg);
	Bool isRoomForDisconnectVoteMessage(NetCommandRef *msg);
	Bool addProgressMessage( NetCommandRef *msg );
	Bool isRoomForProgressMessage( NetCommandRef *msg );
	Bool addLoadCompleteMessage( NetCommandRef *msg );
	Bool isRoomForLoadCompleteMessage( NetCommandRef *msg );
	Bool addTimeOutGameStartMessage( NetCommandRef *msg );
	Bool isRoomForTimeOutGameStartMessage( NetCommandRef *msg );
	Bool addWrapperCommand(NetCommandRef *msg);
	Bool isRoomForWrapperMessage(NetCommandRef *msg);
	Bool addFileCommand(NetCommandRef *msg);
	Bool isRoomForFileMessage(NetCommandRef *msg);
	Bool addFileAnnounceCommand(NetCommandRef *msg);
	Bool isRoomForFileAnnounceMessage(NetCommandRef *msg);
	Bool addFileProgressCommand(NetCommandRef *msg);
	Bool isRoomForFileProgressMessage(NetCommandRef *msg);
	Bool addDisconnectFrameCommand(NetCommandRef *msg);
	Bool isRoomForDisconnectFrameMessage(NetCommandRef *msg);
	Bool addDisconnectScreenOffCommand(NetCommandRef *msg);
	Bool isRoomForDisconnectScreenOffMessage(NetCommandRef *msg);
	Bool addFrameResendRequestCommand(NetCommandRef *msg);
	Bool isRoomForFrameResendRequestMessage(NetCommandRef *msg);

	Bool isAckRepeat(NetCommandRef *msg);
	Bool isAckBothRepeat(NetCommandRef *msg);
	Bool isAckStage1Repeat(NetCommandRef *msg);
	Bool isAckStage2Repeat(NetCommandRef *msg);
	Bool isFrameRepeat(NetCommandRef *msg);

	static NetCommandMsg * readCommandData(NetCommandType commandType, NetPacketReader &reader);
	static Bool readChatText(NetPacketReader &reader, UnicodeString &text);
	static NetCommandMsg * readGameMessage(NetPacketReader &reader);
	static NetCommandMsg * readAckBothMessage(NetPacketReader &reader);
	static NetCommandMsg * readAckStage1Message(NetPacketReader &reader);
	static NetCommandMsg * readAckStage2Message(NetPacketReader &reader);
	static NetCommandMsg * readFrameMessage(NetPacketReader &reader);
	static NetCommandMsg * readPlayerLeaveMessage(NetPacketReader &reader);
	static NetCommandMsg * readRunAheadMetricsMessage(NetPacketReader &reader);
	static NetCommandMsg * readRunAheadMessage(NetPacketReader &reader);
	static NetCommandMsg * readDestroyPlayerMessage(NetPacketReader &reader);
	static NetCommandMsg * readKeepAliveMessage(NetPacketReader &reader);
	static NetCommandMsg * readDisconnectKeepAliveMessage(NetPacketReader &reader);
	static NetCommandMsg * readDisconnectPlayerMessage(NetPacketReader &reader);
	static NetCommandMsg * readPacketRouterQueryMessage(NetPacketReader &reader);
	static NetCommandMsg * readPacketRouterAckMessage(NetPacketReader &reader);
	static NetCommandMsg * readDisconnectChatMessage(NetPacketReader &reader);
	static NetCommandMsg * readDisconnectVoteMessage(NetPacketReader &reader);
	static NetCommandMsg * readChatMessage(NetPacketReader &reader);
	static NetCommandMsg * readProgressMessage(NetPacketReader &reader);
	static NetCommandMsg * readLoadCompleteMessage(NetPacketReader &reader);
	static NetCommandMsg * readTimeOutGameStartMessage(NetPacketReader &reader);
	static NetCommandMsg * readWrapperMessage(NetPacketReader &reader);
	static NetCommandMsg * readFileMessage(NetPacketReader &reader);
	static NetCommandMsg * readFileAnnounceMessage(NetPacketReader &reader);
	static NetCommandMsg * readFileProgressMessage(NetPacketReader &reader);
	static NetCommandMsg * readDisconnectFrameMessage(NetPacketReader &reader);
	static NetCommandMsg * readDisconnectScreenOffMessage(NetPacketReader &reader);
	static NetCommandMsg * readFrameResendRequestMessage(NetPacketReader &reader);

	void writeGameMessageArgumentToPacket(GameMessageArgumentDataType type, GameMessageArgumentType arg);
	static Bool readGameMessageArgumentFromPacket(GameMessageArgumentDataType type, NetGameCommandMsg *msg, NetPacketReader &reader);

	void dumpPacketToLog();

protected:
	UnsignedByte		m_packet[MAX_PACKET_SIZE];
	Int							m_packetLen;
	UnsignedInt			m_addr;
	Int							m_numCommands;
	NetCommandRef*	m_lastCommand;
	UnsignedInt			m_lastFrame;
	UnsignedShort		m_port;
	UnsignedShort		m_lastCommandID;
	UnsignedByte		m_lastPlayerID;
	UnsignedByte		m_lastCommandType;
	UnsignedByte		m_lastRelay;
};

#endif // __NETPACKET_H
