#ifndef IGASYNC_TASK_LIST_H
#define IGASYNC_TASK_LIST_H

#include <concurrentqueue.h>
#include <igasync/concepts.h>
#include <igasync/task_list_base.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>

#include "task.h"

namespace igasync {

template <class>
class Promise;

/** Interface for an object that listens for tasks being added to a queue */
class ITaskScheduledListener {
 public:
  virtual void on_task_added() = 0;
};

/**
 * Thread-safe list of tasks that need to be executed with
 */
class TaskList : public TaskListBase,
                 public std::enable_shared_from_this<TaskList> {
 public:
  struct Desc {
    /**
     * Rough estimate for initial size of the task list (how many elements is a
     * reasonable maximum?)
     */
    size_t QueueSizeHint = 20;

    /**
     * Estimate of the number of listeners that will be registered to listen for
     * new tasks being enqueued in this task list
     */
    size_t EnqueueListenerSizeHint = 1;
  };

 public:
  TaskList(const TaskList&) = delete;
  TaskList(TaskList&&) = delete;
  TaskList& operator=(const TaskList&) = delete;
  TaskList& operator=(TaskList&&) = delete;

  static std::shared_ptr<TaskList> Create(Desc desc = Desc());

  /** Add a task to the queue. To execute a task, call execute_next */
  void add_task(std::unique_ptr<Task> task);

  /** Execute a task, return true if a task was executed, false otherwise */
  bool execute_next();

  /** Register an event listener for when a new task is added to the queue */
  void register_listener(std::shared_ptr<ITaskScheduledListener> listener);

  /** Unregister a previously registered event listener */
  void unregister_listener(std::shared_ptr<ITaskScheduledListener> listener);

 private:
  TaskList(Desc desc);

  moodycamel::ConcurrentQueue<std::unique_ptr<Task>> tasks_;

  std::shared_mutex m_enqueue_listeners_;
  std::vector<std::shared_ptr<ITaskScheduledListener>> enqueue_listeners_;
};

}  // namespace igasync

#endif
