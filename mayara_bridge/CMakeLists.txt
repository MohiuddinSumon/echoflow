cmake_minimum_required(VERSION 3.8)
project(mayara_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(marine_sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

# WebSocket library - using libwebsockets (lightweight, fast)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS libwebsockets)

# If libwebsockets not found via pkg-config, try manual find
if(NOT LIBWEBSOCKETS_FOUND)
  find_library(WEBSOCKETS_LIB websockets)
  find_path(WEBSOCKETS_INCLUDE_DIR libwebsockets.h)
  if(WEBSOCKETS_LIB AND WEBSOCKETS_INCLUDE_DIR)
    set(LIBWEBSOCKETS_LIBRARIES ${WEBSOCKETS_LIB})
    set(LIBWEBSOCKETS_INCLUDE_DIRS ${WEBSOCKETS_INCLUDE_DIR})
    set(LIBWEBSOCKETS_FOUND TRUE)
  endif()
endif()

# Protobuf
find_package(Protobuf REQUIRED)

# CURL for HTTP requests
find_package(CURL QUIET)
if(NOT CURL_FOUND)
  # Try pkg-config
  pkg_check_modules(CURL libcurl)
  if(NOT CURL_FOUND)
    find_library(CURL_LIBRARIES curl)
    find_path(CURL_INCLUDE_DIRS curl/curl.h)
    if(CURL_LIBRARIES AND CURL_INCLUDE_DIRS)
      set(CURL_FOUND TRUE)
    else()
      message(WARNING "CURL not found. Install with: sudo apt install libcurl4-openssl-dev")
    endif()
  endif()
endif()

# JSON library (nlohmann/json) - optional
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  # Try pkg-config
  pkg_check_modules(NLOHMANN_JSON nlohmann_json)
  if(NOT NLOHMANN_JSON_FOUND)
    message(WARNING "nlohmann_json not found. Using simple JSON parser fallback. "
                    "Install with: sudo apt install nlohmann-json3-dev for better JSON support")
  endif()
endif()

# Include directories
include_directories(
  include
  ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# Source files
set(SOURCES
  src/mayara_bridge_node.cpp
  src/protobuf_parser.cpp
  src/websocket_client.cpp
)

# Headers
set(HEADERS
  include/mayara_bridge/mayara_bridge_node.hpp
  include/mayara_bridge/protobuf_parser.hpp
  include/mayara_bridge/websocket_client.hpp
)

# Executable
add_executable(mayara_bridge ${SOURCES} ${HEADERS})

ament_target_dependencies(mayara_bridge
  rclcpp
  std_msgs
  marine_sensor_msgs
  geometry_msgs
)

# Link libraries
if(LIBWEBSOCKETS_FOUND)
  target_link_libraries(mayara_bridge ${LIBWEBSOCKETS_LIBRARIES})
  target_include_directories(mayara_bridge PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS})
  target_compile_definitions(mayara_bridge PRIVATE HAVE_LIBWEBSOCKETS)
else()
  message(WARNING "libwebsockets not found. Install with: sudo apt install libwebsockets-dev")
endif()

target_link_libraries(mayara_bridge ${Protobuf_LIBRARIES})
target_include_directories(mayara_bridge PRIVATE ${Protobuf_INCLUDE_DIRS})

if(CURL_FOUND)
  target_link_libraries(mayara_bridge ${CURL_LIBRARIES})
  target_include_directories(mayara_bridge PRIVATE ${CURL_INCLUDE_DIRS})
  target_compile_definitions(mayara_bridge PRIVATE HAVE_CURL)
endif()

if(nlohmann_json_FOUND)
  target_link_libraries(mayara_bridge nlohmann_json::nlohmann_json)
  target_compile_definitions(mayara_bridge PRIVATE HAVE_NLOHMANN_JSON)
elseif(NLOHMANN_JSON_FOUND)
  target_link_libraries(mayara_bridge ${NLOHMANN_JSON_LIBRARIES})
  target_include_directories(mayara_bridge PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
  target_compile_definitions(mayara_bridge PRIVATE HAVE_NLOHMANN_JSON)
endif()

# Install
install(TARGETS mayara_bridge
  DESTINATION lib/${PROJECT_NAME}
)

# Install launch and config directories if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/launch")
  install(DIRECTORY launch
    DESTINATION share/${PROJECT_NAME}
  )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
  install(DIRECTORY config
    DESTINATION share/${PROJECT_NAME}
  )
endif()

# Install Python scripts
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
  install(PROGRAMS
    scripts/log_radar_data.py
    scripts/inspect_websocket.py
    scripts/inspect_data.sh
    DESTINATION lib/${PROJECT_NAME}
  )
endif()

ament_package()
