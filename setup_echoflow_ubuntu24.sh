#!/bin/bash
set -e

echo "=========================================="
echo "echoflow Setup Script for Ubuntu 24.04"
echo "ROS 2 Distribution: Jazzy"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect shell and set appropriate files
if [ -n "$ZSH_VERSION" ]; then
    SHELL_NAME="zsh"
    SHELL_RC="$HOME/.zshrc"
    ROS_SETUP_FILE="/opt/ros/jazzy/setup.zsh"
elif [ -n "$BASH_VERSION" ]; then
    SHELL_NAME="bash"
    SHELL_RC="$HOME/.bashrc"
    ROS_SETUP_FILE="/opt/ros/jazzy/setup.bash"
else
    # Default to bash if can't detect
    SHELL_NAME="bash"
    SHELL_RC="$HOME/.bashrc"
    ROS_SETUP_FILE="/opt/ros/jazzy/setup.bash"
fi

echo -e "${GREEN}Detected shell: $SHELL_NAME${NC}"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo -e "${RED}Please do not run this script as root${NC}"
   exit 1
fi

# Step 1: Check Ubuntu version
echo -e "${GREEN}[1/8] Checking Ubuntu version...${NC}"
if ! lsb_release -d | grep -q "Ubuntu 24.04"; then
    echo -e "${YELLOW}Warning: This script is designed for Ubuntu 24.04. Continuing anyway...${NC}"
fi
echo "✓ Ubuntu version check complete"
echo ""

# Step 2: Install ROS 2 Jazzy (if not already installed)
echo -e "${GREEN}[2/8] Installing ROS 2 Jazzy...${NC}"
if [ ! -d "/opt/ros/jazzy" ]; then
    echo "ROS 2 Jazzy not found. Installing..."
    
    # Set locale (as per official ROS 2 docs)
    sudo apt update && sudo apt install -y locales
    sudo locale-gen en_US en_US.UTF-8
    sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
    export LANG=en_US.UTF-8
    
    # Enable required repositories (as per official ROS 2 docs)
    sudo apt install -y software-properties-common
    sudo add-apt-repository universe
    
    # Install ros2-apt-source package (official method from ROS 2 docs)
    sudo apt update && sudo apt install -y curl
    export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}')
    curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo ${UBUNTU_CODENAME:-${VERSION_CODENAME}})_all.deb"
    sudo dpkg -i /tmp/ros2-apt-source.deb
    
    # Update apt cache and upgrade system (recommended by ROS 2 docs)
    sudo apt update
    sudo apt upgrade -y
    
    # Install ROS 2 Jazzy Desktop (as per official ROS 2 docs)
    sudo apt install -y ros-jazzy-desktop
    
    echo "✓ ROS 2 Jazzy installed"
else
    echo "✓ ROS 2 Jazzy already installed"
fi
echo ""

# Step 3: Source ROS 2 in current session
echo -e "${GREEN}[3/8] Setting up ROS 2 environment...${NC}"
if [ -f "$ROS_SETUP_FILE" ]; then
    source "$ROS_SETUP_FILE"
    echo "✓ ROS 2 environment sourced"
    
    # Add to shell rc file if not already there
    if ! grep -q "source $ROS_SETUP_FILE" "$SHELL_RC"; then
        echo "" >> "$SHELL_RC"
        echo "# ROS 2 Jazzy" >> "$SHELL_RC"
        echo "source $ROS_SETUP_FILE" >> "$SHELL_RC"
        echo "✓ Added ROS 2 sourcing to $SHELL_RC"
    else
        echo "✓ ROS 2 sourcing already in $SHELL_RC"
    fi
else
    echo -e "${RED}Error: ROS 2 Jazzy setup file not found at $ROS_SETUP_FILE!${NC}"
    exit 1
fi
echo ""

# Step 4: Install build tools and system dependencies
echo -e "${GREEN}[4/8] Installing build tools and system dependencies...${NC}"
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    libeigen3-dev \
    python3-pip \
    ros-dev-tools

echo "✓ Build tools installed"
echo ""

# Step 5: Initialize and update rosdep
echo -e "${GREEN}[5/8] Setting up rosdep...${NC}"
if [ ! -f "/etc/ros/rosdep/sources.list.d/20-default.list" ]; then
    sudo rosdep init
    echo "✓ rosdep initialized"
else
    echo "✓ rosdep already initialized"
fi

rosdep update
echo "✓ rosdep updated"
echo ""

