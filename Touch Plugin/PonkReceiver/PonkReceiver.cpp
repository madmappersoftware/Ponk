#include "PonkReceiver.h"

#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <chrono>

extern "C"
{

DLLEXPORT
void
FillSOPPluginInfo(SOP_PluginInfo *info)
{
	if (!info->setAPIVersion(SOPCPlusPlusAPIVersion))
		return;

	// Unique type name: must start with upper case, rest lower case / digits
	info->customOPInfo.opType->setString("Ponkreceiver");

	// Human-readable label shown in the OP Create Menu dialog
	info->customOPInfo.opLabel->setString("Ponk Receiver");

	// Three-letter icon on the node
	info->customOPInfo.opIcon->setString("PKI");

	info->customOPInfo.authorName->setString("Tyrell");
	info->customOPInfo.authorEmail->setString("colas@tyrell.studio");

	// No SOP inputs required; this node generates geometry from the network
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 0;

	// Kick-start cooking so the receive thread can populate geometry
	info->customOPInfo.cookOnStart = true;
}

DLLEXPORT
SOP_CPlusPlusBase*
CreateSOPInstance(const OP_NodeInfo* info)
{
	return new PonkReceiver(info);
}

DLLEXPORT
void
DestroySOPInstance(SOP_CPlusPlusBase* instance)
{
	delete (PonkReceiver*)instance;
}

};


PonkReceiver::PonkReceiver(const OP_NodeInfo* info)
{
	m_socket = new DatagramSocket(INADDR_ANY, PONK_PORT);
	m_socket->joinMulticastGroup(PONK_MULTICAST_IP, INADDR_ANY);

	m_running = true;
	m_receiveThread = std::thread(&PonkReceiver::receiveThreadFunc, this);
}

PonkReceiver::~PonkReceiver()
{
	m_running = false;

	if (m_receiveThread.joinable())
		m_receiveThread.join();

	if (m_socket)
	{
		m_socket->leaveMulticastGroup(PONK_MULTICAST_IP, INADDR_ANY);
		delete m_socket;
		m_socket = nullptr;
	}
}

void
PonkReceiver::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	ginfo->cookEveryFrame = true;
	ginfo->cookEveryFrameIfAsked = false;
	ginfo->directToGPU = false;
	ginfo->winding = SOP_Winding::CCW;
}


// ---------------------------------------------------------------------------
// Receive thread: listens for UDP packets, assembles chunks, parses frames
// ---------------------------------------------------------------------------

