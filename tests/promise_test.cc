#include <gtest/gtest.h>
#include <igasync/promise.h>
#include <igasync/task_list.h>

using namespace igasync;

namespace {
class NonCopyable {
 public:
  NonCopyable(int val) : val_(val) {}
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&& o) = default;
  NonCopyable& operator=(NonCopyable&& o) = default;

  int val() const { return val_; }

 private:
  int val_;
};
}  // namespace

TEST(Promise, defaultPromiseIsNotResolved) {
  auto p = Promise<int>::Create();

  bool is_set = false;

  p->on_resolve([&is_set](const auto&) { is_set = true; });

  EXPECT_FALSE(is_set);
  EXPECT_FALSE(p->is_finished());
}

TEST(Promise, immediatePromiseIsResolved) {
  auto p = Promise<int>::Immediate(42);

  bool is_set = false;
  int observed_value = 0;

  EXPECT_TRUE(p->is_finished());

  p->on_resolve([&is_set, &observed_value](const auto& v) {
    is_set = true;
    observed_value = v;
  });

  EXPECT_TRUE(is_set);
  EXPECT_EQ(observed_value, 42);
}

TEST(Promise, tasksScheduledOnResolve) {
  auto p = Promise<int>::Create();
  auto task_list = TaskList::Create();

  bool is_set = false;
  int observed_value = 0;

  p->on_resolve(
      [&is_set, &observed_value](const auto& v) {
        is_set = true;
        observed_value = v;
      },
      task_list);

  bool second_task_resolved = false;
  p->on_resolve(
      [&second_task_resolved](const auto&) { second_task_resolved = true; },
      task_list);

  EXPECT_FALSE(is_set);
  EXPECT_EQ(observed_value, 0);

  EXPECT_FALSE(task_list->execute_next());

  p->resolve(42);

  EXPECT_FALSE(is_set);
  EXPECT_EQ(observed_value, 0);

  // This is an implementation detail, may want to flush the task list instead
  // That said, it'll be nice to see if the task list has more than 2 tasks
  //  scheduled for some reason in the future (is that really necessary?)
  EXPECT_TRUE(task_list->execute_next());
  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(is_set);
  EXPECT_EQ(observed_value, 42);
  EXPECT_TRUE(second_task_resolved);
}

TEST(Promise, worksWithNonCopyableTypes) {
  auto p = Promise<NonCopyable>::Create();
  p->resolve(NonCopyable(5));

  int inner_value = 0;
  p->on_resolve(
      [&inner_value](const NonCopyable& v) { inner_value = v.val(); });

  EXPECT_EQ(inner_value, 5);
}

TEST(Promise, consumesWithNonCopyableTypes) {
  auto p = Promise<NonCopyable>::Create();
  p->resolve(NonCopyable(5));

  int consumed_value = 0;
  p->consume([&consumed_value](NonCopyable v) { consumed_value = v.val(); });

  EXPECT_EQ(consumed_value, 5);
}

TEST(Promise, doesThensThenConsumes) {
  auto p = Promise<NonCopyable>::Create();
  auto task_list = TaskList::Create();

  int consumed_value = 0;
  int peeked_value = 0;

  p->on_resolve([&peeked_value](const auto& v) { peeked_value = v.val(); },
                task_list)
      ->consume([&consumed_value](auto v) { consumed_value = v.val(); },
                task_list);

  p->resolve(NonCopyable(5));

  EXPECT_EQ(consumed_value, 0);
  EXPECT_EQ(peeked_value, 0);

  // Implementation detail - may schedule more than 1 task, but shouldn't
  EXPECT_TRUE(task_list->execute_next());

  EXPECT_EQ(peeked_value, 5);
  EXPECT_EQ(consumed_value, 0);

  EXPECT_TRUE(task_list->execute_next());

  EXPECT_EQ(peeked_value, 5);
  EXPECT_EQ(consumed_value, 5);
}

TEST(Promise, unsafeSyncGetWorks) {
  auto p = Promise<NonCopyable>::Create();

  p->resolve(NonCopyable(5));

  EXPECT_EQ(p->unsafe_sync_peek().val(), 5);
}

TEST(Promise, unsafeSyncMoveWorks) {
  auto p = Promise<NonCopyable>::Create();

  p->resolve(NonCopyable(5));

  EXPECT_EQ(p->unsafe_sync_move().val(), 5);
}
