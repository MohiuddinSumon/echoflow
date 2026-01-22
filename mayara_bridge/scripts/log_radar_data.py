#!/usr/bin/env python3
"""
ROS 2 node to log and inspect radar data flowing through the system.
This node subscribes to RadarSector messages and prints detailed information.
"""

import rclpy
from rclpy.node import Node
from marine_sensor_msgs.msg import RadarSector
import json
from datetime import datetime


class RadarDataLogger(Node):
    """Logs and displays radar sector data in a human-readable format."""
    
    def __init__(self):
        super().__init__('radar_data_logger')
        
        # Parameters
        self.declare_parameter('topic_name', 'data')
        self.declare_parameter('verbose', False)
        self.declare_parameter('log_file', '')
        
        self.topic_name = self.get_parameter('topic_name').as_string()
        self.verbose = self.get_parameter('verbose').as_bool()
        self.log_file = self.get_parameter('log_file').as_string()
        
        # Statistics
        self.message_count = 0
        self.last_timestamp = None
        
        # Create subscriber
        self.subscription = self.create_subscription(
            RadarSector,
            self.topic_name,
            self.radar_callback,
            10
        )
        
        self.get_logger().info(f'Radar Data Logger started')
        self.get_logger().info(f'  Subscribing to: {self.topic_name}')
        self.get_logger().info(f'  Verbose mode: {self.verbose}')
        if self.log_file:
            self.get_logger().info(f'  Logging to: {self.log_file}')
        self.get_logger().info('')
        self.get_logger().info('=' * 60)
    
    def radar_callback(self, msg):
        """Process incoming radar sector message."""
        self.message_count += 1
        
        # Calculate time delta
        current_time = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
        if self.last_timestamp:
            delta = current_time - self.last_timestamp
        else:
            delta = 0.0
        self.last_timestamp = current_time
        
        # Format timestamp
        dt = datetime.fromtimestamp(current_time)
        time_str = dt.strftime('%H:%M:%S.%f')[:-3]
        
        # Print summary
        print(f"\n[{time_str}] Message #{self.message_count} (Δt={delta:.3f}s)")
        print(f"  Frame ID: {msg.header.frame_id}")
        print(f"  Angle: start={msg.angle_start:.3f} rad ({msg.angle_start*180/3.14159:.1f}°), "
              f"increment={msg.angle_increment:.3f} rad ({msg.angle_increment*180/3.14159:.3f}°)")
        print(f"  Range: min={msg.range_min:.1f}m, max={msg.range_max:.1f}m")
        print(f"  Intensities: {len(msg.intensities)} spokes")
        
        # Calculate total echoes
        total_echoes = sum(len(intensity.echoes) for intensity in msg.intensities)
        print(f"  Total echoes: {total_echoes}")
        
        # Show first few spokes
        if self.verbose and msg.intensities:
            print(f"\n  First 3 spokes:")
            for i, intensity in enumerate(msg.intensities[:3]):
                echo_count = len(intensity.echoes)
                if echo_count > 0:
                    max_echo = max(intensity.echoes)
                    avg_echo = sum(intensity.echoes) / echo_count
                    print(f"    Spoke {i}: {echo_count} echoes, "
                          f"max={max_echo:.3f}, avg={avg_echo:.3f}")
                else:
                    print(f"    Spoke {i}: empty")
        
        # Log to file if specified
        if self.log_file:
            self.log_to_file(msg)
    
    def log_to_file(self, msg):
        """Log message to file in JSON format."""
        data = {
            'timestamp': {
                'sec': msg.header.stamp.sec,
                'nanosec': msg.header.stamp.nanosec
            },
            'frame_id': msg.header.frame_id,
            'angle_start': msg.angle_start,
            'angle_increment': msg.angle_increment,
            'range_min': msg.range_min,
            'range_max': msg.range_max,
            'num_spokes': len(msg.intensities),
            'total_echoes': sum(len(intensity.echoes) for intensity in msg.intensities)
        }
        
        with open(self.log_file, 'a') as f:
            f.write(json.dumps(data) + '\n')


def main(args=None):
    rclpy.init(args=args)
    
    node = RadarDataLogger()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.get_logger().info(f'\nTotal messages received: {node.message_count}')
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
