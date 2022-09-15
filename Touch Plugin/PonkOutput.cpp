#include "PonkOutput.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <iostream>

#ifndef M_PI // M_PI not defined on Windows
	#define M_PI 3.14159265358979323846
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
		// Always return SOP_CPLUSPLUS_API_VERSION in this function.
		info->apiVersion = SOPCPlusPlusAPIVersion;

		// The opType is the unique name for this TOP. It must start with a 
		// capital A-Z character, and all the following characters must lower case
		// or numbers (a-z, 0-9)
		info->customOPInfo.opType->setString("Ponkoutput");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("Ponk Output");

		// Will be turned into a 3 letter icon on the nodes
		info->customOPInfo.opIcon->setString("PNK");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("Tyrell");
		info->customOPInfo.authorEmail->setString("colas@tyrell.studio");

		// This SOP works with 0 or 1 inputs
		info->customOPInfo.minInputs = 1;
		info->customOPInfo.maxInputs = 1;

	}

	DLLEXPORT
	SOP_CPlusPlusBase*
	CreateSOPInstance(const OP_NodeInfo* info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per SOP that is using the .dll
		return new PonkOutput(info);
	}

	DLLEXPORT
	void
	DestroySOPInstance(SOP_CPlusPlusBase* instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the SOP using that instance is deleted, or
		// if the SOP loads a different DLL
		delete (PonkOutput*)instance;
	}

};


PonkOutput::PonkOutput(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myOffset = 0.0;
	myChop = "";

	myChopChanName = "";
	myChopChanVal = 0;

	myDat = "N/A";

	socket = new DatagramSocket (INADDR_ANY, 0);;
}

PonkOutput::~PonkOutput()
{
	delete socket;
}

void
PonkOutput::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = false;

	ginfo->cookEveryFrame = true;

	//if direct to GPU loading:
	ginfo->directToGPU = false;

}