# Step 6: Install ROS 2 dependencies
echo -e "${GREEN}[6/8] Installing ROS 2 package dependencies...${NC}"
sudo apt install -y \
    ros-jazzy-grid-map-core \
    ros-jazzy-grid-map-ros \
    ros-jazzy-grid-map-msgs \
    ros-jazzy-grid-map-cv \
    ros-jazzy-grid-map-costmap-2d \
    ros-jazzy-tf2 \
    ros-jazzy-tf2-ros \
    ros-jazzy-tf2-geometry-msgs \
    ros-jazzy-geometry-msgs \
    ros-jazzy-sensor-msgs \
    ros-jazzy-nav-msgs \
    ros-jazzy-rclcpp \
    ros-jazzy-rviz2 \
    ros-jazzy-rqt

echo "✓ ROS 2 dependencies installed"
echo ""

# Step 7: Create workspace and clone echoflow
echo -e "${GREEN}[7/8] Setting up workspace...${NC}"
WORKSPACE_DIR="$HOME/ros2_ws"
SRC_DIR="$WORKSPACE_DIR/src"

mkdir -p "$SRC_DIR"
cd "$SRC_DIR"

if [ ! -d "echoflow" ]; then
    echo "Cloning echoflow repository..."
    git clone https://github.com/SeawardScience/echoflow.git
    echo "✓ echoflow cloned"
else
    echo "✓ echoflow already exists, skipping clone"
    echo "  (To update, run: cd $SRC_DIR/echoflow && git pull)"
fi

# Check for marine_sensor_msgs
echo "Checking for marine_sensor_msgs..."
if ! ros2 pkg list 2>/dev/null | grep -q "marine_sensor_msgs"; then
    echo -e "${YELLOW}Warning: marine_sensor_msgs not found in installed packages${NC}"
    echo "  You may need to install it from source if rosdep can't find it"
    echo "  Check: https://github.com/UNH-CCOM/marine_sensor_msgs or similar repositories"
fi
echo ""

# Step 8: Install package dependencies and build
echo -e "${GREEN}[8/8] Installing package dependencies and building...${NC}"
cd "$WORKSPACE_DIR"

# Install dependencies via rosdep
echo "Installing dependencies via rosdep..."
rosdep install --from-paths src --ignore-src -r -y || {
    echo -e "${YELLOW}Warning: Some dependencies may need manual installation${NC}"
}

# Build the package
echo "Building echoflow package..."
colcon build --packages-select echoflow --cmake-args -DCMAKE_BUILD_TYPE=Release || {
    echo -e "${RED}Build failed! Check the error messages above.${NC}"
    exit 1
}

echo "✓ Build successful!"
echo ""

# Step 9: Setup workspace sourcing
echo -e "${GREEN}Setting up workspace sourcing...${NC}"
WORKSPACE_SETUP_FILE="$WORKSPACE_DIR/install/setup.$SHELL_NAME"

if ! grep -q "source $WORKSPACE_SETUP_FILE" "$SHELL_RC"; then
    echo "" >> "$SHELL_RC"
    echo "# echoflow workspace" >> "$SHELL_RC"
    echo "source $WORKSPACE_SETUP_FILE" >> "$SHELL_RC"
    echo "✓ Added workspace sourcing to $SHELL_RC"
else
    echo "✓ Workspace sourcing already in $SHELL_RC"
fi

# Source the workspace for current session
if [ -f "$WORKSPACE_SETUP_FILE" ]; then
    source "$WORKSPACE_SETUP_FILE"
else
    # Fallback to bash if zsh version doesn't exist
    source "$WORKSPACE_DIR/install/setup.bash"
fi
echo ""

# Verification
echo -e "${GREEN}=========================================="
echo "Setup Complete!"
echo "==========================================${NC}"
echo ""
echo "Verification:"
echo "  ROS 2 version: $(ros2 --version 2>/dev/null || echo 'Not found')"
echo "  Workspace: $WORKSPACE_DIR"
echo "  Package location: $SRC_DIR/echoflow"
echo ""
echo "To use echoflow in new terminals:"
echo "  source $SHELL_RC"
echo ""
echo "Or manually source:"
echo "  source $ROS_SETUP_FILE"
echo "  source $WORKSPACE_SETUP_FILE"
echo ""
echo "Test the installation:"
echo "  ros2 pkg list | grep echoflow"
echo "  ros2 run echoflow radar_grid_map --help"
echo ""
echo -e "${GREEN}Setup script completed successfully!${NC}"
