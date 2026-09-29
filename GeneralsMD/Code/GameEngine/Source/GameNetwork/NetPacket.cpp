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

////////// NetPacket.cpp ///////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file int the GameEngine

#include "GameNetwork/NetPacket.h"
#include "GameNetwork/NetCommandMsg.h"
#include "GameNetwork/NetworkDefs.h"
#include "GameNetwork/NetworkUtil.h"
#include "GameNetwork/GameMessageParser.h"

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

//-------------------------------------------------------------------------------------------------
// NetPacketReader
//-------------------------------------------------------------------------------------------------
NetPacketReader::NetPacketReader(UnsignedByte *data, Int length) :
	m_data(data),
	m_length(length),
	m_position(0),
	m_failed(FALSE)
{
	if (m_data == NULL || m_length < 0) {
		m_length = 0;
	}
}

UnsignedByte * NetPacketReader::readInPlace(UnsignedInt size) {
	if (!hasBytes(size)) {
		m_failed = TRUE;
		return NULL;
	}
	UnsignedByte *retval = m_data + m_position;
	m_position += (Int)size;
	return retval;
}

Bool NetPacketReader::read(void *dest, UnsignedInt size) {
	UnsignedByte *src = readInPlace(size);
	if (src == NULL) {
		return FALSE;
	}
	memcpy(dest, src, size);
	return TRUE;
}

Bool NetPacketReader::readString(char *dest, Int destSize) {
	if (m_failed || destSize <= 0) {
		m_failed = TRUE;
		return FALSE;
	}
	Int maxLength = m_length - m_position;
	if (maxLength > destSize) {
		maxLength = destSize;
	}
	for (Int len = 0; len < maxLength; ++len) {
		if (m_data[m_position + len] == 0) {
			memcpy(dest, m_data + m_position, len + 1);
			m_position += len + 1;
			return TRUE;
		}
	}
	// unterminated, or too long for the destination buffer.
	m_failed = TRUE;
	return FALSE;
}

// This function assumes that all of the fields are either of default value or are
// present in the raw data.  Returns NULL if the data is malformed.
NetCommandRef * NetPacket::ConstructNetCommandMsgFromRawData(UnsignedByte *data, UnsignedInt dataLength) {
	UnsignedByte commandType = NETCOMMANDTYPE_GAMECOMMAND;
	UnsignedShort commandID = 0;
	UnsignedInt frame = 0;
	UnsignedByte playerID = 0;
	UnsignedByte relay = 0;

	if (dataLength > MAX_WRAPPED_COMMAND_DATA_LENGTH) {
		return NULL;
	}

	NetPacketReader reader(data, (Int)dataLength);

	while (!reader.isAtEnd()) {
		UnsignedByte tag = 0;
		reader.read(&tag, sizeof(tag));
		if (tag == 'T') {
			reader.read(&commandType, sizeof(UnsignedByte));
		} else if (tag == 'R') {
			reader.read(&relay, sizeof(UnsignedByte));
		} else if (tag == 'P') {
			reader.read(&playerID, sizeof(UnsignedByte));
		} else if (tag == 'C') {
			reader.read(&commandID, sizeof(UnsignedShort));
		} else if (tag == 'F') {
			reader.read(&frame, sizeof(UnsignedInt));
		} else if (tag == 'D') {
			NetCommandMsg *msg = readCommandData((NetCommandType)commandType, reader);
			if (msg == NULL) {
				DEBUG_LOG(("NetPacket::ConstructNetCommandMsgFromRawData - malformed command data of type %d\n", commandType));
				return NULL;
			}

			msg->setExecutionFrame(frame);
			msg->setID(commandID);
			msg->setPlayerID(playerID);
			msg->setNetCommandType((NetCommandType)commandType);

			NetCommandRef *ref = NEW_NETCOMMANDREF(msg);

			ref->setRelay(relay);

			msg->detach();
			msg = NULL;

			return ref;
		} else {
			DEBUG_LOG(("NetPacket::ConstructNetCommandMsgFromRawData - unrecognized entry at index %d\n", reader.getPosition() - 1));
			return NULL;
		}
	}

	return NULL;
}

/**
 * Reads the data portion of a command of the given type.  Returns NULL if the command type is
 * unknown or the data is malformed or truncated.
 */
NetCommandMsg * NetPacket::readCommandData(NetCommandType commandType, NetPacketReader &reader) {
	NetCommandMsg *msg = NULL;

	switch (commandType)
	{
	case NETCOMMANDTYPE_GAMECOMMAND:
		msg = readGameMessage(reader);
		break;
	case NETCOMMANDTYPE_ACKBOTH:
		msg = readAckBothMessage(reader);
		break;
	case NETCOMMANDTYPE_ACKSTAGE1:
		msg = readAckStage1Message(reader);
		break;
	case NETCOMMANDTYPE_ACKSTAGE2:
		msg = readAckStage2Message(reader);
		break;
	case NETCOMMANDTYPE_FRAMEINFO:
		msg = readFrameMessage(reader);
		break;
	case NETCOMMANDTYPE_PLAYERLEAVE:
		msg = readPlayerLeaveMessage(reader);
		break;
	case NETCOMMANDTYPE_RUNAHEADMETRICS:
		msg = readRunAheadMetricsMessage(reader);
		break;
	case NETCOMMANDTYPE_RUNAHEAD:
		msg = readRunAheadMessage(reader);
		break;
	case NETCOMMANDTYPE_DESTROYPLAYER:
		msg = readDestroyPlayerMessage(reader);
		break;
	case NETCOMMANDTYPE_KEEPALIVE:
		msg = readKeepAliveMessage(reader);
		break;
	case NETCOMMANDTYPE_DISCONNECTKEEPALIVE:
		msg = readDisconnectKeepAliveMessage(reader);
		break;
	case NETCOMMANDTYPE_DISCONNECTPLAYER:
		msg = readDisconnectPlayerMessage(reader);
		break;
	case NETCOMMANDTYPE_PACKETROUTERQUERY:
		msg = readPacketRouterQueryMessage(reader);
		break;
	case NETCOMMANDTYPE_PACKETROUTERACK:
		msg = readPacketRouterAckMessage(reader);
		break;
	case NETCOMMANDTYPE_DISCONNECTCHAT:
		msg = readDisconnectChatMessage(reader);
		break;
	case NETCOMMANDTYPE_DISCONNECTVOTE:
		msg = readDisconnectVoteMessage(reader);
		break;
	case NETCOMMANDTYPE_CHAT:
		msg = readChatMessage(reader);
		break;
	case NETCOMMANDTYPE_PROGRESS:
		msg = readProgressMessage(reader);
		break;
	case NETCOMMANDTYPE_LOADCOMPLETE:
		msg = readLoadCompleteMessage(reader);
		break;
	case NETCOMMANDTYPE_TIMEOUTSTART:
		msg = readTimeOutGameStartMessage(reader);
		break;
	case NETCOMMANDTYPE_WRAPPER:
		msg = readWrapperMessage(reader);
		break;
	case NETCOMMANDTYPE_FILE:
		msg = readFileMessage(reader);
		break;
	case NETCOMMANDTYPE_FILEANNOUNCE:
		msg = readFileAnnounceMessage(reader);
		break;
	case NETCOMMANDTYPE_FILEPROGRESS:
		msg = readFileProgressMessage(reader);
		break;
	case NETCOMMANDTYPE_DISCONNECTFRAME:
		msg = readDisconnectFrameMessage(reader);
		break;
	case NETCOMMANDTYPE_DISCONNECTSCREENOFF:
		msg = readDisconnectScreenOffMessage(reader);
		break;
	case NETCOMMANDTYPE_FRAMERESENDREQUEST:
		msg = readFrameResendRequestMessage(reader);
		break;
	default:
		break;
	}

	if (msg != NULL && reader.hasFailed()) {
		msg->detach();
		msg = NULL;
	}

	return msg;
}

NetPacketList NetPacket::ConstructBigCommandPacketList(NetCommandRef *ref) {
	// if we don't have a unique command ID, then the wrapped command cannot
	// be identified.  Therefore don't allow commands without a unique ID to
	// be wrapped.
	NetCommandMsg *msg = ref->getCommand();

	if (!DoesCommandRequireACommandID(msg->getNetCommandType())) {
		DEBUG_CRASH(("Trying to wrap a command that doesn't have a unique command ID"));
		return NULL;
	}

	UnsignedInt bufferSize = GetBufferSizeNeededForCommand(msg);  // need to implement.  I have a drinking problem.
	UnsignedByte *bigPacketData = NULL;

	NetPacketList packetList;

	// create the buffer for the huge message and fill the buffer with that message.
	UnsignedInt bigPacketCurrentOffset = 0;
	bigPacketData = NEW UnsignedByte[bufferSize];
	FillBufferWithCommand(bigPacketData, ref);

	// create the wrapper command message we'll be using.
	NetWrapperCommandMsg *wrapperMsg = newInstance(NetWrapperCommandMsg);
	// get the amount of space needed for the wrapper message, not including the wrapped command data.
	UnsignedInt wrapperSize = GetBufferSizeNeededForCommand(wrapperMsg);
	UnsignedInt commandSizePerPacket = MAX_PACKET_SIZE - wrapperSize;

	UnsignedInt numChunks = bufferSize / commandSizePerPacket;
	if ((bufferSize % commandSizePerPacket) > 0) {
		++numChunks;
	}
	UnsignedInt currentChunk = 0;

	// create the packets and the wrapper messages.
	while (currentChunk < numChunks) {
		NetPacket *packet = newInstance(NetPacket);

		UnsignedShort dataSizeThisPacket = commandSizePerPacket;
		if ((bufferSize - bigPacketCurrentOffset) < dataSizeThisPacket) {
			dataSizeThisPacket = bufferSize - bigPacketCurrentOffset;
		}

		if (DoesCommandRequireACommandID(wrapperMsg->getNetCommandType())) {
			wrapperMsg->setID(GenerateNextCommandID());
		}
		wrapperMsg->setPlayerID(msg->getPlayerID());
		wrapperMsg->setExecutionFrame(msg->getExecutionFrame());

		wrapperMsg->setChunkNumber(currentChunk);
		wrapperMsg->setNumChunks(numChunks);
		wrapperMsg->setDataOffset(bigPacketCurrentOffset);
		wrapperMsg->setData(bigPacketData + bigPacketCurrentOffset, dataSizeThisPacket);
		wrapperMsg->setTotalDataLength(bufferSize);
		wrapperMsg->setWrappedCommandID(msg->getID());

		bigPacketCurrentOffset += dataSizeThisPacket;

		NetCommandRef * ref = NEW_NETCOMMANDREF(wrapperMsg);
		ref->setRelay(ref->getRelay());

		if (packet->addCommand(ref) == FALSE) {
			DEBUG_LOG(("NetPacket::BeginBigCommandPacketList - failed to add a wrapper command to the packet\n")); // I still have a drinking problem.
		}

		packetList.push_back(packet);

		ref->deleteInstance();
		ref = NULL;

		++currentChunk;
	}
	wrapperMsg->detach();
	wrapperMsg = NULL;

	delete bigPacketData;
	bigPacketData = NULL;

	return packetList;
}

UnsignedInt NetPacket::GetBufferSizeNeededForCommand(NetCommandMsg *msg) {
	// This is where the fun begins...

	if (msg == NULL) {
		return TRUE; // There was nothing to add, so it was successful.
	}

	switch(msg->getNetCommandType())
	{
		case NETCOMMANDTYPE_GAMECOMMAND:
			return GetGameCommandSize(msg);
		case NETCOMMANDTYPE_ACKSTAGE1:
		case NETCOMMANDTYPE_ACKSTAGE2:
		case NETCOMMANDTYPE_ACKBOTH:
			return GetAckCommandSize(msg);
		case NETCOMMANDTYPE_FRAMEINFO:
			return GetFrameCommandSize(msg);
		case NETCOMMANDTYPE_PLAYERLEAVE:
			return GetPlayerLeaveCommandSize(msg);
		case NETCOMMANDTYPE_RUNAHEADMETRICS:
			return GetRunAheadMetricsCommandSize(msg);
		case NETCOMMANDTYPE_RUNAHEAD:
			return GetRunAheadCommandSize(msg);
		case NETCOMMANDTYPE_DESTROYPLAYER:
			return GetDestroyPlayerCommandSize(msg);
		case NETCOMMANDTYPE_KEEPALIVE:
			return GetKeepAliveCommandSize(msg);
		case NETCOMMANDTYPE_DISCONNECTKEEPALIVE:
			return GetDisconnectKeepAliveCommandSize(msg);
		case NETCOMMANDTYPE_DISCONNECTPLAYER:
			return GetDisconnectPlayerCommandSize(msg);
		case NETCOMMANDTYPE_PACKETROUTERQUERY:
			return GetPacketRouterQueryCommandSize(msg);
		case NETCOMMANDTYPE_PACKETROUTERACK:
			return GetPacketRouterAckCommandSize(msg);
		case NETCOMMANDTYPE_DISCONNECTCHAT:
			return GetDisconnectChatCommandSize(msg);
		case NETCOMMANDTYPE_DISCONNECTVOTE:
			return GetDisconnectVoteCommandSize(msg);
		case NETCOMMANDTYPE_CHAT:
			return GetChatCommandSize(msg);
		case NETCOMMANDTYPE_PROGRESS:
			return GetProgressMessageSize(msg);
		case NETCOMMANDTYPE_LOADCOMPLETE:
			return GetLoadCompleteMessageSize(msg);
		case NETCOMMANDTYPE_TIMEOUTSTART:
			return GetTimeOutGameStartMessageSize(msg);
		case NETCOMMANDTYPE_WRAPPER:
			return GetWrapperCommandSize(msg);
		case NETCOMMANDTYPE_FILE:
			return GetFileCommandSize(msg);
		case NETCOMMANDTYPE_FILEANNOUNCE:
			return GetFileAnnounceCommandSize(msg);
		case NETCOMMANDTYPE_FILEPROGRESS:
			return GetFileProgressCommandSize(msg);
		case NETCOMMANDTYPE_DISCONNECTFRAME:
			return GetDisconnectFrameCommandSize(msg);
		case NETCOMMANDTYPE_DISCONNECTSCREENOFF:
			return GetDisconnectScreenOffCommandSize(msg);
		case NETCOMMANDTYPE_FRAMERESENDREQUEST:
			return GetFrameResendRequestCommandSize(msg);
		default:
			DEBUG_CRASH(("Unknown NETCOMMANDTYPE %d", msg->getNetCommandType()));
			break;
	}

	return 0;
}

UnsignedInt NetPacket::GetGameCommandSize(NetCommandMsg *msg) {
	NetGameCommandMsg *cmdMsg = (NetGameCommandMsg *)msg;

	UnsignedShort msglen = 0;
	msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte); // frame number
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // relay
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // command type
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte); // command ID
	msglen += sizeof(UnsignedByte); // the 'D' for the data section.

	GameMessage *gmsg = cmdMsg->constructGameMessage();
	GameMessageParser *parser = newInstance(GameMessageParser)(gmsg);

	msglen += sizeof(GameMessage::Type);
	msglen += sizeof(UnsignedByte);
