# Mayara to ROS 2 Bridge

This package provides a bridge between the Mayara radar server and ROS 2 echoflow package.

## Installation

1. Install Python dependencies:
```bash
pip3 install websocket-client protobuf numpy
```

2. Build the package:
```bash
cd ~/ros2_ws
colcon build --packages-select mayara_bridge
source install/setup.zsh
```

## Usage

### Basic Usage

```bash
ros2 run mayara_bridge mayara_bridge
```

### With Parameters

```bash
ros2 run mayara_bridge mayara_bridge \
  --ros-args \
  -p mayara_host:=localhost \
  -p mayara_port:=6502 \
  -p radar_id:=radar-1 \
  -p frame_id:=radar \
  -p topic_name:=data
```

### In a Namespace (for echoflow)

```bash
ros2 run mayara_bridge mayara_bridge \
  --ros-args \
  -r __ns:=/aura/perception/sensors/halo_a \
  -p mayara_host:=localhost \
  -p mayara_port:=6502 \
  -p radar_id:=radar-1
```

## Parameters

- `mayara_host` (string, default: "localhost") - Mayara server hostname
- `mayara_port` (int, default: 6502) - Mayara server port
- `radar_id` (string, default: "radar-1") - Radar ID from Mayara
- `frame_id` (string, default: "radar") - TF frame ID for radar messages
- `topic_name` (string, default: "data") - ROS 2 topic to publish to
- `range_min` (double, default: 0.0) - Minimum radar range in meters
- `range_max` (double, default: 10000.0) - Maximum radar range in meters
- `use_bearing` (bool, default: true) - Use true bearing if available

## Complete Workflow

1. Start Mayara server:
```bash
cd ~/Projects/KAHU/mayara
./target/release/mayara-server
```

2. Start the bridge:
```bash
ros2 run mayara_bridge mayara_bridge \
  --ros-args \
  -r __ns:=/aura/perception/sensors/halo_a
```

3. Start echoflow:
```bash
ros2 launch echoflow flow_tracker.launch.xml \
  radar_ns:=/aura/perception/sensors/halo_a
```

## Notes

- The bridge connects to Mayara's WebSocket stream and converts protobuf messages to ROS 2 RadarSector format
- You may need to compile the RadarMessage.proto file for full protobuf support
- The current implementation includes a simplified parser - you may need to enhance it based on your specific Mayara version
