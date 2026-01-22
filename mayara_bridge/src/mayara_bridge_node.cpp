#include "mayara_bridge/mayara_bridge_node.hpp"
#include "mayara_bridge/protobuf_parser.hpp"
#include <rclcpp/rclcpp.hpp>
#include <cmath>
#include <algorithm>

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

// JSON parsing - use simple approach if nlohmann not available
#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
// Simple JSON parser fallback (minimal implementation)
#include <sstream>
#include <map>
#endif

namespace mayara_bridge
{

#ifdef HAVE_CURL
// Helper for HTTP requests
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data)
{
  data->append((char*)contents, size * nmemb);
  return size * nmemb;
}
#endif

MayaraBridgeNode::MayaraBridgeNode()
  : Node("mayara_bridge")
  , running_(false)
  , spokes_per_revolution_(2048)
  , max_spoke_len_(1024)
{
  // Declare parameters
  this->declare_parameter<std::string>("mayara_host", "localhost");
  this->declare_parameter<int>("mayara_port", 6502);
  this->declare_parameter<std::string>("radar_id", "radar-0");
  this->declare_parameter<std::string>("frame_id", "radar");
  this->declare_parameter<std::string>("topic_name", "data");
  this->declare_parameter<double>("range_min", 0.0);
  this->declare_parameter<double>("range_max", 10000.0);

  // Get parameters
  mayara_host_ = this->get_parameter("mayara_host").as_string();
  mayara_port_ = this->get_parameter("mayara_port").as_int();
  radar_id_ = this->get_parameter("radar_id").as_string();
  frame_id_ = this->get_parameter("frame_id").as_string();
  topic_name_ = this->get_parameter("topic_name").as_string();
  range_min_ = this->get_parameter("range_min").as_double();
  range_max_ = this->get_parameter("range_max").as_double();

  // Create publisher
  publisher_ = this->create_publisher<marine_sensor_msgs::msg::RadarSector>(topic_name_, 10);

  RCLCPP_INFO(this->get_logger(), "Mayara Bridge Node initialized");
  RCLCPP_INFO(this->get_logger(), "  Host: %s:%d", mayara_host_.c_str(), mayara_port_);
  RCLCPP_INFO(this->get_logger(), "  Radar ID: %s", radar_id_.c_str());
  RCLCPP_INFO(this->get_logger(), "  Frame ID: %s", frame_id_.c_str());
  RCLCPP_INFO(this->get_logger(), "  Topic: %s", topic_name_.c_str());

  // Fetch radar info and connect
  fetchRadarInfo();
}

MayaraBridgeNode::~MayaraBridgeNode()
{
  running_ = false;
  if (ws_client_)
  {
    ws_client_->disconnect();
  }
  if (ws_thread_.joinable())
  {
    ws_thread_.join();
  }
}

