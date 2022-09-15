#pragma once

/*
 *  PONK (Pathes Over NetworK) is a minimal protocol to transfer 2D colored pathes from a source to
 *  a receiver. It has been developped to transfer laser path from a software to another over network using UDP.
 *
 *  Requirements are:
 *      - Transfer over network of frames composed of 2D geometry path with colors along the path
 *      - Make it simple to implement on both sides
 *      - Make it work on almost any network - no specific hardware requirement or os settings tricks
 *      - Make it extensible while not increasing bandwidth requirements in most common cases
 *      - Avoid using unecessary bandwidth:
 *          - For laser, having more than 8 bits per component colors is mostly useless (laser projector
 *          diodes have a very poor definition in low values anyway)
 *          - Support multiple formats to adapt bandwidth to project requirements
 *      - If the frame to transmit is too long to fit a single UDP packet, it should be cut in multiple
 *        chunks. PONK_MAX_DATA_BYTES_PER_PACKET is set to 8192 by default. This value should make
 *        it work on all popular OS.
 *      - The sender should be able to attach "meta data" to each path transmitted, that the receiver
 *        might handle for specific behaviors. When rasterizing a path for laser rendering
 *        user might give some hints like "should we favor scan speed or render precision ?"
 *      - Receiver must be able to detect network issue and ignore a frame if something when wrong (CRC)
 *
 *  Implementation in Sender
 *      - A software can send instanciate multiple senders.
 *      - A sender is identified by a 32 bits number which can be a  random generated value when instanciating
 *        the sending component
 *      - It also transmits a "sender name" string (UTF8) that can be used to display a readable
 *        source name is the receiving application.
 *      - The stream identifier should not change when reloading a project file so the receiver can
 *        reconnect the stream. Changing the stream name should not affect existing connection (the
 *        receiver must use the integer identifier, the string is used to display a meaningful text only)
 *
 *  Implementation in Receiver
 *      - The receiver should handle the fact that multiple senders can be sending packets.
 *      - It should be as reactive as possible to reduce latency and improve synchronization.
 *      - The receiver should accept any packet size (chunk size can be adjusted on sender side)
 *      - The receiver should at least support data format "PONK_DATA_FORMAT_XY_F32_RGB_U8"
 *      - If the receiver doesn't handle sender data type, it should notify it in the user interface in some way
 *      - Protocol Version field in the packets shouldn't be ignored: if the specified protocol version is not
 *        handled, the receiver should ignore the packet and notify the user that it is not compatible with sender
 *        for this reason (protocol version will be increased only if breaking compatibility, not if we decide
 *        too add a new data format)
 *
 *  Meta Data Format
 *      - Each path can have a list of meta data attached
 *      - A meta data is identified by 8 characters (the key)
 *      - Meta data value is a 32 bits floating point number,
 *        if value should be an boolean (ie "optimize angles"), any value different of zero is considered true
 *        if value should be an integer (ie "min ilda points"), value should be rounded to nearest int
 *        if value should be a floating point, no problem
 *
 *  Possible improvements / extensions:
 *      Synchronization
 *          - If the receiver notify sender that it has used the last received frame, the sender could
 *            adjust to the receiver framerate... (since the sender doesn't know how long it will take
 *            to the laser to travel the path, but receiver might know)
 *      New Data Formats:
 *          - XY_U16_SingleRGB: if you send a path of 2000 points with the same color,
 *                              we could reduce bandwidth a lot by removing color
 *          - XYRGBU1U2U3: would be useful to control additional diodes (ie yellow, deep blue...)
 *          - XYZRGB: providing the Z would let the user handle the 3D->2D projection with a
 *            controllable camera in the receiver
 *      Laser Rasterization Settings:
 *          If you use a software for path generation (sender) and a receiver for the display,
 *          you might want to provide, for each path, some parameters that the receiver could
 *          use for rasterizing the 2D geometry pathes to an ILDA frame. For instance:
 *              - Should we skip scanning long black sections ? (parameter "Skip Black Sections" ?)
 *              - Should we prior scan speed or path render precision ? (parameter "Max Scan Speed" in radians/ms ?)
 *              - Should we optimize angles for this path ? In some case you might want (ie text display)
 *                but is some not (ie generative noisy curved shapes)
 *          Those settings can actually be transmitted in meta data, but uniformizing it is an idea.
 *
 *  Packet Format:
 *      - Header:
 *          - Header String - char[8]: "PONK-UDP"
 *          - Protocol Version - char: 0
 *          - Sender Indetifier - 32 bits int
 *          - Sender Name - char[32]
 *          - Frame Number - unsigned char: incremented on each frame
 *          - Chunk Count - unsigned char
 *          - Chunk Number - unsigned char
 *          - CRC - unsigned int: sum of all data contained in this frame (of all chunks)
 *      - Data:
 *          - For each path:
 *              - Data format - unsigned char (PONK_DATA_FORMAT_XY_F32_RGB_U8...)
 *              - Meta Data count - unsigned char
 *                  - For each Meta Data:
 *                      - Key - char[8]
 *                      - Value - 32 bits float
 *              - Point Count - unsigned short (16 bits)
 *              - For each point
 *                  - Point data, depending on data format, ie X,Y as float 32, R,G,B as unsigned char
 *
 *  List of Meta Data support by:
 *
 *      - MadMapper / MadLaser: most of those parameters can be adjusted at surface level. Adding meta data will override
 *        settings set at surface level for the path it is attached to. Those parameters are documented in MadLaser
 *        documentation
 *          - PATHNUMB: Integer number for identifying the shape (ie if the first shape for previous frame disappeared,
 *            MadMapper can anyway know this shape corresponds to a shape in previous frame using this identifier, it
 *            might be used for instance in dispatching algorithms)
 *          - MAXSPEED: Floating point number defining the maximum laser scan speed for this shape. Normal value is 1.0
 *            and is a compromise between scan speed and scan accuracy at a reasonable projection angle. A value of 2 will
 *            tell MadMapper it can scan twice faster when rendering this shape.
 *          - SKIPBLCK: Boolean value to tell MadMapper it should or not skip scanning "long" black sections of the path
 *          - PRESRVOR: Boolean value to tell MadMapper it should not looks for the best rendering order for the pathes
 *            transmitted by this media, but those pathes should be rendered in the order they are received.
 *          - ANGLEOPT: Boolean value to tell MadMapper we want angle optimization or not
 *          - ANGLETHR: Floating point value - minimum angle in degrees for activating angle optimization (between 22.5 & 90)
 *          - ANGLEMXD: Floating point value - maximum time to spend on angle points
 *          - FRSTPNTR: Integer - First Point Repeat
 *          - LASTPNTR: Integer - Last Point Repeat
 *          - POLYFADI: Floating point value - how much we should decrease luminosity when starting scanning a path (0.0-8.0)
 *          - POLYFADO: Floating point value - how much we should decrease luminosity when ending scanning a path (0.0-0.1)
 *          - MINIPNTS: Integer - minimum number of ILDA points that should be generated to render this path (allows to make
 *            a small path very lightened and a long path less, normally luminosity (= scanning time) is dispatched depending
 *            on the path length (except for single beam)
 *          - SOFTCLOS: Integer - number of ILDA points we should use for "edge blend" start & end of a closed shape
 *          - SNGLPTIN: Integer - number of ILDA points we should use for this shape if it's a single beam (a single position or
 *            at least all points are at the same position)
 *
 *      - ... Other software to be added
*/

