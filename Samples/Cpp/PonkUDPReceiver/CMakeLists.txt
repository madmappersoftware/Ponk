cmake_minimum_required(VERSION 3.5)

project(PonkReceiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    ../../../Common/Cpp/DatagramSocket/DatagramSocket.cpp
    main.cpp
)
set(HEADERS
    ../../../Common/Cpp/PonkDefs.h
    ../../../Common/Cpp/DatagramSocket/DatagramSocket.h
)

add_executable(PonkReceiver ${SOURCES} ${HEADERS})
target_include_directories(PonkReceiver PRIVATE "../../../Common/Cpp/")

