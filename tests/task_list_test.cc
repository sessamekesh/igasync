#include <gtest/gtest.h>
#include <igasync/task_list.h>

using namespace igasync;

namespace {
void flush_task_list(TaskList* tasks) {
  while (tasks->execute_next()) {
  }
}
}  // namespace

TEST(TaskList, schedulesAndRunsTask) {
  auto task_list = TaskList::Create();
  bool is_run = false;
  task_list->add_task(Task::of([&is_run]() { is_run = true; }));
  EXPECT_FALSE(is_run);
  ::flush_task_list(task_list.get());
  EXPECT_TRUE(is_run);
}

TEST(TaskList, schedulesPromiseThatResolves) {
  auto task_list = TaskList::Create();

  // auto rslPromise = task_list->run([]() {});

  // EXPECT_FALSE(rslPromise->unsafe_is_finished());
}
