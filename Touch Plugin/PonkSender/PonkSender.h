/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#pragma once

#include "DatagramSocket/DatagramSocket.h"
#include "PonkDefs.h"

#include "SOP_CPlusPlusBase.h"
#include <string>

#include <vector>
#include <map>
#include <string>
#include <chrono>
#include "matrix.h"

using namespace TD;


// To get more help about these functions, look at SOP_CPlusPlusBase.h
class PonkSender : public SOP_CPlusPlusBase
{
public:

	PonkSender(const OP_NodeInfo* info);

	virtual ~PonkSender();

	virtual void getGeneralInfo(SOP_GeneralInfo*, const OP_Inputs*, void* reserved1) override;

	virtual void execute(SOP_Output*, const OP_Inputs*, void* reserved) override;


	virtual void executeVBO(SOP_VBOOutput* output, const OP_Inputs* inputs,
							void* reserved) override;


	virtual int32_t getNumInfoCHOPChans(void* reserved) override;

	virtual void getInfoCHOPChan(int index, OP_InfoCHOPChan* chan, void* reserved) override;

	virtual bool getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved) override;

	virtual void getInfoDATEntries(int32_t index, int32_t nEntries,
									OP_InfoDATEntries* entries,
									void* reserved) override;

	virtual void setupParameters(OP_ParameterManager* manager, void* reserved) override;
	virtual void pulsePressed(const char* name, void* reserved) override;

	virtual void getErrorString(OP_String* error, void* reserved) override;

private:
	void push16bits(std::vector<unsigned char>& fullData, unsigned short value);
    void push32bits(std::vector<unsigned char>& fullData, int value);
    void pushFloat32(std::vector<unsigned char>& fullData, float value);
	void pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], int value);
	void pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], float value);
    //void pushPoint_XYRGB_U16(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor);
    void pushPoint_XY_F32_RGB_U8(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor);
	std::map<std::string, float*> getMetadata(const OP_SOPInput* sinput);

	Matrix44<double> buildCameraTransProjMatrix(const OP_Inputs* inputs);

	DatagramSocket* socket;

	std::vector<unsigned char> fullData;
	std::vector<unsigned char> packet;

	/// PONK frame counter; wraps at 256 (protocol uses 8-bit field).
	unsigned char frameNumber = 0;

	/// Current error message; when non-empty, the node is in error state.
	std::string m_errorMessage;

	const OP_NodeInfo* myNodeInfo;

	bool m_firstExecute = true;

	int m_lastUid = -1;
	std::chrono::steady_clock::time_point m_uidChangeTime;
	bool m_uidJustChanged = false;
};
