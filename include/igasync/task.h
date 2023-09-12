#ifndef IGASYNC_TASK_H
#define IGASYNC_TASK_H

#include <functional>
#include <memory>

namespace igasync {

/**
 * Dumb wrapper around a void function.
 *
 * Future versions of this library may include profiling information
 */
class Task {
 public:
  template <class F, class... Args>
  static std::unique_ptr<Task> Of(F&& f, Args&&... args);

  void run();

 private:
  Task(std::function<void()>&& fn) : fn_(fn) {}
  std::function<void()> fn_;
};

#include <igasync/task.inl>

}  // namespace igasync

#endif
