#include <gtest/gtest.h>
#include <igasync/task_list.h>

#include <type_traits>

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

TEST(TaskList, runReturnsVoidPromise_noParams) {
  auto task_list = TaskList::Create();

  auto rsl = task_list->run([]() {});
  bool is_same = std::is_same_v<decltype(rsl), std::shared_ptr<Promise<void>>>;

  EXPECT_FALSE(rsl->is_finished());
  EXPECT_TRUE(is_same);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(rsl->is_finished());
}

TEST(TaskList, runReturnsVoidPromise_withParams) {
  auto task_list = TaskList::Create();

  auto rsl = task_list->run([](int a) {}, 2);
  bool is_same = std::is_same_v<decltype(rsl), std::shared_ptr<Promise<void>>>;

  EXPECT_FALSE(rsl->is_finished());
  EXPECT_TRUE(is_same);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(rsl->is_finished());
}

TEST(TaskList, runReturnsNonVoidPromise_noParams) {
  auto task_list = TaskList::Create();

  int val = 0;

  auto rsl = task_list->run([]() { return 42; });
  bool is_same = std::is_same_v<decltype(rsl), std::shared_ptr<Promise<int>>>;

  EXPECT_FALSE(rsl->is_finished());
  EXPECT_TRUE(is_same);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(rsl->is_finished());

  rsl->on_resolve([&val](const int& v) { val = v; }, task_list);
  ::flush_task_list(task_list.get());
  EXPECT_EQ(val, 42);
}

TEST(TaskList, runReturnsNonVoidPromise_withParams) {
  auto task_list = TaskList::Create();

  int val = 0;

  auto rsl = task_list->run([](int a) { return a; }, 50);
  bool is_same = std::is_same_v<decltype(rsl), std::shared_ptr<Promise<int>>>;

  EXPECT_FALSE(rsl->is_finished());
  EXPECT_TRUE(is_same);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(rsl->is_finished());

  rsl->on_resolve([&val](const int& v) { val = v; }, task_list);
  ::flush_task_list(task_list.get());
  EXPECT_EQ(val, 50);
}

TEST(TaskList, runReturnsNonCopyable_noParams) {
  auto task_list = TaskList::Create();

  int val = 0;

  auto rsl = task_list->run([] { return NonCopyable(42); });
  bool is_same =
      std::is_same_v<decltype(rsl), std::shared_ptr<Promise<NonCopyable>>>;

  EXPECT_FALSE(rsl->is_finished());
  EXPECT_TRUE(is_same);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(rsl->is_finished());

  rsl->on_resolve([&val](const auto& v) { val = v.val(); }, task_list);
  ::flush_task_list(task_list.get());
  EXPECT_EQ(val, 42);
}

TEST(TaskList, runReturnsNonCopyable_withParams) {
  auto task_list = TaskList::Create();

  int val = 0;

  auto rsl = task_list->run([](int a) { return NonCopyable(a); }, 50);
  bool is_same =
      std::is_same_v<decltype(rsl), std::shared_ptr<Promise<NonCopyable>>>;

  EXPECT_FALSE(rsl->is_finished());
  EXPECT_TRUE(is_same);

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_FALSE(task_list->execute_next());

  EXPECT_TRUE(rsl->is_finished());

  rsl->on_resolve([&val](const auto& v) { val = v.val(); }, task_list);
  ::flush_task_list(task_list.get());
  EXPECT_EQ(val, 50);
}

TEST(TaskList, correctlyProfilesTasks) {
  auto test_start = std::chrono::high_resolution_clock::now();
  auto task_list = TaskList::Create();
  TaskProfile task_profile;
  auto get_profile_cb = [&task_profile](TaskProfile profile) {
    task_profile = profile;
  };

  bool was_run = false;

  task_list->schedule(
      Task::WithProfile(get_profile_cb, [&was_run] { was_run = true; }));

  EXPECT_TRUE(task_list->execute_next());
  EXPECT_TRUE(task_profile.Created > test_start);
  EXPECT_TRUE(task_profile.Scheduled >= task_profile.Created);
  EXPECT_TRUE(task_profile.Started >= task_profile.Scheduled);
  EXPECT_TRUE(task_profile.Finished > task_profile.Started);
  EXPECT_EQ(task_profile.ExecutorThreadId, std::this_thread::get_id());
}