//	Int numTypes = parser->getNumTypes();
	GameMessageParserArgumentType *arg = parser->getFirstArgumentType();
	while (arg != NULL) {
		msglen += 2 * sizeof(UnsignedByte); // for the type and number of args of that type declaration.
		GameMessageArgumentDataType type = arg->getType();
		if (type == ARGUMENTDATATYPE_INTEGER) {
			msglen += arg->getArgCount() * sizeof(Int);
		} else if (type == ARGUMENTDATATYPE_REAL) {
			msglen += arg->getArgCount() * sizeof(Real);
		} else if (type == ARGUMENTDATATYPE_BOOLEAN) {
			msglen += arg->getArgCount() * sizeof(Bool);
		} else if (type == ARGUMENTDATATYPE_OBJECTID) {
			msglen += arg->getArgCount() * sizeof(ObjectID);
		} else if (type == ARGUMENTDATATYPE_DRAWABLEID) {
			msglen += arg->getArgCount() * sizeof(DrawableID);
		} else if (type == ARGUMENTDATATYPE_TEAMID) {
			msglen += arg->getArgCount() * sizeof(UnsignedInt);
		} else if (type == ARGUMENTDATATYPE_LOCATION) {
			msglen += arg->getArgCount() * sizeof(Coord3D);
		} else if (type == ARGUMENTDATATYPE_PIXEL) {
			msglen += arg->getArgCount() * sizeof(ICoord2D);
		} else if (type == ARGUMENTDATATYPE_PIXELREGION) {
			msglen += arg->getArgCount() * sizeof(IRegion2D);
		} else if (type == ARGUMENTDATATYPE_TIMESTAMP) {
			msglen += arg->getArgCount() * sizeof(UnsignedInt);
		} else if (type == ARGUMENTDATATYPE_WIDECHAR) {
			msglen += arg->getArgCount() * sizeof(WideChar);
		}
		arg = arg->getNext();
	}

	parser->deleteInstance();
	parser = NULL;

	gmsg->deleteInstance();
	gmsg = NULL;

	return msglen;
}

UnsignedInt NetPacket::GetAckCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;
	++msglen;
	msglen += sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen;
	msglen += sizeof(UnsignedShort);
	msglen += sizeof(UnsignedByte);

	return msglen;
}

UnsignedInt NetPacket::GetFrameCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen;
	msglen += sizeof(UnsignedShort);

	return msglen;
}

UnsignedInt NetPacket::GetPlayerLeaveCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen;
	msglen += sizeof(UnsignedByte);

	return msglen;
}

UnsignedInt NetPacket::GetRunAheadMetricsCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(Real);

	return msglen;
}

UnsignedInt NetPacket::GetRunAheadCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen;
	msglen += sizeof(UnsignedShort);
	msglen += sizeof(UnsignedByte);

	return msglen;
}

UnsignedInt NetPacket::GetDestroyPlayerCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen;
	msglen += sizeof(UnsignedInt);

	return msglen;
}

UnsignedInt NetPacket::GetKeepAliveCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // For the 'D'

	return msglen;
}

UnsignedInt NetPacket::GetDisconnectKeepAliveCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // For the 'D'

	return msglen;
}

UnsignedInt NetPacket::GetDisconnectPlayerCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen; // the 'D'
	msglen += sizeof(UnsignedByte); // slot number
	msglen += sizeof(UnsignedInt);	// disconnect frame

	return msglen;
}

UnsignedInt NetPacket::GetPacketRouterQueryCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // the 'D'

	return msglen;
}

UnsignedInt NetPacket::GetPacketRouterAckCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // the 'D'

	return msglen;
}

UnsignedInt NetPacket::GetDisconnectChatCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;
	NetDisconnectChatCommandMsg *cmdMsg = (NetDisconnectChatCommandMsg *)(msg);

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // the 'D'
	msglen += sizeof(UnsignedByte); // string msglength
	UnsignedByte textmsglen = cmdMsg->getText().getLength();
	msglen += textmsglen * sizeof(UnsignedShort);

	return msglen;
}

UnsignedInt NetPacket::GetDisconnectVoteCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen; // the 'D'
	msglen += sizeof(UnsignedByte); // slot number
	msglen += sizeof(UnsignedInt); // vote frame.

	return msglen;
}

UnsignedInt NetPacket::GetChatCommandSize(NetCommandMsg *msg) {
	Int msglen = 0;
	NetChatCommandMsg *cmdMsg = (NetChatCommandMsg *)(msg);

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);

	++msglen; // the 'D'
	msglen += sizeof(UnsignedByte); // string msglength
	UnsignedByte textmsglen = cmdMsg->getText().getLength();
	msglen += textmsglen * sizeof(UnsignedShort);
	msglen += sizeof(Int); // playerMask

	return msglen;
}

UnsignedInt NetPacket::GetProgressMessageSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // For the 'D'
	++msglen; // percentage

	return msglen;
}

UnsignedInt NetPacket::GetLoadCompleteMessageSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // For the 'D'

	return msglen;
}

UnsignedInt NetPacket::GetTimeOutGameStartMessageSize(NetCommandMsg *msg) {
	Int msglen = 0;

	++msglen;
	msglen += sizeof(UnsignedByte);
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	++msglen;
	msglen += sizeof(UnsignedByte);

	++msglen; // For the 'D'

	return msglen;
}

// type, player, ID, relay, Data
UnsignedInt NetPacket::GetWrapperCommandSize(NetCommandMsg *msg) {
	UnsignedInt msglen = 0;

	++msglen; // 'T'
	msglen += sizeof(UnsignedByte); // command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'R' and relay
	++msglen; // 'D'

	msglen += sizeof(UnsignedShort); // m_wrappedCommandID
	msglen += sizeof(UnsignedInt); // m_chunkNumber
	msglen += sizeof(UnsignedInt); // m_numChunks
	msglen += sizeof(UnsignedInt); // m_totalDataLength
	msglen += sizeof(UnsignedInt); // m_dataLength
	msglen += sizeof(UnsignedInt); // m_dataOffset

	return msglen;
}

UnsignedInt NetPacket::GetFileCommandSize(NetCommandMsg *msg) {
	NetFileCommandMsg *filemsg = (NetFileCommandMsg *)msg;
	UnsignedInt msglen = 0;
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'T' and command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'R' and relay

	++msglen; // 'D'

	msglen += filemsg->getPortableFilename().getLength() + 1; // PORTABLE filename and the terminating 0
	msglen += sizeof(UnsignedInt); // file data length
	msglen += filemsg->getFileLength(); // the file data

	return msglen;
}

UnsignedInt NetPacket::GetFileAnnounceCommandSize(NetCommandMsg *msg) {
	NetFileAnnounceCommandMsg *filemsg = (NetFileAnnounceCommandMsg *)msg;
	UnsignedInt msglen = 0;
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'T' and command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'R' and relay

	++msglen; // 'D'

	msglen += filemsg->getPortableFilename().getLength() + 1; // PORTABLE filename and the terminating 0
	msglen += sizeof(UnsignedShort); // m_fileID
	msglen += sizeof(UnsignedByte); // m_playerMask

	return msglen;
}

UnsignedInt NetPacket::GetFileProgressCommandSize(NetCommandMsg *msg) {
	UnsignedInt msglen = 0;
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'T' and command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte); // 'R' and relay

	++msglen; // 'D'

	msglen += sizeof(UnsignedShort); // m_fileID
	msglen += sizeof(Int); // m_progress

	return msglen;
}

UnsignedInt NetPacket::GetDisconnectFrameCommandSize(NetCommandMsg *msg) {
	UnsignedInt msglen = 0;
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'T' and command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'R' and relay

	++msglen; // 'D'
	msglen += sizeof(UnsignedInt); // disconnect frame

	return msglen;
}

UnsignedInt NetPacket::GetDisconnectScreenOffCommandSize(NetCommandMsg *msg) {
	UnsignedInt msglen = 0;
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'T' and command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'R' and relay

	++msglen; // 'D'
	msglen += sizeof(UnsignedInt); // new frame

	return msglen;
}

UnsignedInt NetPacket::GetFrameResendRequestCommandSize(NetCommandMsg *msg) {
	UnsignedInt msglen = 0;
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'T' and command type
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'P' and player ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedShort); // 'C' and command ID
	msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);	// 'R' and relay

	++msglen; // 'D'
	msglen += sizeof(UnsignedInt); // frame to resend

	return msglen;
}

// this function assumes that buffer is already the correct size.
void NetPacket::FillBufferWithCommand(UnsignedByte *buffer, NetCommandRef *ref) {
	NetCommandMsg *msg = ref->getCommand();

	switch(msg->getNetCommandType())
	{
		case NETCOMMANDTYPE_GAMECOMMAND:
			FillBufferWithGameCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_ACKSTAGE1:
		case NETCOMMANDTYPE_ACKSTAGE2:
		case NETCOMMANDTYPE_ACKBOTH:
			FillBufferWithAckCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_FRAMEINFO:
			FillBufferWithFrameCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_PLAYERLEAVE:
			FillBufferWithPlayerLeaveCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_RUNAHEADMETRICS:
			FillBufferWithRunAheadMetricsCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_RUNAHEAD:
			FillBufferWithRunAheadCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_DESTROYPLAYER:
			FillBufferWithDestroyPlayerCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_KEEPALIVE:
			FillBufferWithKeepAliveCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_DISCONNECTKEEPALIVE:
			FillBufferWithDisconnectKeepAliveCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_DISCONNECTPLAYER:
			FillBufferWithDisconnectPlayerCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_PACKETROUTERQUERY:
			FillBufferWithPacketRouterQueryCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_PACKETROUTERACK:
			FillBufferWithPacketRouterAckCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_DISCONNECTCHAT:
			FillBufferWithDisconnectChatCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_DISCONNECTVOTE:
			FillBufferWithDisconnectVoteCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_CHAT:
			FillBufferWithChatCommand(buffer, ref);
			break;
		case NETCOMMANDTYPE_PROGRESS:
			FillBufferWithProgressMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_LOADCOMPLETE:
			FillBufferWithLoadCompleteMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_TIMEOUTSTART:
			FillBufferWithTimeOutGameStartMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_FILE:
			FillBufferWithFileMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_FILEANNOUNCE:
			FillBufferWithFileAnnounceMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_FILEPROGRESS:
			FillBufferWithFileProgressMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_DISCONNECTFRAME:
			FillBufferWithDisconnectFrameMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_DISCONNECTSCREENOFF:
			FillBufferWithDisconnectScreenOffMessage(buffer, ref);
			break;
		case NETCOMMANDTYPE_FRAMERESENDREQUEST:
			FillBufferWithFrameResendRequestMessage(buffer, ref);
			break;
		default:
			DEBUG_CRASH(("Unknown NETCOMMANDTYPE %d", msg->getNetCommandType()));
			break;
	}
}

void NetPacket::FillBufferWithGameCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetGameCommandMsg *cmdMsg = (NetGameCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
	// get the game message from the NetCommandMsg
	GameMessage *gmsg = cmdMsg->constructGameMessage();

	//DEBUG_LOG(("NetPacket::FillBufferWithGameCommand for command ID %d\n", cmdMsg->getID()));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// If necessary, put the execution frame into the packet.
	buffer[offset] = 'F';
	++offset;
	UnsignedInt newframe = cmdMsg->getExecutionFrame();
	memcpy(buffer+offset, &newframe, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);

	// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	// If necessary, put the playerID into the packet.
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

	buffer[offset] = 'D';
	++offset;

	// Now copy the GameMessage type into the packet.
	GameMessage::Type newType = gmsg->getType();
	memcpy(buffer + offset, &newType, sizeof(GameMessage::Type));
	offset += sizeof(GameMessage::Type);


	GameMessageParser *parser = newInstance(GameMessageParser)(gmsg);
	UnsignedByte numTypes = parser->getNumTypes();
	memcpy(buffer + offset, &numTypes, sizeof(numTypes));
	offset += sizeof(numTypes);

	GameMessageParserArgumentType *argType = parser->getFirstArgumentType();
	while (argType != NULL) {
		UnsignedByte type = (UnsignedByte)(argType->getType());
		memcpy(buffer + offset, &type, sizeof(type));
		offset += sizeof(type);

		UnsignedByte argTypeCount = argType->getArgCount();
		memcpy(buffer + offset, &argTypeCount, sizeof(argTypeCount));
		offset += sizeof(argTypeCount);

		argType = argType->getNext();
	}

	Int numArgs = gmsg->getArgumentCount();
	for (Int i = 0; i < numArgs; ++i) {
		GameMessageArgumentDataType type = gmsg->getArgumentDataType(i);
		GameMessageArgumentType arg = *(gmsg->getArgument(i));
//		writeGameMessageArgumentToPacket(type, arg);

		if (type == ARGUMENTDATATYPE_INTEGER) {
			memcpy(buffer + offset, &(arg.integer), sizeof(arg.integer));
			offset += sizeof(arg.integer);
		} else if (type == ARGUMENTDATATYPE_REAL) {
			memcpy(buffer + offset, &(arg.real), sizeof(arg.real));
			offset += sizeof(arg.real);
		} else if (type == ARGUMENTDATATYPE_BOOLEAN) {
			memcpy(buffer + offset, &(arg.boolean), sizeof(arg.boolean));
			offset += sizeof(arg.boolean);
		} else if (type == ARGUMENTDATATYPE_OBJECTID) {
			memcpy(buffer + offset, &(arg.objectID), sizeof(arg.objectID));
			offset += sizeof(arg.objectID);
		} else if (type == ARGUMENTDATATYPE_DRAWABLEID) {
			memcpy(buffer + offset, &(arg.drawableID), sizeof(arg.drawableID));
			offset += sizeof(arg.drawableID);
		} else if (type == ARGUMENTDATATYPE_TEAMID) {
			memcpy(buffer + offset, &(arg.teamID), sizeof(arg.teamID));
			offset += sizeof(arg.teamID);
		} else if (type == ARGUMENTDATATYPE_LOCATION) {
			memcpy(buffer + offset, &(arg.location), sizeof(arg.location));
			offset += sizeof(arg.location);
		} else if (type == ARGUMENTDATATYPE_PIXEL) {
			memcpy(buffer + offset, &(arg.pixel), sizeof(arg.pixel));
			offset += sizeof(arg.pixel);
		} else if (type == ARGUMENTDATATYPE_PIXELREGION) {
			memcpy(buffer + offset, &(arg.pixelRegion), sizeof(arg.pixelRegion));
			offset += sizeof(arg.pixelRegion);
		} else if (type == ARGUMENTDATATYPE_TIMESTAMP) {
			memcpy(buffer + offset, &(arg.timestamp), sizeof(arg.timestamp));
			offset += sizeof(arg.timestamp);
		} else if (type == ARGUMENTDATATYPE_WIDECHAR) {
			memcpy(buffer + offset, &(arg.wChar), sizeof(arg.wChar));
			offset += sizeof(arg.wChar);
		}
	}

	parser->deleteInstance();
	parser = NULL;

//		DEBUG_LOG(("NetPacket::addGameMessage - added game message, frame %d, player %d, command ID %d\n", m_lastFrame, m_lastPlayerID, m_lastCommandID));

	if (gmsg)
		gmsg->deleteInstance();
	gmsg = NULL;
}

