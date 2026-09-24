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

////// NetCommandWrapperList.cpp ////////////////////////////////
// Bryan Cleveland

#include "PreRTS.h"	// This must go first in EVERY cpp file int the GameEngine

#include "GameNetwork/NetCommandWrapperList.h"
#include "GameNetwork/NetPacket.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////// NetCommandWrapperListNode ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Upper bound on the size of a command that can be reassembled from wrapper chunks.
static const UnsignedInt MAX_WRAPPED_COMMAND_LENGTH = 8 * 1024 * 1024;

NetCommandWrapperListNode::NetCommandWrapperListNode(NetWrapperCommandMsg *msg) 
{
	//Added By Sadullah Nader
	//Initializations inserted
	m_next = NULL;

	//

	m_numChunks = 0;
	m_numChunksPresent = 0;
	m_chunksPresent = NULL;
	m_dataLength = 0;
	m_data = NULL;
	m_isValid = FALSE;
	m_commandID = msg->getWrappedCommandID();

	UnsignedInt numChunks = msg->getNumChunks();
	UnsignedInt dataLength = msg->getTotalDataLength();

	// These fields come straight off the wire, so reject anything that could not have been
	// produced by NetPacket::ConstructBigCommandPacketList before allocating from them.  Every
	// chunk holds at least one byte and at most MAX_PACKET_SIZE bytes of the wrapped command.
	if ((numChunks == 0) || (dataLength == 0) || (dataLength > MAX_WRAPPED_COMMAND_LENGTH) ||
			(numChunks > dataLength) || (dataLength > (numChunks * (UnsignedInt)MAX_PACKET_SIZE))) {
		DEBUG_LOG(("NetCommandWrapperListNode - rejecting wrapper command %d with %d chunks and total length %d\n",
			m_commandID, numChunks, dataLength));
		return;
	}

	m_numChunks = numChunks;
	m_chunksPresent = NEW Bool[m_numChunks];	// pool[]ify

	for (UnsignedInt i = 0; i < m_numChunks; ++i) {
		m_chunksPresent[i] = FALSE;
	}

	m_dataLength = dataLength;
	m_data = NEW UnsignedByte[m_dataLength];	// pool[]ify

	m_isValid = TRUE;
}

NetCommandWrapperListNode::~NetCommandWrapperListNode() {
	if (m_chunksPresent != NULL) {
		delete[] m_chunksPresent;
		m_chunksPresent = NULL;
	}

	if (m_data != NULL) {
		delete[] m_data;
		m_data = NULL;
	}
}

Bool NetCommandWrapperListNode::isComplete() {
	return m_isValid && (m_numChunksPresent == m_numChunks);
}

Bool NetCommandWrapperListNode::isValid() {
	return m_isValid;
}

Int NetCommandWrapperListNode::getPercentComplete(void) {
	if (!m_isValid)
		return 0;

	if (isComplete())
		return 100;
	else
		return min(99, REAL_TO_INT( ((Real)m_numChunksPresent)/((Real)m_numChunks)*100.0f ));
}

UnsignedShort NetCommandWrapperListNode::getCommandID() {
	return m_commandID;
}

UnsignedInt NetCommandWrapperListNode::getRawDataLength() {
	return m_dataLength;
}

