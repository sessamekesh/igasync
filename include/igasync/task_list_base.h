#ifndef IGASYNC_TASK_LIST_INTERFACE_H
#define IGASYNC_TASK_LIST_INTERFACE_H

#include <igasync/task.h>

#include <memory>

namespace igasync {

class TaskListBase {
 public:
  virtual void add_task(std::unique_ptr<Task> task) = 0;
  virtual bool execute_next() = 0;
};

}  // namespace igasync

#endif