void NetPacket::FillBufferWithAckCommand(UnsignedByte *buffer, NetCommandRef *msg) {
//		DEBUG_LOG(("NetPacket::FillBufferWithAckCommand - adding ack for command %d for player %d\n", cmdMsg->getCommandID(), msg->getCommand()->getPlayerID()));

	NetCommandMsg *cmdMsg = msg->getCommand();
	UnsignedShort offset = 0;

	UnsignedShort commandID = 0;
	UnsignedByte originalPlayerID = 0;

	if (cmdMsg->getNetCommandType() == NETCOMMANDTYPE_ACKBOTH) {
		NetAckBothCommandMsg *ackmsg = (NetAckBothCommandMsg *)msg;
		commandID = ackmsg->getCommandID();
		originalPlayerID = ackmsg->getOriginalPlayerID();
	} else if (cmdMsg->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE1) {
		NetAckStage1CommandMsg *ackmsg = (NetAckStage1CommandMsg *)msg;
		commandID = ackmsg->getCommandID();
		originalPlayerID = ackmsg->getOriginalPlayerID();
	} else if (cmdMsg->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE2) {
		NetAckStage2CommandMsg *ackmsg = (NetAckStage2CommandMsg *)msg;
		commandID = ackmsg->getCommandID();
		originalPlayerID = ackmsg->getOriginalPlayerID();
	}

	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// Put in the command id of the command we are acking.
	buffer[offset] = 'D';
	++offset;
	memcpy(buffer + offset, &commandID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);
	memcpy(buffer + offset, &originalPlayerID, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	//		DEBUG_LOG(("outgoing - added ACK, original player %d, command id %d\n", origPlayerID, cmdID));
}

void NetPacket::FillBufferWithFrameCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetFrameCommandMsg *cmdMsg = (NetFrameCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
	//		DEBUG_LOG(("NetPacket::addFrameCommand - adding frame command for frame %d, command count = %d, command id = %d\n", cmdMsg->getExecutionFrame(), cmdMsg->getCommandCount(), cmdMsg->getID()));

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the execution frame into the packet.
	buffer[offset] = 'F';
	++offset;
	UnsignedInt newframe = cmdMsg->getExecutionFrame();
	memcpy(buffer+offset, &newframe, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnsignedShort cmdCount = cmdMsg->getCommandCount();
	memcpy(buffer + offset, &cmdCount, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

	// frameinfodebug
//		DEBUG_LOG(("outgoing - added frame %d, player %d, command count = %d, command id = %d\n", cmdMsg->getExecutionFrame(), cmdMsg->getPlayerID(), cmdMsg->getCommandCount(), cmdMsg->getID()));
}

void NetPacket::FillBufferWithPlayerLeaveCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetPlayerLeaveCommandMsg *cmdMsg = (NetPlayerLeaveCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addPlayerLeaveCommand - adding player leave command for player %d\n", cmdMsg->getLeavingPlayerID()));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

// If necessary, put the execution frame into the packet.
	buffer[offset] = 'F';
	++offset;
	UnsignedInt newframe = cmdMsg->getExecutionFrame();
	memcpy(buffer+offset, &newframe, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnsignedByte leavingPlayerID = cmdMsg->getLeavingPlayerID();
	memcpy(buffer + offset, &leavingPlayerID, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);
}

void NetPacket::FillBufferWithRunAheadMetricsCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetRunAheadMetricsCommandMsg *cmdMsg = (NetRunAheadMetricsCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addRunAheadMetricsCommand - adding run ahead metrics for player %d, fps = %d, latency = %f\n", cmdMsg->getPlayerID(), cmdMsg->getAverageFps(), cmdMsg->getAverageLatency()));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	// write the average latency
	Real averageLatency = cmdMsg->getAverageLatency();
	memcpy(buffer + offset, &averageLatency, sizeof(averageLatency));
	offset += sizeof(averageLatency);
	// write the average fps
	UnsignedShort averageFps = (UnsignedShort)(cmdMsg->getAverageFps());
	memcpy(buffer + offset, &averageFps, sizeof(averageFps));
	offset += sizeof(averageFps);
}

void NetPacket::FillBufferWithRunAheadCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetRunAheadCommandMsg *cmdMsg = (NetRunAheadCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
	//DEBUG_LOG(("NetPacket::FillBufferWithRunAheadCommand - adding run ahead command\n"));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	// If necessary, put the execution frame into the packet.
	buffer[offset] = 'F';
	++offset;
	UnsignedInt newframe = cmdMsg->getExecutionFrame();
	memcpy(buffer+offset, &newframe, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

	// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnsignedShort newRunAhead = cmdMsg->getRunAhead();
	memcpy(buffer + offset, &newRunAhead, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

	UnsignedByte newFrameRate = cmdMsg->getFrameRate();
	memcpy(buffer + offset, &newFrameRate, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("NetPacket - added run ahead command, frame %d, player id %d command id %d\n", m_lastFrame, m_lastPlayerID, m_lastCommandID));
}

void NetPacket::FillBufferWithDestroyPlayerCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDestroyPlayerCommandMsg *cmdMsg = (NetDestroyPlayerCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addRunAheadCommand - adding run ahead command\n"));

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

// If necessary, put the execution frame into the packet.
	buffer[offset] = 'F';
	++offset;
	UnsignedInt newframe = cmdMsg->getExecutionFrame();
	memcpy(buffer+offset, &newframe, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnsignedInt newVal = cmdMsg->getPlayerIndex();
	memcpy(buffer + offset, &newVal, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);
}

void NetPacket::FillBufferWithKeepAliveCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetKeepAliveCommandMsg *cmdMsg = (NetKeepAliveCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	buffer[offset] = 'D';
	++offset;
}

void NetPacket::FillBufferWithDisconnectKeepAliveCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDisconnectKeepAliveCommandMsg *cmdMsg = (NetDisconnectKeepAliveCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;

	// Put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// Put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);
	
	// Put the player ID into the packet.
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	buffer[offset] = 'D';
	++offset;
}

void NetPacket::FillBufferWithDisconnectPlayerCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDisconnectPlayerCommandMsg *cmdMsg = (NetDisconnectPlayerCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addDisconnectPlayerCommand - adding run ahead command\n"));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

	//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnsignedByte slot = cmdMsg->getDisconnectSlot();
	memcpy(buffer + offset, &slot, sizeof(slot));
	offset += sizeof(slot);

	UnsignedInt disconnectFrame = cmdMsg->getDisconnectFrame();
	memcpy(buffer + offset, &disconnectFrame, sizeof(disconnectFrame));
	offset += sizeof(disconnectFrame);
}

void NetPacket::FillBufferWithPacketRouterQueryCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetPacketRouterQueryCommandMsg *cmdMsg = (NetPacketRouterQueryCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addPacketRouterQueryCommand - adding packet router query command\n"));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

	buffer[offset] = 'D';
	++offset;
}

void NetPacket::FillBufferWithPacketRouterAckCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetPacketRouterAckCommandMsg *cmdMsg = (NetPacketRouterAckCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addPacketRouterAckCommand - adding packet router query command\n"));

	// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

	buffer[offset] = 'D';
	++offset;
}

void NetPacket::FillBufferWithDisconnectChatCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDisconnectChatCommandMsg *cmdMsg = (NetDisconnectChatCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addDisconnectChatCommand - adding run ahead command\n"));

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

	buffer[offset] = 'D';
	++offset;
	UnicodeString unitext = cmdMsg->getText();
	UnsignedByte length = unitext.getLength();
	memcpy(buffer + offset, &length, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	memcpy(buffer + offset, unitext.str(), length * sizeof(UnsignedShort));
	offset += length * sizeof(UnsignedShort);
}

void NetPacket::FillBufferWithDisconnectVoteCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDisconnectVoteCommandMsg *cmdMsg = (NetDisconnectVoteCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addDisconnectVoteCommand - adding run ahead command\n"));

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnsignedByte slot = cmdMsg->getSlot();
	memcpy(buffer + offset, &slot, sizeof(slot));
	offset += sizeof(slot);

	UnsignedInt voteFrame = cmdMsg->getVoteFrame();
	memcpy(buffer + offset, &voteFrame, sizeof(voteFrame));
	offset += sizeof(voteFrame);
}

void NetPacket::FillBufferWithChatCommand(UnsignedByte *buffer, NetCommandRef *msg) {
	NetChatCommandMsg *cmdMsg = (NetChatCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;
//		DEBUG_LOG(("NetPacket::addDisconnectChatCommand - adding run ahead command\n"));

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the execution frame into the packet.
	buffer[offset] = 'F';
	++offset;
	UnsignedInt newframe = cmdMsg->getExecutionFrame();
	memcpy(buffer+offset, &newframe, sizeof(UnsignedInt));
	offset += sizeof(UnsignedInt);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

	buffer[offset] = 'D';
	++offset;
	UnicodeString unitext = cmdMsg->getText();
	UnsignedByte length = unitext.getLength();
	Int playerMask = cmdMsg->getPlayerMask();
	memcpy(buffer + offset, &length, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	memcpy(buffer + offset, unitext.str(), length * sizeof(UnsignedShort));
	offset += length * sizeof(UnsignedShort);

	memcpy(buffer + offset, &playerMask, sizeof(Int));
	offset += sizeof(Int);
}

void NetPacket::FillBufferWithProgressMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetProgressCommandMsg *cmdMsg = (NetProgressCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

// Put the player ID into the packet.
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	buffer[offset] = 'D';
	++offset;

	buffer[offset] = cmdMsg->getPercentage();
	++offset;
}

void NetPacket::FillBufferWithLoadCompleteMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetCommandMsg *cmdMsg = (NetCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);

	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

	buffer[offset] = 'D';
	++offset;
}

void NetPacket::FillBufferWithTimeOutGameStartMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetCommandMsg *cmdMsg = (NetCommandMsg *)(msg->getCommand());
	UnsignedShort offset = 0;

// If necessary, put the NetCommandType into the packet.
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

// If necessary, put the relay into the packet.
	buffer[offset] = 'R';
	++offset;
	UnsignedByte newRelay = msg->getRelay();
	memcpy(buffer+offset, &newRelay, sizeof(UnsignedByte));
	offset += sizeof(UnsignedByte);
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

// If necessary, specify the command ID of this command.
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(UnsignedShort));
	offset += sizeof(UnsignedShort);

	buffer[offset] = 'D';
	++offset;
}

void NetPacket::FillBufferWithFileMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetFileCommandMsg *cmdMsg = (NetFileCommandMsg *)(msg->getCommand());
	UnsignedInt offset = 0;

	// command type
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// relay
	buffer[offset] = 'R';
	++offset;
	buffer[offset] = msg->getRelay();
	offset += sizeof(UnsignedByte);

	// player ID
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// command ID
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(newID));
	offset += sizeof(newID);

	// data
	buffer[offset] = 'D';
	++offset;

	AsciiString filename = cmdMsg->getPortableFilename();	// PORTABLE
	for (Int i = 0; i < filename.getLength(); ++i) {
		buffer[offset] = filename.getCharAt(i);
		++offset;
	}
	buffer[offset] = 0;
	++offset;

	UnsignedInt newInt = cmdMsg->getFileLength();
	memcpy(buffer + offset, &newInt, sizeof(newInt));
	offset += sizeof(newInt);

	memcpy(buffer + offset, cmdMsg->getFileData(), cmdMsg->getFileLength());
	offset += cmdMsg->getFileLength();
}

void NetPacket::FillBufferWithFileAnnounceMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetFileAnnounceCommandMsg *cmdMsg = (NetFileAnnounceCommandMsg *)(msg->getCommand());
	UnsignedInt offset = 0;

	// command type
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// relay
	buffer[offset] = 'R';
	++offset;
	buffer[offset] = msg->getRelay();
	offset += sizeof(UnsignedByte);

	// player ID
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// command ID
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(newID));
	offset += sizeof(newID);

	// data
	buffer[offset] = 'D';
	++offset;

	AsciiString filename = cmdMsg->getPortableFilename();	// PORTABLE
	for (Int i = 0; i < filename.getLength(); ++i) {
		buffer[offset] = filename.getCharAt(i);
		++offset;
	}
	buffer[offset] = 0;
	++offset;

	UnsignedShort fileID = cmdMsg->getFileID();
	memcpy(buffer + offset, &fileID, sizeof(fileID));
	offset += sizeof(fileID);

	UnsignedByte playerMask = cmdMsg->getPlayerMask();
	memcpy(buffer + offset, &playerMask, sizeof(playerMask));
	offset += sizeof(playerMask);
}

void NetPacket::FillBufferWithFileProgressMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetFileProgressCommandMsg *cmdMsg = (NetFileProgressCommandMsg *)(msg->getCommand());
	UnsignedInt offset = 0;

	// command type
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// relay
	buffer[offset] = 'R';
	++offset;
	buffer[offset] = msg->getRelay();
	offset += sizeof(UnsignedByte);

	// player ID
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// command ID
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(newID));
	offset += sizeof(newID);

	// data
	buffer[offset] = 'D';
	++offset;

	UnsignedShort fileID = cmdMsg->getFileID();
	memcpy(buffer + offset, &fileID, sizeof(fileID));
	offset += sizeof(fileID);

	Int progress = cmdMsg->getProgress();
	memcpy(buffer + offset, &progress, sizeof(progress));
	offset += sizeof(progress);
}

void NetPacket::FillBufferWithDisconnectFrameMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDisconnectFrameCommandMsg *cmdMsg = (NetDisconnectFrameCommandMsg *)(msg->getCommand());
	UnsignedInt offset = 0;

	// command type
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// relay
	buffer[offset] = 'R';
	++offset;
	buffer[offset] = msg->getRelay();
	offset += sizeof(UnsignedByte);

	// player ID
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// command ID
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(newID));
	offset += sizeof(newID);

	// data
	buffer[offset] = 'D';
	++offset;

	UnsignedInt disconnectFrame = cmdMsg->getDisconnectFrame();
	memcpy(buffer + offset, &disconnectFrame, sizeof(disconnectFrame));
	offset += sizeof(disconnectFrame);
}

void NetPacket::FillBufferWithDisconnectScreenOffMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetDisconnectScreenOffCommandMsg *cmdMsg = (NetDisconnectScreenOffCommandMsg *)(msg->getCommand());
	UnsignedInt offset = 0;

	// command type
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// relay
	buffer[offset] = 'R';
	++offset;
	buffer[offset] = msg->getRelay();
	offset += sizeof(UnsignedByte);

	// player ID
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// command ID
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(newID));
	offset += sizeof(newID);

	// data
	buffer[offset] = 'D';
	++offset;

	UnsignedInt newFrame = cmdMsg->getNewFrame();
	memcpy(buffer + offset, &newFrame, sizeof(newFrame));
	offset += sizeof(newFrame);
}

void NetPacket::FillBufferWithFrameResendRequestMessage(UnsignedByte *buffer, NetCommandRef *msg) {
	NetFrameResendRequestCommandMsg *cmdMsg = (NetFrameResendRequestCommandMsg *)(msg->getCommand());
	UnsignedInt offset = 0;

	// command type
	buffer[offset] = 'T';
	++offset;
	buffer[offset] = cmdMsg->getNetCommandType();
	offset += sizeof(UnsignedByte);

	// relay
	buffer[offset] = 'R';
	++offset;
	buffer[offset] = msg->getRelay();
	offset += sizeof(UnsignedByte);

	// player ID
	buffer[offset] = 'P';
	++offset;
	buffer[offset] = cmdMsg->getPlayerID();
	offset += sizeof(UnsignedByte);

	// command ID
	buffer[offset] = 'C';
	++offset;
	UnsignedShort newID = cmdMsg->getID();
	memcpy(buffer + offset, &newID, sizeof(newID));
	offset += sizeof(newID);

	// data
	buffer[offset] = 'D';
	++offset;

	UnsignedInt frameToResend = cmdMsg->getFrameToResend();
	memcpy(buffer + offset, &frameToResend, sizeof(frameToResend));
	offset += sizeof(frameToResend);
}


/**
 * Constructor
 */
NetPacket::NetPacket() {
	init();
}

/**
 * Constructor given raw transport data.
 */
