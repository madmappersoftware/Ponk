#include <iostream>
#include <thread>
#include <vector>
#include <cmath>
#include <cassert>
#include "DatagramSocket/DatagramSocket.h"
#include "PonkDefs.h"

int main()
{
    std::cout << "Starting" << std::endl;

    DatagramSocket socket(INADDR_ANY,PONK_PORT);

    // TODO: let user choose a network interface or join for all active networkinterfaces
    // Zero means first active network adapter if I'm not wrong
    const int networkInterfaceIp = 0; //((192<<24) + (168<<16) + (1<<8) + 3);

    int currentFrameNumber = -1;

    // We'll keep all chunks data in a vector until we get all chunks
    // Protocol supports up to 255 chunks
    // We'll accept received data chunks in the wrong order though I doubt it should happen
    int currentFrameChunkCount = -1;
    int currentFrameDataCrc = -1;
    std::vector<bool> chunksDataHasBeenReceived;
    std::vector<std::vector<unsigned char>> chunksData;
    chunksDataHasBeenReceived.resize(255);
    chunksData.resize(255);

    while (true) {
        unsigned char buffer[65536];
        unsigned int bufferSize = static_cast<unsigned int>(sizeof(buffer));

        GenericAddr sourceAddr;
        if (!socket.recvFrom(sourceAddr, buffer, bufferSize)) {
            assert(false); // Should never happen
            return -1;
        }

        if (bufferSize == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        //std::cout << "Received packet of " << std::to_string(bufferSize) << " bytes" << std::endl;

        // Parse buffer
        if (bufferSize < sizeof(GeomUdpHeader)) {
            std::cout << "Error in frame, frame size " << std::to_string(bufferSize) << " is lower than header size" << std::endl;
            continue;
        }

        const GeomUdpHeader* header = reinterpret_cast<const GeomUdpHeader*>(buffer);

        // Check protocol header string
        if (strncmp(header->headerString,PONK_HEADER_STRING,8) != 0) {
            std::cout << "Error in frame, invalid header" << std::endl;
            continue;
        }

        // Check protocol version
        if (header->protocolVersion > 0) {
            std::cout << "Source protocol version is " << std::to_string(header->protocolVersion)
                      << " but this sample code only support protocol version 0" << std::endl;
            continue;
        }

        // Read Sender Name string (32 bytes null terminated UTF8 string)
        //std::cout << "Sender name " << senderName << std::endl;

        // Read Frame Number
        // If we actually received part of a frame, we shouldn't received a different frame number
        if (currentFrameNumber != -1 && header->frameNumber != currentFrameNumber) {
            std::cout << "Error: frame number has changed to " << std::to_string(header->frameNumber)
                      << " but we haved received all chunks from frame number " << std::to_string(currentFrameNumber)
                      << ". Resetting chunks data" << std::endl;
            assert(false);
            // Reset state. Note that ideally we shouldn't skip all chunks from a frame if we receive the first chunk
            // of next frame before last chunk of previous frame, but keep the sample code simple (we should keep received chunks
            // for the 256 possible frame number) - a frame will generally fit a single chunk / UDP packet and this case should rarely happen
            for (auto& chunkData: chunksData) {
                chunkData.clear();
            }
            for (int i=0; i<255; i++) {
                chunksDataHasBeenReceived[i] = false;
            }
            currentFrameNumber = -1;
        }

        // If we're actually reading a frame, ensure chunkCount doesn't change accross same frame headers (buggy sender)
        if (currentFrameNumber != -1 && currentFrameChunkCount != header->chunkCount) {
            std::cout << "Error: received a new chunk for a frame with a different chunk count" << std::endl;
            assert(false);
        }

        // If we're actually reading a frame, ensure dataCrc doesn't change (buggy sender)
        if (currentFrameNumber != -1 && currentFrameDataCrc != header->dataCrc) {
            std::cout << "Error: received a new chunk for a frame with a different data CRC" << std::endl;
            assert(false);
        }

        currentFrameNumber = header->frameNumber;
        currentFrameChunkCount = header->chunkCount;
        currentFrameDataCrc = header->dataCrc;

        // Check Chunk Count
        if (header->chunkCount == 0) {
            std::cout << "Error in frame, chunk count is zero" << std::endl;
            assert(false);
        }

        // Check Chunk number
        if (header->chunkNumber >= header->chunkCount) {
            // Sender is buggy
            std::cout << "Error in frame, chunk number (" << std::to_string(header->chunkNumber) << ") is over chunk count (" << std::to_string(header->chunkCount) << ")" << std::endl;
            assert(false);
            continue;
        }
        if (!chunksData[header->chunkNumber].empty()) {
            // Buggy sender or dying network
            std::cout << "Error in frame, we already received data for chunk " << std::to_string(header->chunkNumber) << std::endl;
            assert(false);
        }

        // Now read data
        const auto dataLength = bufferSize - sizeof(GeomUdpHeader);
        unsigned int bufferOffset = sizeof(GeomUdpHeader);
        for (unsigned int i=0; i<dataLength; i++) {
            chunksData[header->chunkNumber].push_back(buffer[bufferOffset++]);
        }
        chunksDataHasBeenReceived[header->chunkNumber] = true;

        //std::cout << "Received chunk " << std::to_string(chunkNumber) << "/" << std::to_string(chunkCount) << " for frame " << std::to_string(frameNumber) << std::endl;

        // If we received all frame chunks, log the frame
        bool receivedAllChunksYet = true;
        for (int i=0; i<header->chunkCount; i++) {
            if (!chunksDataHasBeenReceived[i]) {
                receivedAllChunksYet = false;
                break;
            }
        }

        if (receivedAllChunksYet) {
            // Put all frame data together in a single buffer
            std::vector<unsigned char> allData;
            allData.reserve(255*PONK_MAX_DATA_BYTES_PER_PACKET);
            for (int i=0; i<header->chunkCount; i++) {
                allData.insert(allData.end(),chunksData[i].begin(),chunksData[i].end());
            }

            // Seems we're all good, we know have complete frame data
            std::cout << "Received frame " << std::to_string(currentFrameNumber) << std::endl;

            // Reset state
            for (int i=0; i<header->chunkCount; i++) {
                chunksData[i].clear();
                chunksDataHasBeenReceived[i] = false;
            }
            currentFrameNumber = -1;

            // Parse Frame Data
            const auto dataSize = allData.size();
            if (dataSize < 1) {
                std::cout << "Error: frame data is empty";
                assert(false);
                continue;
            }

            // Check Data CRC
            int computedCrc = 0;
            for (auto v: allData) {
                computedCrc += v;
            }
            if (computedCrc != header->dataCrc) {
                std::cout << "Error: invalid data CRC, ignoring frame";
                assert(false);
                continue;
            }

            unsigned int dataOffset = 0;

            // Read Pathes
            struct Path {
                struct Point {
                    float x,y,r,g,b;
                };
                std::vector<Point> points;
            };

            // Loop over pathes until there's no more data to read
            std::vector<Path> pathes;
            while (dataOffset < dataSize) {
                // Read data format
                const auto dataFormat = allData[dataOffset];
                dataOffset++;

                // Read Meta Data Count
                if (dataSize < dataOffset+1) {
                    std::cout << "Error: not enough data to read path meta data count" << std::endl;
                    break;
                }
                const unsigned short metaDataCount = allData[dataOffset];
                dataOffset++;

                // Read Meta Data
                if (dataSize < dataOffset+12 * metaDataCount) {
                    std::cout << "Error: not enough data to read path meta data" << std::endl;
                    break;
                }
                for (int idx=0; idx<metaDataCount; idx++) {
                    // Not that we don't know what value type is carried. Sender and Receiver
                    // should know what value type to transfer for a meta. Receiver
                    // should check meta value is in acceptable range
                    std::string metaName((char*)&allData[dataOffset],8);
                    dataOffset += 8;
                    const auto floatValue = *reinterpret_cast<float*>(&allData[dataOffset]);
                    dataOffset += 4;
                    std::cout << "Path Meta " << metaName << " = " << floatValue << std::endl;
                }

                // Read Point Count
                if (dataSize < dataOffset+2) {
                    std::cout << "Error: not enough data to read path point count" << std::endl;
                    break;
                }
                const unsigned short pointCount = allData[dataOffset] + (allData[dataOffset+1]<<8);
                std::cout << "  -> Path " << std::to_string(pathes.size()) << " / Point Count = " << std::to_string(pointCount) << std::endl;
                dataOffset += 2;

                unsigned char bytesPerPoint;
                if (dataFormat == PONK_DATA_FORMAT_XYRGB_U16) {
                    bytesPerPoint = 5 * sizeof(unsigned short);
                } else if (dataFormat == PONK_DATA_FORMAT_XY_F32_RGB_U8) {
                    bytesPerPoint = 2 * sizeof(float) + 3 * sizeof(unsigned char);
                } else {
                    std::cout << "Error: unhandled data format: " << dataFormat << std::endl;
                    break;
                }

                if (dataSize < dataOffset + pointCount * bytesPerPoint) {
                    std::cout << "Error: not enough data to read path points" << std::endl;
                    break;
                }

                Path path;
                for (int i=0; i<pointCount; i++) {
                    Path::Point point;

                    if (dataFormat == PONK_DATA_FORMAT_XYRGB_U16) {
                        unsigned short x16bits = allData[dataOffset] + (allData[dataOffset+1]<<8);
                        dataOffset += 2;
                        unsigned short y16bits = allData[dataOffset] + (allData[dataOffset+1]<<8);
                        dataOffset += 2;
                        unsigned short r16bits = allData[dataOffset] + (allData[dataOffset+1]<<8);
                        dataOffset += 2;
                        unsigned short g16bits = allData[dataOffset] + (allData[dataOffset+1]<<8);
                        dataOffset += 2;
                        unsigned short b16bits = allData[dataOffset] + (allData[dataOffset+1]<<8);
                        dataOffset += 2;

                        point.x = -1+2*(x16bits/65535.f);
                        point.y = -1+2*(y16bits/65535.f);
                        point.r = r16bits/65535.f;
                        point.g = g16bits/65535.f;
                        point.b = b16bits/65535.f;
                    } else if (dataFormat == PONK_DATA_FORMAT_XY_F32_RGB_U8) {
                        point.x = *reinterpret_cast<float*>(&allData[dataOffset]);
                        dataOffset += sizeof(float);
                        point.y = *reinterpret_cast<float*>(&allData[dataOffset]);
                        dataOffset += sizeof(float);
                        point.r = allData[dataOffset];
                        dataOffset++;
                        point.g = allData[dataOffset];
                        dataOffset++;
                        point.b = allData[dataOffset];
                        dataOffset++;
                    }

                    path.points.push_back(point);
                }

                pathes.push_back(path); // Note that this is very inefficient
            }

            assert(dataOffset == dataSize);
        }
    }

    return 0;
}
