
import dgram from 'dgram';

let TARGET_IP = '127.0.0.1';
let PONK_HEADER_STRING = "PONK-UDP"
let PONK_PROTOCOL_VERSION = 0
let PONK_DATA_FORMAT_XYRGB_U16 = 0
let PONK_DATA_FORMAT_XY_F32_RGB_U8 = 1
let PONK_MAX_CHUNK_SIZE = 1472
let PONK_PORT = 5583
let PONK_IDENTIFIER = 1
let PONK_SERVER_NAME = "SimplePonk"
let frameCount = 0
var sender;

export default { connect, sendFrame, setServerName, setPath, setPathWithColors, setVertices, circle, line, lineWithColors, rect, triangle, point }


class GeomUdpHeader {
  constructor(buffer) {
    if (buffer) {
      this.fromBuffer(buffer);
    } else {
      this.headerString = PONK_HEADER_STRING;
      this.protocolVersion = PONK_PROTOCOL_VERSION;
      this.senderIdentifier = PONK_IDENTIFIER;
      this.senderName = PONK_SERVER_NAME;
      this.frameNumber = 0;
      this.chunkCount = 0;
      this.chunkNumber = 0;
      this.dataCrc = 0;
    }
  }
  static SIZE = 52;

  fromBuffer(buffer) {
    if (buffer.length < GeomUdpHeader.SIZE) {
      throw new Error("Buffer too small for GeomUdpHeader");
    }
    this.headerString = buffer.toString("utf8", 0, 8).replace(/\0.*$/, '');
    this.protocolVersion = buffer.readUInt8(8);
    this.senderIdentifier = buffer.readUInt32LE(9);
    this.senderName = buffer.toString("utf8", 13, 45).replace(/\0.*$/, '');
    this.frameNumber = buffer.readUInt8(45);
    this.chunkCount = buffer.readUInt8(46);
    this.chunkNumber = buffer.readUInt8(47);
    this.dataCrc = buffer.readUInt32LE(48);
  }

  toBuffer() {
    const buffer = Buffer.alloc(GeomUdpHeader.SIZE);

    buffer.write(this.headerString.padEnd(8, '\0'), 0, 'utf8');
    buffer.writeUInt8(this.protocolVersion, 8);
    buffer.writeUInt32LE(this.senderIdentifier, 9);
    buffer.write(this.senderName.padEnd(32, '\0'), 13, 'utf8');
    buffer.writeUInt8(this.frameNumber, 45);
    buffer.writeUInt8(this.chunkCount, 46);
    buffer.writeUInt8(this.chunkNumber, 47);
    buffer.writeUInt32LE(this.dataCrc, 48);

    return buffer;
  }
}

function setServerName(name, identifier = 1) {
  PONK_IDENTIFIER = identifier
  PONK_SERVER_NAME = name
}


function setPath(frameData, pathIndex, points, red = 255, green = 255, blue = 255) {
  push8bits(frameData, PONK_DATA_FORMAT_XY_F32_RGB_U8); // Write Format Data
  push8bits(frameData, 2); // Write meta data count
  pushMetaData(frameData, "PATHNUMB", Math.round(pathIndex));
  pushMetaData(frameData, "MAXSPEED", 0.1);
  push16bits(frameData, points.length);

  for (let point in points) {
    pushPoint(frameData, points[point][0], points[point][1], red, green, blue);
  }
}

function setPathWithColors(frameData, pathIndex, points, colors) {
  push8bits(frameData, PONK_DATA_FORMAT_XY_F32_RGB_U8); // Write Format Data
  push8bits(frameData, 2); // Write meta data count
  pushMetaData(frameData, "PATHNUMB", Math.round(pathIndex));
  pushMetaData(frameData, "MAXSPEED", 0.1);
  push16bits(frameData, points.length);

  for (let point in points) {
    pushPoint(frameData, points[point][0], points[point][1], colors[point].r, colors[point].g, colors[point].b);
  }
}