void PonkOutput::push16bits(std::vector<unsigned char>& fullData, unsigned short value) {
	fullData.push_back(static_cast<unsigned char>((value >> 0) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
}

void PonkOutput::push32bits(std::vector<unsigned char>& fullData, int value) {
	fullData.push_back(static_cast<unsigned char>((value >> 0) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
	fullData.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
}

void PonkOutput::pushFloat32(std::vector<unsigned char>& fullData, float value) {
    const auto asInt = *reinterpret_cast<int*>(&value);
    push32bits(fullData,asInt);
}

void PonkOutput::pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], int value) {
	for (int i = 0; i < 8; i++) {
		fullData.push_back(eightCC[i]);
	}
	push32bits(fullData, value);
}
void PonkOutput::pushMetaData(std::vector<unsigned char>& fullData, const char(&eightCC)[9], float value) {
	for (int i = 0; i < 8; i++) {
		fullData.push_back(eightCC[i]);
	}
	push32bits(fullData, *(int*)&value);
}

void PonkOutput::pushPoint_XY_F32_RGB_U8(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor) {
    pushFloat32(fullData, pointPosition.x);
    pushFloat32(fullData, pointPosition.y);
	#define CLAMP_IN_ZERO_ONE(x) (x<0?0:(x>1?1:x))
    fullData.push_back(static_cast<unsigned char>(CLAMP_IN_ZERO_ONE(pointColor.r)*255));
    fullData.push_back(static_cast<unsigned char>(CLAMP_IN_ZERO_ONE(pointColor.g)*255));
    fullData.push_back(static_cast<unsigned char>(CLAMP_IN_ZERO_ONE(pointColor.b)*255));
}

void PonkOutput::pushPoint_XYRGB_U16(std::vector<unsigned char>& fullData, const Position& pointPosition, const Color& pointColor) {
    if (pointPosition.x < -1 || pointPosition.x > 1 || pointPosition.y < -1 || pointPosition.y > 1) {
        // Clamp position and set color = 0
        unsigned short x16Bits, y16Bits;
        if (pointPosition.x < -1) {
            x16Bits = 0;
        } else if (pointPosition.x > 1) {
            x16Bits = 65535;
        } else {
            x16Bits = static_cast<unsigned short>(((pointPosition.x + 1) / 2) * 65535);
        }
        if (pointPosition.y < -1) {
            y16Bits = 0;
        } else if (pointPosition.y > 1) {
            y16Bits = 65535;
        } else {
            y16Bits = static_cast<unsigned short>(((pointPosition.y + 1) / 2) * 65535);
        }

        // Push X - LSB first
        push16bits(fullData, x16Bits);
        // Push Y - LSB first
        push16bits(fullData, y16Bits);
        // Push R - LSB first
        push16bits(fullData, 0);
        // Push G - LSB first
        push16bits(fullData, 0);
        // Push B - LSB first
        push16bits(fullData, 0);
    } else {
        const auto x16Bits = static_cast<unsigned short>(((pointPosition.x + 1) / 2) * 65535);
        const auto y16Bits = static_cast<unsigned short>(((pointPosition.y + 1) / 2) * 65535);

        const auto r16Bits = static_cast<unsigned short>(((pointColor.r + 1) / 2) * 65535);
        const auto g16Bits = static_cast<unsigned short>(((pointColor.g + 1) / 2) * 65535);
        const auto b16Bits = static_cast<unsigned short>(((pointColor.b + 1) / 2) * 65535);

        // Push X - LSB first
        push16bits(fullData, x16Bits);
        // Push Y - LSB first
        push16bits(fullData, y16Bits);
        // Push R - LSB first
        push16bits(fullData, r16Bits);
        // Push G - LSB first
        push16bits(fullData, g16Bits);
        // Push B - LSB first
        push16bits(fullData, b16Bits);
    }
}

bool PonkOutput::validatePrimitiveDat(const OP_DATInput* primitive, int numPrimitives) {
	// Check that the dat is table
	if (!primitive->isTable) {
		return false;
	}

	// Check that that the number of row match the number of primitive + title
	if (primitive->numRows != (numPrimitives + 1)) {
		return false;
	}

	// Check that the title of the third column is close
	if (strcmp(primitive->getCell(0, 2), "close") != 0)
	{
		return false;
	}

	return true;
}


std::map<std::string, float> PonkOutput::getMetadata(const OP_DATInput* primitive, int primitiveIndex) {
	std::map<std::string, float> metadata;
	
	// check how many metadata attribute the primitive dat contains
	int numMetadata = primitive->numCols - 4;

	// get the metadata from the dat
	for (int i = 0; i < numMetadata; i++) {
		std::string metadataName = primitive->getCell(0, 3 + i);
		float metadataValue = (float)std::strtod(primitive->getCell(primitiveIndex + 1, 3 + i), NULL);
		metadata[metadataName] = metadataValue;
	}

	return metadata;
}

Matrix44<double>
PonkOutput::buildCameraTransProjMatrix(const OP_Inputs* inputs)
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
		cameraProjection[2][2],
		cameraProjection[3][3]);
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
PonkOutput::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	myExecuteCount++;
	if (!inputs->getParInt("Active")) {
		return;
	}

	// Get the primitive dat from the attribute
	const OP_DATInput* primitive = inputs->getParDAT("Primitive"); 

	// Only run if a SOP is connected on the first input and a
	// is set on the primitive parameter
	if (inputs->getNumInputs() > 0 && primitive)
	{
		// Get the input sop
		const OP_SOPInput	*sinput = inputs->getInputSOP(0);

		// build the matrix to do the world space to screen projection
		Matrix44<double> cameraTransProj = buildCameraTransProjMatrix(inputs);

		// Create the vector  will store our the data that need to be send to madLaser
		std::vector<unsigned char> fullData;
		fullData.reserve(65536);

		// Check that the primitive dat is valid
		if(validatePrimitiveDat(primitive, sinput->getNumPrimitives()))
		{
			const Position* ptArr = sinput->getPointPositions();
			const Color* colors = nullptr;

			if (sinput->hasColors()) {
				colors = sinput->getColors()->colors;
			}

			for (int primitiveNumber = 0; primitiveNumber < sinput->getNumPrimitives(); primitiveNumber++)
			{
				//std::cout << "-------------------- primitive : " << i << std::endl;

                // Write Format Data
                fullData.push_back(PONK_DATA_FORMAT_XY_F32_RGB_U8);

				// get the metadata
				std::map<std::string, float> metadata = getMetadata(primitive, primitiveNumber);

				// Write meta data count
				fullData.push_back(metadata.size());

				for (const auto& kv : metadata) {
					char charMetadata[9];
					std::copy(kv.first.begin(), kv.first.end(), charMetadata);

					pushMetaData(fullData, charMetadata, kv.second);
				}

				const SOP_PrimitiveInfo primInfo = sinput->getPrimitive(primitiveNumber);

				const int32_t* primVert = primInfo.pointIndices;

				int numPoints = primInfo.numVertices;

				// check if the primitve is closed
				bool isClosed = false;
				if (strcmp(primitive->getCell(primitiveNumber+1, 2), "1") == 0) {
					isClosed = true;

                    // Write point count
                    push16bits(fullData, numPoints+1);
                } else {
                    push16bits(fullData, numPoints);
                }

                static const Color s_white(1.0f, 1.0f, 1.0f, 1.0f);
                
				for (int pointNumber = 0; pointNumber < numPoints; pointNumber++) {
					Position pointPosition = cameraTransProj * ptArr[primVert[pointNumber]];
					pushPoint_XY_F32_RGB_U8(fullData, pointPosition, sinput->hasColors()?colors[primVert[pointNumber]]:s_white);
				}

				// If the primitive is close add the first point at the end
				if (isClosed) {
					Position pointPosition = cameraTransProj * ptArr[primVert[0]];
                    pushPoint_XY_F32_RGB_U8(fullData, pointPosition, sinput->hasColors()?colors[primVert[0]]:s_white);

				}
			}
		}
		else
		{
			//std::cout << "Invalid Primitive Dat" << std::endl;
		}

		// Check if we don't reach the maximum number of chunck
		size_t chunksCount64 = 1 + fullData.size() / PONK_MAX_DATA_BYTES_PER_PACKET;
		if (chunksCount64 > 255) {
			throw std::runtime_error("Protocol doesn't accept sending "
				"a packet that would be splitted "
				"in more than 255 chunks");
		}

		// Get the ip address from the attribute
		int ip[4];
		inputs->getParInt4("Netaddress", ip[0], ip[1], ip[2], ip[3]);

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
		while (written < fullData.size()) {
			// Write packet header - 8 bytes
			GeomUdpHeader header;
			strncpy(header.headerString, PONK_HEADER_STRING, sizeof(header.headerString));
			header.protocolVersion = 0;
			header.senderIdentifier = uid; // Unique ID (so when changing name in sender, the receiver can just rename existing stream)
			strncpy(header.senderName, "Touch Designer", sizeof(header.senderName));
			header.frameNumber = frameNumber;
			header.chunkCount = chunksCount;
			header.chunkNumber = chunkNumber;
            header.dataCrc = dataCrc;

			// Prepare buffer
			std::vector<unsigned char> packet;
			size_t dataBytesForThisChunk = std::min<size_t>(fullData.size() - written, PONK_MAX_DATA_BYTES_PER_PACKET-sizeof(GeomUdpHeader));
			packet.resize(sizeof(GeomUdpHeader) + dataBytesForThisChunk);
			// Write header
			memcpy(&packet[0], &header, sizeof(GeomUdpHeader));
			// Write data
			memcpy(&packet[sizeof(GeomUdpHeader)], &fullData[written], dataBytesForThisChunk);
			written += dataBytesForThisChunk;

			// Now send chunk packet
			GenericAddr destAddr;
			destAddr.family = AF_INET;

            // Unicast UDP
			destAddr.ip = ((ip[0] << 24) + (ip[1] << 16) + (ip[2] << 8) + ip[3]);
			destAddr.port = PONK_PORT;
			socket->sendTo(destAddr, &packet[0], static_cast<unsigned int>(packet.size()));

			chunkNumber++;
		}

		//std::cout << "Sent frame " << std::to_string(frameNumber) << std::endl;

		frameNumber++;
	}

}

