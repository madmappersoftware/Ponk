#pragma once

#include "DatagramSocket/DatagramSocket.h"
#include "PonkDefs.h"
#include "SOP_CPlusPlusBase.h"

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>

using namespace TD;

struct ReceivedPoint
{
	float x, y;
	float r, g, b;
};

struct ReceivedPath
{
	std::vector<ReceivedPoint> points;
	std::unordered_map<std::string, float> metadata;
};

struct SenderFrame
{
	std::string senderName;
	unsigned int senderIdentifier = 0;
	std::vector<ReceivedPath> paths;
};

class PonkInput : public SOP_CPlusPlusBase
{
public:

	PonkInput(const OP_NodeInfo* info);
	virtual ~PonkInput();

	virtual void getGeneralInfo(SOP_GeneralInfo*, const OP_Inputs*, void* reserved1) override;
	virtual void execute(SOP_Output*, const OP_Inputs*, void* reserved) override;
	virtual void executeVBO(SOP_VBOOutput* output, const OP_Inputs* inputs, void* reserved) override;

	virtual int32_t getNumInfoCHOPChans(void* reserved) override;
	virtual void getInfoCHOPChan(int index, OP_InfoCHOPChan* chan, void* reserved) override;

	virtual bool getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved) override;
	virtual void getInfoDATEntries(int32_t index, int32_t nEntries,
									OP_InfoDATEntries* entries, void* reserved) override;

	virtual void setupParameters(OP_ParameterManager* manager, void* reserved) override;
	virtual void pulsePressed(const char* name, void* reserved) override;
	virtual void buildDynamicMenu(const OP_Inputs* inputs, OP_BuildDynamicMenuInfo* info, void* reserved1) override;

	virtual void getErrorString(OP_String* error, void* reserved) override;

private:

	void receiveThreadFunc();

	/// Parse a complete frame's data bytes into paths and store under m_mutex.
	void parseAndStoreFrame(unsigned int senderIdentifier,
							const char* senderName,
							const std::vector<unsigned char>& data);

	DatagramSocket* m_socket;

	std::thread m_receiveThread;
	std::atomic<bool> m_running{false};

	// Per-sender chunk assembly (only accessed from receive thread)
	struct ChunkAssembly
	{
		int frameNumber = -1;
		int chunkCount = -1;
		unsigned int dataCrc = 0;
		std::vector<bool> received;
		std::vector<std::vector<unsigned char>> chunks;
		char senderName[32] = {};

		ChunkAssembly()
		{
			received.resize(255, false);
			chunks.resize(255);
		}

		void reset()
		{
			int count = (chunkCount > 0) ? chunkCount : 0;
			for (int i = 0; i < count; i++)
			{
				received[i] = false;
				chunks[i].clear();
			}
			frameNumber = -1;
			chunkCount = -1;
			dataCrc = 0;
		}
	};

	std::unordered_map<unsigned int, ChunkAssembly> m_assemblies;

	// Protected by m_mutex: latest complete frame per sender
	std::mutex m_mutex;
	std::unordered_map<unsigned int, SenderFrame> m_latestFrames;

	std::string m_errorMessage;

	// Cached info for Info CHOP / DAT (snapshot taken in execute)
	int m_numSenders = 0;
	int m_numPaths = 0;
	int m_numPoints = 0;
	std::vector<std::pair<std::string, unsigned int>> m_senderList;
};
