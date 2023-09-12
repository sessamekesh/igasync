#include <igasync/promise.h>

namespace igasync {

template <class ValT>
std::shared_ptr<Promise<ValT>> Promise<ValT>::Create() {
  return std::shared_ptr<Promise<ValT>>(new Promise<ValT>());
}

template <class ValT>
std::shared_ptr<Promise<ValT>> Promise<ValT>::Immediate(ValT val) {
  auto p = Create();
  p->resolve(val);
  return p;
}

template <class ValT>
std::shared_ptr<Promise<ValT>> Promise<ValT>::resolve(ValT val) {
  {
    std::scoped_lock l(m_result_);

    if (result_.has_value()) {
      // TODO (sessamekesh): Handle this error case (global callback on
      // double-resolve registered against igasync singleton?)
      return nullptr;
    }

    result_ = std::move(val);
    is_finished_ = true;
  }

  // Flush queue of pending operations
  {
    std::scoped_lock l(m_then_queue_);
    std::scoped_lock l2(m_consume_);
    while (!then_queue_.empty()) {
      ThenOp v = std::move(then_queue_.front());
      then_queue_.pop();

      v.Scheduler->schedule(
          Task::Of([fn = std::move(v.Fn), this,
                    l = this->shared_from_this()]() { fn(*result_); }));
    }

    if (consume_.has_value()) {
      consume_->Scheduler->schedule(Task::Of(
          [fn = std::move(consume_->Fn), this, l = this->shared_from_this()]() {
            fn(std::move(*result_));
          }));
    }
  }

  return this->shared_from_this();
}

template <class ValT>
template <class F>
requires(NonVoidPromiseThenCb<ValT, F>)
    std::shared_ptr<Promise<ValT>> Promise<ValT>::on_resolve(
        F&& f, std::shared_ptr<ExecutionContext> execution_context) {
  std::lock_guard l(m_then_queue_);
  std::lock_guard lcons(m_consume_);
  if (!accept_thens_) {
    // TODO (sessamekesh): Invoke a global callback here
    return nullptr;
  }

  std::shared_lock l2(m_result_);
  if (result_.has_value()) {
    execution_context->schedule(
        Task::Of([fn = std::move(f), this, l = this->shared_from_this()]() {
          fn(*result_);
        }));
    return this->shared_from_this();
  }

  // Promsie is still pending - add as a callback
  then_queue_.emplace(ThenOp{std::move(f), std::move(execution_context)});
  return this->shared_from_this();
}

template <class ValT>
bool Promise<ValT>::is_finished() {
  std::shared_lock l(m_result_);
  return is_finished_;
}

template <class ValT>
template <typename F>
requires(NonVoidPromiseConsumeCb<ValT, F>)
    std::shared_ptr<Promise<ValT>> Promise<ValT>::consume(
        F&& f, std::shared_ptr<ExecutionContext> execution_context) {
  std::lock_guard l(m_then_queue_);
  if (!accept_thens_) {
    // TODO (sessamekesh): Error handling here, this promise is already consume
    return nullptr;
  }

  accept_thens_ = false;

  std::shared_lock l2(m_result_);
  if (result_.has_value()) {
    execution_context->schedule(Task::Of(
        [f = std::move(f), this, lifetime = this->shared_from_this()]() {
          f(std::move(*result_));
        }));
    return this->shared_from_this();
  }

  // Promise is still pending, add this as a callback
  consume_ = ConsumeOp{std::move(f), std::move(execution_context)};
  return this->shared_from_this();
}

}  // namespace igasync