NetPacket::NetPacket(TransportMessage *msg) {
	init();
	m_packetLen = msg->length;
	if (m_packetLen < 0 || m_packetLen > MAX_PACKET_SIZE) {
		// too big to be a valid packet; treat it as empty so it gets dropped.
		m_packetLen = 0;
	}
	memcpy(m_packet, msg->data, MAX_PACKET_SIZE);
	m_numCommands = -1;
	m_addr = msg->addr;
	m_port = msg->port;
}

/**
 * Destructor
 */
NetPacket::~NetPacket() {
	if (m_lastCommand != NULL) {
		m_lastCommand->deleteInstance();
		m_lastCommand = NULL;
	}
}

/**
 * Initialize all the member variable values.
 */
void NetPacket::init() {
	m_addr = 0;
	m_port = 0;
	m_numCommands = 0;
	m_packetLen = 0;
	m_packet[0] = 0;

	m_lastPlayerID = 0;
	m_lastFrame = 0;
	m_lastCommandID = 0;
	m_lastCommandType = 0;
	m_lastRelay = 0;

	m_lastCommand = NULL;
}

void NetPacket::reset() {
	if (m_lastCommand != NULL) {
		m_lastCommand->deleteInstance();
		m_lastCommand = NULL;
	}
	init();
}

/**
 * Set the address to which this packet is to be sent.
 */
void NetPacket::setAddress(Int addr, Int port) {
	m_addr = addr;
	m_port = port;
}

/**
 * Adds this command to the packet.  Returns false if there wasn't enough room
 * in the packet for this message, true otherwise.
 */
Bool NetPacket::addCommand(NetCommandRef *msg) {
	// This is where the fun begins...

	NetCommandMsg *cmdMsg = msg->getCommand();

	if (msg == NULL) {
		return TRUE; // There was nothing to add, so it was successful.
	}

	switch(cmdMsg->getNetCommandType())
	{
		case NETCOMMANDTYPE_GAMECOMMAND:
			return addGameCommand(msg);
		case NETCOMMANDTYPE_ACKSTAGE1:
			return addAckStage1Command(msg);
		case NETCOMMANDTYPE_ACKSTAGE2:
			return addAckStage2Command(msg);
		case NETCOMMANDTYPE_ACKBOTH:
			return addAckBothCommand(msg);
		case NETCOMMANDTYPE_FRAMEINFO:
			return addFrameCommand(msg);
		case NETCOMMANDTYPE_PLAYERLEAVE:
			return addPlayerLeaveCommand(msg);
		case NETCOMMANDTYPE_RUNAHEADMETRICS:
			return addRunAheadMetricsCommand(msg);
		case NETCOMMANDTYPE_RUNAHEAD:
			return addRunAheadCommand(msg);
		case NETCOMMANDTYPE_DESTROYPLAYER:
			return addDestroyPlayerCommand(msg);
		case NETCOMMANDTYPE_KEEPALIVE:
			return addKeepAliveCommand(msg);
		case NETCOMMANDTYPE_DISCONNECTKEEPALIVE:
			return addDisconnectKeepAliveCommand(msg);
		case NETCOMMANDTYPE_DISCONNECTPLAYER:
			return addDisconnectPlayerCommand(msg);
		case NETCOMMANDTYPE_PACKETROUTERQUERY:
			return addPacketRouterQueryCommand(msg);
		case NETCOMMANDTYPE_PACKETROUTERACK:
			return addPacketRouterAckCommand(msg);
		case NETCOMMANDTYPE_DISCONNECTCHAT:
			return addDisconnectChatCommand(msg);
		case NETCOMMANDTYPE_DISCONNECTVOTE:
			return addDisconnectVoteCommand(msg);
		case NETCOMMANDTYPE_CHAT:
			return addChatCommand(msg);
		case NETCOMMANDTYPE_PROGRESS:
			return addProgressMessage(msg);
		case NETCOMMANDTYPE_LOADCOMPLETE:
			return addLoadCompleteMessage(msg);
		case NETCOMMANDTYPE_TIMEOUTSTART:
			return addTimeOutGameStartMessage(msg);
		case NETCOMMANDTYPE_WRAPPER:
			return addWrapperCommand(msg);
		case NETCOMMANDTYPE_FILE:
			return addFileCommand(msg);
		case NETCOMMANDTYPE_FILEANNOUNCE:
			return addFileAnnounceCommand(msg);
		case NETCOMMANDTYPE_FILEPROGRESS:
			return addFileProgressCommand(msg);
		case NETCOMMANDTYPE_DISCONNECTFRAME:
			return addDisconnectFrameCommand(msg);
		case NETCOMMANDTYPE_DISCONNECTSCREENOFF:
			return addDisconnectScreenOffCommand(msg);
		case NETCOMMANDTYPE_FRAMERESENDREQUEST:
			return addFrameResendRequestCommand(msg);
		default:
			DEBUG_CRASH(("Unknown NETCOMMANDTYPE %d", cmdMsg->getNetCommandType()));
			break;
	}

	return TRUE;
}

/*
T = Net command type
F = Execution frame
P = Player ID
C = Command ID
R = Relay
D = Command Data
Z = Repeat last command
*/
Bool NetPacket::addFrameResendRequestCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForFrameResendRequestMessage(msg)) {
		NetFrameResendRequestCommandMsg *cmdMsg = (NetFrameResendRequestCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		UnsignedInt frameToResend = cmdMsg->getFrameToResend();
		memcpy(m_packet + m_packetLen, &frameToResend, sizeof(frameToResend));
		m_packetLen += sizeof(frameToResend);

		DEBUG_LOG(("NetPacket::addFrameResendRequest - added frame resend request command from player %d for frame %d, command id = %d\n", m_lastPlayerID, frameToResend, m_lastCommandID));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForFrameResendRequestMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetFrameResendRequestCommandMsg *cmdMsg = (NetFrameResendRequestCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedInt); // for the frame to be resent
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addDisconnectScreenOffCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForDisconnectScreenOffMessage(msg)) {
		NetDisconnectScreenOffCommandMsg *cmdMsg = (NetDisconnectScreenOffCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		UnsignedInt newFrame = cmdMsg->getNewFrame();
		memcpy(m_packet + m_packetLen, &newFrame, sizeof(newFrame));
		m_packetLen += sizeof(newFrame);

		DEBUG_LOG(("NetPacket::addDisconnectScreenOff - added disconnect screen off command from player %d for frame %d, command id = %d\n", m_lastPlayerID, newFrame, m_lastCommandID));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForDisconnectScreenOffMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetDisconnectScreenOffCommandMsg *cmdMsg = (NetDisconnectScreenOffCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedInt); // for the disconnect frame
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addDisconnectFrameCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForDisconnectFrameMessage(msg)) {
		NetDisconnectFrameCommandMsg *cmdMsg = (NetDisconnectFrameCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		UnsignedInt disconnectFrame = cmdMsg->getDisconnectFrame();
		memcpy(m_packet + m_packetLen, &disconnectFrame, sizeof(disconnectFrame));
		m_packetLen += sizeof(disconnectFrame);

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		DEBUG_LOG(("NetPacket::addDisconnectFrame - added disconnect frame command from player %d for frame %d, command id = %d\n", m_lastPlayerID, disconnectFrame, m_lastCommandID));

		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForDisconnectFrameMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetDisconnectFrameCommandMsg *cmdMsg = (NetDisconnectFrameCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedInt); // for the disconnect frame
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addFileCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForFileMessage(msg)) {
		NetFileCommandMsg *cmdMsg = (NetFileCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		
		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		AsciiString filename = cmdMsg->getPortableFilename();		// PORTABLE
		strcpy((char *)(m_packet + m_packetLen), filename.str());
		m_packetLen += filename.getLength() + 1;

		UnsignedInt fileLength = cmdMsg->getFileLength();
		memcpy(m_packet + m_packetLen, &fileLength, sizeof(fileLength));
		m_packetLen += sizeof(fileLength);

		memcpy(m_packet + m_packetLen, cmdMsg->getFileData(), fileLength);
		m_packetLen += fileLength;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForFileMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetFileCommandMsg *cmdMsg = (NetFileCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedShort);
	}

	++len; // 'D'
	len += cmdMsg->getPortableFilename().getLength() + 1; // PORTABLE filename + the terminating 0
	len += sizeof(UnsignedInt); // filedata length
	len += cmdMsg->getFileLength();

	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}

	return TRUE;
}

Bool NetPacket::addFileAnnounceCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForFileAnnounceMessage(msg)) {
		NetFileAnnounceCommandMsg *cmdMsg = (NetFileAnnounceCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		
		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		AsciiString filename = cmdMsg->getPortableFilename();	// PORTABLE
		strcpy((char *)(m_packet + m_packetLen), filename.str());
		m_packetLen += filename.getLength() + 1;

		UnsignedShort fileID = cmdMsg->getFileID();
		memcpy(m_packet + m_packetLen, &fileID, sizeof(fileID));
		m_packetLen += sizeof(fileID);

		UnsignedByte playerMask = cmdMsg->getPlayerMask();
		memcpy(m_packet + m_packetLen, &playerMask, sizeof(playerMask));
		m_packetLen += sizeof(playerMask);

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		DEBUG_LOG(("Adding file announce message for fileID %d, ID %d to packet\n",
			cmdMsg->getFileID(), cmdMsg->getID()));
		return TRUE;
	}
	DEBUG_LOG(("No room to add file announce message to packet\n"));
	return FALSE;
}

Bool NetPacket::isRoomForFileAnnounceMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetFileAnnounceCommandMsg *cmdMsg = (NetFileAnnounceCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedShort);
	}

	++len; // 'D'
	len += cmdMsg->getPortableFilename().getLength() + 1; // PORTABLE filename + the terminating 0
	len += sizeof(UnsignedShort); // m_fileID
	len += sizeof(UnsignedByte); // m_playerMask

	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}

	return TRUE;
}

Bool NetPacket::addFileProgressCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForFileProgressMessage(msg)) {
		NetFileProgressCommandMsg *cmdMsg = (NetFileProgressCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		
		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		UnsignedShort fileID = cmdMsg->getFileID();
		memcpy(m_packet + m_packetLen, &fileID, sizeof(fileID));
		m_packetLen += sizeof(fileID);

		Int progress = cmdMsg->getProgress();
		memcpy(m_packet + m_packetLen, &progress, sizeof(progress));
		m_packetLen += sizeof(progress);

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForFileProgressMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetFileProgressCommandMsg *cmdMsg = (NetFileProgressCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedShort);
	}

	++len; // 'D'
	len += sizeof(UnsignedShort); // m_fileID
	len += sizeof(Int); // m_progress

	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}

	return TRUE;
}

Bool NetPacket::addWrapperCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForWrapperMessage(msg)) {
		NetWrapperCommandMsg *cmdMsg = (NetWrapperCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet + m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary put the player ID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		
		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		// wrapped command ID
		UnsignedShort wrappedCommandID = cmdMsg->getWrappedCommandID();
		memcpy(m_packet + m_packetLen, &wrappedCommandID, sizeof(wrappedCommandID));
		m_packetLen += sizeof(wrappedCommandID);

		// chunk number
//		m_packet[m_packetLen] = cmdMsg->getChunkNumber();
//		++m_packetLen;
		UnsignedInt chunkNumber = cmdMsg->getChunkNumber();
		memcpy(m_packet + m_packetLen, &chunkNumber, sizeof(chunkNumber));
		m_packetLen += sizeof(chunkNumber);

		// number of chunks
//		m_packet[m_packetLen] = cmdMsg->getNumChunks();
//		++m_packetLen;
		UnsignedInt numChunks = cmdMsg->getNumChunks();
		memcpy(m_packet + m_packetLen, &numChunks, sizeof(numChunks));
		m_packetLen += sizeof(numChunks);

		// total length of data for all chunks
		UnsignedInt totalDataLength = cmdMsg->getTotalDataLength();
		memcpy(m_packet + m_packetLen, &totalDataLength, sizeof(totalDataLength));
		m_packetLen += sizeof(totalDataLength);

		// data length for this chunk
		UnsignedInt dataLength = cmdMsg->getDataLength();
		memcpy(m_packet + m_packetLen, &dataLength, sizeof(dataLength));
		m_packetLen += sizeof(dataLength);

		// the offset into the data of this chunk
		UnsignedInt dataOffset = cmdMsg->getDataOffset();
		memcpy(m_packet + m_packetLen, &dataOffset, sizeof(dataOffset));
		m_packetLen += sizeof(dataOffset);

		// the data for this chunk
		UnsignedByte *data = cmdMsg->getData();
		memcpy(m_packet + m_packetLen, data, dataLength);
		m_packetLen += dataLength;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForWrapperMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetWrapperCommandMsg *cmdMsg = (NetWrapperCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedShort);
	}

	++len; // 'D'
	len += sizeof(UnsignedShort); // wrapped command ID
	len += sizeof(UnsignedInt); // chunk number
	len += sizeof(UnsignedInt); // number of chunks
	len += sizeof(UnsignedInt); // total data length
	len += sizeof(UnsignedInt); // data length of this chunk
	len += sizeof(UnsignedInt); // offset of this chunk
	len += cmdMsg->getDataLength(); // for the data of this chunk

	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}

	return TRUE;
}

/**
 * Add a TimeOutGameStart  to the packet. Returns true if successful.
 */
