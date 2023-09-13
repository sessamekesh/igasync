#ifndef IGASYNC_THREAD_POOL_H
#define IGASYNC_THREAD_POOL_H

#include <igasync/task_list.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>

namespace igasync {

/**
 * @brief Thread pool implementation
 *
 * May execute tasks from zero or more TaskList instances
 */
class ThreadPool : public std::enable_shared_from_this<ThreadPool>,
                   public ITaskScheduledListener {
 public:
  /**
   * @brief Descriptor used to initialize a ThreadPool instance
   */
  struct Desc {
    Desc() noexcept {}

    /** Use hardware concurrency to determine the number of threads */
    bool UseHardwareConcurrency{true};

    /** Additional threads to add to the pool (positive or negative) */
    int AdditionalThreads{0};
  };

 public:
  static std::shared_ptr<ThreadPool> Create(Desc desc = Desc{});
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  void add_task_list(std::shared_ptr<TaskList> task_list);
  void remove_task_list(std::shared_ptr<TaskList> task_list);
  void clear_all_task_lists();

  // ITaskScheduledListener
  virtual void on_task_added() override;

 private:
  ThreadPool(Desc desc);

 private:
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
