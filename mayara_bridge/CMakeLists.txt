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
else()
  message(WARNING "libwebsockets not found. Install with: sudo apt install libwebsockets-dev")
endif()

target_link_libraries(mayara_bridge ${Protobuf_LIBRARIES})
target_include_directories(mayara_bridge PRIVATE ${Protobuf_INCLUDE_DIRS})

# Install
install(TARGETS mayara_bridge
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
