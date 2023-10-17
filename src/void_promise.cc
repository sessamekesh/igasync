#include <igasync/promise.h>

namespace igasync {

std::shared_ptr<Promise<void>> Promise<void>::Create() {
  return std::shared_ptr<Promise<void>>(new Promise<void>());
}

std::shared_ptr<Promise<void>> Promise<void>::Immediate() {
  auto p = Create();
  p->resolve();
  return p;
}

std::shared_ptr<Promise<void>> Promise<void>::resolve() {
  std::scoped_lock l(m_then_queue_);

  if (is_finished_) {
    // TODO (sessamekesh): Handle this error case (global callback)
    return nullptr;
  }

  is_finished_ = true;

  {
    while (!then_queue_.empty()) {
      ThenOp v = std::move(then_queue_.front());
      then_queue_.pop();

      // Optimization: do not need to hold on to Promise implementation, since
      // the invoked method does not require any access to the data itself!
      v.Scheduler->schedule(Task::Of(std::move(v.Fn)));
    }
  }

  return this->shared_from_this();
}

bool Promise<void>::is_finished() { return is_finished_; }

}  // namespace igasync