void
PonkReceiver::receiveThreadFunc()
{
	unsigned char buffer[65536];

	while (m_running)
	{
		unsigned int bufferSize = static_cast<unsigned int>(sizeof(buffer));
		GenericAddr sourceAddr;

		// The socket is non-blocking: recvFrom returns immediately.
		// On error (returns false) or when no data is available (bufferSize == 0),
		// sleep briefly to avoid burning CPU in a busy-wait loop.
		if (!m_socket->recvFrom(sourceAddr, buffer, bufferSize))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		if (bufferSize == 0)
		{
			// No data available yet, yield and try again.
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		// Packet must be at least large enough to hold a full header.
		if (bufferSize < sizeof(GeomUdpHeader))
			continue;

		const GeomUdpHeader* header = reinterpret_cast<const GeomUdpHeader*>(buffer);

		// Validate the magic string to confirm this is a PONK packet.
		if (strncmp(header->headerString, PONK_HEADER_STRING, 8) != 0)
			continue;

		// Reject packets from a protocol version we don't support.
		// Version 0 is the only one defined; newer breaking versions will have a higher number.
		if (header->protocolVersion > 0)
			continue;

		// Each sender is tracked independently, identified by its 32-bit sender ID.
		const unsigned int senderId = header->senderIdentifier;

		// Look up (or create) the chunk assembly state for this sender.
		ChunkAssembly& asm_ = m_assemblies[senderId];

		// If we have an in-progress assembly for this sender, check whether the
		// incoming packet belongs to the same frame. Any mismatch (new frame number,
		// different chunk count, or different CRC) means the previous frame was
		// either lost or superseded — discard it and start fresh.
		if (asm_.frameNumber != -1 && header->frameNumber != asm_.frameNumber)
			asm_.reset();

		if (asm_.frameNumber != -1 && asm_.chunkCount != header->chunkCount)
			asm_.reset();

		if (asm_.frameNumber != -1 && asm_.dataCrc != header->dataCrc)
			asm_.reset();

		// Record the frame metadata from this packet's header.
		asm_.frameNumber = header->frameNumber;
		asm_.chunkCount = header->chunkCount;
		asm_.dataCrc = header->dataCrc;
		memcpy(asm_.senderName, header->senderName, sizeof(asm_.senderName));

		// Sanity checks on chunk indices before storing.
		if (header->chunkCount == 0)
			continue;

		if (header->chunkNumber >= header->chunkCount)
			continue;

		// Skip duplicate chunks (can happen with multicast retransmission).
		if (asm_.received[header->chunkNumber])
			continue;

		// Copy this chunk's payload (everything after the header) into the assembly buffer.
		const unsigned int dataLength = bufferSize - sizeof(GeomUdpHeader);
		const unsigned int dataStart = sizeof(GeomUdpHeader);
		asm_.chunks[header->chunkNumber].assign(
			buffer + dataStart,
			buffer + dataStart + dataLength);
		asm_.received[header->chunkNumber] = true;

		// Check if all chunks have arrived.
		bool complete = true;
		for (int i = 0; i < header->chunkCount; i++)
		{
			if (!asm_.received[i])
			{
				complete = false;
				break;
			}
		}

		if (!complete)
			continue;

		// All chunks are in — concatenate them in order to rebuild the full frame payload.
		std::vector<unsigned char> allData;
		allData.reserve(static_cast<size_t>(header->chunkCount) * PONK_MAX_CHUNK_SIZE);
		for (int i = 0; i < header->chunkCount; i++)
			allData.insert(allData.end(), asm_.chunks[i].begin(), asm_.chunks[i].end());

		// Release the assembly slot so it's ready for the next frame from this sender.
		asm_.reset();

		// Validate integrity: the CRC is a simple byte sum over the entire payload.
		// If it doesn't match, the frame was corrupted in transit — discard it.
		unsigned int computedCrc = 0;
		for (auto v : allData)
			computedCrc += v;

		if (computedCrc != header->dataCrc)
			continue;

		// Frame is complete and valid — parse paths and store for the main thread to consume.
		parseAndStoreFrame(senderId, header->senderName, allData);
	}
}


void
PonkReceiver::parseAndStoreFrame(unsigned int senderIdentifier,
							  const char* senderNameRaw,
							  const std::vector<unsigned char>& data)
{
	SenderFrame frame;
	frame.senderIdentifier = senderIdentifier;

	// The sender name is a fixed 32-byte field, not necessarily null-terminated within
	// those 32 bytes, so we copy into a 33-byte buffer and force a terminator.
	char nameBuf[33] = {};
	memcpy(nameBuf, senderNameRaw, 32);
	nameBuf[32] = '\0';
	frame.senderName = nameBuf;

	const size_t dataSize = data.size();
	unsigned int offset = 0;

	// Each iteration of this loop parses one path from the payload.
	// The binary layout per path is:
	//   [1 byte] data format
	//   [1 byte] metadata count
	//   [N x 12 bytes] metadata entries (8-byte key + 4-byte float value)
	//   [2 bytes LE] point count
	//   [pointCount x bytesPerPoint] point data
	// We break out early if the remaining data is too short (truncated/corrupt frame).
	while (offset < dataSize)
	{
		// Data format selects how each point is encoded.
		if (dataSize < offset + 1)
			break;
		const unsigned char dataFormat = data[offset++];

		// Number of metadata key-value pairs attached to this path.
		if (dataSize < offset + 1)
			break;
		const unsigned char metaCount = data[offset++];

		ReceivedPath path;

		// Each metadata entry is 12 bytes: an 8-character EightCC key and a 4-byte float.
		// Keys shorter than 8 characters are padded with null bytes on the sender side;
		// we trim trailing nulls so the stored key matches its natural length.
		if (dataSize < offset + 12u * metaCount)
			break;
		for (int m = 0; m < metaCount; m++)
		{
			const char* keyPtr = reinterpret_cast<const char*>(&data[offset]);
			int keyLen = 8;
			while (keyLen > 0 && keyPtr[keyLen - 1] == '\0') --keyLen;
			std::string key(keyPtr, keyLen);
			offset += 8;
			float value;
			memcpy(&value, &data[offset], sizeof(float));
			offset += 4;
			path.metadata[key] = value;
		}

		// Point count is a 16-bit little-endian unsigned integer.
		if (dataSize < offset + 2)
			break;
		const unsigned short pointCount =
			static_cast<unsigned short>(data[offset]) |
			(static_cast<unsigned short>(data[offset + 1]) << 8);
		offset += 2;

		// Determine the stride (bytes per point) based on the data format.
		// Unknown formats are not supported — skip the rest of this frame.
		unsigned int bytesPerPoint = 0;
		if (dataFormat == PONK_DATA_FORMAT_XYRGB_U16)
			bytesPerPoint = 5 * sizeof(unsigned short);  // X, Y, R, G, B — each 16 bits
		else if (dataFormat == PONK_DATA_FORMAT_XY_F32_RGB_U8)
			bytesPerPoint = 2 * sizeof(float) + 3;       // X, Y as float32; R, G, B as uint8
		else
			break;

		// Guard against a point count that would read past the end of the buffer.
		if (dataSize < offset + static_cast<size_t>(pointCount) * bytesPerPoint)
			break;

		path.points.reserve(pointCount);

		for (int i = 0; i < pointCount; i++)
		{
			ReceivedPoint pt;

			if (dataFormat == PONK_DATA_FORMAT_XYRGB_U16)
			{
				// All five components are 16-bit unsigned integers in little-endian order.
				// X and Y are mapped from [0, 65535] to [-1, +1].
				// R, G, B are mapped from [0, 65535] to [0, 1].
				auto read16 = [&]() -> unsigned short {
					unsigned short v = static_cast<unsigned short>(data[offset]) |
									   (static_cast<unsigned short>(data[offset + 1]) << 8);
					offset += 2;
					return v;
				};
				unsigned short x16 = read16();
				unsigned short y16 = read16();
				unsigned short r16 = read16();
				unsigned short g16 = read16();
				unsigned short b16 = read16();

				pt.x = -1.0f + 2.0f * (x16 / 65535.0f);
				pt.y = -1.0f + 2.0f * (y16 / 65535.0f);
				pt.r = r16 / 65535.0f;
				pt.g = g16 / 65535.0f;
				pt.b = b16 / 65535.0f;
			}
			else if (dataFormat == PONK_DATA_FORMAT_XY_F32_RGB_U8)
			{
				// X and Y are 32-bit floats (already in [-1, +1] or any float range).
				// R, G, B are single bytes mapped from [0, 255] to [0, 1].
				memcpy(&pt.x, &data[offset], sizeof(float));
				offset += sizeof(float);
				memcpy(&pt.y, &data[offset], sizeof(float));
				offset += sizeof(float);
				pt.r = data[offset++] / 255.0f;
				pt.g = data[offset++] / 255.0f;
				pt.b = data[offset++] / 255.0f;
			}

			path.points.push_back(pt);
		}

		frame.paths.push_back(std::move(path));
	}

	// Publish the parsed frame under the lock so execute() on the main thread
	// can safely read it. Replaces any previously stored frame for this sender.
	std::lock_guard<std::mutex> lock(m_mutex);
	m_latestFrames[senderIdentifier] = std::move(frame);
}


// ---------------------------------------------------------------------------
// execute(): output received paths as SOP line geometry with colors
// ---------------------------------------------------------------------------

void
PonkReceiver::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	m_errorMessage.clear();

	if (!inputs->getParInt("Active"))
		return;

	// The Sender parameter is either "*" (all senders) or the string representation
	// of a specific sender's 32-bit identifier. We parse it once here and use
	// filterAll / filterSenderId throughout to decide which frames to output.
	const char* senderParVal = inputs->getParString("Sender");
	const bool filterAll = (!senderParVal || strcmp(senderParVal, "*") == 0);
	unsigned int filterSenderId = 0;
	if (!filterAll && senderParVal)
		filterSenderId = static_cast<unsigned int>(std::strtoul(senderParVal, nullptr, 10));

	// Hold the mutex for the entire cook so the receive thread cannot overwrite
	// m_latestFrames while we are reading it.
	std::lock_guard<std::mutex> lock(m_mutex);

	// Reset per-cook stats; they will be repopulated below.
	m_numSenders = 0;
	m_numPaths = 0;
	m_numPoints = 0;
	m_senderList.clear();

	if (m_latestFrames.empty())
		return;

	// --- Pass 1: build the sender list (used by Info DAT and the dynamic menu)
	// and, for senders that pass the filter, tally the total point count and
	// collect the union of all metadata keys across all paths.
	//
	// metaKeyIndex maps each unique metadata key to a dense integer index so we
	// can address the flat metaArrays storage below without a string lookup per point.
	std::unordered_map<std::string, int> metaKeyIndex;
	int totalPoints = 0;

	for (auto& kv : m_latestFrames)
	{
		// Always add every known sender to the list regardless of the filter,
		// so the Info DAT and the Sender drop-down stay up to date.
		m_senderList.push_back({kv.second.senderName, kv.second.senderIdentifier});

		if (!filterAll && kv.first != filterSenderId)
			continue;

		const SenderFrame& frame = kv.second;

		for (auto& path : frame.paths)
		{
			totalPoints += static_cast<int>(path.points.size());

			// try_emplace does nothing if the key already exists, so each unique
			// metadata key gets a stable index assigned on its first occurrence.
			for (auto& meta : path.metadata)
				metaKeyIndex.try_emplace(meta.first, static_cast<int>(metaKeyIndex.size()));
		}
	}

	if (totalPoints == 0)
		return;

	// --- Allocate metadata storage ---
	// One flat float array per metadata key, sized to totalPoints.
	// Points belonging to paths that don't carry a given key default to 0.
	std::vector<std::vector<float>> metaArrays(metaKeyIndex.size());
	for (auto& arr : metaArrays)
		arr.resize(totalPoints, 0.0f);

	// indices is reused for every path to avoid a per-path heap allocation.
	int pointIndex = 0;
	std::vector<int32_t> indices;

	// --- Pass 2: emit geometry ---
	// For each matching sender and each of its paths, we:
	//   1. Add all points and their colors to the SOP output.
	//   2. Write this path's metadata values into the correct rows of metaArrays.
	//   3. Register a line primitive connecting all the path's points in order.
	for (auto& kv : m_latestFrames)
	{
		if (!filterAll && kv.first != filterSenderId)
			continue;

		const SenderFrame& frame = kv.second;
		m_numSenders++;

		for (auto& path : frame.paths)
		{
			if (path.points.empty())
				continue;

			m_numPaths++;

			// Remember where this path's points start in the global point list
			// so we can build the line index array and fill metadata correctly.
			int firstPtIdx = pointIndex;

			for (auto& pt : path.points)
			{
				// Z is always 0 — PONK is a 2D protocol.
				Position pos(pt.x, pt.y, 0.0f);
				output->addPoint(pos);

				// Alpha is always 1; the protocol carries RGB only.
				Color col(pt.r, pt.g, pt.b, 1.0f);
				output->setColor(col, pointIndex);

				pointIndex++;
			}

			m_numPoints += static_cast<int>(path.points.size());

			// Each metadata key has a single float value for the whole path,
			// so we broadcast it to every point in this path's range.
			for (auto& meta : path.metadata)
			{
				int idx = metaKeyIndex[meta.first];
				for (int p = firstPtIdx; p < pointIndex; p++)
					metaArrays[idx][p] = meta.second;
			}

			// Connect this path's points as an open polyline.
			indices.resize(path.points.size());
			for (size_t i = 0; i < path.points.size(); i++)
				indices[i] = firstPtIdx + static_cast<int32_t>(i);

			output->addLine(indices.data(), static_cast<int32_t>(indices.size()));
		}
	}

	// --- Publish metadata as custom SOP float attributes ---
	// Each key becomes a per-point float attribute named after the metadata key.
	// TD can then access these via the Attribute SOP or CHOP-based workflows.
	for (auto& kv : metaKeyIndex)
	{
		SOP_CustomAttribData attrib;
		attrib.name = kv.first.c_str();
		attrib.numComponents = 1;
		attrib.attribType = AttribType::Float;
		attrib.floatData = metaArrays[kv.second].data();
		attrib.intData = nullptr;
		output->setCustomAttribute(&attrib, totalPoints);
	}

	// The PONK coordinate space is [-1, +1] on both axes at Z=0.
	// A fixed bounding box lets TouchDesigner cull and frame the node correctly
	// without having to scan every point.
	BoundingBox bbox(-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	output->setBoundingBox(bbox);
}


void
PonkReceiver::executeVBO(SOP_VBOOutput* output, const OP_Inputs* inputs, void* reserved)
{
}


// ---------------------------------------------------------------------------
// Info CHOP
// ---------------------------------------------------------------------------

int32_t
PonkReceiver::getNumInfoCHOPChans(void* reserved)
{
	return 3;
}

void
PonkReceiver::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved)
{
	switch (index)
	{
	case 0:
		chan->name->setString("senders");
		chan->value = static_cast<float>(m_numSenders);
		break;
	case 1:
		chan->name->setString("paths");
		chan->value = static_cast<float>(m_numPaths);
		break;
	case 2:
		chan->name->setString("points");
		chan->value = static_cast<float>(m_numPoints);
		break;
	}
}