// Header String
#define PONK_HEADER_STRING "PONK-UDP"
// Protocol Version
#define PONK_PROTOCOL_VERSION 0
// Data Formats
#define PONK_DATA_FORMAT_XYRGB_U16 0
#define PONK_DATA_FORMAT_XY_F32_RGB_U8 1
// Maximum chunk size
#define PONK_MAX_DATA_BYTES_PER_PACKET 8192
// Geom UDP port = 5583
#define PONK_PORT 5583

#ifdef _MSC_VER
    // ms VC .NET
    #pragma pack( push, before_definition )
    #pragma pack(1)
    #define ATTRIBUTE_PACKED ;
#else
    // gcc
    #ifdef ATTRIBUTE_PACKED
        #undef ATTRIBUTE_PACKED
    #endif
    #define ATTRIBUTE_PACKED __attribute__( ( packed ) )
#endif

struct GeomUdpHeader {
    char headerString[8];           // = "PONK-UDP"
    unsigned char protocolVersion;  // 0 at the moment
    unsigned int senderIdentifier;  // 4 bytes - used to identify the source, so when changing name in sender, the receiver can just rename existing stream
    char senderName[32];            // 32 bytes UTF8 null terminated string
    unsigned char frameNumber;      // Increase by one on each frame
    unsigned char chunkCount;       // Number of chunks in this frame
    unsigned char chunkNumber;      // Number of this chunk
    unsigned int dataCrc;           // CRC of all data in the frame (data from  chunks, to detect network transmission issues)
} ATTRIBUTE_PACKED;

struct GeomUdpMetaData {
    char name[8];                   // EightCC (64 bits / 8 bytes), ie "POLYNUMB"
    char value[4];                  // 4 bytes for value, must be casted to int / bool / float
} ATTRIBUTE_PACKED;

struct GeomUdpPathData {
    unsigned short pointCount;      // Point Count
    // Data: XYRGBXYRGB....
} ATTRIBUTE_PACKED;

struct GeomUdpPath {
    unsigned char metaDataCount;    // Number of meta data
    // Data: N x GeomUdpMetaData, then N x GeomUdpPathData
} ATTRIBUTE_PACKED;

struct GeomUdpPacketData {
    unsigned char dataFormat;       // ie: PONK_DATA_FORMAT_XY_F32_RGB_U8
    // Data: N x GeomUdpPath
} ATTRIBUTE_PACKED;

#if defined(_MSC_VER)
    #pragma pack( pop, before_definition )
#endif

#undef ATTRIBUTE_PACKED
