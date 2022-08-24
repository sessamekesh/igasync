#ifndef IGASYNC_TASK_H
#define IGASYNC_TASK_H

#include <functional>
#include <memory>

namespace igasync {

/**
 * A task is a wrapper around a single void function - any side effects should
 * be part of a lambda capture.
 *
 * Future versions of this library may include profiling information (schedule
 * time, execute time, run time, etc) but for now this is basic.
 */
struct Task {
  std::function<void()> Fn;

  static std::unique_ptr<Task> of(std::function<void()> fn);
};

}  // namespace igasync

#endif
