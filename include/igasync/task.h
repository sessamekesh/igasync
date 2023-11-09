#ifndef IGASYNC_TASK_H
#define IGASYNC_TASK_H

#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace igasync {

struct TaskProfile {
  std::chrono::high_resolution_clock::time_point Created;
  std::chrono::high_resolution_clock::time_point Scheduled;
  std::chrono::high_resolution_clock::time_point Started;
  std::chrono::high_resolution_clock::time_point Finished;
  std::thread::id ExecutorThreadId;
};

/**
 * Dumb wrapper around a void function.
 */
class Task {
 public:
  template <class F, class... Args>
  static std::unique_ptr<Task> WithProfile(
      std::function<void(TaskProfile)> profile_cb, F&& f, Args&&... args);

  template <class F, class... Args>
  static std::unique_ptr<Task> Of(F&& f, Args&&... args);

  void mark_scheduled();
  void run();

 private:
  Task(std::function<void()>&& fn,
       std::function<void(TaskProfile)> profile_cb = nullptr)
      : fn_(fn), profile_cb_(std::move(profile_cb)) {
    profile_data_.Created = std::chrono::high_resolution_clock::now();
  }
  std::function<void()> fn_;
  std::function<void(TaskProfile)> profile_cb_;
  TaskProfile profile_data_;
};

#include <igasync/task.inl>

}  // namespace igasync

#endif
