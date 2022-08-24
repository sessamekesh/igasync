#ifndef IGASYNC_TESTS_INCLUDE_TEST_OBJECTS_H
#define IGASYNC_TESTS_INCLUDE_TEST_OBJECTS_H

namespace igasync {

class NonCopyableObject {
 public:
  NonCopyableObject(int v) : InnerValue(v) {}
  ~NonCopyableObject() = default;
  NonCopyableObject(const NonCopyableObject&) = delete;
  NonCopyableObject(NonCopyableObject&&) = default;
  NonCopyableObject& operator=(const NonCopyableObject&) = delete;
  NonCopyableObject& operator=(NonCopyableObject&& o) = default;

  int InnerValue;
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

}  // namespace igasync

#endif
