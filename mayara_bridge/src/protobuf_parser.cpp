#include "mayara_bridge/protobuf_parser.hpp"
#include <stdexcept>

namespace mayara_bridge
{

uint64_t ProtobufParser::decodeVarint(const std::vector<uint8_t>& data, size_t& offset)
{
  uint64_t result = 0;
  int shift = 0;
  
  while (offset < data.size())
  {
    uint8_t byte = data[offset++];
    result |= static_cast<uint64_t>(byte & 0x7F) << shift;
    
    if ((byte & 0x80) == 0)
    {
      break;
    }
    shift += 7;
  }
  
  return result;
}

std::vector<uint8_t> ProtobufParser::decodeLengthDelimited(const std::vector<uint8_t>& data, size_t& offset)
{
  uint64_t length = decodeVarint(data, offset);
  std::vector<uint8_t> result;
  
  if (offset + length <= data.size())
  {
    result.assign(data.begin() + offset, data.begin() + offset + length);
    offset += length;
  }
  
  return result;
}

int64_t ProtobufParser::decodeZigzag(uint64_t val)
{
  return static_cast<int64_t>((val >> 1) ^ (-static_cast<int64_t>(val & 1)));
}

Spoke ProtobufParser::parseSpoke(const std::vector<uint8_t>& data)
{
  Spoke spoke;
  size_t offset = 0;
  
  while (offset < data.size())
  {
    uint64_t tag = decodeVarint(data, offset);
    if (offset >= data.size()) break;
    
    uint32_t field_number = static_cast<uint32_t>(tag >> 3);
    uint32_t wire_type = static_cast<uint32_t>(tag & 0x7);
    
    switch (field_number)
    {
      case 1: // angle (uint32)
        if (wire_type == 0)
        {
          spoke.angle = static_cast<uint32_t>(decodeVarint(data, offset));
        }
        break;
        
      case 2: // bearing (optional uint32)
        if (wire_type == 0)
        {
          spoke.bearing = static_cast<uint32_t>(decodeVarint(data, offset));
        }
        break;
        
      case 3: // range (uint32)
        if (wire_type == 0)
        {
          spoke.range = static_cast<uint32_t>(decodeVarint(data, offset));
        }
        break;
        
      case 4: // time (optional uint64)
        if (wire_type == 0)
        {
          spoke.time = decodeVarint(data, offset);
        }
        break;
        
      case 5: // data (bytes)
        if (wire_type == 2)
        {
          spoke.data = decodeLengthDelimited(data, offset);
        }
        break;
        
      case 6: // lat (optional int64)
        if (wire_type == 0)
        {
          uint64_t val = decodeVarint(data, offset);
          spoke.lat = decodeZigzag(val);
        }
        break;
        
      case 7: // lon (optional int64)
        if (wire_type == 0)
        {
          uint64_t val = decodeVarint(data, offset);
          spoke.lon = decodeZigzag(val);
        }
        break;
        
      default:
        // Skip unknown fields
        if (wire_type == 0)
        {
          decodeVarint(data, offset);
        }
        else if (wire_type == 2)
        {
          decodeLengthDelimited(data, offset);
        }
        else if (wire_type == 1)
        {
          offset += 8;
        }
        else if (wire_type == 5)
        {
          offset += 4;
        }
        break;
    }
  }
  
  return spoke;
}

RadarMessage ProtobufParser::parseRadarMessage(const std::vector<uint8_t>& data)
{
  RadarMessage msg;
  size_t offset = 0;
  
  while (offset < data.size())
  {
    uint64_t tag = decodeVarint(data, offset);
    if (offset >= data.size()) break;
    
    uint32_t field_number = static_cast<uint32_t>(tag >> 3);
    uint32_t wire_type = static_cast<uint32_t>(tag & 0x7);
    
    if (field_number == 1) // radar (uint32)
    {
      if (wire_type == 0)
      {
        msg.radar = static_cast<uint32_t>(decodeVarint(data, offset));
      }
    }
    else if (field_number == 2) // spokes (repeated Spoke)
    {
      if (wire_type == 2)
      {
        std::vector<uint8_t> spoke_data = decodeLengthDelimited(data, offset);
        Spoke spoke = parseSpoke(spoke_data);
        msg.spokes.push_back(spoke);
      }
    }
    else
    {
      // Skip unknown fields
      if (wire_type == 0)
      {
        decodeVarint(data, offset);
      }
      else if (wire_type == 2)
      {
        decodeLengthDelimited(data, offset);
      }
      else if (wire_type == 1)
      {
        offset += 8;
      }
      else if (wire_type == 5)
      {
        offset += 4;
      }
    }
  }
  
  return msg;
}

} // namespace mayara_bridge
