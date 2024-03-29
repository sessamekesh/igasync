#include <igasync/task.h>

#include <utility>

template <class F, class... Args>
std::unique_ptr<Task> Task::WithProfile(
    std::function<void(TaskProfile)> profile_cb, F&& f, Args&&... args) {
  std::function<void()> fn =
      std::bind(std::forward<F>(f), std::forward<Args>(args)...);
  return std::unique_ptr<Task>(new Task(std::move(fn), std::move(profile_cb)));
}

template <class F, class... Args>
std::unique_ptr<Task> Task::Of(F&& f, Args&&... args) {
  std::function<void()> fn =
      std::bind(std::forward<F>(f), std::forward<Args>(args)...);
  return std::unique_ptr<Task>(new Task(std::move(fn)));
}
