#ifndef IGASYNC_TASK_LIST_H
#define IGASYNC_TASK_LIST_H

#include <concurrentqueue.h>
#include <igasync/execution_context.h>
#include <igasync/promise.h>
#include <igasync/task.h>

#include <shared_mutex>

namespace igasync {

/**
 * @brief Subscriber type for object receiving notifications for a task list
 *
 * Task lists are intended to be used with thread pools, which will need to
 * update a condition variable when tasks are added to a task list.
 */
class ITaskScheduledListener {
 public:
  virtual void on_task_added() = 0;
};

/**
 * @brief Thread-safe list of tasks that should be executed
 */
class TaskList : public ExecutionContext {
 public:
  /**
   * @brief Describes all parameters used to construct a TaskList, with
   *        reasonable defaults.
   */
  struct Desc {
    Desc() noexcept {}

    /**
     * @brief Hint for the initial size of underlying task store
     */
    size_t QueueSizeHint{20};

    /**
     * @brief Hint for the initial size of task listener store
     */
    size_t EnqueueListenerSizeHint{1};
  };

 public:
  TaskList(const TaskList&) = delete;
  TaskList(TaskList&&) = delete;
  TaskList& operator=(const TaskList&) = delete;
  TaskList& operator=(TaskList&&) = delete;

  /**
   * @brief Create a new TaskList from a given configuration object
   * @param desc Configuration object detailing how to build a TaskList
   * @return a new TaskList in a shared_ptr
   */
  static std::shared_ptr<TaskList> Create(Desc desc = Desc());

  /**
   * @brief Add a task to this task list, to be executed later
   * @param task Task to execute at some point in the future
   */
  virtual void schedule(std::unique_ptr<Task> task) override;

  /**
   * @brief Schedule a task, and return a promise containing the result
   */
  template <typename F, typename... Args>
  auto run(F&& f, Args&&... args)
      -> std::shared_ptr<Promise<std::invoke_result_t<F, Args...>>> {
    using ValT = std::invoke_result_t<F, Args...>;
    auto promise = Promise<ValT>::Create();

    if constexpr (std::same_as<ValT, void>) {
      schedule(Task::Of([promise, f, args...] {
        f(args...);
        promise->resolve();
      }));
    } else {
      schedule(
          Task::Of([promise, f, args...] { promise->resolve(f(args...)); }));
    }
    return promise;
  }

  /**
   * @brief Execute the next task in the task queue
   * @return True if a task was executed, false otherwise
   */
  bool execute_next();

  /**
   * @brief Register an ITaskScheduledListener with this task list
   * @param listener ITaskScheduledListener that should receive updates when
   *                 tasks are scheduled on this task list
   */
  void register_listener(std::shared_ptr<ITaskScheduledListener> listener);

  /**
   * @brief Unregister an ITaskScheduledListener with this task list
   * @param listener ITaskScheduledListener instance that should no longer
   *                 receive updates when tasks are scheduled on this task list
   */
  void unregister_listener(std::shared_ptr<ITaskScheduledListener> listener);

 private:
  TaskList(Desc desc);

  moodycamel::ConcurrentQueue<std::unique_ptr<Task>> tasks_;

  std::shared_mutex m_enqueue_listeners_;
  std::vector<std::shared_ptr<ITaskScheduledListener>> enqueue_listeners_;
};

}  // namespace igasync

#endif
