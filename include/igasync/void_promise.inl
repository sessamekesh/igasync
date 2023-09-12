#include <igasync/promise.h>

namespace igasync {

template <typename F>
requires(VoidPromiseThenCb<F>)
    std::shared_ptr<Promise<void>> Promise<void>::on_resolve(
        F&& f, std::shared_ptr<ExecutionContext> execution_context) {
  if (is_finished_) {
    execution_context->schedule(Task::Of(f));
    return this->shared_from_this();
  }

  std::lock_guard l(m_then_queue_);
  then_queue_.emplace(ThenOp{std::move(f), std::move(execution_context)});
  return this->shared_from_this();
}

}  // namespace igasync