#include "PonkSender.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <iostream>

#ifdef _WIN32
	#include <Python.h>
	#include <structmember.h>
#else
	#include <Python/Python.h>
	#include <Python/structmember.h>
#endif

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

	DLLEXPORT
	void
	FillSOPPluginInfo(SOP_PluginInfo *info)
	{
		// Check to make sure the running TD version supports our API version.
		if (!info->setAPIVersion(SOPCPlusPlusAPIVersion))
			return;

		// The opType is the unique name for this TOP. It must start with a 
		// capital A-Z character, and all the following characters must lower case
		// or numbers (a-z, 0-9)
		info->customOPInfo.opType->setString("Ponksender");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("Ponk Sender");

		// Will be turned into a 3 letter icon on the nodes
		info->customOPInfo.opIcon->setString("PKS");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("Tyrell");
		info->customOPInfo.authorEmail->setString("colas@tyrell.studio");

		// This SOP works with 0 or 1 inputs
		info->customOPInfo.minInputs = 1;
		info->customOPInfo.maxInputs = 1;

		// Custom website URL that the Operator Help can point to
		info->customOPInfo.opHelpURL->setString("yourwebsiteurl.com");

		info->customOPInfo.pythonVersion->setString(PY_VERSION);
	}

	DLLEXPORT
	SOP_CPlusPlusBase*
	CreateSOPInstance(const OP_NodeInfo* info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per SOP that is using the .dll
		return new PonkSender(info);
	}

	DLLEXPORT
	void
	DestroySOPInstance(SOP_CPlusPlusBase* instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the SOP using that instance is deleted, or
		// if the SOP loads a different DLL
		delete (PonkSender*)instance;
	}

};


PonkSender::PonkSender(const OP_NodeInfo* info) : myNodeInfo(info)
{
	socket = new DatagramSocket(INADDR_ANY, 0);
}

PonkSender::~PonkSender()
{
	delete socket;
}

void
PonkSender::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = false;

	ginfo->cookEveryFrame = true;

	//if direct to GPU loading:
	ginfo->directToGPU = false;

}

void PonkSender::push16bits(std::vector<unsigned char>& fullData, unsigned short value) {
	fullData.push_back(static_cast<unsigned char>((value >> 0) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
}

void PonkSender::push32bits(std::vector<unsigned char>& fullData, int value) {
	fullData.push_back(static_cast<unsigned char>((value >> 0) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
}

void PonkSender::pushFloat32(std::vector<unsigned char>& fullData, float value) {
    push32bits(fullData,*reinterpret_cast<int*>(&value));
}

void PonkSender::pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], int value) {
	for (int i = 0; i < 8; i++) {
		fullData.push_back(eightCC[i]);
	}
	push32bits(fullData, value);
}
void PonkSender::pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], float value) {
	for (int i = 0; i < 8; i++) {
		fullData.push_back(eightCC[i]);
	}
	push32bits(fullData, *(int*)&value);
}

#define CLAMP_IN_ZERO_ONE(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

void PonkSender::pushPoint_XY_F32_RGB_U8(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor) {
    pushFloat32(fullData, pointPosition.x);
    pushFloat32(fullData, pointPosition.y);
    fullData.push_back(static_cast<unsigned char>(CLAMP_IN_ZERO_ONE(pointColor.r) * 255));
    fullData.push_back(static_cast<unsigned char>(CLAMP_IN_ZERO_ONE(pointColor.g) * 255));
    fullData.push_back(static_cast<unsigned char>(CLAMP_IN_ZERO_ONE(pointColor.b) * 255));
}

std::map<std::string, float*> PonkSender::getMetadata(const OP_SOPInput* sinput) {
	std::map<std::string, float*> metadata;
	
	// check how many metadata attribute the sop input contains	
	int numMetadata = sinput->getNumCustomAttributes();

	// get the metadata from the sop input
	for (int i = 0; i < numMetadata; i++) {
		const SOP_CustomAttribData* customAttribData = sinput->getCustomAttribute(i);
		if (!customAttribData->floatData)
			continue;
		std::string metadataName = customAttribData->name;
		metadata[metadataName] = customAttribData->floatData;
	}

	return metadata;
}

