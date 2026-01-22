#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

namespace mayara_bridge
{

class WebSocketClient
{
public:
  using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;
  using ErrorCallback = std::function<void(const std::string&)>;

  WebSocketClient(const std::string& url);
  ~WebSocketClient();

  bool connect();
  void disconnect();
  bool isConnected() const { return connected_; }

  void setMessageCallback(MessageCallback callback) { message_callback_ = callback; }
  void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }

private:
  std::string url_;
  std::atomic<bool> connected_;
  std::atomic<bool> running_;
  std::thread ws_thread_;
  
  MessageCallback message_callback_;
  ErrorCallback error_callback_;

  void run();
};

} // namespace mayara_bridge
