#include <gtest/gtest.h>
#include <igasync/concepts.h>

using namespace igasync;

namespace {
class NonCopyable {
 public:
  NonCopyable() = default;
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = default;
  NonCopyable& operator=(NonCopyable&&) = default;
  ~NonCopyable() = default;
};

template <typename ValT, typename F>
  requires(NonVoidPromiseThenCb<ValT, F>)
bool test_non_void_promise_cb(F&& f) {
  return true;
}

template <typename ValT, typename F>
  requires(!NonVoidPromiseThenCb<ValT, F>)
bool test_non_void_promise_cb(F&& f) {
  return false;
}

template <typename ValT, typename F>
  requires(HasSingleConstRefParam<ValT, F>)
bool test_has_single_param(F&& f) {
  return true;
}

template <typename ValT, typename F>
  requires(!HasSingleConstRefParam<ValT, F>)
bool test_has_single_param(F&& f) {
  return false;
}

template <typename ValT, typename F>
  requires(NonVoidPromiseConsumeCb<ValT, F>)
bool test_non_void_promise_consume_cb(F&& f) {
  return true;
}

template <typename ValT, typename F>
  requires(!NonVoidPromiseConsumeCb<ValT, F>)
bool test_non_void_promise_consume_cb(F&& f) {
  return false;
}

}  // namespace

TEST(HasSingleConstRefParam, isTrueForSingleParameterCallback) {
  std::function<void(const int&)> cb;
  EXPECT_TRUE(::test_has_single_param<int>(cb));
}

TEST(HasSingleConstRefParam, isFalseForTwoParameterCallback) {
  std::function<void(const int&, const int&)> cb;
  EXPECT_FALSE(::test_has_single_param<int>(cb));
}

TEST(HasSingleConstRefParam, isFalseForRValueCallback) {
  std::function<void(int&&)> cb;
  EXPECT_FALSE(::test_has_single_param<int>(cb));
}

TEST(HasSingleConstRefParam, isFalseForNonConstCallback) {
  std::function<void(NonCopyable)> cb;
  EXPECT_FALSE(::test_has_single_param<NonCopyable>(cb));
}

TEST(HasSingleConstRefParam, isNotBamboozledByConstRefInput) {
  std::function<void(const int&)> cb;
  EXPECT_TRUE(::test_has_single_param<const int&>(cb));
}

TEST(NonVoidPromiseThenCb, isTrueForStandardCallbacks) {
  std::function<void(const int&)> cb = [](const auto&) {};
  EXPECT_TRUE(::test_non_void_promise_cb<int>(cb));
}

TEST(NonVoidPromiseThenCb, isFalseWithTooManyParameters) {
  std::function<void(const int&, const bool&)> cb = [](const auto&,
                                                       const auto&) {};
  EXPECT_FALSE(::test_non_void_promise_cb<int>(cb));
}

TEST(NonVoidPromiseThenCb, isFalseForNoParamsInCallback) {
  std::function<void()> cb = []() {};
  EXPECT_FALSE(::test_non_void_promise_cb<int>(cb));
}

TEST(NonVoidPromiseThenCb, isFalseIfReturnsValue) {
  std::function<int(const int&)> cb = [](const auto&) { return 42; };
  EXPECT_FALSE(::test_non_void_promise_cb<int>(cb));
}

TEST(NonVoidPromiseThenCb, isFalseForMismatchedTypes) {
  std::function<int(const NonCopyable&)> cb = [](const auto&) { return 42; };
  EXPECT_FALSE(::test_non_void_promise_cb<int>(cb));
}

TEST(NonVoidPromiseThenCb, isTrueEvenWithNonCopyableType) {
  auto cb = [](const NonCopyable&) {};
  EXPECT_TRUE(::test_non_void_promise_cb<NonCopyable>(cb));
}

TEST(NonVoidPromiseConsumeCb, isTrueForExpectedCb) {
  auto cb = [](NonCopyable c) {};
  EXPECT_TRUE(::test_non_void_promise_consume_cb<NonCopyable>(cb));
}

TEST(NonVoidPromiseConsumeCb, isFalseForNonVoidCb) {
  auto cb = [](NonCopyable c) { return 5; };
  EXPECT_FALSE(::test_non_void_promise_consume_cb<NonCopyable>(cb));
}

TEST(NonVoidPromiseConsumeCb, isFalseWithTooManyParams) {
  auto cb = [](NonCopyable c, NonCopyable c2) {};
  EXPECT_FALSE(::test_non_void_promise_consume_cb<NonCopyable>(cb));
}

TEST(NonVoidPromiseConsumeCb, isFalseForMismatchedTypes) {
  auto cb = [](NonCopyable c) {};
  EXPECT_FALSE(::test_non_void_promise_consume_cb<int>(cb));
}
