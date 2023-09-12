#include <gtest/gtest.h>
#include <igasync/promise_combiner.h>
#include <test_objects.h>

using namespace igasync;

namespace {}

TEST(PromiseCombiner, basic_combine) {
  auto task_list = TaskList::Create();

  auto p1 = Promise<int>::Create();
  auto p2 = Promise<int>::Create();

  auto combiner =
      PromiseCombiner::Create(PromiseCombiner::default_options(task_list));

  auto key_1 = combiner->add(p1);
  auto key_2 = combiner->add(p2);

  int r1 = -1, r2 = -1;
  bool has_resolved = false;

  combiner->combine<void>([&r1, &r2, &has_resolved, key_1,
                           key_2](const PromiseCombiner::Result& rsl) {
    has_resolved = true;

    r1 = rsl.get(key_1);
    r2 = rsl.get(key_2);
  });

  // No execution yet
  ASSERT_FALSE(has_resolved);
  EXPECT_EQ(r1, -1);
  EXPECT_EQ(r2, -1);

  // No tasks should have been queued yet
  while (task_list->execute_next()) {
    FAIL() << "Task list should be empty at this point";
  }

  // Incomplete combiners should not trigger
  p1->resolve(1);

  while (task_list->execute_next()) {
  }

  ASSERT_FALSE(has_resolved);
  EXPECT_EQ(r1, -1);
  EXPECT_EQ(r2, -1);

  // Finished combiner should trigger callback
  p2->resolve(2);

  while (task_list->execute_next()) {
  }

  ASSERT_TRUE(has_resolved);
  EXPECT_EQ(r1, 1);
  EXPECT_EQ(r2, 2);
}

TEST(PromiseCombiner, AllowsConsumingMembers) {
  auto task_list = TaskList::Create();

  auto p1 = Promise<NonCopyableObject>::Create();
  auto p2 = Promise<NonCopyableObject>::Create();

  auto combiner =
      PromiseCombiner::Create(PromiseCombiner::default_options(task_list));

  auto key_1 = combiner->add_consuming(p1);
  auto key_2 = combiner->add(p2);

  NonCopyableObject v1(-1);
  int v2 = -1;
  bool is_finished = false;

  combiner->combine<void>(
      [key_1, key_2, &v1, &v2, &is_finished](PromiseCombiner::Result rsl) {
        v1 = rsl.move(key_1);
        v2 = rsl.get(key_2).InnerValue;

        is_finished = true;
      });

  p1->resolve(NonCopyableObject(1));
  p2->resolve(NonCopyableObject(2));

  while (task_list->execute_next()) {
  }

  EXPECT_TRUE(is_finished);
  EXPECT_EQ(v1.InnerValue, 1);
  EXPECT_EQ(v2, 2);
}

TEST(PromiseCombiner, DestructsAfterResolving) {
  auto task_list = TaskList::Create();

  int dtor_1 = 0, dtor_2 = 0;
  bool has_run = false;

  std::shared_ptr<Promise<void>> rsl_promise = nullptr;

  {
    auto p1 = Promise<DestructorTracker>::Create();
    auto p2 = Promise<DestructorTracker>::Create();

    auto combiner =
        PromiseCombiner::Create(PromiseCombiner::default_options(task_list));

    auto key_1 = combiner->add(p1);
    auto key_2 = combiner->add(p2);

    combiner->combine([&has_run](auto rsl) { has_run = true; });

    p1->resolve(DestructorTracker(&dtor_1));
    p2->resolve(DestructorTracker(&dtor_2));

    while (task_list->execute_next()) {
    }

    ASSERT_TRUE(has_run);
    ASSERT_EQ(dtor_1, 0);
    ASSERT_EQ(dtor_2, 0);
  }

  ASSERT_EQ(dtor_1, 1);
  ASSERT_EQ(dtor_2, 1);
}
