#include <iostream>
#include <thread>
#include <vector>
#include <cmath>
#include <cassert>
#include "DatagramSocket/DatagramSocket.h"
#include "PonkDefs.h"
#ifndef M_PI // M_PI not defined on Windows
    #define M_PI 3.14159265358979323846
#endif

void push8bits(std::vector<unsigned char>& fullData, unsigned char value) {
    fullData.push_back(value);
}

void push16bits(std::vector<unsigned char>& fullData, unsigned short value) {
    fullData.push_back(static_cast<unsigned char>((value>>0) & 0xFF));
    fullData.push_back(static_cast<unsigned char>((value>>8) & 0xFF));
}

void push32bits(std::vector<unsigned char>& fullData, int value) {
    fullData.push_back(static_cast<unsigned char>((value>>0) & 0xFF));
    fullData.push_back(static_cast<unsigned char>((value>>8) & 0xFF));
    fullData.push_back(static_cast<unsigned char>((value>>16) & 0xFF));
    fullData.push_back(static_cast<unsigned char>((value>>24) & 0xFF));
}

void push32bits(std::vector<unsigned char>& fullData, float value) {
    push32bits(fullData,*reinterpret_cast<int*>(&value));
}

void pushMetaData(std::vector<unsigned char>& fullData, const char (&eightCC)[9],float value) {
    for (int i=0; i<8; i++) {
        fullData.push_back(eightCC[i]);
    }
    push32bits(fullData,*(int*)&value);
}