void MayaraBridgeNode::fetchRadarInfo()
{
  std::string url = "http://" + mayara_host_ + ":" + std::to_string(mayara_port_) + "/v1/api/radars";
  std::string response_data;
  
#ifdef HAVE_CURL
  CURL* curl = curl_easy_init();
  if (!curl)
  {
    RCLCPP_ERROR(this->get_logger(), "Failed to initialize CURL");
    return;
  }
  
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  
  CURLcode res = curl_easy_perform(curl);
  
  if (res != CURLE_OK)
  {
    RCLCPP_ERROR(this->get_logger(), "Failed to fetch radar info: %s", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return;
  }
  
  curl_easy_cleanup(curl);
#else
  // Fallback: use simple HTTP request or file read
  RCLCPP_WARN(this->get_logger(), "CURL not available. Cannot fetch radar info automatically.");
  RCLCPP_INFO(this->get_logger(), "Please install libcurl4-openssl-dev or manually configure radar settings.");
  return;
#endif

  try
  {
#ifdef HAVE_NLOHMANN_JSON
    auto json_data = json::parse(response_data);
    
    if (json_data.contains(radar_id_))
    {
      auto radar_info = json_data[radar_id_];
      spokes_per_revolution_ = radar_info.value("spokes_per_revolution", 2048);
      max_spoke_len_ = radar_info.value("maxSpokeLen", 1024);
      std::string stream_url = radar_info.value("streamUrl", "");
#else
    // Simple JSON parsing fallback - just extract basic info
    // This is a minimal implementation - for production use nlohmann-json
    size_t id_pos = response_data.find(radar_id_);
    if (id_pos != std::string::npos)
    {
      // Extract spokes_per_revolution
      size_t spokes_pos = response_data.find("spokes_per_revolution", id_pos);
      if (spokes_pos != std::string::npos)
      {
        size_t colon = response_data.find(":", spokes_pos);
        size_t comma = response_data.find(",", colon);
        if (colon != std::string::npos)
        {
          std::string val = response_data.substr(colon + 1, comma - colon - 1);
          spokes_per_revolution_ = std::stoi(val);
        }
      }
      
      // Extract maxSpokeLen
      size_t max_pos = response_data.find("maxSpokeLen", id_pos);
      if (max_pos != std::string::npos)
      {
        size_t colon = response_data.find(":", max_pos);
        size_t comma = response_data.find(",", colon);
        if (colon != std::string::npos)
        {
          std::string val = response_data.substr(colon + 1, comma - colon - 1);
          max_spoke_len_ = std::stoi(val);
        }
      }
      
      // Extract streamUrl
      std::string stream_url;
      size_t url_pos = response_data.find("streamUrl", id_pos);
      if (url_pos != std::string::npos)
      {
        size_t quote1 = response_data.find("\"", url_pos + 10);
        size_t quote2 = response_data.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos)
        {
          stream_url = response_data.substr(quote1 + 1, quote2 - quote1 - 1);
        }
      }
#endif
      
      // Convert http:// to ws://
      size_t pos = stream_url.find("http://");
      if (pos != std::string::npos)
      {
        stream_url.replace(pos, 7, "ws://");
      }
      pos = stream_url.find("https://");
      if (pos != std::string::npos)
      {
        stream_url.replace(pos, 8, "wss://");
      }
      
      RCLCPP_INFO(this->get_logger(), "Found radar: %s", radar_id_.c_str());
      RCLCPP_INFO(this->get_logger(), "  Spokes per revolution: %d", spokes_per_revolution_);
      RCLCPP_INFO(this->get_logger(), "  Max spoke length: %d", max_spoke_len_);
      RCLCPP_INFO(this->get_logger(), "  Stream URL: %s", stream_url.c_str());
      
      // Create WebSocket client and connect
      ws_client_ = std::make_unique<WebSocketClient>(stream_url);
      ws_client_->setMessageCallback(
        [this](const std::vector<uint8_t>& data) {
          this->onWebSocketMessage(data);
        });
      
      running_ = true;
      ws_thread_ = std::thread([this]() {
        if (ws_client_->connect())
        {
          RCLCPP_INFO(this->get_logger(), "WebSocket connected");
        }
      });
    }
    else
    {
      RCLCPP_ERROR(this->get_logger(), "Radar %s not found in response", 
                   radar_id_.c_str());
    }
#endif
  }
  catch (const std::exception& e)
  {
    RCLCPP_ERROR(this->get_logger(), "Failed to parse radar info: %s", e.what());
  }
}

void MayaraBridgeNode::onWebSocketMessage(const std::vector<uint8_t>& data)
{
  try
  {
    RadarMessage msg = ProtobufParser::parseRadarMessage(data);
    processRadarMessage(msg);
  }
  catch (const std::exception& e)
  {
    RCLCPP_ERROR(this->get_logger(), "Failed to process WebSocket message: %s", e.what());
  }
}

void MayaraBridgeNode::processRadarMessage(const RadarMessage& msg)
{
  if (msg.spokes.empty())
  {
    return;
  }
  
  // Convert spokes to RadarSector
  marine_sensor_msgs::msg::RadarSector sector = convertToRadarSector(msg.spokes);
  
  // Publish
  publisher_->publish(sector);
}

marine_sensor_msgs::msg::RadarSector MayaraBridgeNode::convertToRadarSector(
  const std::vector<Spoke>& spokes)
{
  marine_sensor_msgs::msg::RadarSector sector;
  
  // Header
  sector.header.stamp = this->now();
  sector.header.frame_id = frame_id_;
  
  if (spokes.empty())
  {
    return sector;
  }
  
  // Sort spokes by angle
  std::vector<Spoke> sorted_spokes = spokes;
  std::sort(sorted_spokes.begin(), sorted_spokes.end(),
            [](const Spoke& a, const Spoke& b) { return a.angle < b.angle; });
  
  // Angle information
  uint32_t first_angle = sorted_spokes.front().angle;
  uint32_t last_angle = sorted_spokes.back().angle;
  
  sector.angle_start = (first_angle / static_cast<double>(spokes_per_revolution_)) * 2.0 * M_PI;
  sector.angle_increment = (1.0 / static_cast<double>(spokes_per_revolution_)) * 2.0 * M_PI;
  
  // Range information
  uint32_t max_range = 0;
  for (const auto& spoke : sorted_spokes)
  {
    if (spoke.range > max_range)
    {
      max_range = spoke.range;
    }
  }
  
  sector.range_min = range_min_;
  sector.range_max = std::max(static_cast<double>(max_range), range_max_);
  
  // Convert spokes to intensities
  sector.intensities.clear();
  for (const auto& spoke : sorted_spokes)
  {
    marine_sensor_msgs::msg::RadarSector::Intensity intensity;
    
    if (spoke.data.empty())
    {
      // Create empty echoes
      size_t num_bins = static_cast<size_t>((spoke.range - range_min_) / 10.0);
      intensity.echoes.assign(num_bins, 0.0f);
    }
    else
    {
      // Convert bytes to float intensities (normalize 0-255 to 0.0-1.0)
      intensity.echoes.reserve(spoke.data.size());
      for (uint8_t byte : spoke.data)
      {
        intensity.echoes.push_back(static_cast<float>(byte) / 255.0f);
      }
    }
    
    sector.intensities.push_back(intensity);
  }
  
  return sector;
}

} // namespace mayara_bridge

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  
  auto node = std::make_shared<mayara_bridge::MayaraBridgeNode>();
  
  rclcpp::spin(node);
  
  rclcpp::shutdown();
  return 0;
}
