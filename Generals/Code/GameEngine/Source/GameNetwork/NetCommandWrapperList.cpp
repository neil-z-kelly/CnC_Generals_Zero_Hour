/*
**	Command & Conquer Generals(tm)
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
#include "GameNetwork/NetworkDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////// NetCommandWrapperListNode ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

NetCommandWrapperListNode::NetCommandWrapperListNode(NetWrapperCommandMsg *msg) 
{
	//Added By Sadullah Nader
	//Initializations inserted
	m_next = NULL;

	//

	m_numChunks = 0;
	m_chunksPresent = NULL;
	m_numChunksPresent = 0;
	m_dataLength = 0;
	m_data = NULL;
	m_commandID = msg->getWrappedCommandID();

	UnsignedInt numChunks = msg->getNumChunks();
	UnsignedInt dataLength = msg->getTotalDataLength();

	// These values come straight off the wire, so they describe the allocations
	// below only if they describe a command that could actually have been sent:
	// every chunk carries at least one byte of the wrapped command.
	if ((numChunks == 0) || (dataLength == 0) ||
			(dataLength > MAX_WRAPPER_TOTAL_DATA_LENGTH) || (numChunks > dataLength)) {
		DEBUG_LOG(("NetCommandWrapperListNode - rejecting wrapper command %d, %d chunks, %d bytes\n",
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

Bool NetCommandWrapperListNode::isValid() {
	return m_data != NULL;
}

Bool NetCommandWrapperListNode::isComplete() {
	return isValid() && (m_numChunksPresent == m_numChunks);
}

Int NetCommandWrapperListNode::getPercentComplete(void) {
	if (isComplete())
		return 100;
	else if (!isValid())
		return 0;
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

	if (!isValid()) {
		return;
	}

	// a chunk only belongs to this reassembly if it agrees with it about the shape
	// of the command being reassembled.
	if ((msg->getNumChunks() != m_numChunks) || (msg->getTotalDataLength() != m_dataLength)) {
		return;
	}

	DEBUG_ASSERTCRASH(msg->getChunkNumber() < m_numChunks, ("MunkeeChunk %d of %d\n",
		msg->getChunkNumber(), m_numChunks));
	if (msg->getChunkNumber() >= m_numChunks)
		return;

	// the offset and length are attacker controlled, so bound them against the
	// reassembly buffer without ever computing offset + length (which can wrap).
	UnsignedInt offset = msg->getDataOffset();
	UnsignedInt dataLength = msg->getDataLength();
	if ((offset > m_dataLength) || (dataLength > (m_dataLength - offset))) {
		DEBUG_LOG(("NetCommandWrapperListNode::copyChunkData() - dropping chunk %d, offset %d length %d doesn't fit in %d bytes\n",
			msg->getChunkNumber(), offset, dataLength, m_dataLength));
		return;
	}

	DEBUG_LOG(("NetCommandWrapperListNode::copyChunkData() - copying chunk %d\n",
		msg->getChunkNumber()));

	if (m_chunksPresent[msg->getChunkNumber()] == TRUE) {
		// we already received this chunk, no need to recopy it.
		return;
	}

	m_chunksPresent[msg->getChunkNumber()] = TRUE;
	if (dataLength > 0) {
		memcpy(m_data + offset, msg->getData(), dataLength);
	}
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
		if (!temp->isValid()) {
			// the wrapper described a command we're not willing to reassemble.
			temp->deleteInstance();
			return;
		}
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