int main()
{
    std::cout << "Starting" << std::endl;

    DatagramSocket socket(INADDR_ANY,0);

    // send a moving circle and a triangle in loop
    double animTime = 0;
    auto nextFrametime = std::chrono::system_clock::now();
    unsigned char frameNumber = 0;
    while (true) {
        std::vector<unsigned char> fullData;
        fullData.reserve(65536);

        #ifdef USE_PONK_DATA_FORMAT_XYRGB_U16
            // Generate circle data with 1024 points
            fullData.push_back(PONK_DATA_FORMAT_XYRGB_U16); // Write Format Data

            // Meta Data
            fullData.push_back(2); // Write meta data count
            pushMetaData(fullData,"PATHNUMB",1.f);
            pushMetaData(fullData,"MAXSPEED",0.1f);

            // Write point count - LSB first
            #define CIRCLE_POINT_COUNT 1024
            #define CIRCLE_MOVE_SIZE 0.2
            #define CIRCLE_SIZE 0.5
            push16bits(fullData,CIRCLE_POINT_COUNT);
            // Write 1024 points
            const auto circleCenterX = CIRCLE_MOVE_SIZE * cos(animTime*3);
            const auto circleCenterY = CIRCLE_MOVE_SIZE * sin(animTime*3);
            for (int i=0; i<1024; i++) {
                // Be sure to close circle
                const auto normalizedPosInCircle = double(i)/(CIRCLE_POINT_COUNT-1);
                const auto x = circleCenterX + CIRCLE_SIZE * cos(normalizedPosInCircle*2*M_PI);
                const auto y = circleCenterY + CIRCLE_SIZE * sin(normalizedPosInCircle*2*M_PI);
                assert(x>=-1 && x<=1 && y>=-1 && y<=1);
                const auto x16Bits = static_cast<unsigned short>(((x+1)/2) * 65535);
                const auto y16Bits = static_cast<unsigned short>(((y+1)/2) * 65535);
                // Push X - LSB first
                push16bits(fullData,x16Bits);
                // Push Y - LSB first
                push16bits(fullData,y16Bits);
                // Push R - LSB first
                push16bits(fullData,0xFFFF);
                // Push G - LSB first
                push16bits(fullData,0xFFFF);
                // Push B - LSB first
                push16bits(fullData,0xFFFF);
            }

            // Generate a triangle with 4 points (to close it)
            fullData.push_back(PONK_DATA_FORMAT_XYRGB_U16); // Write Format Data

            // Meta Data
            fullData.push_back(1); // Write meta data count
            pushMetaData(fullData,"PATHNUMB",2.f);

            // Write point count - LSB first
            #define TRIANGLE_POINT_COUNT 4
            #define TRIANGLE_SIZE 0.5
            fullData.push_back((TRIANGLE_POINT_COUNT>>0) & 0xFF);
            fullData.push_back((TRIANGLE_POINT_COUNT>>8) & 0xFF);
            for (int i=0; i<TRIANGLE_POINT_COUNT; i++) {
                const auto normalizedPosInTriangle = double(i)/(TRIANGLE_POINT_COUNT-1);
                const auto x = TRIANGLE_SIZE * cos(normalizedPosInTriangle*2*M_PI);
                const auto y = TRIANGLE_SIZE * sin(normalizedPosInTriangle*2*M_PI);
                assert(x>=-1 && x<=1 && y>=-1 && y<=1);
                const auto x16Bits = static_cast<unsigned short>(((x+1)/2) * 65535);
                const auto y16Bits = static_cast<unsigned short>(((y+1)/2) * 65535);
                // Push X - LSB first
                push16bits(fullData,x16Bits);
                // Push Y - LSB first
                push16bits(fullData,y16Bits);
                // Push R - LSB first
                push16bits(fullData,0xFFFF);
                // Push G - LSB first
                push16bits(fullData,0);
                // Push B - LSB first
                push16bits(fullData,0);
            }
        #else
            // Generate circle data with 1024 points
            fullData.push_back(PONK_DATA_FORMAT_XY_F32_RGB_U8); // Write Format Data

            // Meta Data
            fullData.push_back(2); // Write meta data count
            pushMetaData(fullData,"PATHNUMB",1.f);
            pushMetaData(fullData,"MAXSPEED",1.0f);

            // Write point count - LSB first
            #define CIRCLE_POINT_COUNT 1024
            #define CIRCLE_MOVE_SIZE 0.2f
            #define CIRCLE_SIZE 0.5f
            push16bits(fullData,CIRCLE_POINT_COUNT);
            // Write 1024 points
            const auto circleCenterX = CIRCLE_MOVE_SIZE * cos(animTime*3);
            const auto circleCenterY = CIRCLE_MOVE_SIZE * sin(animTime*3);
            for (int i=0; i<1024; i++) {
                // Be sure to close circle
                const auto normalizedPosInCircle = double(i)/(CIRCLE_POINT_COUNT-1);
                const auto x = static_cast<float>(circleCenterX + CIRCLE_SIZE * cos(normalizedPosInCircle*2*M_PI));
                const auto y = static_cast<float>(circleCenterY + CIRCLE_SIZE * sin(normalizedPosInCircle*2*M_PI));
                assert(x>=-1 && x<=1 && y>=-1 && y<=1);
                // Push X - LSB first
                push32bits(fullData,x);
                // Push Y - LSB first
                push32bits(fullData,y);
                // Push R - LSB first
                push8bits(fullData,0xFF);
                // Push G - LSB first
                push8bits(fullData,0xFF);
                // Push B - LSB first
                push8bits(fullData,0xFF);
            }

            // Generate a triangle with 4 points (to close it)
            // Generate circle data with 1024 points
            fullData.push_back(PONK_DATA_FORMAT_XY_F32_RGB_U8); // Write Format Data

            // Meta Data
            fullData.push_back(1); // Write meta data count
            pushMetaData(fullData,"PATHNUMB",2.f);

            // Write point count - LSB first
            #define TRIANGLE_POINT_COUNT 4
            #define TRIANGLE_SIZE 0.5f
            push16bits(fullData,TRIANGLE_POINT_COUNT);
            for (int i=0; i<TRIANGLE_POINT_COUNT; i++) {
                const auto normalizedPosInTriangle = double(i)/(TRIANGLE_POINT_COUNT-1);
                const auto x = static_cast<float>(TRIANGLE_SIZE * cos(normalizedPosInTriangle*2*M_PI));
                const auto y = static_cast<float>(TRIANGLE_SIZE * sin(normalizedPosInTriangle*2*M_PI));
                assert(x>=-1 && x<=1 && y>=-1 && y<=1);
                // Push X - LSB first
                push32bits(fullData,x);
                // Push Y - LSB first
                push32bits(fullData,y);
                // Push R - LSB first
                push8bits(fullData,0xFF);
                // Push G - LSB first
                push8bits(fullData,0);
                // Push B - LSB first
                push8bits(fullData,0);
            }
        #endif

        // Compute necessary chunk count
        size_t chunksCount64 = 1 + fullData.size() / PONK_MAX_DATA_BYTES_PER_PACKET;
        if (chunksCount64 > 255) {
            throw std::runtime_error("Protocol doesn't accept sending "
                                     "a packet that would be splitted "
                                     "in more than 255 chunks");
        }

        // Compute data CRC
        unsigned int dataCrc = 0;
        for (auto v: fullData) {
            dataCrc += v;
        }

        // Send all chunks to the desired IP address
        size_t written = 0;
        unsigned char chunkNumber = 0;
        unsigned char chunksCount = static_cast<unsigned char>(chunksCount64);
        while (written < fullData.size()) {
            // Write packet header - 8 bytes
            GeomUdpHeader header;
            strncpy(header.headerString,PONK_HEADER_STRING,sizeof(header.headerString));
            header.protocolVersion = 0;
            header.senderIdentifier = 123123; // Unique ID (so when changing name in sender, the receiver can just rename existing stream)
            strncpy(header.senderName,"Sample Sender",sizeof(header.senderName));
            header.frameNumber = frameNumber;
            header.chunkCount = chunksCount;
            header.chunkNumber = chunkNumber;
            header.dataCrc = dataCrc;

            // Prepare buffer
            std::vector<unsigned char> packet;
            size_t dataBytesForThisChunk = std::min<size_t>(fullData.size()-written, PONK_MAX_DATA_BYTES_PER_PACKET);
            packet.resize(sizeof(GeomUdpHeader) + dataBytesForThisChunk);
            // Write header
            memcpy(&packet[0],&header,sizeof(GeomUdpHeader));
            // Write data
            memcpy(&packet[sizeof(GeomUdpHeader)],&fullData[written],dataBytesForThisChunk);
            written += dataBytesForThisChunk;

            // Now send chunk packet
            GenericAddr destAddr;
            destAddr.family = AF_INET;
            // Unicast on localhost 127.0.0.1
            destAddr.ip = ((127 << 24) + (0 << 16) + (0 << 8) + 1);
            destAddr.port = PONK_PORT;
            socket.sendTo(destAddr, &packet.front(), static_cast<unsigned int>(packet.size()));

            chunkNumber++;
        }

        std::cout << "Sent frame " << std::to_string(frameNumber) << std::endl;

        animTime += 1/60.;
        frameNumber++;

        nextFrametime += std::chrono::microseconds(1000000/60);
        std::this_thread::sleep_until(nextFrametime);
    }

    return 0;
}
