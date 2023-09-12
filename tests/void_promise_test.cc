#include <gtest/gtest.h>
#include <igasync/promise.h>
#include <igasync/task_list.h>

using namespace igasync;

TEST(VoidPromise, defaultPromiseIsNotResolved) {
  auto p = Promise<void>::Create();
  EXPECT_FALSE(p->is_finished());
}

TEST(VoidPromise, immediatePromiseIsResolved) {
  auto p = Promise<void>::Immediate();
  bool is_set = false;
  EXPECT_TRUE(p->is_finished());
  p->on_resolve([&is_set]() { is_set = true; });
  EXPECT_TRUE(is_set);
}

TEST(VoidPromise, tasksScheduledOnResolve) {
  auto p = Promise<void>::Create();
  auto task_list = TaskList::Create();

  bool is_set = false;
  p->on_resolve([&is_set]() { is_set = true; }, task_list);

  bool second_resolved = false;
  p->on_resolve([&second_resolved]() { second_resolved = true; }, task_list);

  EXPECT_FALSE(is_set);
  EXPECT_FALSE(second_resolved);

  p->resolve();

  EXPECT_FALSE(is_set);
  EXPECT_FALSE(second_resolved);

  EXPECT_TRUE(task_list->execute_next());

  EXPECT_TRUE(is_set || second_resolved);
  EXPECT_FALSE(is_set && second_resolved);

  EXPECT_TRUE(task_list->execute_next());

  EXPECT_TRUE(is_set);
  EXPECT_TRUE(second_resolved);
}
