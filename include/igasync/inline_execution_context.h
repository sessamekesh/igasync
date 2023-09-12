#ifndef IGASYNC_INLINE_EXECUTION_CONTEXT_H
#define IGASYNC_INLINE_EXECUTION_CONTEXT_H

#include <igasync/execution_context.h>
#include <igasync/task.h>

namespace igasync {

/**
 * @brief Execution context that immediately executes scheduled tasks
 *
 * This execution context invokes a given task immediately, on the current
 * thread. It's a useful default in cases where an execution context is desired
 * but there's no task list set up for use.
 */
class InlineExecutionContext : public ExecutionContext {
 public:
  virtual void schedule(std::unique_ptr<Task> task) override;
};
}  // namespace igasync

#endif
