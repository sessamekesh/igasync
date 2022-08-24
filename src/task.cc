#include <igasync/task.h>

using namespace igasync;

std::unique_ptr<Task> Task::of(std::function<void()> fn) {
  return std::unique_ptr<Task>(new Task{std::move(fn)});
}
