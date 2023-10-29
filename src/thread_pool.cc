#include <igasync/thread_pool.h>

using namespace igasync;

std::shared_ptr<ThreadPool> ThreadPool::Create(ThreadPool::Desc desc) {
  return std::shared_ptr<ThreadPool>(new ThreadPool(desc));
}

ThreadPool::ThreadPool(ThreadPool::Desc desc)
    : is_cancelled_(false), next_task_list_idx_(0) {
  size_t num_threads = desc.AdditionalThreads;
  if (desc.UseHardwareConcurrency) {
    num_threads += std::thread::hardware_concurrency();
  }

  // No threads - no-op
  if (num_threads <= 0) {
    return;
  }

  for (size_t i = 0; i < num_threads; i++) {
    threads_.push_back(std::thread([this, t = this]() {
      while (!t->is_cancelled_) {
        // Execute tasks from the task provider until there are no more tasks to
        // execute...
        while (!t->is_cancelled_) {
          std::shared_lock l(t->m_task_lists_);
          bool task_executed = false;
          for (int i = 0; i < t->task_lists_.size(); i++) {
            int idx =
                (int)((i + t->next_task_list_idx_) % t->task_lists_.size());
            if (t->task_lists_[i]->execute_next()) {
              t->next_task_list_idx_ = (i + 1ll) % t->task_lists_.size();
              task_executed = true;
              break;
            }
          }

          // If no tasks are executed, exit inner loop and wait for condition
          // variable
          if (!task_executed) {
            break;
          }
        }

        // This thread can rest, since all task lists are empty
        std::unique_lock l(t->m_has_task_);
        t->cv_has_task_.wait(l, [t]() {
          // Predicate is not matched if task provider is empty, leave and wait
          std::shared_lock l(t->m_task_lists_);

          // If the task provider successfully executed a task, stop blocking!
          for (int i = 0; i < t->task_lists_.size(); i++) {
            int idx =
                (int)((i + t->next_task_list_idx_) % t->task_lists_.size());
            if (t->task_lists_[idx]->execute_next()) {
              t->next_task_list_idx_ = (idx + 1ll) % t->task_lists_.size();
              return true;
            }
          }

          // No task was executed, but still continue if this thread is
          // shutting down
          return t->is_cancelled_.load();
        });
      }
    }));
  }
}

ThreadPool::~ThreadPool() {
  clear_all_task_lists();
  is_cancelled_ = true;
  cv_has_task_.notify_all();

  for (auto& t : threads_) {
    t.join();
  }
}

void ThreadPool::add_task_list(std::shared_ptr<TaskList> task_list) {
  // Prevent double-add
  remove_task_list(task_list);
  {
    std::unique_lock l(m_task_lists_);
    task_lists_.push_back(task_list);
    task_list->register_listener(shared_from_this());
  }
  cv_has_task_.notify_all();
}

void ThreadPool::remove_task_list(std::shared_ptr<TaskList> task_list) {
  std::unique_lock l(m_task_lists_);
  task_lists_.erase(
      std::remove(task_lists_.begin(), task_lists_.end(), task_list),
      task_lists_.end());
}

void ThreadPool::clear_all_task_lists() {
  {
    std::unique_lock l(m_task_lists_);
    for (int i = 0; i < task_lists_.size(); i++) {
      task_lists_[i]->unregister_listener(shared_from_this());
    }
    task_lists_.clear();
  }
  cv_has_task_.notify_all();
}

std::vector<std::thread::id> ThreadPool::thread_ids() const {
  std::vector<std::thread::id> ids;

  for (int i = 0; i < threads_.size(); i++) {
    ids.push_back(threads_[i].get_id());
  }

  return ids;
}

void ThreadPool::on_task_added() { cv_has_task_.notify_one(); }
