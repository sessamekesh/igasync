#include <gtest/gtest.h>
#include <igasync/task.h>

using namespace igasync;

TEST(Task, executesVoid) {
  int rsl = 0;
  auto task = Task::Of([&rsl]() { rsl = 5; });

  EXPECT_EQ(rsl, 0);
  task->run();
  EXPECT_EQ(rsl, 5);
}

TEST(Task, executesVoidFnWithParams) {
  int rsl = 0;
  auto task = Task::Of([&rsl](int a, int b) { rsl = a + b; }, 2, 4);

  EXPECT_EQ(rsl, 0);
  task->run();
  EXPECT_EQ(rsl, 6);
}

TEST(Task, executesNonVoidFn) {
  int rsl = 0;
  auto task = Task::Of(
      [&rsl](int a, int b) {
        rsl = a + b;
        return a + b;
      },
      2, 4);

  EXPECT_EQ(rsl, 0);
  task->run();
  EXPECT_EQ(rsl, 6);
}

TEST(Task, ExposesProfilingInformation) {
  auto test_start = std::chrono::high_resolution_clock::now();

  TaskProfile task_profile;
  int profile_ct = 0;
  auto get_profile_cb = [&task_profile, &profile_ct](TaskProfile profile) {
    task_profile = profile;
    profile_ct++;
  };

  double rsl = 0;
  int invoke_ct = 0;
  auto method = [&rsl, &invoke_ct](double a, double b) {
    rsl = std::sqrt(a * a + b * b);
    invoke_ct++;
  };

  auto task = Task::WithProfile(get_profile_cb, method, 3., 4.);
  task->mark_scheduled();
  task->run();

  EXPECT_EQ(invoke_ct, 1);
  EXPECT_EQ(profile_ct, 1);
  EXPECT_TRUE(task_profile.Created > test_start);
  EXPECT_TRUE(task_profile.Scheduled >= task_profile.Created);
  EXPECT_TRUE(task_profile.Started >= task_profile.Scheduled);
  EXPECT_TRUE(task_profile.Finished > task_profile.Started);
  EXPECT_EQ(task_profile.ExecutorThreadId, std::this_thread::get_id());
}