Matrix44<double>
PonkSender::buildCameraTransProjMatrix(const OP_Inputs* inputs)
{

	// get the camera object
	const OP_ObjectInput* camera = inputs->getParObject("Camera");

	Matrix44<double> cameraWorldTransform;
	// check if the parameter is set
	if (camera)
	{
		// it seems that the array returned by OP_ObjectInput->worldTransform
		// is not in the correct order therefore we need to reverse
		// it before creating our matrix
		// TODO: check if that really needed because i had to 
		// change the order of the Position Matrix multiplication
		// in multPositionMatrix member function of the matrix
		// class to compensate that 90 degress matrix rotation
		cameraWorldTransform = Matrix44<double>();
		for (int x = 0; x < 4; x++)
		{
			for (int y = 0; y < 4; y++)
			{
				cameraWorldTransform[x][y] = camera->worldTransform[x][y];
			}
		}
		cameraWorldTransform.invert();
	}

	// get the camera projection matrix
	Matrix44<double> cameraProjection;
	inputs->getParDouble4("Projectionmatrixa", cameraProjection[0][0],
		cameraProjection[0][1],
		cameraProjection[0][2],
		cameraProjection[0][3]);
	inputs->getParDouble4("Projectionmatrixb", cameraProjection[1][0],
		cameraProjection[1][1],
		cameraProjection[1][2],
		cameraProjection[1][3]);
	inputs->getParDouble4("Projectionmatrixc", cameraProjection[2][0],
		cameraProjection[2][1],
		cameraProjection[2][2],
		cameraProjection[2][3]);
	inputs->getParDouble4("Projectionmatrixd", cameraProjection[3][0],
		cameraProjection[3][1],
		cameraProjection[3][2],
		cameraProjection[3][3]);

	return cameraProjection * cameraWorldTransform;
}


