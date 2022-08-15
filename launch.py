from launch import LaunchDescription
from launch_ros.actions import LoadComposableNodes, Node
from launch_ros.descriptions import ComposableNode

def generate_launch_description():
    container = Node(
        name='my_container',
        package='rclcpp_components',
        executable='component_container_mt',
        output='both',
    )

    load_composable_nodes = LoadComposableNodes(
        target_container='my_container',
        composable_node_descriptions=[
            ComposableNode(
                 package='minimal_composition',
                plugin='minimal_composition::MinimalPublisher',
                name='minimal_publisher',
            ),
        ],
    )

    return LaunchDescription([container, load_composable_nodes])
