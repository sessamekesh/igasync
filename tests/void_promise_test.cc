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

TEST(VoidPromise, thenWorks) {
  int final_value = 0;

  auto p = Promise<void>::Create();
  auto p2 = p->then([]() { return NonCopyable(5); });
  auto p3 =
      p2->then([](const NonCopyable& nc) { return NonCopyable(nc.val() * 2); });
  auto p4 = p3->then(
      [&final_value](const NonCopyable& nc) { final_value = nc.val(); });

  p->resolve();

  EXPECT_EQ(final_value, 10);
}

TEST(VoidPromise, thenChainWorks) {
  auto p = Promise<void>::Create();
  auto f = [] { return Promise<void>::Immediate(); };

  auto p2 = p->then_chain(f)->then_chain(f)->then_chain(f);

  EXPECT_FALSE(p2->is_finished());
  p->resolve();
  EXPECT_TRUE(p2->is_finished());
}
