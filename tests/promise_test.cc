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

void flush_task_list(std::shared_ptr<TaskList> tl) {
  while (tl->execute_next())
    ;
}
}  // namespace

TEST(Promise, defaultPromiseIsNotResolved) {
  auto tl = TaskList::Create();
  auto p = Promise<int>::Create();

  bool is_set = false;

  p->on_resolve([&is_set](const auto&) { is_set = true; }, tl);
  ::flush_task_list(tl);

  EXPECT_FALSE(is_set);
  EXPECT_FALSE(p->is_finished());
}

TEST(Promise, immediatePromiseIsResolved) {
  auto tl = TaskList::Create();
  auto p = Promise<int>::Immediate(42);

  bool is_set = false;
  int observed_value = 0;

  EXPECT_TRUE(p->is_finished());

  p->on_resolve(
      [&is_set, &observed_value](const auto& v) {
        is_set = true;
        observed_value = v;
      },
      tl);
  ::flush_task_list(tl);

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
  auto tl = TaskList::Create();
  auto p = Promise<NonCopyable>::Create();
  p->resolve(NonCopyable(5));

  int inner_value = 0;
  p->on_resolve([&inner_value](const NonCopyable& v) { inner_value = v.val(); },
                tl);
  ::flush_task_list(tl);

  EXPECT_EQ(inner_value, 5);
}

TEST(Promise, consumesWithNonCopyableTypes) {
  auto tl = TaskList::Create();
  auto p = Promise<NonCopyable>::Create();
  p->resolve(NonCopyable(5));

  int consumed_value = 0;
  p->consume([&consumed_value](NonCopyable v) { consumed_value = v.val(); },
             tl);
  ::flush_task_list(tl);

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

TEST(Promise, nonVoidThenChainingWorks) {
  auto tl = TaskList::Create();

  int final_value = 0;

  auto p = Promise<NonCopyable>::Create();
  auto p2 = p->then(
      [](const NonCopyable& nc) { return NonCopyable(nc.val() * 2); }, tl);
  auto p3 = p2->then(
      [&final_value](const NonCopyable& n) { final_value = n.val(); }, tl);
  ::flush_task_list(tl);

  constexpr bool p2TypeIsExpected =
      std::same_as<decltype(p2), std::shared_ptr<Promise<NonCopyable>>>;
  constexpr bool p3TypeIsExpected =
      std::same_as<decltype(p3), std::shared_ptr<Promise<void>>>;

  EXPECT_TRUE(p2TypeIsExpected);
  EXPECT_TRUE(p3TypeIsExpected);

  EXPECT_EQ(final_value, 0);

  p->resolve(NonCopyable(1));
  ::flush_task_list(tl);

  EXPECT_EQ(final_value, 2);
}

TEST(Promise, thenConsumeWorks) {
  auto tl = TaskList::Create();

  int final_value = 0;
  std::invocable<int, bool>;
  auto p = Promise<int>::Create();
  auto p2 =
      p->then_consuming([](int a) { return NonCopyable(a); }, tl)
          ->then_consuming(
              [](NonCopyable a) { return NonCopyable(a.val() * 2); }, tl)
          ->then_consuming(
              [&final_value](NonCopyable a) { final_value = a.val(); }, tl);

  p->resolve(2);
  ::flush_task_list(tl);

  EXPECT_EQ(final_value, 4);
}

TEST(Promise, thenChainWorks) {
  auto tl = TaskList::Create();

  int final_value = 0;

  auto p = Promise<int>::Create();
  auto pout =
      p->then_consuming([](int val) { return NonCopyable(val); }, tl)
          ->then_chain(
              [](const NonCopyable& val) {
                return Promise<NonCopyable>::Immediate(
                    NonCopyable(val.val() * 2));
              },
              tl)
          ->then([&final_value](const auto& val) { final_value = val.val(); },
                 tl);

  p->resolve(2);
  ::flush_task_list(tl);

  EXPECT_EQ(final_value, 4);
}

TEST(Promise, thenChainConsumingWorks) {
  auto tl = TaskList::Create();

  int final_value = 0;

  auto p = Promise<int>::Create();
  auto pout =
      p->then_chain_consuming(
           [](int val) {
             return Promise<NonCopyable>::Immediate(NonCopyable(val));
           },
           tl)
          ->then_chain_consuming(
              [](NonCopyable v) {
                return Promise<NonCopyable>::Immediate(
                    NonCopyable(v.val() * 2));
              },
              tl)
          ->consume(
              [&final_value](NonCopyable val) { final_value = val.val(); }, tl);

  p->resolve(2);
  ::flush_task_list(tl);
  EXPECT_EQ(final_value, 4);
}

TEST(Promise, consumeHappensLastInVariableSpeedExecutionContexts) {
  auto p = Promise<int>::Create();

  auto slow_list = TaskList::Create();
  auto fast_list = TaskList::Create();

  bool is_thenned = false;
  bool is_consumed = false;

  p->on_resolve([&is_thenned](const auto&) { is_thenned = true; }, slow_list);
  p->consume([&is_consumed](auto) { is_consumed = true; }, fast_list);

  EXPECT_FALSE(is_thenned);
  EXPECT_FALSE(is_consumed);

  p->resolve(10);

  EXPECT_FALSE(is_thenned);
  EXPECT_FALSE(is_consumed);

  EXPECT_FALSE(fast_list->execute_next());

  EXPECT_FALSE(is_thenned);
  EXPECT_FALSE(is_consumed);

  EXPECT_TRUE(slow_list->execute_next());

  EXPECT_TRUE(is_thenned);
  EXPECT_FALSE(is_consumed);

  EXPECT_TRUE(fast_list->execute_next());

  EXPECT_TRUE(is_thenned);
  EXPECT_TRUE(is_consumed);
}
