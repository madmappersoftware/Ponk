#include "PonkInput.h"

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
	info->customOPInfo.opType->setString("Ponkinput");

	// Human-readable label shown in the OP Create Menu dialog
	info->customOPInfo.opLabel->setString("Ponk Input");

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
	return new PonkInput(info);
}

DLLEXPORT
void
DestroySOPInstance(SOP_CPlusPlusBase* instance)
{
	delete (PonkInput*)instance;
}

};


PonkInput::PonkInput(const OP_NodeInfo* info)
{
	m_socket = new DatagramSocket(INADDR_ANY, PONK_PORT);
	m_socket->joinMulticastGroup(PONK_MULTICAST_IP, INADDR_ANY);

	m_running = true;
	m_receiveThread = std::thread(&PonkInput::receiveThreadFunc, this);
}

PonkInput::~PonkInput()
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
PonkInput::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
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
PonkInput::receiveThreadFunc()
{
	unsigned char buffer[65536];

	while (m_running)
	{
		unsigned int bufferSize = static_cast<unsigned int>(sizeof(buffer));
		GenericAddr sourceAddr;

		if (!m_socket->recvFrom(sourceAddr, buffer, bufferSize))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		if (bufferSize == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		if (bufferSize < sizeof(GeomUdpHeader))
			continue;

		const GeomUdpHeader* header = reinterpret_cast<const GeomUdpHeader*>(buffer);

		if (strncmp(header->headerString, PONK_HEADER_STRING, 8) != 0)
			continue;

		if (header->protocolVersion > 0)
			continue;

		const unsigned int senderId = header->senderIdentifier;

		ChunkAssembly& asm_ = m_assemblies[senderId];

		// New frame from this sender?
		if (asm_.frameNumber != -1 && header->frameNumber != asm_.frameNumber)
			asm_.reset();

		if (asm_.frameNumber != -1 && asm_.chunkCount != header->chunkCount)
			asm_.reset();

		if (asm_.frameNumber != -1 && asm_.dataCrc != header->dataCrc)
			asm_.reset();

		asm_.frameNumber = header->frameNumber;
		asm_.chunkCount = header->chunkCount;
		asm_.dataCrc = header->dataCrc;
		memcpy(asm_.senderName, header->senderName, sizeof(asm_.senderName));

		if (header->chunkCount == 0)
			continue;

		if (header->chunkNumber >= header->chunkCount)
			continue;

		if (asm_.received[header->chunkNumber])
			continue;

		const unsigned int dataLength = bufferSize - sizeof(GeomUdpHeader);
		const unsigned int dataStart = sizeof(GeomUdpHeader);
		asm_.chunks[header->chunkNumber].assign(
			buffer + dataStart,
			buffer + dataStart + dataLength);
		asm_.received[header->chunkNumber] = true;

		// Check if all chunks have arrived
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

		// Concatenate all chunk data
		std::vector<unsigned char> allData;
		allData.reserve(255 * PONK_MAX_CHUNK_SIZE);
		for (int i = 0; i < header->chunkCount; i++)
			allData.insert(allData.end(), asm_.chunks[i].begin(), asm_.chunks[i].end());

		asm_.reset();

		// CRC check
		unsigned int computedCrc = 0;
		for (auto v : allData)
			computedCrc += v;

		if (computedCrc != header->dataCrc)
			continue;

		parseAndStoreFrame(senderId, header->senderName, allData);
	}
}


void
PonkInput::parseAndStoreFrame(unsigned int senderIdentifier,
							  const char* senderNameRaw,
							  const std::vector<unsigned char>& data)
{
	SenderFrame frame;
	frame.senderIdentifier = senderIdentifier;

	char nameBuf[33] = {};
	memcpy(nameBuf, senderNameRaw, 32);
	nameBuf[32] = '\0';
	frame.senderName = nameBuf;

	const size_t dataSize = data.size();
	unsigned int offset = 0;

	while (offset < dataSize)
	{
		// Data format byte
		if (dataSize < offset + 1)
			break;
		const unsigned char dataFormat = data[offset++];

		// Meta data count
		if (dataSize < offset + 1)
			break;
		const unsigned char metaCount = data[offset++];

		ReceivedPath path;

		// Read metadata
		if (dataSize < offset + 12u * metaCount)
			break;
		for (int m = 0; m < metaCount; m++)
		{
			std::string key(reinterpret_cast<const char*>(&data[offset]), 8);
			offset += 8;
			float value;
			memcpy(&value, &data[offset], sizeof(float));
			offset += 4;
			path.metadata[key] = value;
		}

		// Point count (16-bit LE)
		if (dataSize < offset + 2)
			break;
		const unsigned short pointCount =
			static_cast<unsigned short>(data[offset]) |
			(static_cast<unsigned short>(data[offset + 1]) << 8);
		offset += 2;

		// Bytes per point
		unsigned int bytesPerPoint = 0;
		if (dataFormat == PONK_DATA_FORMAT_XYRGB_U16)
			bytesPerPoint = 5 * sizeof(unsigned short);
		else if (dataFormat == PONK_DATA_FORMAT_XY_F32_RGB_U8)
			bytesPerPoint = 2 * sizeof(float) + 3;
		else
			break;

		if (dataSize < offset + static_cast<size_t>(pointCount) * bytesPerPoint)
			break;

		path.points.reserve(pointCount);

		for (int i = 0; i < pointCount; i++)
		{
			ReceivedPoint pt;

			if (dataFormat == PONK_DATA_FORMAT_XYRGB_U16)
			{
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

	std::lock_guard<std::mutex> lock(m_mutex);
	m_latestFrames[senderIdentifier] = std::move(frame);
}


// ---------------------------------------------------------------------------
// execute(): output received paths as SOP line geometry with colors
// ---------------------------------------------------------------------------

void
PonkInput::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	m_errorMessage.clear();

	if (!inputs->getParInt("Active"))
		return;

	// Read sender filter: "*" means all senders, otherwise it's a sender ID
	const char* senderParVal = inputs->getParString("Sender");
	const bool filterAll = (!senderParVal || strcmp(senderParVal, "*") == 0);
	unsigned int filterSenderId = 0;
	if (!filterAll && senderParVal)
		filterSenderId = static_cast<unsigned int>(std::strtoul(senderParVal, nullptr, 10));

	std::lock_guard<std::mutex> lock(m_mutex);

	m_numSenders = 0;
	m_numPaths = 0;
	m_numPoints = 0;
	m_senderList.clear();

	if (m_latestFrames.empty())
		return;

	// Always populate the sender list (for Info DAT / dynamic menu),
	// but only output geometry for matching senders.
	for (auto& kv : m_latestFrames)
		m_senderList.push_back({kv.second.senderName, kv.second.senderIdentifier});

	// Collect metadata keys and total point count for matching senders
	std::map<std::string, int> metaKeyIndex;
	int totalPoints = 0;

	for (auto& kv : m_latestFrames)
	{
		if (!filterAll && kv.first != filterSenderId)
			continue;

		const SenderFrame& frame = kv.second;

		for (auto& path : frame.paths)
		{
			totalPoints += static_cast<int>(path.points.size());
			for (auto& meta : path.metadata)
			{
				if (metaKeyIndex.find(meta.first) == metaKeyIndex.end())
					metaKeyIndex[meta.first] = static_cast<int>(metaKeyIndex.size());
			}
		}
	}

	if (totalPoints == 0)
		return;

	// Prepare metadata float arrays (one value per point for each meta key)
	std::vector<std::vector<float>> metaArrays(metaKeyIndex.size());
	for (auto& arr : metaArrays)
		arr.resize(totalPoints, 0.0f);

	int pointIndex = 0;

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

			int firstPtIdx = pointIndex;

			for (auto& pt : path.points)
			{
				Position pos(pt.x, pt.y, 0.0f);
				output->addPoint(pos);

				Color col(pt.r, pt.g, pt.b, 1.0f);
				output->setColor(col, pointIndex);

				pointIndex++;
			}

			m_numPoints += static_cast<int>(path.points.size());

			// Fill metadata arrays for this path's points
			for (auto& meta : path.metadata)
			{
				int idx = metaKeyIndex[meta.first];
				for (int p = firstPtIdx; p < pointIndex; p++)
					metaArrays[idx][p] = meta.second;
			}

			// Create a line primitive from this path's points
			std::vector<int32_t> indices(path.points.size());
			for (size_t i = 0; i < path.points.size(); i++)
				indices[i] = firstPtIdx + static_cast<int32_t>(i);

			output->addLine(indices.data(), static_cast<int32_t>(indices.size()));
		}
	}

	// Set custom attributes for metadata
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

	// Set a reasonable bounding box for the -1..1 coordinate range
	BoundingBox bbox(-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	output->setBoundingBox(bbox);
}


void
PonkInput::executeVBO(SOP_VBOOutput* output, const OP_Inputs* inputs, void* reserved)
{
}


// ---------------------------------------------------------------------------
// Info CHOP
// ---------------------------------------------------------------------------

int32_t
PonkInput::getNumInfoCHOPChans(void* reserved)
{
	return 3;
}

void
PonkInput::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved)
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
PonkInput::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 1 + static_cast<int32_t>(m_senderList.size());
	infoSize->cols = 2;
	infoSize->byColumn = false;
	return true;
}

void
PonkInput::getInfoDATEntries(int32_t index, int32_t nEntries,
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
PonkInput::setupParameters(OP_ParameterManager* manager, void* reserved)
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
}

void
PonkInput::pulsePressed(const char* name, void* reserved)
{
}

void
PonkInput::buildDynamicMenu(const OP_Inputs* inputs, OP_BuildDynamicMenuInfo* info, void* reserved1)
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
PonkInput::getErrorString(OP_String* error, void* reserved)
{
	error->setString(m_errorMessage.c_str());
}
