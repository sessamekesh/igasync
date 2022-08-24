#include <gtest/gtest.h>
#include <igasync/promise.h>
#include <test_objects.h>

using namespace igasync;

namespace {
void flush_task_list(TaskList* tasks) {
  while (tasks->execute_next()) {
  }
}

}  // namespace

TEST(Promise, initializedPromiseIsNotResolved) {
  auto p = Promise<int>::Create();

  auto task_list = TaskList::Create();

  bool is_set = false;
  p->on_resolve([&is_set](const auto&) { is_set = true; }, task_list);

  ::flush_task_list(task_list.get());

  EXPECT_FALSE(is_set);
  EXPECT_FALSE(p->unsafe_is_finished());
}

TEST(Promise, immediatePromiseIsResolved) {
  auto p = Promise<int>::Immediate(100);

  auto task_list = TaskList::Create();

  bool is_set = false;
  int val = 0;
  p->on_resolve(
      [&is_set, &val](const auto& v) {
        is_set = true;
        val = v;
      },
      task_list);

  ::flush_task_list(task_list.get());

  EXPECT_TRUE(is_set);
  EXPECT_TRUE(p->unsafe_is_finished());
  EXPECT_EQ(val, 100);
  EXPECT_EQ(p->unsafe_sync_get(), 100);
}

TEST(Promise, resolutionTriggersExistingCallbacks) {
  auto p = Promise<int>::Create();

  auto task_list = TaskList::Create();

  bool first_method_called = false;
  int first_value = -1;
  bool second_method_called = false;
  int second_value = -1;

  p->on_resolve(
      [&first_method_called, &first_value](const auto& v) {
        first_method_called = true;
        first_value = v;
      },
      task_list);
  p->on_resolve(
      [&second_method_called, &second_value](const auto& v) {
        second_method_called = true;
        second_value = v;
      },
      task_list);

  ::flush_task_list(task_list.get());

  EXPECT_FALSE(first_method_called);
  EXPECT_FALSE(second_method_called);

  EXPECT_NE(p->resolve(100), nullptr);

  ::flush_task_list(task_list.get());

  EXPECT_TRUE(first_method_called);
  EXPECT_TRUE(second_method_called);
  EXPECT_EQ(first_value, 100);
  EXPECT_EQ(second_value, 100);
}

TEST(Promise, resolutionTriggersNewCallbacks) {
  auto p = Promise<int>::Create();

  auto task_list = TaskList::Create();

  bool first_method_called = false;
  int first_value = -1;
  bool second_method_called = false;
  int second_value = -1;

  EXPECT_NE(p->resolve(100), nullptr);

  p->on_resolve(
      [&first_method_called, &first_value](const auto& v) {
        first_method_called = true;
        first_value = v;
      },
      task_list);
  p->on_resolve(
      [&second_method_called, &second_value](const auto& v) {
        second_method_called = true;
        second_value = v;
      },
      task_list);

  ::flush_task_list(task_list.get());

  EXPECT_TRUE(first_method_called);
  EXPECT_TRUE(second_method_called);
  EXPECT_EQ(first_value, 100);
  EXPECT_EQ(second_value, 100);
}

TEST(Promise, canUseNonCopyableObjects) {
  auto p = Promise<::NonCopyableObject>::Create();

  auto task_list = TaskList::Create();

  int val = -1;
  int val2 = -1;

  p->on_resolve([&val](const auto& v) { val = v.InnerValue; }, task_list);
  p->consume([&val2](auto v) { val2 = v.InnerValue; }, task_list);

  EXPECT_NE(p->resolve(::NonCopyableObject(100)), nullptr);

  ::flush_task_list(task_list.get());

  EXPECT_EQ(val, 100);
  EXPECT_EQ(val2, 100);
}

TEST(Promise, holdsReferenceUntilDeletionIfNotConsumed) {
  int call_count = 0;
  int dtor_count = 0;

  auto p = Promise<::DestructorTracker>::Create();
  auto task_list = TaskList::Create();

  p->on_resolve([&call_count](const auto&) { call_count++; }, task_list);
  p->on_resolve([&call_count](const auto&) { call_count++; }, task_list);
  p->on_resolve([&call_count](const auto&) { call_count++; }, task_list);

  p->resolve(::DestructorTracker(&dtor_count));
  ::flush_task_list(task_list.get());

  EXPECT_EQ(call_count, 3);
  EXPECT_EQ(dtor_count, 0);

  // Trigger promise cleanup
  p = nullptr;

  EXPECT_EQ(call_count, 3);
  EXPECT_EQ(dtor_count, 1);
}

TEST(Promise, releasesOwnershipOnConsume) {
  int call_count = 0;
  int dtor_count = 0;

  auto p = Promise<::DestructorTracker>::Create();
  auto task_list = TaskList::Create();

  p->on_resolve([&call_count](const auto&) { call_count++; }, task_list);
  p->consume([&call_count](auto) { call_count++; }, task_list);

  p->resolve(::DestructorTracker(&dtor_count));
  ::flush_task_list(task_list.get());

  EXPECT_EQ(call_count, 2);
  EXPECT_EQ(dtor_count, 1);

  // Shouldn't do anything else
  p = nullptr;

  EXPECT_EQ(call_count, 2);
  EXPECT_EQ(dtor_count, 1);
}

TEST(Promise, thenPopulatesNewPromises) {
  int final_value = 0;

  auto p = Promise<int>::Create();
  auto task_list = TaskList::Create();

  auto p2 = p->then([](const int& v) { return v * 2; }, task_list);
  auto p4 = p2->then([](const int& v) { return v * 2; }, task_list);

  p4->on_resolve([&final_value](const int& v) { final_value = v; }, task_list);

  ::flush_task_list(task_list.get());

  EXPECT_EQ(final_value, 0);

  p->resolve(1);

  ::flush_task_list(task_list.get());

  EXPECT_EQ(final_value, 4);
}

TEST(Promise, thenChainPopulatesNewPromises) {
  int final_value = 0;

  auto p = Promise<int>::Create();
  auto task_list = TaskList::Create();
  auto background_task_list = TaskList::Create();

  auto p2 = p->then_chain(
      [background_task_list](const int& v) {
        auto scheduled_update = Promise<int>::Create();

        // Simulate a long-running task
        background_task_list->add_task(Task::of(
            [scheduled_update, v]() { scheduled_update->resolve(v * 2); }));

        return scheduled_update;
      },
      task_list);

  p2->on_resolve([&final_value](const int& v) { final_value = v; }, task_list);

  p->resolve(1);

  // Flush main task list without simulated background long-running task list
  ::flush_task_list(task_list.get());

  EXPECT_EQ(final_value, 0);

  // Flush background task list, triggering finish of inner promise for p2
  ::flush_task_list(background_task_list.get());

  // Percolate thennables from main task list
  ::flush_task_list(task_list.get());

  EXPECT_EQ(final_value, 2);
}
