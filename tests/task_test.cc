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
