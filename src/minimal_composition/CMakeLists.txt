cmake_minimum_required(VERSION 3.5)
project(minimal_composition)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)

# Create ament index resource which references the libraries in the binary directory
set(node_plugins "")

add_library(composition_nodes SHARED
  src/publisher.cpp)
target_compile_definitions(composition_nodes
  PRIVATE "COMPOSITION_BUILDING_DLL")
ament_target_dependencies(composition_nodes rclcpp rclcpp_components std_msgs)
rclcpp_components_register_nodes(composition_nodes "minimal_composition::MinimalPublisher")
set(node_plugins "${node_plugins}minimal_composition::MinimalPublisher;$<TARGET_FILE:composition_nodes>\n")

set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES /usr/local/lib ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})
target_link_libraries(composition_nodes pfm)

install(TARGETS
  composition_nodes
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

ament_package()
