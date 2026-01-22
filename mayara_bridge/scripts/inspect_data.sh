#!/bin/bash
# Data inspection script for Mayara -> Echoflow pipeline
# This script helps you view data at different stages

set -e

echo "=========================================="
echo "Mayara-Echoflow Data Inspector"
echo "=========================================="
echo ""

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}1. Checking ROS 2 topics...${NC}"
echo ""
ros2 topic list | grep -E "(data|radar|occupancy)" || echo "No radar topics found"
echo ""

echo -e "${BLUE}2. Topic information:${NC}"
echo ""
if ros2 topic list | grep -q "/data"; then
    echo -e "${GREEN}✓ /data topic exists${NC}"
    echo "  Type: $(ros2 topic type /data)"
    echo "  Frequency: $(ros2 topic hz /data 2>&1 | head -1 || echo 'No messages yet')"
    echo ""
    echo "  Sample message (first 20 lines):"
    timeout 2 ros2 topic echo /data --once 2>/dev/null | head -20 || echo "  (Waiting for messages...)"
else
    echo -e "${YELLOW}⚠ /data topic not found${NC}"
fi
echo ""

if ros2 topic list | grep -q "radar_grid_map"; then
    echo -e "${GREEN}✓ /radar_grid_map topic exists${NC}"
    echo "  Type: $(ros2 topic type /radar_grid_map)"
    echo "  Frequency: $(ros2 topic hz /radar_grid_map 2>&1 | head -1 || echo 'No messages yet')"
else
    echo -e "${YELLOW}⚠ /radar_grid_map topic not found${NC}"
fi
echo ""

echo -e "${BLUE}3. Checking Mayara API...${NC}"
echo ""
MAYARA_HOST="${MAYARA_HOST:-localhost}"
MAYARA_PORT="${MAYARA_PORT:-6502}"
echo "  Fetching from http://${MAYARA_HOST}:${MAYARA_PORT}/v1/api/radars"
curl -s "http://${MAYARA_HOST}:${MAYARA_PORT}/v1/api/radars" | python3 -m json.tool 2>/dev/null || echo "  (Mayara not responding)"
echo ""

echo -e "${BLUE}4. Active nodes:${NC}"
ros2 node list | grep -E "(mayara|radar|echoflow)" || echo "  (No matching nodes found)"
echo ""

echo "=========================================="
echo "Quick Commands:"
echo "=========================================="
echo ""
echo "View ROS 2 messages:"
echo "  ros2 topic echo /data"
echo "  ros2 topic echo /radar_grid_map"
echo ""
echo "Monitor message rates:"
echo "  ros2 topic hz /data"
echo "  ros2 topic hz /radar_grid_map"
echo ""
echo "View message structure:"
echo "  ros2 interface show marine_sensor_msgs/msg/RadarSector"
echo ""
echo "Check TF transforms:"
echo "  ros2 run tf2_ros tf2_echo map radar"
echo ""
