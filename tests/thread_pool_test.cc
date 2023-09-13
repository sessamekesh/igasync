#include <gtest/gtest.h>
#include <igasync/thread_pool.h>

#ifdef __EMSCRIPTEN__
#ifndef __EMSCRIPTEN_PTHREADS__
#error "Cannot build tests without pthreads support!"
#endif

#include <emscripten.h>
#endif

using namespace igasync;

namespace {
std::shared_ptr<ThreadPool> CreateTestThreadPool() {
  ThreadPool::Desc desc;
  desc.UseHardwareConcurrency = false;
  desc.AdditionalThreads = 1;

  return ThreadPool::Create(desc);
}
}  // namespace

TEST(ThreadPool, consumesTasks) {
  auto thread_pool = ::CreateTestThreadPool();
  auto task_list = TaskList::Create();

  thread_pool->add_task_list(task_list);

  bool is_executed = false;
  task_list->schedule(Task::Of([&is_executed] { is_executed = true; }));

  for (int i = 0; i < 100; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (is_executed) {
      return;
    }
  }

  FAIL();
}

TEST(ThreadPool, consumesTasksFromMultipleTaskLists) {
  auto thread_pool = ::CreateTestThreadPool();
  auto task_list = TaskList::Create();
  auto other_task_list = TaskList::Create();

  thread_pool->add_task_list(task_list);
  thread_pool->add_task_list(other_task_list);

  bool is_executed = false;
  task_list->schedule(Task::Of([&is_executed] { is_executed = true; }));

  bool is_executed_other = false;
  other_task_list->schedule(
      Task::Of([&is_executed_other] { is_executed_other = true; }));

  for (int i = 0; i < 100; i++) {
#ifdef __EMSCRIPTEN__
    emscripten_sleep(10);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
#endif
    if (is_executed && is_executed_other) {
      return;
    }
  }

  FAIL();
}

TEST(ThreadPool, canRemoveTaskList) {
  auto thread_pool = ::CreateTestThreadPool();
  auto task_list = TaskList::Create();
  auto other_task_list = TaskList::Create();

  thread_pool->add_task_list(task_list);
  thread_pool->add_task_list(other_task_list);

  bool is_executed = false;
  task_list->schedule(Task::Of([&is_executed] { is_executed = true; }));

  bool is_executed_other = false;
  other_task_list->schedule(
      Task::Of([&is_executed_other] { is_executed_other = true; }));

  bool is_first_passed = false;
  for (int i = 0; i < 100; i++) {
    is_first_passed = is_executed && is_executed_other;
    if (is_first_passed) break;
#ifdef __EMSCRIPTEN__
    emscripten_sleep(10);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
#endif
  }

  ASSERT_TRUE(is_first_passed);

  thread_pool->remove_task_list(other_task_list);

  bool never_do_this = false;
  other_task_list->schedule(
      Task::Of([&never_do_this] { never_do_this = true; }));

#ifdef __EMSCRIPTEN__
  emscripten_sleep(10);
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
#endif

  EXPECT_FALSE(never_do_this);
}

TEST(ThreadPool, picksUpExistingTasks) {
  auto thread_pool = ::CreateTestThreadPool();
  auto task_list = TaskList::Create();

  bool is_executed = false;
  task_list->schedule(Task::Of([&is_executed] { is_executed = true; }));

  EXPECT_FALSE(is_executed);

  thread_pool->add_task_list(task_list);

  for (int i = 0; i < 100; i++) {
#ifdef __EMSCRIPTEN__
    emscripten_sleep(10);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
#endif
    if (is_executed) {
      return;
    }
  }

  FAIL();
}