void
PonkSender::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	m_errorMessage.clear();

	// Run Python code the first time the node executes
	// to set the expression on the camera matrix 
	if (m_firstExecute)
	{
		m_firstExecute = false;
		PyRun_SimpleString(""); // write your Python code here
	}

	// Disable the netaddress parameter if multicast is enabled
	if (inputs->getParInt("Multicast")) {
		inputs->enablePar("Netaddress", false);
	} else {
		inputs->enablePar("Netaddress", true);
	}
	
	if (!inputs->getParInt("Active")) {
		return;
	}

	// Get the sender name (ensure we don't overflow the buffer)
	char senderName[32];
	const char* raw = inputs->getParString("Sendername");
	if (raw) {
		strncpy(senderName, raw, 31);
		senderName[31] = '\0';
	} else {
		senderName[0] = '\0';
	}

	// Check if the sender name is empty
	if (senderName[0] == '\0') {
		m_errorMessage = "Sendername parameter is empty. Please enter a sender name.";
		return;
	}

	// Only run if a SOP is connected on the first input
	if (inputs->getNumInputs() > 0)
	{
		// Get the input sop
		const OP_SOPInput	*sinput = inputs->getInputSOP(0);

		// double check if the input sop is valid
		if (!sinput) {
			return;
		}

		// build the matrix to do the world space to screen projection
		Matrix44<double> cameraTransProj = buildCameraTransProjMatrix(inputs);

		// Clear the full data vector and reserve the maximum size
		fullData.clear();
		fullData.reserve(65536);

		const Position* ptArr = sinput->getPointPositions();
		const Color* colors = nullptr;

		if (sinput->hasColors()) {
			colors = sinput->getColors()->colors;
		}
		
		// get the metadata
		std::map<std::string, float*> metadata = getMetadata(sinput);

		static const Color s_white(1.0f, 1.0f, 1.0f, 1.0f);

		for (int primitiveNumber = 0; primitiveNumber < sinput->getNumPrimitives(); primitiveNumber++)
		{
			//std::cout << "-------------------- primitive : " << i << std::endl;

            // Write Format Data
            fullData.push_back(PONK_DATA_FORMAT_XY_F32_RGB_U8);

			// Write meta data count
			fullData.push_back(metadata.size());

			const SOP_PrimitiveInfo primInfo = sinput->getPrimitive(primitiveNumber);

			const int32_t* primVert = primInfo.pointIndices;

			int numPoints = primInfo.numVertices;

			for (const auto& kv : metadata) {
				char charMetadata[9];
				memset(charMetadata, 0, sizeof(charMetadata));
				size_t copyLen = std::min<size_t>(kv.first.size(), 8u);
				std::copy(kv.first.begin(), kv.first.begin() + copyLen, charMetadata);

				pushMetaData(fullData, charMetadata, kv.second[primVert[0]]);  
			}

			// check if the primitve is closed
			if (primInfo.isClosed) {
                // Write point count
                push16bits(fullData, numPoints+1);
            } else {
                push16bits(fullData, numPoints);
            }

            
                
			for (int pointNumber = 0; pointNumber < numPoints; pointNumber++) {
				Position pointPosition = cameraTransProj * ptArr[primVert[pointNumber]];
				pushPoint_XY_F32_RGB_U8(fullData, pointPosition, sinput->hasColors()?colors[primVert[pointNumber]]:s_white);
			}

			// If the primitive is close add the first point at the end
			if (primInfo.isClosed) {
				Position pointPosition = cameraTransProj * ptArr[primVert[0]];
                pushPoint_XY_F32_RGB_U8(fullData, pointPosition, sinput->hasColors()?colors[primVert[0]]:s_white);

			}
		}


		// Check if we don't reach the maximum number of chunck
        size_t chunksCount64 = 1 + fullData.size() / (PONK_MAX_CHUNK_SIZE-sizeof(GeomUdpHeader));
		if (chunksCount64 > 255) {
			m_errorMessage = "Protocol doesn't accept sending a packet "
			                 "that would be splitted in more than 255 chunks";
			return;
		}

		// Get the Unique identifier from the attribute
		int uid = inputs->getParInt("Uid");
		//std::cout << "Uid " << uid << std::endl;

        // Compute packet CRC
        unsigned int dataCrc = 0;
        for (auto v: fullData) {
            dataCrc += v;
        }

		size_t written = 0;
		unsigned char chunkNumber = 0;
		unsigned char chunksCount = static_cast<unsigned char>(chunksCount64);
		GenericAddr destAddr;

		// Multicast UDP
		if (inputs->getParInt("Multicast")) {
			destAddr.family = AF_INET;
			destAddr.ip = PONK_MULTICAST_IP;
			destAddr.port = PONK_PORT;
		}
		// Unicast UDP
		else {
			// Get the ip address from the attribute
			int ip[4];
			inputs->getParInt4("Netaddress", ip[0], ip[1], ip[2], ip[3]);
			destAddr.family = AF_INET;
			destAddr.ip = ((ip[0] << 24) + (ip[1] << 16) + (ip[2] << 8) + ip[3]);
			destAddr.port = PONK_PORT;
		}

		while (written < fullData.size()) {
			// Write packet header - 8 bytes
			GeomUdpHeader header;
			strncpy(header.headerString, PONK_HEADER_STRING, sizeof(header.headerString));
			header.protocolVersion = 0;
			header.senderIdentifier = uid; // Unique ID (so when changing name in sender, the receiver can just rename existing stream)
			strncpy(header.senderName, senderName, sizeof(header.senderName));
			header.frameNumber = frameNumber;
			header.chunkCount = chunksCount;
			header.chunkNumber = chunkNumber;
            header.dataCrc = dataCrc;

			// Prepare buffer
			packet.clear();
            size_t dataBytesForThisChunk = std::min<size_t>(fullData.size() - written, PONK_MAX_CHUNK_SIZE-sizeof(GeomUdpHeader));
			packet.resize(sizeof(GeomUdpHeader) + dataBytesForThisChunk);
			// Write header
			memcpy(&packet[0], &header, sizeof(GeomUdpHeader));
			// Write data
			memcpy(&packet[sizeof(GeomUdpHeader)], &fullData[written], dataBytesForThisChunk);
			written += dataBytesForThisChunk;

			// Send the packet
			socket->sendTo(destAddr, &packet[0], static_cast<unsigned int>(packet.size()));

			chunkNumber++;
		}

		frameNumber++;
	}

}

void
PonkSender::executeVBO(SOP_VBOOutput* output,
						const OP_Inputs* inputs,
						void* reserved)
{

}

