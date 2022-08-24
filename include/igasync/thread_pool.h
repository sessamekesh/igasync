#ifndef IGASYNC_EXECUTOR_THREAD_H
#define IGASYNC_EXECUTOR_THREAD_H

#include <atomic>
#include <condition_variable>
#include <memory>

#include "task_list.h"

namespace igasync {

/**
 * ThreadPool
 *
 * Spawns and maintains a collection of threads that attempts to pull from
 * provided task lists. Threads may be slept if all task lists are empty, and
 * re-awoken when tasks are added (via ITaskScheduledListener)
 *
 * Task lists and thread pools have a potentially many:many relationship, but
 * typically there is only one thread pool and a small collection of task lists
 * that feed it.
 */
class ThreadPool : public std::enable_shared_from_this<ThreadPool>,
                   public ITaskScheduledListener {
 public:
  struct Desc {
    /** Use hardware concurrency to determine number of threads */
    bool UseHardwareConcurrency = true;

    /** Additional threads to add to the pool (positive or negative) */
    int AddAdditionalThreads = 0;

    /** Log debug messages (to cout) */
    bool EnableDebugMessages = false;
  };

 public:
  static std::shared_ptr<ThreadPool> Create(Desc desc = Desc());
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  void add_task_list(std::shared_ptr<TaskList> task_list);
  void remove_task_list(std::shared_ptr<TaskList> task_list);
  void clear_all_task_lists();

  // ITaskScheduledListener
  void on_task_added() override;

 private:
  ThreadPool(Desc desc = Desc());

 private:
  const bool enable_debug_messages_;

  std::atomic_bool is_cancelled_;
  std::vector<std::thread> threads_;
  std::atomic_size_t next_task_list_idx_;

  std::shared_mutex m_task_lists_;
  std::vector<std::shared_ptr<TaskList>> task_lists_;

  std::condition_variable cv_has_task_;
  std::mutex m_has_task_;
};

}  // namespace igasync

#endif