void NetCommandWrapperListNode::copyChunkData(NetWrapperCommandMsg *msg) {
	if (msg == NULL) {
		DEBUG_CRASH(("Trying to copy data from a non-existent wrapper command message"));
		return;
	}

	if (!m_isValid) {
		return;
	}

	DEBUG_ASSERTCRASH(msg->getChunkNumber() < m_numChunks, ("MunkeeChunk %d of %d\n",
		msg->getChunkNumber(), m_numChunks));
	if (msg->getChunkNumber() >= m_numChunks)
		return;

	// The offset and length are attacker controllable, so make sure the chunk lands entirely
	// inside the reassembly buffer.  Written so it cannot overflow the arithmetic.
	UnsignedInt offset = msg->getDataOffset();
	UnsignedInt length = msg->getDataLength();
	if ((length > m_dataLength) || (offset > (m_dataLength - length))) {
		DEBUG_LOG(("NetCommandWrapperListNode::copyChunkData() - rejecting chunk %d at offset %d of length %d, buffer is %d bytes\n",
			msg->getChunkNumber(), offset, length, m_dataLength));
		return;
	}

	DEBUG_LOG(("NetCommandWrapperListNode::copyChunkData() - copying chunk %d\n",
		msg->getChunkNumber()));

	if (m_chunksPresent[msg->getChunkNumber()] == TRUE) {
		// we already received this chunk, no need to recopy it.
		return;
	}

	m_chunksPresent[msg->getChunkNumber()] = TRUE;
	memcpy(m_data + offset, msg->getData(), length);
	++m_numChunksPresent;
}

UnsignedByte * NetCommandWrapperListNode::getRawData() {
	return m_data;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////// NetCommandWrapperList ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

NetCommandWrapperList::NetCommandWrapperList() {
	m_list = NULL;
}

NetCommandWrapperList::~NetCommandWrapperList() {
	NetCommandWrapperListNode *temp;
	while (m_list != NULL) {
		temp = m_list->m_next;
		m_list->deleteInstance();
		m_list = temp;
	}
}

void NetCommandWrapperList::init() {
	m_list = NULL;
}

void NetCommandWrapperList::reset() {
	NetCommandWrapperListNode *temp;
	while (m_list != NULL) {
		temp = m_list->m_next;
		m_list->deleteInstance();
		m_list = temp;
	}
}

Int NetCommandWrapperList::getPercentComplete(UnsignedShort wrappedCommandID)
{
	NetCommandWrapperListNode *temp = m_list;

	while ((temp != NULL) && (temp->getCommandID() != wrappedCommandID)) {
		temp = temp->m_next;
	}

	if (!temp)
		return 0;

	return temp->getPercentComplete();
}

void NetCommandWrapperList::processWrapper(NetCommandRef *ref) {
	NetCommandWrapperListNode *temp = m_list;
	NetWrapperCommandMsg *msg = (NetWrapperCommandMsg *)(ref->getCommand());

	while ((temp != NULL) && (temp->getCommandID() != msg->getWrappedCommandID())) {
		temp = temp->m_next;
	}

	if (temp == NULL) {
		temp = newInstance(NetCommandWrapperListNode)(msg);
		temp->m_next = m_list;
		m_list = temp;
	}

	temp->copyChunkData(msg);
}

NetCommandList * NetCommandWrapperList::getReadyCommands() 
{
	NetCommandList *retlist = newInstance(NetCommandList);
	retlist->init();

	NetCommandWrapperListNode *temp = m_list;
	NetCommandWrapperListNode *next = NULL;

	while (temp != NULL) {
		next = temp->m_next;
		if (temp->isComplete()) {
			NetCommandRef *msg = NetPacket::ConstructNetCommandMsgFromRawData(temp->getRawData(), temp->getRawDataLength());
			NetCommandRef *ret = retlist->addMessage(msg->getCommand());
			ret->setRelay(msg->getRelay());

			msg->deleteInstance();
			msg = NULL;

			removeFromList(temp);
			temp = NULL;
		}
		temp = next;
	}

	return retlist;
}

void NetCommandWrapperList::removeFromList(NetCommandWrapperListNode *node) {
	if (node == NULL) {
		return;
	}

	NetCommandWrapperListNode *temp = m_list;
	NetCommandWrapperListNode *prev = NULL;

	while ((temp != NULL) && (temp->getCommandID() != node->getCommandID())) {
		prev = temp;
		temp = temp->m_next;
	}

	if (temp == NULL) {
		return;
	}

	if (prev == NULL) {
		m_list = temp->m_next;
		temp->deleteInstance();
		temp = NULL;
	} else {
		prev->m_next = temp->m_next;
		temp->deleteInstance();
		temp = NULL;
	}
}
