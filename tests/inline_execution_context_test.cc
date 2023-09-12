#include <gtest/gtest.h>
#include <igasync/inline_execution_context.h>

using namespace igasync;

TEST(InlineExecutionContext, immediatelyExecutesTask) {
  bool executed = false;
  auto task = Task::Of([&executed]() { executed = true; });

  EXPECT_FALSE(executed);

  InlineExecutionContext context;
  context.schedule(std::move(task));

  EXPECT_TRUE(executed);
}

TEST(InlineExecutionContext, satisfiesExecutionContext) {
  std::shared_ptr<ExecutionContext> context =
      std::make_shared<InlineExecutionContext>();

  bool executed = false;
  context->schedule(Task::Of([&executed]() { executed = true; }));

  EXPECT_TRUE(executed);
}