// ---------------------------------------------------------------------------
// Info DAT: table of connected senders
// ---------------------------------------------------------------------------

bool
PonkReceiver::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 1 + static_cast<int32_t>(m_senderList.size());
	infoSize->cols = 2;
	infoSize->byColumn = false;
	return true;
}

void
PonkReceiver::getInfoDATEntries(int32_t index, int32_t nEntries,
							 OP_InfoDATEntries* entries, void* reserved)
{
	if (index == 0)
	{
		entries->values[0]->setString("name");
		entries->values[1]->setString("id");
		return;
	}

	int senderIdx = index - 1;
	if (senderIdx >= 0 && senderIdx < static_cast<int>(m_senderList.size()))
	{
		entries->values[0]->setString(m_senderList[senderIdx].first.c_str());
		entries->values[1]->setString(std::to_string(m_senderList[senderIdx].second).c_str());
	}
}


// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

void
PonkReceiver::setupParameters(OP_ParameterManager* manager, void* reserved)
{
	// Active toggle (default ON for receiver)
	{
		OP_NumericParameter np;
		np.name = "Active";
		np.label = "Active";
		np.defaultValues[0] = 1;
		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// Sender selection (dynamic string menu populated in buildDynamicMenu)
	{
		OP_StringParameter sp;
		sp.name = "Sender";
		sp.label = "Sender";
		sp.defaultValue = "*";
		OP_ParAppendResult res = manager->appendDynamicStringMenu(sp);
		assert(res == OP_ParAppendResult::Success);
	}

	// Clear all received sender data
	{
		OP_NumericParameter np;
		np.name = "Refresh";
		np.label = "Refresh";
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}
}

void
PonkReceiver::pulsePressed(const char* name, void* reserved)
{
	if (strcmp(name, "Refresh") == 0)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_latestFrames.clear();
	}
}

void
PonkReceiver::buildDynamicMenu(const OP_Inputs* inputs, OP_BuildDynamicMenuInfo* info, void* reserved1)
{
	if (strcmp(info->name, "Sender") != 0)
		return;

	info->addMenuEntry("*", "All Senders");

	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& kv : m_latestFrames)
	{
		const SenderFrame& frame = kv.second;
		std::string idStr = std::to_string(frame.senderIdentifier);
		info->addMenuEntry(idStr.c_str(), frame.senderName.c_str());
	}
}

void
PonkReceiver::getErrorString(OP_String* error, void* reserved)
{
	error->setString(m_errorMessage.c_str());
}
