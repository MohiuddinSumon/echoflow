# Quick Start Guide: Mayara → echoflow Bridge

## Step 1: Install Dependencies

```bash
pip3 install websocket-client protobuf numpy
```

## Step 2: Build the Bridge Package

```bash
cd ~/ros2_ws/src
# The mayara_bridge folder should already be in echoflow directory
# If not, copy it there

cd ~/ros2_ws
colcon build --packages-select mayara_bridge
source install/setup.zsh
```

## Step 3: Start Mayara Server

In one terminal:

```bash
cd ~/Projects/KAHU/mayara
./target/release/mayara-server
# Or if you need to specify interface:
# ./target/release/mayara-server -i eth0 -p 6502
```

Wait for it to detect your radar. You should see output like:
```
Found radar: radar-1
Listening on port 6502
```

## Step 4: Start the Bridge

In another terminal:

```bash
source ~/ros2_ws/install/setup.zsh

# Basic usage (publishes to /data topic)
ros2 run mayara_bridge mayara_bridge

# Or with namespace (for echoflow compatibility)
ros2 run mayara_bridge mayara_bridge \
  --ros-args \
  -r __ns:=/aura/perception/sensors/halo_a \
  -p mayara_host:=localhost \
  -p mayara_port:=6502 \
  -p radar_id:=radar-1 \
  -p frame_id:=radar
```

## Step 5: Start echoflow

In a third terminal:

```bash
source ~/ros2_ws/install/setup.zsh

# Make sure TF is available (if needed)
ros2 run tf2_ros static_transform_publisher 0 0 0 0 0 0 map radar

# Start echoflow
ros2 launch echoflow flow_tracker.launch.xml \
  radar_ns:=/aura/perception/sensors/halo_a
```

## Step 6: Verify It's Working

Check topics:
```bash
ros2 topic list
ros2 topic echo /aura/perception/sensors/halo_a/data --no-arr
```

You should see RadarSector messages being published.

## Troubleshooting

### Bridge can't connect to Mayara
- Check Mayara is running: `curl http://localhost:6502/v1/api/radars`
- Verify the radar_id matches what Mayara reports
- Check firewall/network settings

### No messages from bridge
- Check bridge logs for errors
- Verify WebSocket connection is established
- Check that Mayara is actually receiving radar data

### echoflow not receiving data
- Verify topic names match: `ros2 topic list | grep data`
- Check namespace matches between bridge and echoflow
- Ensure TF transform is published (map → radar frame)

### Protobuf parsing errors
- The manual parser should work, but if you get errors, you may need to compile the .proto file
- See README.md for instructions on compiling protobuf

## Complete Launch (All-in-One)

You can also create a launch file that starts everything together. See the launch directory for examples.