void
PonkOutput::executeVBO(SOP_VBOOutput* output,
						const OP_Inputs* inputs,
						void* reserved)
{

}

//-----------------------------------------------------------------------------------------------------
//								CHOP, DAT, and custom parameters
//-----------------------------------------------------------------------------------------------------

int32_t
PonkOutput::getNumInfoCHOPChans(void* reserved)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send 4 channels.
	return 4;
}

void
PonkOutput::getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan* chan, void* reserved)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name->setString("offset");
		chan->value = (float)myOffset;
	}

	if (index == 2)
	{
		chan->name->setString(myChop.c_str());
		chan->value = (float)myOffset;
	}

	if (index == 3)
	{
		chan->name->setString(myChopChanName.c_str());
		chan->value = myChopChanVal;
	}
}

bool
PonkOutput::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 3;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
PonkOutput::getInfoDATEntries(int32_t index,
								int32_t nEntries,
								OP_InfoDATEntries* entries,
								void* reserved)
{
	char tempBuffer[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "executeCount");
#else // macOS
		strlcpy(tempBuffer, "executeCount", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%d", myExecuteCount);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 1)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "offset");
#else // macOS
		strlcpy(tempBuffer, "offset", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%g", myOffset);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 2)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "DAT input name");
#else // macOS
		strlcpy(tempBuffer, "offset", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		strcpy_s(tempBuffer, myDat.c_str());
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}
}



void
PonkOutput::setupParameters(OP_ParameterManager* manager, void* reserved)
{	// Active

	{
		OP_NumericParameter	np;

		np.name = "Active";
		np.label = "Active";

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

	// Unique ID
	{
		OP_NumericParameter	np;

		np.name = "Uid";
		np.label = "Unique ID";

		OP_ParAppendResult res = manager->appendInt(np, 1);
        assert(res == OP_ParAppendResult::Success);
	}

	// Primitive data
	{
		OP_StringParameter sopp;

		sopp.name = "Primitive";
		sopp.label = "Primitive";

		OP_ParAppendResult res = manager->appendDAT(sopp);
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
PonkOutput::pulsePressed(const char* name, void* reserved)
{
	if (!strcmp(name, "Reset"))
	{
		myOffset = 0.0;
	}
}