function setVertices(frameData, pathIndex, vertices, position, rotation, red = 255, green = 255, blue = 255, close = true, scaleWidth = 1, scaleHeight = 1) {
  push8bits(frameData, PONK_DATA_FORMAT_XY_F32_RGB_U8); // Write Format Data
  push8bits(frameData, 2); // Write meta data count
  pushMetaData(frameData, "PATHNUMB", Math.round(pathIndex));
  pushMetaData(frameData, "MAXSPEED", 0.1);
  push16bits(frameData, close ? vertices.length + 1 : vertices.length);

  for (let i = 0; i < vertices.length; i++) {
    let vertex = vertices[i]
    vertex = rotatePoint(vertex, rotation)
    vertex = translatePoint(vertex, position.x, position.y)
    vertex = scalePoint(vertex, scaleWidth, scaleHeight, 1, 1)
    pushPoint(frameData, vertex.x, vertex.y, red, green, blue);
  }
  if (close) {
    let vertex = vertices[0]
    vertex = rotatePoint(vertex, rotation)
    vertex = translatePoint(vertex, position.x, position.y)
    vertex = scalePoint(vertex, scaleWidth, scaleHeight, 1, 1)
    pushPoint(frameData, vertex.x, vertex.y, red, green, blue);
  }

}

function circle(frameData, pathIndex, cX, cY, radius, segmentsCount = 30, red = 255, green = 255, blue = 255) {
  let circlePoints = []
  for (let i = 0; i < segmentsCount + 1; i++) {
    let angularPosition = i / (segmentsCount + 1 - 1)
    let x = (cX + radius * Math.cos(angularPosition * 2 * Math.PI))
    let y = (cY + radius * Math.sin(angularPosition * 2 * Math.PI))
    circlePoints.push([x, y])
  }
  setPath(frameData, pathIndex, circlePoints, red, green, blue)
}

function line(frameData, pathIndex, x1, y1, x2, y2, red = 255, green = 255, blue = 255) {
  let startPoint = [x1, y1];
  let endPoint = [x2, y2];
  setPath(frameData, pathIndex, [startPoint, endPoint], red, green, blue)
}


function lineWithColors(frameData, pathIndex, x1, y1, x2, y2, sRed = 255, sGreen = 255, sBlue = 255, eRed = 255, eGreen = 255, eBlue = 255) {
  let startPoint = [x1, y1];
  let endPoint = [x2, y2];
  setPathWithColors(frameData, pathIndex, [startPoint, endPoint], [{ r: sRed, g: sGreen, b: sBlue }, { r: eRed, g: eGreen, b: eBlue }])
}

function triangle(frameData, pathIndex, cx, cy, r, red = 255, green = 255, blue = 255) {
  let x1 = cx - r
  let y1 = cy + r
  let x2 = cx
  let y2 = cy - r
  let x3 = cx + r
  let y3 = cy + r
  setVertices(frameData, pathIndex, [{ x: x1, y: y1 }, { x: x2, y: y2 }, { x: x3, y: y3 }], { x: cx, y: cy }, Math.random(), red, green, blue, true)
}

function rect(frameData, pathIndex, x, y, w, h, red = 255, green = 255, blue = 255) {
  setPath(frameData, pathIndex, [[x, y], [x + w, y], [x + w, y + h], [x, y + h]], red, green, blue)
}

function point(frameData, pathIndex, x, y, red = 255, green = 255, blue = 255) {
  push8bits(frameData, PONK_DATA_FORMAT_XY_F32_RGB_U8); // Write Format Data
  push8bits(frameData, 2); // Write meta data count
  pushMetaData(frameData, "PATHNUMB", pathIndex * 1.0);
  pushMetaData(frameData, "SNGLPTIN", 1); //nb point for beam strike (0 = default)
  push16bits(frameData, 1);
  pushPoint(frameData, x, y, red, green, blue);
}

