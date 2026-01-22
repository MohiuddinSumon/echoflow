#!/usr/bin/env python3
"""
Mayara to ROS 2 Bridge Node

This node connects to Mayara's WebSocket stream and converts radar data
from Mayara's protobuf format to ROS 2 marine_sensor_msgs::msg::RadarSector
messages that echoflow can consume.
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import Header
from marine_sensor_msgs.msg import RadarSector
import websocket
import json
import struct
import numpy as np
from typing import Optional, Dict, List
import threading
import time

try:
    from .protobuf_parser import parse_radar_message
except ImportError:
    from protobuf_parser import parse_radar_message


class MayaraToROS2Bridge(Node):
    """
    Bridge node that converts Mayara WebSocket radar data to ROS 2 RadarSector messages.
    """
    
    def __init__(self):
        super().__init__('mayara_to_ros2_bridge')
        
        # Declare parameters
        self.declare_parameter('mayara_host', 'localhost')
        self.declare_parameter('mayara_port', 6502)
        self.declare_parameter('radar_id', 'radar-0')
        self.declare_parameter('frame_id', 'radar')
        self.declare_parameter('topic_name', 'data')
        self.declare_parameter('range_min', 0.0)
        self.declare_parameter('range_max', 10000.0)  # 10km default
        self.declare_parameter('use_bearing', True)  # Use true bearing if available
        
        # Get parameters
        self.mayara_host = self.get_parameter('mayara_host').get_parameter_value().string_value
        self.mayara_port = self.get_parameter('mayara_port').get_parameter_value().integer_value
        self.radar_id = self.get_parameter('radar_id').get_parameter_value().string_value
        self.frame_id = self.get_parameter('frame_id').get_parameter_value().string_value
        self.topic_name = self.get_parameter('topic_name').get_parameter_value().string_value
        self.range_min = self.get_parameter('range_min').get_parameter_value().double_value
        self.range_max = self.get_parameter('range_max').get_parameter_value().double_value
        self.use_bearing = self.get_parameter('use_bearing').get_parameter_value().bool_value
        
        # Publisher for RadarSector messages
        self.publisher = self.create_publisher(
            RadarSector,
            self.topic_name,
            10
        )
        
        # Radar metadata
        self.spokes_per_revolution: Optional[int] = None
        self.max_spoke_len: Optional[int] = None
        self.radar_info: Optional[Dict] = None
        
        # Buffer for accumulating spokes into sectors
        self.spoke_buffer: Dict[int, Dict] = {}  # angle -> spoke data
        self.last_sector_time = time.time()
        self.sector_timeout = 0.1  # Publish sector if no new spokes for 0.1s
        
        # WebSocket connection
        self.ws: Optional[websocket.WebSocketApp] = None
        self.ws_thread: Optional[threading.Thread] = None
        self.running = False
        
        # Timer to publish accumulated sectors
        self.timer = self.create_timer(0.1, self.publish_accumulated_sector)
        
        self.get_logger().info(f'Mayara bridge initialized')
        self.get_logger().info(f'  Mayara: {self.mayara_host}:{self.mayara_port}')
        self.get_logger().info(f'  Radar ID: {self.radar_id}')
        self.get_logger().info(f'  Frame ID: {self.frame_id}')
        self.get_logger().info(f'  Topic: {self.topic_name}')
        
        # Start connection
        self.connect_to_mayara()
    
    def connect_to_mayara(self):
        """Connect to Mayara's API to get radar info and WebSocket URL."""
        import urllib.request
        
        try:
            # Get radar information
            api_url = f'http://{self.mayara_host}:{self.mayara_port}/v1/api/radars'
            self.get_logger().info(f'Fetching radar info from {api_url}')
            
            with urllib.request.urlopen(api_url) as response:
                data = json.loads(response.read().decode())
                
            # Find our radar
            if self.radar_id in data:
                self.radar_info = data[self.radar_id]
                self.spokes_per_revolution = self.radar_info.get('spokes_per_revolution', 2048)
                self.max_spoke_len = self.radar_info.get('maxSpokeLen', 1024)
                stream_url = self.radar_info.get('streamUrl', '').replace('http://', 'ws://').replace('https://', 'wss://')
                
                self.get_logger().info(f'Found radar: {self.radar_id}')
                self.get_logger().info(f'  Spokes per revolution: {self.spokes_per_revolution}')
                self.get_logger().info(f'  Max spoke length: {self.max_spoke_len}')
                self.get_logger().info(f'  Stream URL: {stream_url}')
                
                # Connect to WebSocket
                self.connect_websocket(stream_url)
            else:
                self.get_logger().error(f'Radar {self.radar_id} not found in {list(data.keys())}')
                
        except Exception as e:
            self.get_logger().error(f'Failed to connect to Mayara API: {e}')
            self.get_logger().info('Retrying in 5 seconds...')
            # Retry after 5 seconds
            threading.Timer(5.0, self.connect_to_mayara).start()
    
    def connect_websocket(self, url: str):
        """Connect to Mayara's WebSocket stream."""
        if not PROTOBUF_AVAILABLE:
            self.get_logger().error('protobuf not available. Cannot decode radar messages.')
            self.get_logger().info('Install with: pip install protobuf')
            return
        
        self.get_logger().info(f'Connecting to WebSocket: {url}')
        
        def on_message(ws, message):
            """Handle incoming WebSocket message (protobuf)."""
            try:
                self.process_protobuf_message(message)
            except Exception as e:
                self.get_logger().error(f'Error processing message: {e}', exc_info=True)
        
        def on_error(ws, error):
            self.get_logger().error(f'WebSocket error: {error}')
        
        def on_close(ws, close_status_code, close_msg):
            self.get_logger().warn('WebSocket closed. Reconnecting in 5 seconds...')
            if self.running:
                threading.Timer(5.0, lambda: self.connect_websocket(url)).start()
        
        def on_open(ws):
            self.get_logger().info('WebSocket connected!')
        
        self.ws = websocket.WebSocketApp(
            url,
            on_message=on_message,
            on_error=on_error,
            on_close=on_close,
            on_open=on_open
        )
        
        self.running = True
        self.ws_thread = threading.Thread(target=self.ws.run_forever, daemon=True)
        self.ws_thread.start()
    
    def process_protobuf_message(self, message_bytes: bytes):
        """
        Process protobuf RadarMessage and convert spokes to RadarSector format.
        """
        try:
            # Parse the protobuf message
            radar_msg = parse_radar_message(message_bytes)
            
            if not radar_msg.get('spokes'):
                return
            
            # Process each spoke
            for spoke in radar_msg['spokes']:
                angle = spoke.get('angle')
                if angle is not None:
                    # Store spoke in buffer
                    self.spoke_buffer[angle] = spoke
                    self.last_sector_time = time.time()
            
            # If we have enough spokes, publish a sector
            if len(self.spoke_buffer) >= 10:  # Threshold for publishing
                self.publish_accumulated_sector()
            
        except Exception as e:
            self.get_logger().error(f'Failed to parse protobuf: {e}', exc_info=True)
    
    def convert_spokes_to_radar_sector(self, spokes: List[Dict]) -> Optional[RadarSector]:
        """
        Convert a list of spokes to a RadarSector message.
        
        Args:
            spokes: List of spoke dictionaries with keys: angle, range, data, bearing (optional)
        
        Returns:
            RadarSector message or None if conversion fails
        """
        if not spokes or self.spokes_per_revolution is None:
            return None
        
        # Sort spokes by angle
        sorted_spokes = sorted(spokes, key=lambda s: s.get('angle', 0))
        
        if not sorted_spokes:
            return None
        
        # Create RadarSector message
        msg = RadarSector()
        
        # Header
        msg.header = Header()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = self.frame_id
        
        # Angle information
        first_angle = sorted_spokes[0].get('angle', 0)
        last_angle = sorted_spokes[-1].get('angle', 0)
        
        # Convert angle from [0, spokes_per_revolution) to radians
        angle_span = (last_angle - first_angle + 1) % self.spokes_per_revolution
        if angle_span == 0:
            angle_span = 1
        
        msg.angle_start = (first_angle / self.spokes_per_revolution) * 2.0 * np.pi
        msg.angle_increment = (1.0 / self.spokes_per_revolution) * 2.0 * np.pi
        
        # Range information
        max_range = max(s.get('range', self.range_max) for s in sorted_spokes)
        msg.range_min = self.range_min
        msg.range_max = float(max(max_range, self.range_max))
        
        # Convert spoke data to intensities
        # Each spoke becomes one "intensity" entry with echoes for each range bin
        msg.intensities = []
        
        for spoke in sorted_spokes:
            spoke_data = spoke.get('data', b'')
            spoke_range = spoke.get('range', self.range_max)
            
            # Create intensity entry for this spoke
            intensity_entry = RadarSector.Intensity()
            
            # Convert spoke data bytes to echo intensities
            # Assuming data is intensity values (0-255)
            num_bins = len(spoke_data)
            if num_bins == 0:
                # Create empty echoes
                intensity_entry.echoes = [0.0] * int((spoke_range - self.range_min) / 10.0)
            else:
                # Convert bytes to float intensities (normalize 0-255 to 0.0-1.0)
                intensity_entry.echoes = [float(b) / 255.0 for b in spoke_data[:num_bins]]
            
            msg.intensities.append(intensity_entry)
        
        return msg
    
    def publish_accumulated_sector(self):
        """Publish accumulated spokes as a RadarSector if we have enough data."""
        if len(self.spoke_buffer) < 10:  # Need at least 10 spokes
            return
        
        # Convert buffer to list
        spokes = list(self.spoke_buffer.values())
        self.spoke_buffer.clear()
        
        # Convert and publish
        sector = self.convert_spokes_to_radar_sector(spokes)
        if sector:
            self.publisher.publish(sector)
            self.get_logger().debug(f'Published RadarSector with {len(spokes)} spokes')
    
    def destroy_node(self):
        """Cleanup on shutdown."""
        self.running = False
        if self.ws:
            self.ws.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    
    node = MayaraToROS2Bridge()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
