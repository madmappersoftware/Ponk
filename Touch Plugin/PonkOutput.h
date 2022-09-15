#pragma once

#include "DatagramSocket/DatagramSocket.h"
#include "PonkDefs.h"

#include "SOP_CPlusPlusBase.h"
#include <string>

#include <vector>
#include <map>
#include <string>
#include "matrix.h"


// To get more help about these functions, look at SOP_CPlusPlusBase.h
class PonkOutput : public SOP_CPlusPlusBase
{
public:

	PonkOutput(const OP_NodeInfo* info);

	virtual ~PonkOutput();

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

private:
	void push16bits(std::vector<unsigned char>& fullData, unsigned short value);
    void push32bits(std::vector<unsigned char>& fullData, int value);
    void pushFloat32(std::vector<unsigned char>& fullData, float value);
	void pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], int value);
	void pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], float value);
    void pushPoint_XYRGB_U16(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor);
    void pushPoint_XY_F32_RGB_U8(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor);
	bool validatePrimitiveDat(const OP_DATInput* primitive, int numPrimitive);
	std::map<std::string, float> getMetadata(const OP_DATInput* primitive, int primitiveIndex);

	Matrix44<double> buildCameraTransProjMatrix(const OP_Inputs* inputs);

	// We don't need to store this pointer, but we do for the example.
	// The OP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const OP_NodeInfo*		myNodeInfo;

	// In this example this value will be incremented each time the execute()
	// function is called, then passes back to the SOP
	int32_t					myExecuteCount;


	double					myOffset;
	std::string             myChopChanName;
	float                   myChopChanVal;
	std::string             myChop;

	std::string             myDat;

	int						myNumVBOTexLayers;

	DatagramSocket* socket;

	double animTime = 0;
	unsigned char frameNumber = 0;
};
