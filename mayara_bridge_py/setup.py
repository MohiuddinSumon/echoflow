from setuptools import setup
import os
from glob import glob

package_name = 'mayara_bridge_py'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Mohiuddin Ahmed',
    maintainer_email='m.mohiuddin@brainstation-23.com',
    description='Bridge between Mayara radar server and ROS 2 echoflow',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'mayara_bridge_py = mayara_bridge_py.mayara_to_ros2_bridge:main',
        ],
    },
)
