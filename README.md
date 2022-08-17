# why_pmu_not_work

We need `libpfm` to encode event names for `perf_event_open(2)`.
```
$ git clone https://git.code.sf.net/p/perfmon2/libpfm4 perfmon2-libpfm4
$ cd perfmon2-libpfm4
$ make
$ make install
```

ROS2 library stacks are supposed to be installed.
```
source /opt/ros/galactic/setup.bash
ros2 launch launch.py
```
