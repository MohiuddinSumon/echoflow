from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'mayara_host',
            default_value='localhost',
            description='Mayara server hostname'
        ),
        DeclareLaunchArgument(
            'mayara_port',
            default_value='6502',
            description='Mayara server port'
        ),
        DeclareLaunchArgument(
            'radar_id',
            default_value='radar-0',
            description='Radar ID from Mayara'
        ),
        DeclareLaunchArgument(
            'frame_id',
            default_value='radar',
            description='TF frame ID for radar messages'
        ),
        DeclareLaunchArgument(
            'topic_name',
            default_value='data',
            description='ROS 2 topic to publish to'
        ),
        DeclareLaunchArgument(
            'namespace',
            default_value='',
            description='ROS 2 namespace for the bridge node'
        ),
        
        Node(
            package='mayara_bridge_py',
            executable='mayara_bridge_py',
            name='mayara_bridge_py',
            namespace=LaunchConfiguration('namespace'),
            parameters=[{
                'mayara_host': LaunchConfiguration('mayara_host'),
                'mayara_port': LaunchConfiguration('mayara_port'),
                'radar_id': LaunchConfiguration('radar_id'),
                'frame_id': LaunchConfiguration('frame_id'),
                'topic_name': LaunchConfiguration('topic_name'),
            }],
            output='screen'
        ),
    ])
