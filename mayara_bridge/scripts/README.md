# Data Inspection Tools

Tools to inspect and log data flowing through the Mayara → Echoflow pipeline.

## Quick Start

### 1. Inspect ROS 2 Topics

```bash
# Run the inspection script
./scripts/inspect_data.sh

# Or manually check topics
ros2 topic list
ros2 topic echo /data
ros2 topic hz /data
```

### 2. Log Radar Data (ROS 2)

```bash
# Basic logging (summary only)
ros2 run mayara_bridge log_radar_data.py

# Verbose logging (shows spoke details)
ros2 run mayara_bridge log_radar_data.py --ros-args -p verbose:=true

# Log to file
ros2 run mayara_bridge log_radar_data.py --ros-args -p log_file:=/tmp/radar_data.jsonl
```

### 3. Inspect WebSocket Messages (Mayara)

```bash
# Basic inspection (10 messages)
python3 scripts/inspect_websocket.py

# Verbose mode (shows hex/bytes)
python3 scripts/inspect_websocket.py --verbose

# Unlimited messages
python3 scripts/inspect_websocket.py --max-messages 0

# Custom radar
python3 scripts/inspect_websocket.py --radar-id radar-1 --host localhost --port 6502
```

## Tools Overview

### `inspect_data.sh`
Quick overview script that checks:
- ROS 2 topics and their status
- Message frequencies
- Mayara API status
- Active nodes

### `log_radar_data.py`
ROS 2 node that subscribes to `RadarSector` messages and displays:
- Message timestamps and deltas
- Frame ID
- Angle information (radians and degrees)
- Range information
- Number of spokes and echoes
- Optional: Detailed spoke data

### `inspect_websocket.py`
Python script that connects directly to Mayara's WebSocket and shows:
- Message sizes
- Raw bytes (hex format)
- Message counts
- Timestamps

## Example Output

### ROS 2 Topic Echo
```bash
$ ros2 topic echo /data --once
header:
  stamp:
    sec: 1769116147
    nanosec: 984025693
  frame_id: radar
angle_start: 0.0
angle_increment: 0.00306796
range_min: 0.0
range_max: 10000.0
intensities:
  - echoes: [0.0, 0.1, 0.2, ...]
```

### Data Logger
```
[03:14:07.984] Message #1 (Δt=0.000s)
  Frame ID: radar
  Angle: start=0.000 rad (0.0°), increment=0.003 rad (0.176°)
  Range: min=0.0m, max=10000.0m
  Intensities: 2048 spokes
  Total echoes: 2097152
```

### WebSocket Inspector
```
Connecting to: ws://localhost:6502/v1/api/spokes/radar-1
✓ Connected!
Waiting for messages...

[03:14:08.012] Message #1
  Size: 1234 bytes
  First 100 bytes (hex): 0a0b72616461722d312...
```

## Viewing Sample Data

If you have the pcap file (`halo_and_0183.pcap`), you can replay it with Mayara:

```bash
# In mayara directory
sudo tcpreplay -i lo halo_and_0183.pcap

# Then run Mayara in replay mode
./target/release/mayara-server --replay --brand navico -i lo -p 6502
```

Then use the inspection tools to view the data.

## Integration with Echoflow

To see the full pipeline:

1. **Terminal 1**: Mayara server
2. **Terminal 2**: Bridge (`ros2 run mayara_bridge mayara_bridge ...`)
3. **Terminal 3**: Data logger (`ros2 run mayara_bridge log_radar_data.py`)
4. **Terminal 4**: Echoflow (`ros2 run echoflow radar_grid_map`)

You'll see data flowing through each stage!
