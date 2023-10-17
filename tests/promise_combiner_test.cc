#include <gtest/gtest.h>
#include <igasync/promise_combiner.h>
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

class DestructorTracker {
 public:
  DestructorTracker(int* increment_on_destroy) : p_(increment_on_destroy) {}

  DestructorTracker(DestructorTracker&& o) noexcept
      : p_(std::exchange(o.p_, nullptr)) {}
  DestructorTracker& operator=(DestructorTracker&& o) noexcept {
    p_ = std::exchange(o.p_, nullptr);
    return *this;
  }

  ~DestructorTracker() {
    if (p_ != nullptr) {
      (*p_)++;
    }
    p_ = nullptr;
  }

 private:
  int* p_;
};

template <class RslT, class KeyT>
concept CanMoveFromRsl = requires(RslT rsl, KeyT key) {
  { rsl.move(key) } -> Any;
};

void flush_task_list(std::shared_ptr<TaskList> tl) {
  while (tl->execute_next())
    ;
}
}  // namespace

TEST(PromiseCombiner, basicCombine) {
  auto tl = TaskList::Create();

  auto p1 = Promise<int>::Create();
  auto p2 = Promise<int>::Create();

  auto combiner = PromiseCombiner::Create();

  auto key_1 = combiner->add(p1, tl);
  auto key_2 = combiner->add(p2, tl);

  int r1 = -1, r2 = -1;
  bool has_resolved = false;

  auto p_finished = combiner->combine(
      [&r1, &r2, &has_resolved, key_1, key_2](PromiseCombiner::Result rsl) {
        has_resolved = true;
        r1 = rsl.get(key_1);
        r2 = rsl.get(key_2);

        constexpr bool canMoveKey1 =
            CanMoveFromRsl<decltype(rsl), decltype(key_1)>;
        EXPECT_FALSE(canMoveKey1);

        constexpr bool canMoveKey2 =
            CanMoveFromRsl<decltype(rsl), decltype(key_2)>;
        EXPECT_FALSE(canMoveKey2);
      },
      tl);
  ::flush_task_list(tl);

  constexpr bool isVoidPromise =
      std::same_as<decltype(p_finished), std::shared_ptr<Promise<void>>>;

  EXPECT_TRUE(isVoidPromise);

  EXPECT_FALSE(p_finished->is_finished());

  p1->resolve(1);
  ::flush_task_list(tl);

  EXPECT_FALSE(p_finished->is_finished());

  p2->resolve(2);
  ::flush_task_list(tl);

  EXPECT_TRUE(p_finished->is_finished());
  EXPECT_EQ(r1, 1);
  EXPECT_EQ(r2, 2);
  EXPECT_TRUE(has_resolved);
}

TEST(PromiseCombiner, allowsConsumingMembers) {
  auto tl = TaskList::Create();

  auto p1 = Promise<NonCopyable>::Create();
  auto p2 = Promise<NonCopyable>::Create();

  auto combiner = PromiseCombiner::Create();

  auto key_1 = combiner->add_consuming(p1, tl);
  auto key_2 = combiner->add(p2, tl);

  NonCopyable v1(-1);
  int v2 = -1;
  bool is_finished = false;

  auto p_finished = combiner->combine(
      [key_1, key_2, &v1, &v2, &is_finished](PromiseCombiner::Result rsl) {
        v1 = rsl.move(key_1);
        v2 = rsl.get(key_2).val();

        constexpr bool canMoveKey1 =
            CanMoveFromRsl<decltype(rsl), decltype(key_1)>;
        EXPECT_TRUE(canMoveKey1);

        constexpr bool canMoveKey2 =
            CanMoveFromRsl<decltype(rsl), decltype(key_2)>;
        EXPECT_FALSE(canMoveKey2);

        is_finished = true;
      },
      tl);
  ::flush_task_list(tl);

  constexpr bool isVoidPromise =
      std::same_as<decltype(p_finished), std::shared_ptr<Promise<void>>>;
  EXPECT_TRUE(isVoidPromise);

  p1->resolve(NonCopyable(1));
  p2->resolve(NonCopyable(2));
  ::flush_task_list(tl);
}

TEST(PromiseCombiner, DestructsAfterResolving) {
  auto tl = TaskList::Create();

  auto dtor_1 = 0, dtor_2 = 0;
  bool has_run = false;

  {
    auto p1 = Promise<DestructorTracker>::Create();
    auto p2 = Promise<DestructorTracker>::Create();

    auto combiner = PromiseCombiner::Create();

    auto key_1 = combiner->add(p1, tl);
    auto key_2 = combiner->add(p2, tl);

    combiner->combine([&has_run](auto rsl) { has_run = true; }, tl);

    p1->resolve(DestructorTracker(&dtor_1));
    p2->resolve(DestructorTracker(&dtor_2));
    ::flush_task_list(tl);

    EXPECT_TRUE(has_run);
    EXPECT_EQ(dtor_1, 0);
    EXPECT_EQ(dtor_2, 0);
  }

  EXPECT_EQ(dtor_1, 1);
  EXPECT_EQ(dtor_2, 1);
}

TEST(PromiseCombiner, CanUseVoidPromises) {
  auto tl = TaskList::Create();

  auto combiner = PromiseCombiner::Create();

  auto p1 = Promise<void>::Create();
  auto p2 = Promise<void>::Create();
  auto p3 = Promise<int>::Create();

  constexpr bool isVoidReturn =
      std::same_as<decltype(combiner->add(p1, tl)), void>;
  constexpr bool isVoidReturn2 =
      std::same_as<decltype(combiner->add(p2, tl)), void>;
  EXPECT_TRUE(isVoidReturn);
  EXPECT_TRUE(isVoidReturn2);

  combiner->add(p1, tl);
  combiner->add(p2, tl);
  auto int_key = combiner->add(p3, tl);

  bool has_run = false;

  combiner->combine([&has_run](auto rsl) { has_run = true; }, tl);
  ::flush_task_list(tl);

  EXPECT_FALSE(has_run);

  p3->resolve(56);
  ::flush_task_list(tl);

  EXPECT_FALSE(has_run);

  p1->resolve();
  ::flush_task_list(tl);

  EXPECT_FALSE(has_run);

  p2->resolve();
  ::flush_task_list(tl);

  EXPECT_TRUE(has_run);
}
