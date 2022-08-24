#include <igasync/task_list.h>

using namespace igasync;

std::shared_ptr<TaskList> TaskList::Create(Desc desc) {
  return std::shared_ptr<TaskList>(new TaskList(desc));
}

TaskList::TaskList(Desc desc)
    : tasks_(desc.QueueSizeHint), enqueue_listeners_() {
  enqueue_listeners_.reserve(desc.EnqueueListenerSizeHint);
}

void TaskList::add_task(std::unique_ptr<Task> task) {
  tasks_.enqueue(std::move(task));

  std::shared_lock l(m_enqueue_listeners_);
  for (auto& listener : enqueue_listeners_) {
    listener->on_task_added();
  }
}

void TaskList::register_listener(
    std::shared_ptr<ITaskScheduledListener> listener) {
  std::unique_lock l(m_enqueue_listeners_);
  enqueue_listeners_.push_back(listener);
}

void TaskList::unregister_listener(
    std::shared_ptr<ITaskScheduledListener> listener) {
  std::unique_lock l(m_enqueue_listeners_);
  enqueue_listeners_.erase(std::remove(enqueue_listeners_.begin(),
                                       enqueue_listeners_.end(), listener),
                           enqueue_listeners_.end());
}

bool TaskList::execute_next() {
  std::unique_ptr<Task> task = nullptr;
  if (tasks_.try_dequeue(task)) {
    task->Fn();
    return true;
  }

  return false;
}
