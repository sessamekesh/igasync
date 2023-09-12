#include <gtest/gtest.h>
#include <igasync/task_list.h>

using namespace igasync;

namespace {

void flush_task_list(TaskList* task_list) {
  while (task_list->execute_next()) {
  }
}

void noop() {}

class TestTaskScheduledListener : public ITaskScheduledListener {
 public:
  TestTaskScheduledListener(std::function<void()> cb) : cb_(cb) {}
  virtual void on_task_added() override { cb_(); }

 private:
  std::function<void()> cb_;
};

}  // namespace

using namespace igasync;

TEST(TaskList, executeNextReturnsFalseOnEmptyQueue) {
  auto task_list = TaskList::Create();

  EXPECT_FALSE(task_list->execute_next());
}

TEST(TaskList, executeNextReturnsTrueOnNonEmptyQueue) {
  auto task_list = TaskList::Create();

  task_list->schedule(Task::Of(::noop));

  EXPECT_TRUE(task_list->execute_next());
}

TEST(TaskList, executeInvokesScheduledTasks) {
  auto task_list = TaskList::Create();

  int rsl1 = 0, rsl2 = 0, rsl3 = 0;

  task_list->schedule(Task::Of([&rsl1]() { rsl1 = 1; }));
  task_list->schedule(Task::Of([&rsl2]() { rsl2 = 1; }));
  task_list->schedule(Task::Of([&rsl3]() { rsl3 = 1; }));

  // Notice: It is not necessary that tasks are executed strictly in order.
  // Do not test for tasks to be executed strictly in order.

  EXPECT_EQ(rsl1 + rsl2 + rsl3, 0);

  EXPECT_TRUE(task_list->execute_next());

  // At this point, ONE of the tasks should have executed.
  EXPECT_EQ(rsl1 + rsl2 + rsl3, 1);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_EQ(rsl1 + rsl2 + rsl3, 2);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_EQ(rsl1 + rsl2 + rsl3, 3);

  EXPECT_FALSE(task_list->execute_next());
}

TEST(TaskList, registeredListenersReceiveUpdatesOnSchedule) {
  int tasks_scheduled = 0;

  auto task_list = TaskList::Create();
  auto listener = std::make_shared<TestTaskScheduledListener>(
      [&tasks_scheduled]() { tasks_scheduled++; });

  task_list->register_listener(listener);

  EXPECT_EQ(tasks_scheduled, 0);

  task_list->schedule(Task::Of(::noop));

  EXPECT_EQ(tasks_scheduled, 1);
}

TEST(TaskList, unregisteredListenersDoNotReceiveUpdatesOnSchedule) {
  int tasks_scheduled = 0;

  auto task_list = TaskList::Create();
  auto listener = std::make_shared<TestTaskScheduledListener>(
      [&tasks_scheduled]() { tasks_scheduled++; });

  task_list->register_listener(listener);

  EXPECT_EQ(tasks_scheduled, 0);

  task_list->schedule(Task::Of(::noop));

  EXPECT_EQ(tasks_scheduled, 1);

  task_list->unregister_listener(listener);

  task_list->schedule(Task::Of(::noop));

  EXPECT_EQ(tasks_scheduled, 1);
}
