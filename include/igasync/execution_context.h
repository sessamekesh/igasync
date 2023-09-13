#ifndef IGASYNC_EXECUTION_CONTEXT_H
#define IGASYNC_EXECUTION_CONTEXT_H

#include <igasync/task.h>

namespace igasync {

/**
 * @brief Interface for an object that can schedule a task.
 *
 * The main use cases considered in the design of this library are:
 * 1. Execute a scheduled task immediately, without waiting
 * 2. Enqueue a task to be executed later, perhaps on a separate thread
 */
class ExecutionContext {
 public:
  virtual void schedule(std::unique_ptr<Task> task) = 0;
};

}  // namespace igasync

#endif