Bool NetPacket::addTimeOutGameStartMessage(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForLoadCompleteMessage(msg)) {
		NetCommandMsg *cmdMsg = (NetCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

//		DEBUG_LOG(("Added keep alive command to packet.\n"));

		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room in the packet for this command.
 */
Bool NetPacket::isRoomForTimeOutGameStartMessage(NetCommandRef *msg) {
	Int len = 0;
	NetCommandMsg *cmdMsg = (NetCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // For the 'D'
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}



/**
 * Add a Progress command to the packet. Returns true if successful.
 */
Bool NetPacket::addLoadCompleteMessage(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForLoadCompleteMessage(msg)) {
		NetCommandMsg *cmdMsg = (NetCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

//		DEBUG_LOG(("Added keep alive command to packet.\n"));

		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room in the packet for this command.
 */
Bool NetPacket::isRoomForLoadCompleteMessage(NetCommandRef *msg) {
	Int len = 0;
	NetCommandMsg *cmdMsg = (NetCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // For the 'D'
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}




/**
 * Add a Progress command to the packet. Returns true if successful.
 */
Bool NetPacket::addProgressMessage(NetCommandRef *msg) {
	if (isRoomForProgressMessage(msg)) {
		NetProgressCommandMsg *cmdMsg = (NetProgressCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		m_packet[m_packetLen] = cmdMsg->getPercentage();
		++m_packetLen;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

//		DEBUG_LOG(("Added keep alive command to packet.\n"));

		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room in the packet for this command.
 */
Bool NetPacket::isRoomForProgressMessage(NetCommandRef *msg) {
	Int len = 0;
	NetProgressCommandMsg *cmdMsg = (NetProgressCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // For the 'D'
	++len; // percentage
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}



Bool NetPacket::addDisconnectVoteCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;

//	DEBUG_LOG(("NetPacket::addDisconnectVoteCommand - entering...\n"));
	//  need type, player id, relay, command id, slot number
	if (isRoomForDisconnectVoteMessage(msg)) {
		NetDisconnectVoteCommandMsg *cmdMsg = (NetDisconnectVoteCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addDisconnectVoteCommand - adding run ahead command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnsignedByte slot = cmdMsg->getSlot();
		memcpy(m_packet + m_packetLen, &slot, sizeof(slot));
		m_packetLen += sizeof(slot);

		UnsignedInt voteFrame = cmdMsg->getVoteFrame();
		memcpy(m_packet + m_packetLen, &voteFrame, sizeof(voteFrame));
		m_packetLen += sizeof(voteFrame);

//		DEBUG_LOG(("NetPacket::addDisconnectVoteCommand - added disconnect vote command, player id %d command id %d, voted slot %d\n", m_lastPlayerID, m_lastCommandID, slot));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room for this player disconnect command in this packet.
 */
Bool NetPacket::isRoomForDisconnectVoteMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetDisconnectVoteCommandMsg *cmdMsg = (NetDisconnectVoteCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // the 'D'
	len += sizeof(UnsignedByte); // slot number
	len += sizeof(UnsignedInt); // vote frame

	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addDisconnectChatCommand(NetCommandRef *msg) {
	// type, player, id, relay, data
	// data format: 1 byte string length, string (two bytes per character)
//	DEBUG_LOG(("NetPacket::addDisconnectChatCommand - Entering...\n"));
	if (isRoomForDisconnectChatMessage(msg)) {
		NetDisconnectChatCommandMsg *cmdMsg = (NetDisconnectChatCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addDisconnectChatCommand - adding run ahead command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnicodeString unitext = cmdMsg->getText();
		UnsignedByte length = unitext.getLength();
		memcpy(m_packet + m_packetLen, &length, sizeof(UnsignedByte));
		m_packetLen += sizeof(UnsignedByte);

		memcpy(m_packet + m_packetLen, unitext.str(), length * sizeof(UnsignedShort));
		m_packetLen += length * sizeof(UnsignedShort);

//		DEBUG_LOG(("NetPacket - added disconnect chat command\n"));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForDisconnectChatMessage(NetCommandRef *msg) {
	Int len = 0;
	NetDisconnectChatCommandMsg *cmdMsg = (NetDisconnectChatCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // the 'D'
	len += sizeof(UnsignedByte); // string length
	UnsignedByte textLen = cmdMsg->getText().getLength();
	len += textLen * sizeof(UnsignedShort);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addChatCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForChatMessage(msg)) {
		NetChatCommandMsg *cmdMsg = (NetChatCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addDisconnectChatCommand - adding run ahead command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnicodeString unitext = cmdMsg->getText();
		UnsignedByte length = unitext.getLength();
		Int playerMask = cmdMsg->getPlayerMask();
		memcpy(m_packet + m_packetLen, &length, sizeof(UnsignedByte));
		m_packetLen += sizeof(UnsignedByte);

		memcpy(m_packet + m_packetLen, unitext.str(), length * sizeof(UnsignedShort));
		m_packetLen += length * sizeof(UnsignedShort);

		memcpy(m_packet + m_packetLen, &playerMask, sizeof(Int));
		m_packetLen += sizeof(Int);

//		DEBUG_LOG(("NetPacket - added chat command\n"));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

Bool NetPacket::isRoomForChatMessage(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	Int len = 0;
	NetChatCommandMsg *cmdMsg = (NetChatCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // the 'D'
	len += sizeof(UnsignedByte); // string length
	UnsignedByte textLen = cmdMsg->getText().getLength();
	len += textLen * sizeof(UnsignedShort);
	len += sizeof(Int); // playerMask
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addPacketRouterAckCommand(NetCommandRef *msg) {
	//  need type, player id, relay, command id, slot number
	if (isRoomForPacketRouterAckMessage(msg)) {
		NetPacketRouterAckCommandMsg *cmdMsg = (NetPacketRouterAckCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addPacketRouterAckCommand - adding packet router query command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

//		DEBUG_LOG(("NetPacket - added packet router ack command, player id %d\n", m_lastPlayerID));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room for this packet router ack command in this packet.
 */
Bool NetPacket::isRoomForPacketRouterAckMessage(NetCommandRef *msg) {
	Int len = 0;
	NetPacketRouterAckCommandMsg *cmdMsg = (NetPacketRouterAckCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // the 'D'
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addPacketRouterQueryCommand(NetCommandRef *msg) {
	//  need type, player id, relay, command id, slot number
	if (isRoomForPacketRouterQueryMessage(msg)) {
		NetPacketRouterQueryCommandMsg *cmdMsg = (NetPacketRouterQueryCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addPacketRouterQueryCommand - adding packet router query command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

//		DEBUG_LOG(("NetPacket - added packet router query command, player id %d\n", m_lastPlayerID));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());	
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room for this packet router query command in this packet.
 */
Bool NetPacket::isRoomForPacketRouterQueryMessage(NetCommandRef *msg) {
	Int len = 0;
	NetPacketRouterQueryCommandMsg *cmdMsg = (NetPacketRouterQueryCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // the 'D'
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::addDisconnectPlayerCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;

//	DEBUG_LOG(("NetPacket::addDisconnectPlayerCommand - entering...\n"));
	//  need type, player id, relay, command id, slot number
	if (isRoomForDisconnectPlayerMessage(msg)) {
		NetDisconnectPlayerCommandMsg *cmdMsg = (NetDisconnectPlayerCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addDisconnectPlayerCommand - adding run ahead command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnsignedByte slot = cmdMsg->getDisconnectSlot();
		memcpy(m_packet + m_packetLen, &slot, sizeof(slot));
		m_packetLen += sizeof(slot);

		UnsignedInt disconnectFrame = cmdMsg->getDisconnectFrame();
		memcpy(m_packet + m_packetLen, &disconnectFrame, sizeof(disconnectFrame));
		m_packetLen += sizeof(disconnectFrame);

//		DEBUG_LOG(("NetPacket::addDisconnectPlayerCommand - added disconnect player command, player id %d command id %d, disconnecting slot %d\n", m_lastPlayerID, m_lastCommandID, slot));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room for this player disconnect command in this packet.
 */
Bool NetPacket::isRoomForDisconnectPlayerMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetDisconnectPlayerCommandMsg *cmdMsg = (NetDisconnectPlayerCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // the 'D'
	len += sizeof(UnsignedByte); // slot number
	len += sizeof(UnsignedInt);	// disconnectFrame
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}


/**
 * Add a keep alive command to the packet. Returns true if successful.
 */
Bool NetPacket::addDisconnectKeepAliveCommand(NetCommandRef *msg) {
	if (isRoomForDisconnectKeepAliveMessage(msg)) {
		NetDisconnectKeepAliveCommandMsg *cmdMsg = (NetDisconnectKeepAliveCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

//		DEBUG_LOG(("Added keep alive command to packet.\n"));

		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room in the packet for this command.
 */
Bool NetPacket::isRoomForDisconnectKeepAliveMessage(NetCommandRef *msg) {
	Int len = 0;
	NetDisconnectKeepAliveCommandMsg *cmdMsg = (NetDisconnectKeepAliveCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // For the 'D'
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Add a keep alive command to the packet. Returns true if successful.
 */
Bool NetPacket::addKeepAliveCommand(NetCommandRef *msg) {
	if (isRoomForKeepAliveMessage(msg)) {
		NetKeepAliveCommandMsg *cmdMsg = (NetKeepAliveCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

//		DEBUG_LOG(("Added keep alive command to packet.\n"));

		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room in the packet for this command.
 */
Bool NetPacket::isRoomForKeepAliveMessage(NetCommandRef *msg) {
	Int len = 0;
	NetKeepAliveCommandMsg *cmdMsg = (NetKeepAliveCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // For the 'D'
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Add a run ahead command to the packet. Returns true if successful.
 */
Bool NetPacket::addRunAheadCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForRunAheadMessage(msg)) {
		NetRunAheadCommandMsg *cmdMsg = (NetRunAheadCommandMsg *)(msg->getCommand());
		//DEBUG_LOG(("NetPacket::addRunAheadCommand - adding run ahead command\n"));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnsignedShort newRunAhead = cmdMsg->getRunAhead();
		memcpy(m_packet + m_packetLen, &newRunAhead, sizeof(UnsignedShort));
		m_packetLen += sizeof(UnsignedShort);

		UnsignedByte newFrameRate = cmdMsg->getFrameRate();
		memcpy(m_packet + m_packetLen, &newFrameRate, sizeof(UnsignedByte));
		m_packetLen += sizeof(UnsignedByte);

//		DEBUG_LOG(("NetPacket - added run ahead command, frame %d, player id %d command id %d\n", m_lastFrame, m_lastPlayerID, m_lastCommandID));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room for this run ahead command in this packet.
 */
Bool NetPacket::isRoomForRunAheadMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetRunAheadCommandMsg *cmdMsg = (NetRunAheadCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedShort);
	len += sizeof(UnsignedByte);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Add a DestroyPlayer command to the packet. Returns true if successful.
 */
Bool NetPacket::addDestroyPlayerCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForDestroyPlayerMessage(msg)) {
		NetDestroyPlayerCommandMsg *cmdMsg = (NetDestroyPlayerCommandMsg *)(msg->getCommand());

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnsignedInt newVal = cmdMsg->getPlayerIndex();
		memcpy(m_packet + m_packetLen, &newVal, sizeof(UnsignedInt));
		m_packetLen += sizeof(UnsignedInt);

		//DEBUG_LOG(("NetPacket - added CRC:0x%8.8X info command, frame %d, player id %d command id %d\n", newCRC, m_lastFrame, m_lastPlayerID, m_lastCommandID));

		++m_numCommands;
		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is room for this DestroyPlayer command in this packet.
 */
Bool NetPacket::isRoomForDestroyPlayerMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetDestroyPlayerCommandMsg *cmdMsg = (NetDestroyPlayerCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedInt);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Add a run ahead metrics command to the packet. Returns true if successful.
 */
Bool NetPacket::addRunAheadMetricsCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForRunAheadMetricsMessage(msg)) {
		NetRunAheadMetricsCommandMsg *cmdMsg = (NetRunAheadMetricsCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addRunAheadMetricsCommand - adding run ahead metrics for player %d, fps = %d, latency = %f\n", cmdMsg->getPlayerID(), cmdMsg->getAverageFps(), cmdMsg->getAverageLatency()));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		// write the average latency
		Real averageLatency = cmdMsg->getAverageLatency();
		memcpy(m_packet + m_packetLen, &averageLatency, sizeof(averageLatency));
		m_packetLen += sizeof(averageLatency);
		// write the average fps
		UnsignedShort averageFps = (UnsignedShort)(cmdMsg->getAverageFps());
		memcpy(m_packet + m_packetLen, &averageFps, sizeof(averageFps));
		m_packetLen += sizeof(averageFps);

		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		++m_numCommands;
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is enough room in the packet to fit this message.
 */
Bool NetPacket::isRoomForRunAheadMetricsMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetRunAheadMetricsCommandMsg *cmdMsg = (NetRunAheadMetricsCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // 'D'
	len += sizeof(UnsignedShort);
	len += sizeof(Real);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}


/**
 * Add a player leave command to the packet. Returns true if successful.
 */
Bool NetPacket::addPlayerLeaveCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isRoomForPlayerLeaveMessage(msg)) {
		NetPlayerLeaveCommandMsg *cmdMsg = (NetPlayerLeaveCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addPlayerLeaveCommand - adding player leave command for player %d\n", cmdMsg->getLeavingPlayerID()));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnsignedByte leavingPlayerID = cmdMsg->getLeavingPlayerID();
		memcpy(m_packet + m_packetLen, &leavingPlayerID, sizeof(UnsignedByte));
		m_packetLen += sizeof(UnsignedByte);

		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		++m_numCommands;
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is enough room in the packet to fit this message.
 */
Bool NetPacket::isRoomForPlayerLeaveMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetPlayerLeaveCommandMsg *cmdMsg = (NetPlayerLeaveCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedByte);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Add this frame command message. Returns true if successful.
 */
Bool NetPacket::addFrameCommand(NetCommandRef *msg) {
	Bool needNewCommandID = FALSE;
	if (isFrameRepeat(msg)) {
		if (m_packetLen >= MAX_PACKET_SIZE) {
			return FALSE;
		}
		m_packet[m_packetLen] = 'Z';
		++m_packetLen;
		m_lastCommandID = msg->getCommand()->getID();
		m_lastCommand->deleteInstance();
		m_lastCommand = newInstance(NetCommandRef)(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		++m_lastFrame;		// need this cause we're actually advancing to the next frame by adding this command.
		++m_numCommands;
		// frameinfodebug
//		DEBUG_LOG(("outgoing - added frame %d, player %d, command count = %d, command id = %d, repeat\n", m_lastFrame, m_lastPlayerID, 0, m_lastCommandID));
		return TRUE;
	}
	if (isRoomForFrameMessage(msg)) {
		NetFrameCommandMsg *cmdMsg = (NetFrameCommandMsg *)(msg->getCommand());
//		DEBUG_LOG(("NetPacket::addFrameCommand - adding frame command for frame %d, command count = %d, command id = %d\n", cmdMsg->getExecutionFrame(), cmdMsg->getCommandCount(), cmdMsg->getID()));

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

//		DEBUG_LOG(("relay = %d, ", m_lastRelay));

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
			needNewCommandID = TRUE;
		}

//		DEBUG_LOG(("player = %d", m_lastPlayerID));

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

//		DEBUG_LOG(("command id = %d\n", m_lastCommandID));

		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		UnsignedShort cmdCount = cmdMsg->getCommandCount();
		memcpy(m_packet + m_packetLen, &cmdCount, sizeof(UnsignedShort));
		m_packetLen += sizeof(UnsignedShort);

		// frameinfodebug
//		DEBUG_LOG(("outgoing - added frame %d, player %d, command count = %d, command id = %d\n", cmdMsg->getExecutionFrame(), cmdMsg->getPlayerID(), cmdMsg->getCommandCount(), cmdMsg->getID()));

		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		++m_numCommands;
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is enough room in this packet for this frame message.
 */
Bool NetPacket::isRoomForFrameMessage(NetCommandRef *msg) {
	Int len = 0;
	Bool needNewCommandID = FALSE;
	NetFrameCommandMsg *cmdMsg = (NetFrameCommandMsg *)(msg->getCommand());
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		len += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastRelay != msg->getRelay()) {
		len += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		len += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedShort);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::isFrameRepeat(NetCommandRef *msg) {
	if (m_lastCommand == NULL) {
		return FALSE;
	}
	if (m_lastCommand->getCommand()->getNetCommandType() != NETCOMMANDTYPE_FRAMEINFO) {
		return FALSE;
	}
	NetFrameCommandMsg *framemsg = (NetFrameCommandMsg *)(msg->getCommand());
	NetFrameCommandMsg *lastmsg = (NetFrameCommandMsg *)(m_lastCommand->getCommand());
	if (framemsg->getCommandCount() != 0) {
		return FALSE;
	}
	if (framemsg->getExecutionFrame() != (lastmsg->getExecutionFrame() + 1)) {
		return FALSE;
	}
	if (msg->getRelay() != m_lastCommand->getRelay()) {
		return FALSE;
	}
	if (framemsg->getID() != (lastmsg->getID() + 1)) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Add an ack "both" command.
 */
Bool NetPacket::addAckBothCommand(NetCommandRef *msg) {
	NetAckBothCommandMsg *ackmsg = (NetAckBothCommandMsg *)(msg->getCommand());
	return addAckCommand(msg, ackmsg->getCommandID(), ackmsg->getOriginalPlayerID());
}

/**
 * Add an ack stage 1 command.
 */
Bool NetPacket::addAckStage1Command(NetCommandRef *msg) {
	NetAckStage1CommandMsg *ackmsg = (NetAckStage1CommandMsg *)(msg->getCommand());
	return addAckCommand(msg, ackmsg->getCommandID(), ackmsg->getOriginalPlayerID());
}

/**
 * Add an ack stage 2 command.
 */
Bool NetPacket::addAckStage2Command(NetCommandRef *msg) {
	NetAckStage2CommandMsg *ackmsg = (NetAckStage2CommandMsg *)(msg->getCommand());
	return addAckCommand(msg, ackmsg->getCommandID(), ackmsg->getOriginalPlayerID());
}

/**
 * Add this ack command to the packet.  Returns true if successful.
 */
Bool NetPacket::addAckCommand(NetCommandRef *msg, UnsignedShort commandID, UnsignedByte originalPlayerID) {
	if (isAckRepeat(msg)) {
		if (m_packetLen >= MAX_PACKET_SIZE) {
			return FALSE;
		}
		m_packet[m_packetLen] = 'Z';
		++m_packetLen;
		++m_numCommands;
		m_lastCommand->deleteInstance();
		m_lastCommand = NULL;

		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());
		return TRUE;
	}
	if (isRoomForAckMessage(msg)) {
		NetCommandMsg *cmdMsg = msg->getCommand();
//		DEBUG_LOG(("NetPacket::addAckCommand - adding ack for command %d for player %d\n", cmdMsg->getCommandID(), msg->getCommand()->getPlayerID()));
		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

		// Put in the command id of the command we are acking.
		m_packet[m_packetLen] = 'D';
		++m_packetLen;
		memcpy(m_packet + m_packetLen, &commandID, sizeof(UnsignedShort));
		m_packetLen += sizeof(UnsignedShort);
		memcpy(m_packet + m_packetLen, &originalPlayerID, sizeof(UnsignedByte));
		m_packetLen += sizeof(UnsignedByte);

		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

//		DEBUG_LOG(("outgoing - added ACK, original player %d, command id %d\n", origPlayerID, cmdID));
		++m_numCommands;
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns true if there is enough room in the packet for this ack message.
 */
Bool NetPacket::isRoomForAckMessage(NetCommandRef *msg) {
	Int len = 0;
	NetCommandMsg *cmdMsg = msg->getCommand();
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		++len;
		len += sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		++len;
		len += sizeof(UnsignedByte);
	}

	++len; // for 'D'
	len += sizeof(UnsignedShort);
	len += sizeof(UnsignedByte);
	if ((len + m_packetLen) > MAX_PACKET_SIZE) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::isAckRepeat(NetCommandRef *msg) {
	if (m_lastCommand == NULL) {
		return FALSE;
	}
	if (m_lastCommand->getCommand()->getNetCommandType() != msg->getCommand()->getNetCommandType()) {
		return FALSE;
	}
	if (msg->getCommand()->getNetCommandType() == NETCOMMANDTYPE_ACKBOTH) {
		return isAckBothRepeat(msg);
	}
	if (msg->getCommand()->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE1) {
		return isAckStage1Repeat(msg);
	}
	if (msg->getCommand()->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE2) {
		return isAckStage2Repeat(msg);
	}
	return FALSE;
}

Bool NetPacket::isAckBothRepeat(NetCommandRef *msg) {
	NetAckBothCommandMsg *ack = (NetAckBothCommandMsg *)(msg->getCommand());
	NetAckBothCommandMsg *lastAck = (NetAckBothCommandMsg *)(m_lastCommand->getCommand());
	if (lastAck->getCommandID() != (ack->getCommandID() - 1)) {
		return FALSE;
	}
	if (lastAck->getOriginalPlayerID() != ack->getOriginalPlayerID()) {
		return FALSE;
	}
	if (msg->getRelay() != m_lastCommand->getRelay()) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::isAckStage1Repeat(NetCommandRef *msg) {
	NetAckStage2CommandMsg *ack = (NetAckStage2CommandMsg *)(msg->getCommand());
	NetAckStage2CommandMsg *lastAck = (NetAckStage2CommandMsg *)(m_lastCommand->getCommand());
	if (lastAck->getCommandID() != (ack->getCommandID() - 1)) {
		return FALSE;
	}
	if (lastAck->getOriginalPlayerID() != ack->getOriginalPlayerID()) {
		return FALSE;
	}
	if (msg->getRelay() != m_lastCommand->getRelay()) {
		return FALSE;
	}
	return TRUE;
}

Bool NetPacket::isAckStage2Repeat(NetCommandRef *msg) {
	NetAckStage2CommandMsg *ack = (NetAckStage2CommandMsg *)(msg->getCommand());
	NetAckStage2CommandMsg *lastAck = (NetAckStage2CommandMsg *)(m_lastCommand->getCommand());
	if (lastAck->getCommandID() != (ack->getCommandID() - 1)) {
		return FALSE;
	}
	if (lastAck->getOriginalPlayerID() != ack->getOriginalPlayerID()) {
		return FALSE;
	}
	if (msg->getRelay() != m_lastCommand->getRelay()) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Adds this game command to the packet.  Returns true if successful.
 */
Bool NetPacket::addGameCommand(NetCommandRef *msg) {
	Bool retval = FALSE;
	NetGameCommandMsg *cmdMsg = (NetGameCommandMsg *)(msg->getCommand());
	// get the game message from the NetCommandMsg
	GameMessage *gmsg = cmdMsg->constructGameMessage();

//	DEBUG_LOG(("NetPacket::addGameCommand for command ID %d\n", cmdMsg->getID()));

	if (isRoomForGameMessage(msg, gmsg)) {
		// Now we know there is enough room, put the new game message into the packet.

		Bool needNewCommandID = FALSE; // this is to allow us to force the starting command ID to be respecified with this command.

		// If necessary, put the NetCommandType into the packet.
		if (m_lastCommandType != cmdMsg->getNetCommandType()) {
			m_packet[m_packetLen] = 'T';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getNetCommandType();
			m_packetLen += sizeof(UnsignedByte);

			m_lastCommandType = cmdMsg->getNetCommandType();
		}

		// If necessary, put the execution frame into the packet.
		if (m_lastFrame != cmdMsg->getExecutionFrame()) {
			m_packet[m_packetLen] = 'F';
			++m_packetLen;
			UnsignedInt newframe = cmdMsg->getExecutionFrame();
			memcpy(m_packet+m_packetLen, &newframe, sizeof(UnsignedInt));
			m_packetLen += sizeof(UnsignedInt);

			m_lastFrame = newframe;
		}

		// If necessary, put the relay into the packet.
		if (m_lastRelay != msg->getRelay()) {
			m_packet[m_packetLen] = 'R';
			++m_packetLen;
			UnsignedByte newRelay = msg->getRelay();
			memcpy(m_packet+m_packetLen, &newRelay, sizeof(UnsignedByte));
			m_packetLen += sizeof(UnsignedByte);

			m_lastRelay = newRelay;
		}

		// If necessary, put the playerID into the packet.
		if (m_lastPlayerID != cmdMsg->getPlayerID()) {
			m_packet[m_packetLen] = 'P';
			++m_packetLen;
			m_packet[m_packetLen] = cmdMsg->getPlayerID();
			m_packetLen += sizeof(UnsignedByte);
			//since if we have a new player we need to respecify the starting command ID, lets force the command id to be different.
			needNewCommandID = TRUE;

			m_lastPlayerID = cmdMsg->getPlayerID();
		}

		// If necessary, specify the command ID of this command.
		if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
			m_packet[m_packetLen] = 'C';
			++m_packetLen;
			UnsignedShort newID = cmdMsg->getID();
			memcpy(m_packet + m_packetLen, &newID, sizeof(UnsignedShort));
			m_packetLen += sizeof(UnsignedShort);
		}
		m_lastCommandID = cmdMsg->getID();

		m_packet[m_packetLen] = 'D';
		++m_packetLen;

		// Now copy the GameMessage type into the packet.
		GameMessage::Type newType = gmsg->getType();
		memcpy(m_packet + m_packetLen, &newType, sizeof(GameMessage::Type));
		m_packetLen += sizeof(GameMessage::Type);


		GameMessageParser *parser = newInstance(GameMessageParser)(gmsg);
		UnsignedByte numTypes = parser->getNumTypes();
		memcpy(m_packet + m_packetLen, &numTypes, sizeof(numTypes));
		m_packetLen += sizeof(numTypes);

		GameMessageParserArgumentType *argType = parser->getFirstArgumentType();
		while (argType != NULL) {
			UnsignedByte type = (UnsignedByte)(argType->getType());
			memcpy(m_packet + m_packetLen, &type, sizeof(type));
			m_packetLen += sizeof(type);

			UnsignedByte argTypeCount = argType->getArgCount();
			memcpy(m_packet + m_packetLen, &argTypeCount, sizeof(argTypeCount));
			m_packetLen += sizeof(argTypeCount);

			argType = argType->getNext();
		}

		Int numArgs = gmsg->getArgumentCount();
		for (Int i = 0; i < numArgs; ++i) {
			GameMessageArgumentDataType type = gmsg->getArgumentDataType(i);
			GameMessageArgumentType arg = *(gmsg->getArgument(i));
			writeGameMessageArgumentToPacket(type, arg);
		}

		parser->deleteInstance();
		parser = NULL;

//		DEBUG_LOG(("NetPacket::addGameMessage - added game message, frame %d, player %d, command ID %d\n", m_lastFrame, m_lastPlayerID, m_lastCommandID));

		++m_numCommands;

		if (m_lastCommand != NULL) {
			m_lastCommand->deleteInstance();
			m_lastCommand = NULL;
		}
		m_lastCommand = NEW_NETCOMMANDREF(msg->getCommand());
		m_lastCommand->setRelay(msg->getRelay());

		retval = TRUE;
	}

	if (gmsg) {
		gmsg->deleteInstance();
		gmsg = NULL;
	}

	return retval;
}

void NetPacket::writeGameMessageArgumentToPacket(GameMessageArgumentDataType type, GameMessageArgumentType arg) {
	if (type == ARGUMENTDATATYPE_INTEGER) {
		memcpy(m_packet + m_packetLen, &(arg.integer), sizeof(arg.integer));
		m_packetLen += sizeof(arg.integer);
	} else if (type == ARGUMENTDATATYPE_REAL) {
		memcpy(m_packet + m_packetLen, &(arg.real), sizeof(arg.real));
		m_packetLen += sizeof(arg.real);
	} else if (type == ARGUMENTDATATYPE_BOOLEAN) {
		memcpy(m_packet + m_packetLen, &(arg.boolean), sizeof(arg.boolean));
		m_packetLen += sizeof(arg.boolean);
	} else if (type == ARGUMENTDATATYPE_OBJECTID) {
		memcpy(m_packet + m_packetLen, &(arg.objectID), sizeof(arg.objectID));
		m_packetLen += sizeof(arg.objectID);
	} else if (type == ARGUMENTDATATYPE_DRAWABLEID) {
		memcpy(m_packet + m_packetLen, &(arg.drawableID), sizeof(arg.drawableID));
		m_packetLen += sizeof(arg.drawableID);
	} else if (type == ARGUMENTDATATYPE_TEAMID) {
		memcpy(m_packet + m_packetLen, &(arg.teamID), sizeof(arg.teamID));
		m_packetLen += sizeof(arg.teamID);
	} else if (type == ARGUMENTDATATYPE_LOCATION) {
		memcpy(m_packet + m_packetLen, &(arg.location), sizeof(arg.location));
		m_packetLen += sizeof(arg.location);
	} else if (type == ARGUMENTDATATYPE_PIXEL) {
		memcpy(m_packet + m_packetLen, &(arg.pixel), sizeof(arg.pixel));
		m_packetLen += sizeof(arg.pixel);
	} else if (type == ARGUMENTDATATYPE_PIXELREGION) {
		memcpy(m_packet + m_packetLen, &(arg.pixelRegion), sizeof(arg.pixelRegion));
		m_packetLen += sizeof(arg.pixelRegion);
	} else if (type == ARGUMENTDATATYPE_TIMESTAMP) {
		memcpy(m_packet + m_packetLen, &(arg.timestamp), sizeof(arg.timestamp));
		m_packetLen += sizeof(arg.timestamp);
	} else if (type == ARGUMENTDATATYPE_WIDECHAR) {
		memcpy(m_packet + m_packetLen, &(arg.wChar), sizeof(arg.wChar));
		m_packetLen += sizeof(arg.wChar);
	}
}

/**
 * Returns true if there is enough room in this packet for this message.
 */
Bool NetPacket::isRoomForGameMessage(NetCommandRef *msg, GameMessage *gmsg) {
	// Calculate how much space the NetCommandMsg will take in this packet.
	Int msglen = 0;

	NetGameCommandMsg *cmdMsg = (NetGameCommandMsg *)(msg->getCommand());

	Bool needNewCommandID = FALSE;

	if (m_lastFrame != cmdMsg->getExecutionFrame()) {
		msglen += sizeof(UnsignedInt) + sizeof(UnsignedByte);
	}
	if (m_lastPlayerID != cmdMsg->getPlayerID()) {
		msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
		needNewCommandID = TRUE;
	}
	if (m_lastRelay != msg->getRelay()) {
		msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (m_lastCommandType != cmdMsg->getNetCommandType()) {
		msglen += sizeof(UnsignedByte) + sizeof(UnsignedByte);
	}
	if (((m_lastCommandID + 1) != (UnsignedShort)(cmdMsg->getID())) || (needNewCommandID == TRUE)) {
		msglen += sizeof(UnsignedShort) + sizeof(UnsignedByte);
	}

	GameMessageParser *parser = newInstance(GameMessageParser)(gmsg);

	++msglen; // for 'D'
	msglen += sizeof(GameMessage::Type);
	msglen += sizeof(UnsignedByte);
//	Int numTypes = parser->getNumTypes();
	GameMessageParserArgumentType *arg = parser->getFirstArgumentType();
	while (arg != NULL) {
		msglen += 2 * sizeof(UnsignedByte); // for the type and number of args of that type declaration.
		GameMessageArgumentDataType type = arg->getType();
		if (type == ARGUMENTDATATYPE_INTEGER) {
			msglen += arg->getArgCount() * sizeof(Int);
		} else if (type == ARGUMENTDATATYPE_REAL) {
			msglen += arg->getArgCount() * sizeof(Real);
		} else if (type == ARGUMENTDATATYPE_BOOLEAN) {
			msglen += arg->getArgCount() * sizeof(Bool);
		} else if (type == ARGUMENTDATATYPE_OBJECTID) {
			msglen += arg->getArgCount() * sizeof(ObjectID);
		} else if (type == ARGUMENTDATATYPE_DRAWABLEID) {
			msglen += arg->getArgCount() * sizeof(DrawableID);
		} else if (type == ARGUMENTDATATYPE_TEAMID) {
			msglen += arg->getArgCount() * sizeof(UnsignedInt);
		} else if (type == ARGUMENTDATATYPE_LOCATION) {
			msglen += arg->getArgCount() * sizeof(Coord3D);
		} else if (type == ARGUMENTDATATYPE_PIXEL) {
			msglen += arg->getArgCount() * sizeof(ICoord2D);
		} else if (type == ARGUMENTDATATYPE_PIXELREGION) {
			msglen += arg->getArgCount() * sizeof(IRegion2D);
		} else if (type == ARGUMENTDATATYPE_TIMESTAMP) {
			msglen += arg->getArgCount() * sizeof(UnsignedInt);
		} else if (type == ARGUMENTDATATYPE_WIDECHAR) {
			msglen += arg->getArgCount() * sizeof(WideChar);
		}
		arg = arg->getNext();
	}

	parser->deleteInstance();
	parser = NULL;

	// Is there enough room in the packet for this message?
	if (msglen > (MAX_PACKET_SIZE - m_packetLen)) {
		return FALSE; // there isn't enough room in this packet for this new message.
	}
	return TRUE;
}

/**
 * Returns the list of commands that are in this packet.  If the packet is malformed or truncated,
 * the whole packet is dropped and an empty list is returned.
 */
NetCommandList * NetPacket::getCommandList() {
	NetCommandList *retval = newInstance(NetCommandList);
//	DEBUG_LOG(("NetPacket::getCommandList, packet length = %d\n", m_packetLen));
	retval->init();

	// These need to be the same as the default values for m_lastPlayerID, m_lastFrame, etc.
	UnsignedByte playerID = 0;
	UnsignedInt frame = 0;
	UnsignedShort commandID = 1; // The first command is going to be
	UnsignedByte commandType = 0;
	UnsignedByte relay = 0;
	NetCommandRef *lastCommand = NULL;

	NetPacketReader reader(m_packet, m_packetLen);
	while (!reader.isAtEnd()) {
		UnsignedByte tag = 0;
		reader.read(&tag, sizeof(tag));
		if (tag == 'T') {
			reader.read(&commandType, sizeof(UnsignedByte));
		} else if (tag == 'F') {
			reader.read(&frame, sizeof(UnsignedInt));
		} else if (tag == 'P') {
			reader.read(&playerID, sizeof(UnsignedByte));
		} else if (tag == 'R') {
			reader.read(&relay, sizeof(UnsignedByte));
		} else if (tag == 'C') {
			reader.read(&commandID, sizeof(UnsignedShort));
		} else if (tag == 'D') {
			//DEBUG_LOG(("NetPacket::getCommandList() - command of type %d(%s)\n", commandType, GetAsciiNetCommandType((NetCommandType)commandType).str()));

			NetCommandMsg *msg = readCommandData((NetCommandType)commandType, reader);

			if (msg == NULL) {
				DEBUG_LOG(("NetPacket::getCommandList - failed to read command of type %d from player %d\n", commandType, playerID));
				reader.fail();
				break;
			}

			if (commandType == NETCOMMANDTYPE_FRAMEINFO) {
				// frameinfodebug
				DEBUG_LOG(("read frame %d from player %d, command count = %d, relay = 0x%X\n", frame, playerID, ((NetFrameCommandMsg *)msg)->getCommandCount(), relay));
			} else if (commandType == NETCOMMANDTYPE_WRAPPER) {
				DEBUG_LOG(("Done reading Wrapper message from player %d - wrapped command was %d\n", playerID,
					((NetWrapperCommandMsg *)msg)->getWrappedCommandID()));
			}

			// set the info
			msg->setExecutionFrame(frame);
			msg->setPlayerID(playerID);
			msg->setNetCommandType((NetCommandType)commandType);
			msg->setID(commandID);

//			DEBUG_LOG(("frame = %d, player = %d, command type = %d, id = %d\n", frame, playerID, commandType, commandID));

			// increment to the next command ID.
			if (DoesCommandRequireACommandID((NetCommandType)commandType)) {
				++commandID;
			}

			// add the message to the list.
			NetCommandRef *ref = retval->addMessage(msg);
			if (ref != NULL) {
				ref->setRelay(relay);
			} else {
				DEBUG_LOG(("NetPacket::getCommandList - failed to set relay for message %d\n", msg->getID()));
			}

			if (lastCommand != NULL) {
				lastCommand->deleteInstance();
				lastCommand = NULL;
			}
			lastCommand = newInstance(NetCommandRef)(msg);

			msg->detach();  // Need to detach from new NetCommandMsg created by the "readXMessage" above.

			// since the message is part of the list now, we don't have to keep track of it.  So we'll just set it to NULL.
			msg = NULL;
		} else if (tag == 'Z') {
			// Repeat the last command, doing some funky cool byte-saving stuff
			if (lastCommand == NULL) {
				DEBUG_CRASH(("Got a repeat command with no command to repeat."));
				reader.fail();
				break;
			}
			if (lastCommand->getCommand()->getNetCommandType() != (NetCommandType)commandType) {
				DEBUG_LOG(("NetPacket::getCommandList - repeat of type %d does not match last command type %d\n",
					commandType, lastCommand->getCommand()->getNetCommandType()));
				reader.fail();
				break;
			}
			NetCommandMsg *msg = NULL;
			if (commandType == NETCOMMANDTYPE_ACKSTAGE1) {
				msg = newInstance(NetAckStage1CommandMsg)();
				NetAckStage1CommandMsg *last = (NetAckStage1CommandMsg *)(lastCommand->getCommand());
				((NetAckStage1CommandMsg *)msg)->setCommandID(last->getCommandID() + 1);
				((NetAckStage1CommandMsg *)msg)->setOriginalPlayerID(last->getOriginalPlayerID());
			} else if (commandType == NETCOMMANDTYPE_ACKSTAGE2) {
				msg = newInstance(NetAckStage2CommandMsg)();
				NetAckStage2CommandMsg *last = (NetAckStage2CommandMsg *)(lastCommand->getCommand());
				((NetAckStage2CommandMsg *)msg)->setCommandID(last->getCommandID() + 1);
				((NetAckStage2CommandMsg *)msg)->setOriginalPlayerID(last->getOriginalPlayerID());
			} else if (commandType == NETCOMMANDTYPE_ACKBOTH) {
				msg = newInstance(NetAckBothCommandMsg)();
				NetAckBothCommandMsg *last = (NetAckBothCommandMsg *)(lastCommand->getCommand());
				((NetAckBothCommandMsg *)msg)->setCommandID(last->getCommandID() + 1);
				((NetAckBothCommandMsg *)msg)->setOriginalPlayerID(last->getOriginalPlayerID());
			} else if (commandType == NETCOMMANDTYPE_FRAMEINFO) {
				msg = newInstance(NetFrameCommandMsg)();
				++frame; // this is set below.
				((NetFrameCommandMsg *)msg)->setCommandCount(0);
				DEBUG_LOG(("Read a repeated frame command, frame = %d, player = %d, commandID = %d\n", frame, playerID, commandID));
			} else {
				DEBUG_CRASH(("Trying to repeat a command that shouldn't be repeated."));
				reader.fail();
				break;
			}

			msg->setExecutionFrame(frame);
			msg->setPlayerID(playerID);
			msg->setNetCommandType((NetCommandType)commandType);
			msg->setID(commandID);

//			DEBUG_LOG(("frame = %d, player = %d, command type = %d, id = %d\n", frame, playerID, commandType, commandID));

			// increment to the next command ID.
			if (DoesCommandRequireACommandID((NetCommandType)commandType)) {
				++commandID;
			}

			// add the message to the list.
			NetCommandRef *ref = retval->addMessage(msg);
			if (ref != NULL) {
				ref->setRelay(relay);
			}

			lastCommand->deleteInstance();
			lastCommand = NULL;
//			lastCommand = newInstance(NetCommandRef)(msg);
			lastCommand = NEW_NETCOMMANDREF(msg);

			msg->detach();  // Need to detach from new NetCommandMsg created by the "readXMessage" above.

			// since the message is part of the list now, we don't have to keep track of it.  So we'll just set it to NULL.
			msg = NULL;
		} else {
			// we don't recognize this entry, so we can't know where the next one starts.
			DEBUG_CRASH(("Unrecognized packet entry, dropping packet."));
			DEBUG_LOG(("NetPacket::getCommandList - Unrecognized packet entry at index %d\n", reader.getPosition() - 1));
			reader.fail();
			break;
		}
	}

	if (lastCommand != NULL) {
		lastCommand->deleteInstance();
		lastCommand = NULL;
	}

	if (reader.hasFailed()) {
		DEBUG_LOG(("NetPacket::getCommandList - malformed packet near index %d, dropping packet\n", reader.getPosition()));
		dumpPacketToLog();
		retval->reset();
	}

	return retval;
}

/**
 * Reads the data portion of a game message from the given position in the packet.
 */
NetCommandMsg * NetPacket::readGameMessage(NetPacketReader &reader)
{
//	DEBUG_LOG(("NetPacket::readGameMessage\n"));

	// Get the GameMessage command type.
	GameMessage::Type newType;
	if (!reader.read(&newType, sizeof(GameMessage::Type))) {
		return NULL;
	}

	// Get the number of argument types
	UnsignedByte numArgTypes = 0;
	if (!reader.read(&numArgTypes, sizeof(numArgTypes))) {
		return NULL;
	}

	NetGameCommandMsg *msg = newInstance(NetGameCommandMsg);
	msg->setGameMessageType(newType);

	// Get the types and the number of arguments of those types.
	Bool success = TRUE;
	Int totalArgCount = 0;
	GameMessageParser *parser = newInstance(GameMessageParser)();
	for (Int j = 0; j < numArgTypes; ++j) {
		UnsignedByte type = (UnsignedByte)ARGUMENTDATATYPE_UNKNOWN;
		UnsignedByte argCount = 0;
		if (!reader.read(&type, sizeof(type)) || !reader.read(&argCount, sizeof(argCount))) {
			success = FALSE;
			break;
		}

		totalArgCount += argCount;
		if (type >= ARGUMENTDATATYPE_UNKNOWN || argCount == 0 || totalArgCount > MAX_GAME_MESSAGE_ARGUMENTS) {
			DEBUG_LOG(("NetPacket::readGameMessage - invalid argument type %d or count %d (total %d)\n", type, argCount, totalArgCount));
			success = FALSE;
			break;
		}

		parser->addArgType((GameMessageArgumentDataType)type, argCount);
	}

	// Read the arguments, in the order of the argument types.
	GameMessageParserArgumentType *parserArgType = parser->getFirstArgumentType();
	while (success && parserArgType != NULL) {
		GameMessageArgumentDataType argType = parserArgType->getType();
		Int argCount = parserArgType->getArgCount();
		for (Int k = 0; k < argCount; ++k) {
			if (!readGameMessageArgumentFromPacket(argType, msg, reader)) {
				success = FALSE;
				break;
			}
		}
		parserArgType = parserArgType->getNext();
	}

	parser->deleteInstance();
	parser = NULL;

	if (!success) {
		reader.fail();
		msg->detach();
		return NULL;
	}

	return (NetCommandMsg *)msg;
}

Bool NetPacket::readGameMessageArgumentFromPacket(GameMessageArgumentDataType type, NetGameCommandMsg *msg, NetPacketReader &reader) {
	GameMessageArgumentType arg;
	if (type == ARGUMENTDATATYPE_INTEGER) {
		Int theint;
		if (!reader.read(&theint, sizeof(theint))) {
			return FALSE;
		}
		arg.integer = theint;
	} else if (type == ARGUMENTDATATYPE_REAL) {
		Real thereal;
		if (!reader.read(&thereal, sizeof(thereal))) {
			return FALSE;
		}
		arg.real = thereal;
	} else if (type == ARGUMENTDATATYPE_BOOLEAN) {
		Bool thebool;
		if (!reader.read(&thebool, sizeof(thebool))) {
			return FALSE;
		}
		arg.boolean = thebool;
	} else if (type == ARGUMENTDATATYPE_OBJECTID) {
		ObjectID theint;
		if (!reader.read(&theint, sizeof(theint))) {
			return FALSE;
		}
		arg.objectID = theint;
	} else if (type == ARGUMENTDATATYPE_DRAWABLEID) {
		DrawableID theint;
		if (!reader.read(&theint, sizeof(theint))) {
			return FALSE;
		}
		arg.drawableID = theint;
	} else if (type == ARGUMENTDATATYPE_TEAMID) {
		UnsignedInt theint;
		if (!reader.read(&theint, sizeof(theint))) {
			return FALSE;
		}
		arg.teamID = theint;
	} else if (type == ARGUMENTDATATYPE_LOCATION) {
		Coord3D coord;
		if (!reader.read(&coord, sizeof(coord))) {
			return FALSE;
		}
		arg.location = coord;
	} else if (type == ARGUMENTDATATYPE_PIXEL) {
		ICoord2D pixel;
		if (!reader.read(&pixel, sizeof(pixel))) {
			return FALSE;
		}
		arg.pixel = pixel;
	} else if (type == ARGUMENTDATATYPE_PIXELREGION) {
		IRegion2D reg;
		if (!reader.read(&reg, sizeof(reg))) {
			return FALSE;
		}
		arg.pixelRegion = reg;
	} else if (type == ARGUMENTDATATYPE_TIMESTAMP) {
		UnsignedInt stamp;
		if (!reader.read(&stamp, sizeof(stamp))) {
			return FALSE;
		}
		arg.timestamp = stamp;
	} else if (type == ARGUMENTDATATYPE_WIDECHAR) {
		WideChar c;
		if (!reader.read(&c, sizeof(c))) {
			return FALSE;
		}
		arg.wChar = c;
	} else {
		reader.fail();
		return FALSE;
	}
	msg->addArgument(type, arg);
	return TRUE;
}

/**
 * Reads the data portion of the ack message at this position in the packet.
 */
NetCommandMsg * NetPacket::readAckBothMessage(NetPacketReader &reader) {
	//DEBUG_LOG(("NetPacket::readAckMessage, "));
	UnsignedShort cmdID = 0;
	UnsignedByte origPlayerID = 0;
	if (!reader.read(&cmdID, sizeof(UnsignedShort)) || !reader.read(&origPlayerID, sizeof(UnsignedByte))) {
		return NULL;
	}
	//DEBUG_LOG(("commandID = %d, ", cmdID));
	//DEBUG_LOG(("original player id = %d\n", origPlayerID));

	NetAckBothCommandMsg *msg = newInstance(NetAckBothCommandMsg);
	msg->setCommandID(cmdID);
	msg->setOriginalPlayerID(origPlayerID);

	return msg;
}

/**
 * Reads the data portion of the ack message at this position in the packet.
 */
NetCommandMsg * NetPacket::readAckStage1Message(NetPacketReader &reader) {
//	DEBUG_LOG(("NetPacket::readAckMessage, "));
	UnsignedShort cmdID = 0;
	UnsignedByte origPlayerID = 0;
	if (!reader.read(&cmdID, sizeof(UnsignedShort)) || !reader.read(&origPlayerID, sizeof(UnsignedByte))) {
		return NULL;
	}
//	DEBUG_LOG(("commandID = %d, ", cmdID));
//	DEBUG_LOG(("original player id = %d\n", origPlayerID));

	NetAckStage1CommandMsg *msg = newInstance(NetAckStage1CommandMsg);
	msg->setCommandID(cmdID);
	msg->setOriginalPlayerID(origPlayerID);

	return msg;
}

/**
 * Reads the data portion of the ack message at this position in the packet.
 */
NetCommandMsg * NetPacket::readAckStage2Message(NetPacketReader &reader) {
//	DEBUG_LOG(("NetPacket::readAckMessage, "));
	UnsignedShort cmdID = 0;
	UnsignedByte origPlayerID = 0;
	if (!reader.read(&cmdID, sizeof(UnsignedShort)) || !reader.read(&origPlayerID, sizeof(UnsignedByte))) {
		return NULL;
	}
//	DEBUG_LOG(("commandID = %d, ", cmdID));
//	DEBUG_LOG(("original player id = %d\n", origPlayerID));

	NetAckStage2CommandMsg *msg = newInstance(NetAckStage2CommandMsg);
	msg->setCommandID(cmdID);
	msg->setOriginalPlayerID(origPlayerID);

	return msg;
}

/**
 * Reads the data portion of the frame message at this position in the packet.
 */
NetCommandMsg * NetPacket::readFrameMessage(NetPacketReader &reader) {
//	DEBUG_LOG(("NetPacket::readFrameMessage, "));
	UnsignedShort cmdCount = 0;
	if (!reader.read(&cmdCount, sizeof(UnsignedShort))) {
		return NULL;
	}
//	DEBUG_LOG(("command count = %d, ", cmdCount));

	NetFrameCommandMsg *msg = newInstance(NetFrameCommandMsg);
	msg->setCommandCount(cmdCount);

	return msg;
}

/**
 * Reads the player leave message at this position in the packet.
 */
NetCommandMsg * NetPacket::readPlayerLeaveMessage(NetPacketReader &reader) {
	UnsignedByte leavingPlayerID = 0;
	if (!reader.read(&leavingPlayerID, sizeof(UnsignedByte))) {
		return NULL;
	}

	NetPlayerLeaveCommandMsg *msg = newInstance(NetPlayerLeaveCommandMsg);
	msg->setLeavingPlayerID(leavingPlayerID);

	return msg;
}

/**
 * Reads the run ahead metrics message at this position in the packet.
 */
NetCommandMsg * NetPacket::readRunAheadMetricsMessage(NetPacketReader &reader) {
	Real averageLatency = (Real)0.2;
	UnsignedShort averageFps = 30;
	if (!reader.read(&averageLatency, sizeof(Real)) || !reader.read(&averageFps, sizeof(UnsignedShort))) {
		return NULL;
	}

	NetRunAheadMetricsCommandMsg *msg = newInstance(NetRunAheadMetricsCommandMsg);
	msg->setAverageLatency(averageLatency);
	msg->setAverageFps((Int)averageFps);
	return msg;
}

/**
 * Reads the run ahead message at this position in the packet.
 */
NetCommandMsg * NetPacket::readRunAheadMessage(NetPacketReader &reader) {
	UnsignedShort newRunAhead = 20;
	UnsignedByte newFrameRate = 30;
	if (!reader.read(&newRunAhead, sizeof(UnsignedShort)) || !reader.read(&newFrameRate, sizeof(UnsignedByte))) {
		return NULL;
	}

	NetRunAheadCommandMsg *msg = newInstance(NetRunAheadCommandMsg);
	msg->setRunAhead(newRunAhead);
	msg->setFrameRate(newFrameRate);

	return msg;
}

/**
 * Reads the CRC info message at this position in the packet.
 */
NetCommandMsg * NetPacket::readDestroyPlayerMessage(NetPacketReader &reader) {
	UnsignedInt newVal = 0;
	if (!reader.read(&newVal, sizeof(UnsignedInt))) {
		return NULL;
	}
	//DEBUG_LOG(("Saw CRC of 0x%8.8X\n", newCRC));

	NetDestroyPlayerCommandMsg *msg = newInstance(NetDestroyPlayerCommandMsg);
	msg->setPlayerIndex(newVal);

	return msg;
}

/**
 * Reads the keep alive data, of which there is none.
 */
NetCommandMsg * NetPacket::readKeepAliveMessage(NetPacketReader &reader) {
	NetKeepAliveCommandMsg *msg = newInstance(NetKeepAliveCommandMsg);

	return msg;
}

/**
 * Reads the disconnect keep alive data, of which there is none.
 */
NetCommandMsg * NetPacket::readDisconnectKeepAliveMessage(NetPacketReader &reader) {
	NetDisconnectKeepAliveCommandMsg *msg = newInstance(NetDisconnectKeepAliveCommandMsg);

	return msg;
}

/**
 * Reads the disconnect player data.  Which is the slot number of the player being disconnected.
 */
NetCommandMsg * NetPacket::readDisconnectPlayerMessage(NetPacketReader &reader) {
	UnsignedByte slot = 0;
	UnsignedInt disconnectFrame = 0;
	if (!reader.read(&slot, sizeof(slot)) || !reader.read(&disconnectFrame, sizeof(disconnectFrame))) {
		return NULL;
	}

	NetDisconnectPlayerCommandMsg *msg = newInstance(NetDisconnectPlayerCommandMsg);
	msg->setDisconnectSlot(slot);
	msg->setDisconnectFrame(disconnectFrame);

	return msg;
}

/**
 * Reads the packet router query data, of which there is none.
 */
NetCommandMsg * NetPacket::readPacketRouterQueryMessage(NetPacketReader &reader) {
	NetPacketRouterQueryCommandMsg *msg = newInstance(NetPacketRouterQueryCommandMsg);

	return msg;
}

/**
 * Reads the packet router ack data, of which there is none.
 */
NetCommandMsg * NetPacket::readPacketRouterAckMessage(NetPacketReader &reader) {
	NetPacketRouterAckCommandMsg *msg = newInstance(NetPacketRouterAckCommandMsg);

	return msg;
}

/**
 * Reads a length-prefixed wide character string.  Text longer than MAX_CHAT_TEXT_LENGTH is
 * truncated; the whole string is still consumed from the packet.
 */
Bool NetPacket::readChatText(NetPacketReader &reader, UnicodeString &text) {
	UnsignedByte length = 0;
	if (!reader.read(&length, sizeof(UnsignedByte))) {
		return FALSE;
	}
	UnsignedByte *src = reader.readInPlace(length * sizeof(UnsignedShort));
	if (src == NULL) {
		return FALSE;
	}

	Int textLength = length;
	if (textLength > MAX_CHAT_TEXT_LENGTH) {
		textLength = MAX_CHAT_TEXT_LENGTH;
	}
	UnsignedShort buffer[MAX_CHAT_TEXT_LENGTH + 1];
	memcpy(buffer, src, textLength * sizeof(UnsignedShort));
	buffer[textLength] = 0;

	text.set(buffer);
	return TRUE;
}

/**
 * Reads the disconnect chat data, which is just the string.
 */
NetCommandMsg * NetPacket::readDisconnectChatMessage(NetPacketReader &reader) {
	UnicodeString unitext;
	if (!readChatText(reader, unitext)) {
		return NULL;
	}

	//DEBUG_LOG(("NetPacket::readDisconnectChatMessage - read message, message is %ls\n", unitext.str()));

	NetDisconnectChatCommandMsg *msg = newInstance(NetDisconnectChatCommandMsg);
	msg->setText(unitext);
	return msg;
}

/**
 * Reads the chat data, which is just the string.
 */
NetCommandMsg * NetPacket::readChatMessage(NetPacketReader &reader) {
	UnicodeString unitext;
	Int playerMask = 0;
	if (!readChatText(reader, unitext) || !reader.read(&playerMask, sizeof(Int))) {
		return NULL;
	}

	//DEBUG_LOG(("NetPacket::readChatMessage - read message, message is %ls\n", unitext.str()));

	NetChatCommandMsg *msg = newInstance(NetChatCommandMsg);
	msg->setText(unitext);
	msg->setPlayerMask(playerMask);
	return msg;
}

/**
 * Reads the disconnect vote data.  Which is the slot number of the player being disconnected.
 */
NetCommandMsg * NetPacket::readDisconnectVoteMessage(NetPacketReader &reader) {
	UnsignedByte slot = 0;
	UnsignedInt voteFrame = 0;
	if (!reader.read(&slot, sizeof(slot)) || !reader.read(&voteFrame, sizeof(voteFrame))) {
		return NULL;
	}

	NetDisconnectVoteCommandMsg *msg = newInstance(NetDisconnectVoteCommandMsg);
	msg->setSlot(slot);
	msg->setVoteFrame(voteFrame);

	return msg;
}

/**
 * Reads the Progress data.  Which is the slot number of the player being disconnected.
 */
NetCommandMsg * NetPacket::readProgressMessage(NetPacketReader &reader) {
	UnsignedByte percentage = 0;
	if (!reader.read(&percentage, sizeof(UnsignedByte))) {
		return NULL;
	}

	NetProgressCommandMsg *msg = newInstance(NetProgressCommandMsg);
	msg->setPercentage(percentage);

	return msg;
}

NetCommandMsg * NetPacket::readLoadCompleteMessage(NetPacketReader &reader) {
	NetCommandMsg *msg = newInstance(NetCommandMsg);
	return msg;
}

NetCommandMsg * NetPacket::readTimeOutGameStartMessage(NetPacketReader &reader) {
	NetCommandMsg *msg = newInstance(NetCommandMsg);
	return msg;
}

NetCommandMsg * NetPacket::readWrapperMessage(NetPacketReader &reader) {
	UnsignedShort wrappedCommandID = 0;
	UnsignedInt chunkNumber = 0;
	UnsignedInt numChunks = 0;
	UnsignedInt totalDataLength = 0;
	UnsignedInt dataLength = 0;
	UnsignedInt dataOffset = 0;

	reader.read(&wrappedCommandID, sizeof(wrappedCommandID));
	reader.read(&chunkNumber, sizeof(chunkNumber));
	reader.read(&numChunks, sizeof(numChunks));
	reader.read(&totalDataLength, sizeof(totalDataLength));
	reader.read(&dataLength, sizeof(dataLength));
	reader.read(&dataOffset, sizeof(dataOffset));
	if (reader.hasFailed()) {
		return NULL;
	}

	DEBUG_LOG(("NetPacket::readWrapperMessage - wrapped command ID == %d\n", wrappedCommandID));
	DEBUG_LOG(("NetPacket::readWrapperMessage - chunk number = %d\n", chunkNumber));
	DEBUG_LOG(("NetPacket::readWrapperMessage - number of chunks = %d\n", numChunks));
	DEBUG_LOG(("NetPacket::readWrapperMessage - total data length = %d\n", totalDataLength));
	DEBUG_LOG(("NetPacket::readWrapperMessage - data length = %d\n", dataLength));
	DEBUG_LOG(("NetPacket::readWrapperMessage - data offset = %d\n", dataOffset));

	// Every chunk carries at least one byte and no more than a packet's worth of data.
	if (totalDataLength == 0 || totalDataLength > MAX_WRAPPED_COMMAND_DATA_LENGTH
		|| numChunks == 0 || numChunks > totalDataLength || chunkNumber >= numChunks
		|| dataLength > MAX_PACKET_SIZE
		|| dataOffset > totalDataLength || dataLength > totalDataLength - dataOffset) {
		DEBUG_LOG(("NetPacket::readWrapperMessage - invalid wrapper chunk, dropping\n"));
		reader.fail();
		return NULL;
	}

	UnsignedByte *chunkData = reader.readInPlace(dataLength);
	if (chunkData == NULL) {
		return NULL;
	}

	NetWrapperCommandMsg *msg = newInstance(NetWrapperCommandMsg);
	msg->setWrappedCommandID(wrappedCommandID);
	msg->setChunkNumber(chunkNumber);
	msg->setNumChunks(numChunks);
	msg->setTotalDataLength(totalDataLength);
	msg->setDataOffset(dataOffset);
	msg->setData(chunkData, dataLength);

	return msg;
}

NetCommandMsg * NetPacket::readFileMessage(NetPacketReader &reader) {
	char filename[_MAX_PATH];
	if (!reader.readString(filename, sizeof(filename))) {
		return NULL;
	}

	UnsignedInt dataLength = 0;
	if (!reader.read(&dataLength, sizeof(dataLength))) {
		return NULL;
	}

	UnsignedByte *fileData = reader.readInPlace(dataLength);
	if (fileData == NULL) {
		return NULL;
	}

	NetFileCommandMsg *msg = newInstance(NetFileCommandMsg);
	msg->setPortableFilename(AsciiString(filename));	// it's transferred as a portable filename

	msg->setFileData(fileData, dataLength);

	return msg;
}

NetCommandMsg * NetPacket::readFileAnnounceMessage(NetPacketReader &reader) {
	char filename[_MAX_PATH];
	UnsignedShort fileID = 0;
	UnsignedByte playerMask = 0;
	if (!reader.readString(filename, sizeof(filename))
		|| !reader.read(&fileID, sizeof(fileID))
		|| !reader.read(&playerMask, sizeof(playerMask))) {
		return NULL;
	}

	NetFileAnnounceCommandMsg *msg = newInstance(NetFileAnnounceCommandMsg);
	msg->setPortableFilename(AsciiString(filename));	// it's transferred as a portable filename
	msg->setFileID(fileID);
	msg->setPlayerMask(playerMask);

	return msg;
}

NetCommandMsg * NetPacket::readFileProgressMessage(NetPacketReader &reader) {
	UnsignedShort fileID = 0;
	Int progress = 0;
	if (!reader.read(&fileID, sizeof(fileID)) || !reader.read(&progress, sizeof(progress))) {
		return NULL;
	}

	NetFileProgressCommandMsg *msg = newInstance(NetFileProgressCommandMsg);
	msg->setFileID(fileID);
	msg->setProgress(progress);

	return msg;
}

NetCommandMsg * NetPacket::readDisconnectFrameMessage(NetPacketReader &reader) {
	UnsignedInt disconnectFrame = 0;
	if (!reader.read(&disconnectFrame, sizeof(disconnectFrame))) {
		return NULL;
	}

	DEBUG_LOG(("NetPacket::readDisconnectFrameMessage - read disconnect frame for frame %d\n", disconnectFrame));

	NetDisconnectFrameCommandMsg *msg = newInstance(NetDisconnectFrameCommandMsg);
	msg->setDisconnectFrame(disconnectFrame);

	return msg;
}

NetCommandMsg * NetPacket::readDisconnectScreenOffMessage(NetPacketReader &reader) {
	UnsignedInt newFrame = 0;
	if (!reader.read(&newFrame, sizeof(newFrame))) {
		return NULL;
	}

	NetDisconnectScreenOffCommandMsg *msg = newInstance(NetDisconnectScreenOffCommandMsg);
	msg->setNewFrame(newFrame);

	return msg;
}

NetCommandMsg * NetPacket::readFrameResendRequestMessage(NetPacketReader &reader) {
	UnsignedInt frameToResend = 0;
	if (!reader.read(&frameToResend, sizeof(frameToResend))) {
		return NULL;
	}

	NetFrameResendRequestCommandMsg *msg = newInstance(NetFrameResendRequestCommandMsg);
	msg->setFrameToResend(frameToResend);

	return msg;
}

/**
 * Returns the number of commands in this packet.  Only valid if the packet is locally constructed.
 */
Int NetPacket::getNumCommands() {
	return m_numCommands;
}

/**
 * Returns the address that this packet is to be sent to.  Only valid if the packet is locally constructed.
 */
UnsignedInt NetPacket::getAddr() {
	return m_addr;
}

/**
 * Returns the port that this packet is to be sent to.  Only valid if the packet is locally constructed.
 */
UnsignedShort NetPacket::getPort() {
	return m_port;
}

/**
 * Returns the data of this packet.
 */
UnsignedByte * NetPacket::getData() {
	return m_packet;
}

/**
 * Returns the length of the packet.
 */
Int NetPacket::getLength() {
	return m_packetLen;
}

/**
 * Dumps the packet to the debug log file
 */
void NetPacket::dumpPacketToLog() {
	DEBUG_LOG(("NetPacket::dumpPacketToLog() - packet is %d bytes\n", m_packetLen));
	Int numLines = m_packetLen / 8;
	if ((m_packetLen % 8) != 0) {
		++numLines;
	}
	for (Int dumpindex = 0; dumpindex < numLines; ++dumpindex) {
		DEBUG_LOG(("\t%d\t", dumpindex*8));
		for (Int dumpindex2 = 0; (dumpindex2 < 8) && ((dumpindex*8 + dumpindex2) < m_packetLen); ++dumpindex2) {
			DEBUG_LOG(("%02x '%c' ", m_packet[dumpindex*8 + dumpindex2], m_packet[dumpindex*8 + dumpindex2]));
		}
		DEBUG_LOG(("\n"));
	}
	DEBUG_LOG(("End of packet dump\n"));
}
