#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace mayara_bridge
{

struct Spoke
{
  uint32_t angle = 0;
  uint32_t bearing = 0;  // Optional, 0 means not set
  uint32_t range = 0;
  uint64_t time = 0;     // Optional, 0 means not set
  int64_t lat = 0;       // Optional, 0 means not set
  int64_t lon = 0;       // Optional, 0 means not set
  std::vector<uint8_t> data;
};

struct RadarMessage
{
  uint32_t radar = 0;
  std::vector<Spoke> spokes;
};

class ProtobufParser
{
public:
  static RadarMessage parseRadarMessage(const std::vector<uint8_t>& data);
  static Spoke parseSpoke(const std::vector<uint8_t>& data);

private:
  static uint64_t decodeVarint(const std::vector<uint8_t>& data, size_t& offset);
  static std::vector<uint8_t> decodeLengthDelimited(const std::vector<uint8_t>& data, size_t& offset);
  static int64_t decodeZigzag(uint64_t val);
};

} // namespace mayara_bridge