//-----------------------------------------------------------------------------------------------------
//								CHOP, DAT, and custom parameters
//-----------------------------------------------------------------------------------------------------

int32_t
PonkSender::getNumInfoCHOPChans(void* reserved)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send 4 channels.
	return 0;
}

void
PonkSender::getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan* chan, void* reserved)
{

}

bool
PonkSender::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 0;
	infoSize->cols = 0;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return false;
}

void
PonkSender::getInfoDATEntries(int32_t index,
								int32_t nEntries,
								OP_InfoDATEntries* entries,
								void* reserved)
{
}



void
PonkSender::setupParameters(OP_ParameterManager* manager, void* reserved)
{	
	// Active
	{
		OP_NumericParameter	np;

		np.name = "Active";
		np.label = "Active";

		OP_ParAppendResult res = manager->appendToggle(np);
        assert(res == OP_ParAppendResult::Success);
	}
	// Multicast
	{
		OP_NumericParameter	np;

		np.name = "Multicast";
		np.label = "Multicast";

		OP_ParAppendResult res = manager->appendToggle(np);
        assert(res == OP_ParAppendResult::Success);
	}
	// Ip
	{
		OP_NumericParameter	np;

		np.name = "Netaddress";
		np.label = "Network Address";

		// Minimum values
		np.minValues[0] = 0;
		np.minValues[1] = 0;
		np.minValues[2] = 0;
		np.minValues[3] = 0;

		// Maximum values
		np.maxValues[0] = 255;
		np.maxValues[1] = 255;
		np.maxValues[2] = 255;
		np.maxValues[3] = 255;

		// Default value
		np.defaultValues[0] = 127;
		np.defaultValues[1] = 0;
		np.defaultValues[2] = 0;
		np.defaultValues[3] = 1;

		OP_ParAppendResult res = manager->appendInt(np, 4);
        assert(res == OP_ParAppendResult::Success);
	}

	// Unique ID (matches GeomUdpHeader.senderIdentifier: 32-bit; max 2147483647 because getParInt is int32_t)
	{
		OP_NumericParameter	np;

		np.name = "Uid";
		np.label = "Unique ID";
		np.minValues[0] = 0;
		np.maxValues[0] = 1024;
		np.defaultValues[0] = 0;
		np.minSliders[0] = 0;
		np.maxSliders[0] = 1024;

		np.clampMins[0] = true;
		np.clampMaxes[0] = true;

		OP_ParAppendResult res = manager->appendInt(np, 1);
        assert(res == OP_ParAppendResult::Success);
	}

	// Sender Name
	{
		OP_StringParameter sp;
		sp.name = "Sendername";
		sp.label = "Sender Name";
		sp.defaultValue = "Touch Designer";
		OP_ParAppendResult res = manager->appendString(sp);
		assert(res == OP_ParAppendResult::Success);
	}

	// Camera

	// Camera Object
	{
		OP_StringParameter sp;

		sp.name = "Camera";
		sp.label = "Camera";

		OP_ParAppendResult res = manager->appendObject(sp);
		assert(res == OP_ParAppendResult::Success);
	}

	// projection matrix
	{
		OP_NumericParameter	np;

		np.name = "Projectionmatrixa";
		np.label = "Projection Matrix A";

		np.defaultValues[0] = 1;

		OP_ParAppendResult res = manager->appendFloat(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter	np;

		np.defaultValues[1] = 1;

		np.name = "Projectionmatrixb";
		np.label = "Projection Matrix B";

		OP_ParAppendResult res = manager->appendFloat(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter	np;

		np.defaultValues[2] = 1;

		np.name = "Projectionmatrixc";
		np.label = "Projection Matrix C";

		OP_ParAppendResult res = manager->appendFloat(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter	np;

		np.defaultValues[3] = 1;

		np.name = "Projectionmatrixd";
		np.label = "Projection Matrix D";

		OP_ParAppendResult res = manager->appendFloat(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}
}

void
PonkSender::pulsePressed(const char* name, void* reserved)
{
}

void
PonkSender::getErrorString(OP_String* error, void* reserved)
{
	error->setString(m_errorMessage.c_str());
}

