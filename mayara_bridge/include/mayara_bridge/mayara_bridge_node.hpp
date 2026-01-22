#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <rclcpp/rclcpp.hpp>
#include <marine_sensor_msgs/msg/radar_sector.hpp>
#include "websocket_client.hpp"
#include "protobuf_parser.hpp"

namespace mayara_bridge
{

class MayaraBridgeNode : public rclcpp::Node
{
public:
  MayaraBridgeNode();
  ~MayaraBridgeNode();

private:
  // Parameters
  std::string mayara_host_;
  int mayara_port_;
  std::string radar_id_;
  std::string frame_id_;
  std::string topic_name_;
  double range_min_;
  double range_max_;

  // ROS 2 publisher
  rclcpp::Publisher<marine_sensor_msgs::msg::RadarSector>::SharedPtr publisher_;

  // WebSocket client
  std::unique_ptr<WebSocketClient> ws_client_;

  // Thread for WebSocket
  std::thread ws_thread_;
  std::atomic<bool> running_;

  // Radar metadata
  int spokes_per_revolution_;
  int max_spoke_len_;

  // Callbacks
  void onWebSocketMessage(const std::vector<uint8_t>& data);
  void onRadarInfoReceived(int spokes_per_rev, int max_len);
  void processRadarMessage(const RadarMessage& msg);
  marine_sensor_msgs::msg::RadarSector convertToRadarSector(
    const std::vector<Spoke>& spokes);
  
  // Fetch radar info from REST API
  void fetchRadarInfo();
};

} // namespace mayara_bridge