function sendFrame(frameData, senderName, senderId) {
  //compute chunck counts 
  let chunksCount64 = 1 + Math.floor(frameData.length / (PONK_MAX_CHUNK_SIZE - GeomUdpHeader.SIZE));
  if (chunksCount64 > 255) {
    console.error("Protocol doesn't accept sending a packet that would be splitted in more than 255 chunks");
  }

  let crc = 0;
  // Compute data CRC
  for (let v = 0; v < frameData.length; v++) {
    crc += frameData[v];
  }
  // Send all chunks to the desired IP address
  let written = 0;
  let chunkNumber = 0;
  let chunksCount = (chunksCount64);

  while (written < frameData.length) {

    const newHeader = new GeomUdpHeader();
    newHeader.frameNumber = frameCount % 255;
    newHeader.chunkCount = chunksCount;
    newHeader.chunkNumber = chunkNumber;
    newHeader.dataCrc = crc;

    if (senderName) {
      newHeader.senderName = senderName;
    }
    if (senderId) {
      newHeader.senderIdentifier = senderId;
    }

    // Prepare buffer
    let packetChuncks = [];
    let dataBytesForThisChunk = Math.min(frameData.length - written, PONK_MAX_CHUNK_SIZE - GeomUdpHeader.SIZE);

    //write header
    packetChuncks.push(newHeader.toBuffer());

    //write data
    let frameDataBuffer = Buffer.from(frameData);
    packetChuncks.push(frameDataBuffer.subarray(written, written + dataBytesForThisChunk));
    written += dataBytesForThisChunk;


    const finalBuffer = Buffer.concat(packetChuncks);
    // printArray(packetChuncks, 0, 52, "HEADER")
    // printArray(packetChuncks, 52, 124, "CORE")

    sender.send(finalBuffer, PONK_PORT, TARGET_IP, (err) => {
      if (err) {
        console.error(`Send error: ${err.message}`);
      }
    });
    chunkNumber++;
  }
  frameCount++;
}


function printArray(array, startIndex, endIndex, name) {
  const finalBuffer = Buffer.concat(array);
  console.log(name, finalBuffer.length, "->")
  let hexString = finalBuffer.toString("hex");
  let hexStringWithSpaces = "";
  for (let i = startIndex * 2; i < endIndex * 2; i += 2) {
    hexStringWithSpaces += hexString.substring(i, i + 2) + " ";
  }
  console.log(hexStringWithSpaces);
}

function translatePoint(point, dx, dy) {
  return { x: point.x + dx, y: point.y + dy }
}

function rotatePoint(point, angle) {
  return { x: point.x * Math.cos(angle) - point.y * Math.sin(angle), y: point.x * Math.sin(angle) + point.y * Math.cos(angle) }
}

function scalePoint(point, originX, originY, destX, destY) {
  return { x: point.x / originX * destX, y: point.y / originY * destY }
}

function push8bits(buffer, value) {
  return buffer.push(value & 0xFF);
}

function push16bits(buffer, value) {
  const temp = Buffer.alloc(2);
  temp.writeUInt16LE(value & 0xFFFF); // Keep only lower 16 bits
  return buffer.push(temp[0], temp[1]);
}

function push32bits(buffer, value) {
  const temp = Buffer.alloc(4);
  temp.writeFloatLE(value); // Ensure unsigned 32-bit
  return buffer.push(temp[0], temp[1], temp[2], temp[3]);
}

function pushMetaData(fullData, eightCC, value) {
  if (eightCC.length !== 8) {
    throw new Error("eightCC must be exactly 8 characters");
  }
  for (let i = 0; i < 8; i++) {
    push8bits(fullData, eightCC.charCodeAt(i));
  }
  push32bits(fullData, value);
}

function pushPoint(fullData, x, y, r, g, b) {
  if (x >= -1 && x <= 1 && y >= -1 && y <= 1) {
    push32bits(fullData, x);
    push32bits(fullData, y);
    push8bits(fullData, r);
    push8bits(fullData, g);
    push8bits(fullData, b);
  }
  else {
    console.log("point out of bound [-1,1], skipping ", x, y, r, g, b)
  }
}

function connect() {
  sender = dgram.createSocket('udp4');
  sender.bind(() => {
    sender.setMulticastTTL(1);
    sender.setMulticastLoopback(false);
  });
}
