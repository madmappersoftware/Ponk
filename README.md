# Ponk
Open Protocol to transmit **P**athes **O**ver **N**etwor**K**

PONK (**P**athes **O**ver **N**etwor**K**) is a minimal protocol to transfer 2D colored pathes from a source to a receiver. It has been developped to transfer laser path from a software to another over network using UDP.

[![Demo](/Assets/youtube_demo_link_image.jpg)](https://www.youtube.com/watch?v=VmzsDqeO2RQ)

## Requirements are:
- Transfer over network of frames composed of 2D geometry path with colors along the path
- Make it simple to implement on both sides
- Make it work on almost any network - no specific hardware or OS requirement
- Make it extensible while not increasing bandwidth in most common cases
- Avoid using unecessary bandwidth:
  - For laser, having more than 8 bits per component colors is mostly useless (laser projector diodes have a very poor definition in low values anyway)
  - Support multiple formats to adapt bandwidth to project requirements
  - If the frame to transmit is too long to fit a single UDP packet, it should be cut in multiple chunks. GEOM_UDP_MAX_DATA_BYTES_PER_PACKET is set to 8192 by default. This value should make it work on all popular OS.
  - The sender should be able to attach "meta data" to each path transmitted, that the receiver might handle for specific behaviors. When rasterizing a path for laser rendering user might give some hints like "should we favor scan speed or render precision ?"
  - Receiver must be able to detect network issue and ignore a frame if something when wrong (CRC)

## Transport
 - Data is transmitted over UDP multicast or unicast. By default multicast is preferred because it allows multiple  softwares running on same computer to receive the packets. With unicast, first socket bound on the port will eat the packets and software started after will not receive any data.
 But since some network switches don't handle multicast well, and unicast performances are better, there should be an option to switch to unicast.
 So the expected behavior on both side is:
   - Sender should be in multicast by default, with an option to switch to unicast and enter the target IP
   - Receiver should subscribe to multicast address (setsockopt / IP_ADD_MEMBERSHIP), it will then receive packets coming through multicast or through unicast

## Implementation in Sender
- A software can instanciate multiple senders (different streams)
- A sender is identified by a 32 bits number which can be a random generated value when instanciating the sending component
- It also transmits a "sender name" string (UTF8) that can be used to display a readable source name is the receiving application.
- The stream identifier should not change when reloading a project file so the receiver canreconnect the stream. Changing the stream name should not affect existing connection (the receiver must use the integer identifier, the string is used to display a meaningful text only)

## Implementation in Receiver
- The receiver should handle the fact that multiple senders can be sending packets.
- It should be as reactive as possible to reduce latency and improve synchronization.
- The receiver should accept any packet size (chunk size can be adjusted on sender side)
- The receiver should at least support data format "GEOM_UDP_DATA_FORMAT_XY_F32_RGB_U8"
- If the receiver doesn't handle sender data type, it should notify it in the user interface in some way
- Protocol Version field in the packets shouldn't be ignored: if the specified protocol version is not handled, the receiver should ignore the packet and notify the user that it is not compatible with sender for this reason (protocol version will be increased only if breaking compatibility, not if we decide too add a new data format)

## Meta Data Format
- Each path can have a list of meta data attached
- A meta data is identified by 8 characters (the key)
- Meta data value is a 32 bits floating point number,
    if value should be an boolean (ie "optimize angles"), any value different of zero is considered true
    if value should be an integer (ie "min ilda points"), value should be rounded to nearest int
    if value should be a floating point, no problem

## Possible improvements / extensions:
- Synchronization
  - If the receiver notify sender that it has used the last received frame, the sender could adjust to the receiver framerate... (since the sender doesn't know how long it will take to the laser to travel the path, but receiver might know)
- New Data Formats:
  - XY_U16_SingleRGB: if you send a path of 2000 points with the same color, we could reduce bandwidth a lot by removing color for each point
  - XYRGBU1U2U3: would be useful to control additional diodes (ie yellow, deep blue...)
  - XYZRGB: providing the Z would let the user handle the 3D->2D projection with a controllable camera in the receiver

## Packet Format:
- Header:
  - Header String - char[8]: "PONK-UDP"
  - Protocol Version - char: 0
  - Sender Indetifier - 32 bits int
  - Sender Name - char[32]
  - Frame Number - unsigned char: incremented on each frame
  - Chunk Count - unsigned char
  - Chunk Number - unsigned char
  - CRC - unsigned int: sum of all data contained in this frame (of all chunks)
- Data:
  - For each path:
    - Data format - unsigned char (GEOM_UDP_DATA_FORMAT_XY_F32_RGB_U8...)
    - Meta Data count - unsigned char
    - For each Meta Data:
      - Key - char[8]
      - Value - 32 bits float
    - Point Count - unsigned short (16 bits)
    - For each point
      - Point data, depending on data format, ie X,Y as float 32, R,G,B as unsigned char

## List of Meta Data support by:

- MadMapper / MadLaser: most of those parameters can be adjusted at surface level. Adding meta data will override settings set at surface level for the path it is attached to. Those parameters are documented in MadLaser documentation
  - PATHNUMB: Integer number for identifying the shape (ie if the first shape for previous frame disappeared, MadMapper can anyway know this shape corresponds to a shape in previous frame using this identifier, it might be used for instance in dispatching algorithms)
  - MAXSPEED: Floating point number defining the maximum laser scan speed for this shape. Normal value is 1.0 and is a compromise between scan speed and scan accuracy at a reasonable projection angle. A value of 2 will tell MadMapper it can scan twice faster when rendering this shape.
  - FIXSPEED: Floating point number defining the speed at which we will travel this path with the laser beam. Normal value is 1.0 and is a compromise between scan speed and scan accuracy at a reasonable projection angle. A value of 2 will tell MadMapper to scan twice faster when rendering this shape.
  - SKIPBLCK: Boolean value to tell MadMapper it should or not skip scanning "long" black sections of the path
  - PRESRVOR: Boolean value to tell MadMapper it should not looks for the best rendering order for the pathes transmitted by this media, but those pathes should be rendered in the order they are received.
  - ANGLEOPT: Boolean value to tell MadMapper we want angle optimization or not
  - ANGLETHR: Floating point value - minimum angle in degrees for activating angle optimization (between 22.5 & 90)
  - ANGLEMXD: Floating point value - maximum time to spend on angle points
  - FRSTPNTR: Integer - First Point Repeat
  - LASTPNTR: Integer - Last Point Repeat
  - POLYFADI: Floating point value - how much we should decrease luminosity when starting scanning a path (0.0-8.0)
  - POLYFADO: Floating point value - how much we should decrease luminosity when ending scanning a path (0.0-0.1)
  - MINIPNTS: Integer - minimum number of ILDA points that should be generated to render this path (allows to make a small path very lightened and a long path less, normally luminosity (= scanning time) is dispatched depending on the path length (except for single beam)
  - SOFTCLOS: Integer - number of ILDA points we should use for "edge blend" start & end of a closed shape
  - SNGLPTIN: Integer - number of ILDA points we should use for this shape if it's a single beam (a single position or at least all points are at the same position) - 0 for automatic

- ... Other software to be added
