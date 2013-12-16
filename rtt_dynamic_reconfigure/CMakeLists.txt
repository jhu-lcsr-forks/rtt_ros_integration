cmake_minimum_required(VERSION 2.8.3)
project(rtt_dynamic_reconfigure)

find_package(catkin REQUIRED COMPONENTS
  roscpp rtt_ros dynamic_reconfigure rtt_roscomm)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIRS})

ros_generate_rtt_typekit(dynamic_reconfigure)

catkin_package()

orocos_generate_package(
  DEPENDS roscpp actionlib
  DEPENDS_TARGETS rtt_ros rtt_roscomm
)
