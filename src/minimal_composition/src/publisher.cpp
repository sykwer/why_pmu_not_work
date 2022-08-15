#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

// PMU - start
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#define MONITORED_EVENTS_NUM 1

struct read_format {
  uint64_t nr;            /* The number of events */
  uint64_t  time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
  uint64_t time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
  struct {
    uint64_t value;       /* The value of the event */
    uint64_t id;          /* if PERF_FORMAT_ID */
  } values[MONITORED_EVENTS_NUM];
};

struct event_info {
  const char *event_string;
  char is_leader;
  uint32_t event_type;
  uint64_t event_config;
  struct perf_event_attr event_attr;
  int fd;
  uint64_t id;
  uint64_t measured_value;
};

struct measurement_period_log {
  uint64_t values[MONITORED_EVENTS_NUM];
};

const char *monitored_event_strings[MONITORED_EVENTS_NUM] = {
  /*
  "l1d_pend_miss.pending",
  "mem_load_retired.l1_miss",
  "mem_load_retired.fb_hit",
  "mem_load_retired.l3_miss",
  */
  "instructions",
};

struct event_info event_infos[MONITORED_EVENTS_NUM];
char buffer[1000];
int global_leader_fd = 0;

static void encode_event_string(struct event_info *event_info) {
  pfm_perf_encode_arg_t arg;
 	struct perf_event_attr attr;
 	char *fstr = NULL; // Get event string in [pmu::][event_name][:unit_mask][:modifier|:modifier=val]

 	memset(&arg, 0, sizeof(arg));
 	arg.size = sizeof(arg);
 	arg.attr = &attr;
 	arg.fstr = &fstr;

 	int ret = pfm_get_os_event_encoding(event_info->event_string, PFM_PLM0, PFM_OS_PERF_EVENT_EXT, &arg);
 	if (ret != PFM_SUCCESS) {
 		perror("pfm_get_os_event_encoding error");
 		exit(EXIT_FAILURE);
 	}

  event_info->event_type = attr.type;
  event_info->event_config = attr.config;

  // The returned `fstr` value of "l1d_pend_miss.pending" is
 	// skl::L1D_PEND_MISS:PENDING:e=0:i=0:c=0:t=0:intx=0:intxcp=0:u=0:k=1:period=34:freq=34:excl=0:mg=0:mh=1
 	free(fstr);
}

static void setup_perf_event_attr_grouped(struct event_info *event_info) {
  struct perf_event_attr *attr = &event_info->event_attr;

  memset(attr, 0, sizeof(*attr));
  attr->type = event_info->event_type;
  attr->size = sizeof(*attr);
  attr->config = event_info->event_config;
  attr->disabled = 1;
  attr->exclude_kernel = 1;
  attr->exclude_hv = 1;
  attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING |
                      PERF_FORMAT_ID | PERF_FORMAT_GROUP;
}

static int prepare_event_infos() {
  int leader_fd = 0;
  // First element has to be the leader for this impelentation
  event_infos[0].is_leader = 1;

  for (int i = 0; i < MONITORED_EVENTS_NUM; i++) {
    event_infos[i].event_string = monitored_event_strings[i];
    encode_event_string(&event_infos[i]);
    setup_perf_event_attr_grouped(&event_infos[i]);

    int fd = syscall(__NR_perf_event_open, &event_infos[i].event_attr,
        getpid(), -1/*cpu*/ , event_infos[i].is_leader ? -1 : leader_fd, 0/*flag*/);

    if (fd == -1) {
      perror("perf_event_open error");
      exit(EXIT_FAILURE);
    }

    if (event_infos[i].is_leader) leader_fd = fd;
    event_infos[i].fd = fd;

    ioctl(event_infos[i].fd, PERF_EVENT_IOC_ID, &event_infos[i].id);
  }

  return leader_fd;
}
// PMU - end


/* This example creates a subclass of Node and uses std::bind() to register a
 * member function as a callback from the timer. */
namespace minimal_composition {

class MinimalPublisher : public rclcpp::Node
{
public:
  MinimalPublisher(rclcpp::NodeOptions options)
  : Node("minimal_publisher", options), count_(0)
  {
    // PMU - start
    if (pfm_initialize() != PFM_SUCCESS) {
      perror("pfm_initialize error");
      exit(EXIT_FAILURE);
    }

    global_leader_fd = prepare_event_infos();

    publisher_ = this->create_publisher<std_msgs::msg::String>("topic", 10);
    timer_ = this->create_wall_timer(
      500ms, std::bind(&MinimalPublisher::timer_callback, this));
  }

  ~MinimalPublisher() {
    for (int i = 0; i < MONITORED_EVENTS_NUM; i++) {
       close(event_infos[i].fd);
    }
  }

private:
  void timer_callback()
  {
    measurement_period_log mpl;
    ioctl(global_leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    ioctl(global_leader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    // PMU - end

    volatile int hoge = 1;
    for (int i = 0; i < hoge_count; i++) hoge++;
    hoge_count++;

    // PMU - start
    ioctl(global_leader_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    struct read_format *rf = (struct read_format*) buffer;
    int sz = read(global_leader_fd, buffer, sizeof(buffer));

    if (sz == -1) {
      perror("read error");
      exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < rf->nr; i++) {
      for (size_t j = 0; j < MONITORED_EVENTS_NUM; j++) {
        if (rf->values[i].id == event_infos[j].id) {
          mpl.values[j] = rf->values[i].value;
          RCLCPP_INFO(this->get_logger(), "value: %ld", mpl.values[j]);
        }
      }
    }
    // PMU - end

    auto message = std_msgs::msg::String();
    message.data = "Hello, world! " + std::to_string(count_++);
    RCLCPP_INFO(this->get_logger(), "Publishing: '%s'", message.data.c_str());
    publisher_->publish(message);
  }
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  size_t count_;
  int hoge_count = 10000;
};

} // namespace minimal_composition

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(minimal_composition::MinimalPublisher)
