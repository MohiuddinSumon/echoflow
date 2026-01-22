#include "mayara_bridge/websocket_client.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <regex>
#include <thread>
#include <chrono>

#ifdef HAVE_LIBWEBSOCKETS
#include <libwebsockets.h>
#endif

namespace mayara_bridge
{

#ifdef HAVE_LIBWEBSOCKETS

// Forward declaration
class WebSocketClient;

// libwebsockets callback context
struct WebSocketContext
{
  WebSocketClient* client;
  struct lws* wsi;
  std::vector<uint8_t> buffer;
  bool connection_established;
};

// libwebsockets protocol callbacks
static int callback_websocket(struct lws* wsi [[maybe_unused]], enum lws_callback_reasons reason,
                               void* user, void* in, size_t len)
{
  WebSocketContext* ctx = static_cast<WebSocketContext*>(user);
  
  switch (reason)
  {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      ctx->connection_established = true;
      if (ctx->client)
      {
        // Connection successful
      }
      break;
      
    case LWS_CALLBACK_CLIENT_RECEIVE:
      if (ctx && ctx->client && in && len > 0)
      {
        // Receive binary data
        const uint8_t* data = static_cast<const uint8_t*>(in);
        std::vector<uint8_t> message(data, data + len);
        ctx->client->handleMessage(message);
      }
      break;
      
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      if (ctx && ctx->client)
      {
        ctx->connection_established = false;
        ctx->client->handleError("WebSocket connection error");
      }
      break;
      
    case LWS_CALLBACK_CLOSED:
      ctx->connection_established = false;
      break;
      
    default:
      break;
  }
  
  return 0;
}

// Protocol list for libwebsockets
static struct lws_protocols protocols[] = {
  {
    "mayara-protocol",
    callback_websocket,
    0,      // per_session_data_size
    4096,   // RX buffer size
    0,      // id
    nullptr, // user
    0       // tx_packet_size
  },
  { nullptr, nullptr, 0, 0, 0, nullptr, 0 }  // Terminator
};

WebSocketClient::WebSocketClient(const std::string& url)
  : url_(url), connected_(false), running_(false), context_(nullptr), ws_context_(nullptr)
{
}

WebSocketClient::~WebSocketClient()
{
  disconnect();
}

bool WebSocketClient::connect()
{
  if (connected_)
  {
    return true;
  }
  
  // Parse URL: ws://host:port/path or wss://host:port/path
  std::regex url_regex(R"((wss?)://([^:/]+)(?::(\d+))?(/.*)?)");
  std::smatch match;
  
  if (!std::regex_match(url_, match, url_regex))
  {
    if (error_callback_)
    {
      error_callback_("Invalid WebSocket URL: " + url_);
    }
    return false;
  }
  
  std::string protocol = match[1].str();
  std::string host = match[2].str();
  std::string port_str = match[3].str();
  std::string path = match[4].str();
  
  if (port_str.empty())
  {
    port_str = (protocol == "wss") ? "443" : "80";
  }
  
  if (path.empty())
  {
    path = "/";
  }
  
  int port = std::stoi(port_str);
  bool use_ssl = (protocol == "wss");
  
  // Create libwebsockets context
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));
  
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.gid = -1;
  info.uid = -1;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  
  struct lws_context* context = lws_create_context(&info);
  if (!context)
  {
    if (error_callback_)
    {
      error_callback_("Failed to create libwebsockets context");
    }
    return false;
  }
  
  context_ = context;
  
  // Create client connection info
  struct lws_client_connect_info ccinfo;
  memset(&ccinfo, 0, sizeof(ccinfo));
  
  ccinfo.context = context;
  ccinfo.address = host.c_str();
  ccinfo.port = port;
  ccinfo.path = path.c_str();
  ccinfo.host = host.c_str();
  ccinfo.origin = host.c_str();
  ccinfo.protocol = protocols[0].name;
  ccinfo.ssl_connection = use_ssl ? LCCSCF_USE_SSL : 0;
  
  // Allocate context for callback
  WebSocketContext* ws_ctx = new WebSocketContext();
  ws_ctx->client = this;
  ws_ctx->wsi = nullptr;
  ws_ctx->connection_established = false;
  ws_context_ = ws_ctx;
  
  ccinfo.userdata = ws_ctx;
  
  struct lws* wsi = lws_client_connect_via_info(&ccinfo);
  if (!wsi)
  {
    if (error_callback_)
    {
      error_callback_("Failed to create WebSocket connection");
    }
    delete ws_ctx;
    lws_context_destroy(context);
    context_ = nullptr;
    return false;
  }
  
  ws_ctx->wsi = wsi;
  
  // Start event loop in a thread
  running_ = true;
  ws_thread_ = std::thread([this]() { this->run(); });
  
  // Wait a bit for connection to establish
  for (int i = 0; i < 50 && !connected_; ++i)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  return connected_;
}

void WebSocketClient::disconnect()
{
  running_ = false;
  connected_ = false;
  
  if (context_)
  {
    lws_context_destroy(static_cast<struct lws_context*>(context_));
    context_ = nullptr;
  }
  
  if (ws_context_)
  {
    delete static_cast<WebSocketContext*>(ws_context_);
    ws_context_ = nullptr;
  }
  
  if (ws_thread_.joinable())
  {
    ws_thread_.join();
  }
}

void WebSocketClient::run()
{
  struct lws_context* context = static_cast<struct lws_context*>(context_);
  
  while (running_ && context)
  {
    int ret = lws_service(context, 50);  // 50ms timeout
    
    if (ret < 0)
    {
      break;
    }
    
    // Check connection status
    if (ws_context_)
    {
      WebSocketContext* ctx = static_cast<WebSocketContext*>(ws_context_);
      if (ctx->connection_established && !connected_)
      {
        connected_ = true;
      }
      else if (!ctx->connection_established && connected_)
      {
        connected_ = false;
      }
    }
  }
  
  connected_ = false;
}

void WebSocketClient::handleMessage(const std::vector<uint8_t>& data)
{
  if (message_callback_)
  {
    message_callback_(data);
  }
}

void WebSocketClient::handleError(const std::string& error)
{
  if (error_callback_)
  {
    error_callback_(error);
  }
}

#else  // HAVE_LIBWEBSOCKETS not defined

WebSocketClient::WebSocketClient(const std::string& url)
  : url_(url), connected_(false), running_(false), context_(nullptr), ws_context_(nullptr)
{
}

WebSocketClient::~WebSocketClient()
{
  disconnect();
}

bool WebSocketClient::connect()
{
  std::cerr << "ERROR: libwebsockets not available. "
            << "Install libwebsockets-dev: sudo apt install libwebsockets-dev" << std::endl;
  
  if (error_callback_)
  {
    error_callback_("libwebsockets not available. Install libwebsockets-dev");
  }
  
  return false;
}

void WebSocketClient::disconnect()
{
  running_ = false;
  connected_ = false;
  
  if (ws_thread_.joinable())
  {
    ws_thread_.join();
  }
}

void WebSocketClient::run()
{
  // Not implemented without libwebsockets
}

void WebSocketClient::handleMessage(const std::vector<uint8_t>& data)
{
  if (message_callback_)
  {
    message_callback_(data);
  }
}

void WebSocketClient::handleError(const std::string& error)
{
  if (error_callback_)
  {
    error_callback_(error);
  }
}

#endif  // HAVE_LIBWEBSOCKETS

} // namespace mayara_bridge
