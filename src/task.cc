#include <igasync/task.h>

using namespace igasync;

void Task::run() {
  if (profile_cb_) {
    profile_data_.ExecutorThreadId = std::this_thread::get_id();
    profile_data_.Started = std::chrono::high_resolution_clock::now();
    fn_();
    profile_data_.Finished = std::chrono::high_resolution_clock::now();
    profile_cb_(profile_data_);
  } else {
    fn_();
  }
}

void Task::mark_scheduled() {
  profile_data_.Scheduled = std::chrono::high_resolution_clock::now();
}
