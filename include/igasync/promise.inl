#include <igasync/promise.h>

#include <mutex>

namespace igasync {

template <class ValT>
std::shared_ptr<Promise<ValT>> Promise<ValT>::Create() {
  return std::shared_ptr<Promise<ValT>>(new Promise<ValT>());
}

template <class ValT>
std::shared_ptr<Promise<ValT>> Promise<ValT>::Immediate(ValT val) {
  auto p = Create();
  p->resolve(std::move(val));
  return p;
}

template <class ValT>
std::shared_ptr<Promise<ValT>> Promise<ValT>::resolve(ValT val) {
  std::scoped_lock l(m_result_);
  if (result_.has_value()) {
    // TODO (sessamekesh): Handle this error case (global callback on
    // double-resolve registered against igasync singleton?)
    return nullptr;
  }

  result_ = std::move(val);
  is_finished_ = true;

  // Flush queue of pending operations
  while (!then_queue_.empty()) {
    ThenOp v = std::move(then_queue_.front());
    then_queue_.pop();

    v.Scheduler->schedule(
        Task::Of([fn = std::move(v.Fn), this, l = this->shared_from_this()]() {
          fn(*result_);
          std::scoped_lock l(this->m_result_);
          remaining_thens_--;
          maybe_consume();
        }));
  }

  maybe_consume();

  return this->shared_from_this();
}

template <class ValT>
template <class F>
  requires(NonVoidPromiseThenCb<ValT, F>)
std::shared_ptr<Promise<ValT>> Promise<ValT>::on_resolve(
    F&& f, std::shared_ptr<ExecutionContext> execution_context) {
  std::scoped_lock l(m_result_);
  if (!accept_thens_) {
    // TODO (sessamekesh): Invoke a global callback here
    return nullptr;
  }

  if (result_.has_value()) {
    execution_context->schedule(
        Task::Of([fn = std::move(f), this,
                  lifetime = this->shared_from_this()]() { fn(*result_); }));
    return this->shared_from_this();
  }

  // Promsie is still pending - add as a callback
  remaining_thens_++;
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
  std::scoped_lock l(m_result_);
  if (!accept_thens_) {
    // TODO (sessamekesh): Error handling here, this promise is already consume
    return nullptr;
  }

  accept_thens_ = false;

  if (remaining_thens_ == 0 && result_.has_value()) {
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

template <class ValT>
const ValT& Promise<ValT>::unsafe_sync_peek() {
  return *result_;
}

template <class ValT>
ValT Promise<ValT>::unsafe_sync_move() {
  return *(std::move(result_));
}

template <class ValT>
template <typename F, typename RslT>
  requires(CanApplyFunctor<F, const ValT&>)
auto Promise<ValT>::then(F&& f,
                         std::shared_ptr<ExecutionContext> execution_context)
    -> std::shared_ptr<Promise<RslT>> {
  auto tr = Promise<RslT>::Create();

  on_resolve(
      [tr, f = std::move(f)](const ValT& v) {
        if constexpr (std::is_void_v<RslT>) {
          f(v);
          tr->resolve();
        } else {
          tr->resolve(f(v));
        }
      },
      execution_context);
  return tr;
}

template <class ValT>
template <typename F, typename RslT>
  requires(CanApplyFunctor<F, ValT>)
auto Promise<ValT>::then_consuming(
    F&& f, std::shared_ptr<ExecutionContext> execution_context)
    -> std::shared_ptr<Promise<RslT>> {
  auto tr = Promise<RslT>::Create();

  consume(
      [tr, f = std::move(f)](ValT v) {
        if constexpr (std::is_void_v<RslT>) {
          f(std::move(v));
          tr->resolve();
        } else {
          tr->resolve(f(std::move(v)));
        }
      },
      execution_context);
  return tr;
}

template <class ValT>
template <typename F, typename RslT>
  requires(
      HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F, const ValT&>)
auto Promise<ValT>::then_chain(
    F&& f, std::shared_ptr<ExecutionContext> outer_execution_context,
    std::shared_ptr<ExecutionContext> inner_execution_context_override)
    -> std::shared_ptr<Promise<RslT>> {
  if (inner_execution_context_override == nullptr) {
    inner_execution_context_override = outer_execution_context;
  }

  auto tr = Promise<RslT>::Create();
  on_resolve(
      [tr, f = std::move(f),
       inner_execution_context_override](const ValT& val) {
        if constexpr (std::is_void_v<RslT>) {
          f(val)->on_resolve([tr]() { tr->resolve(); },
                             inner_execution_context_override);
        } else {
          f(val)->consume([tr](auto v) { tr->resolve(std::move(v)); },
                          inner_execution_context_override);
        }
      },
      outer_execution_context);
  return tr;
}

template <class ValT>
template <typename F, typename RslT>
  requires(HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F, ValT>)
auto Promise<ValT>::then_chain_consuming(
    F&& f, std::shared_ptr<ExecutionContext> outer_execution_context,
    std::shared_ptr<ExecutionContext> inner_execution_context_override)
    -> std::shared_ptr<Promise<RslT>> {
  if (inner_execution_context_override == nullptr) {
    inner_execution_context_override = outer_execution_context;
  }

  auto tr = Promise<RslT>::Create();
  consume(
      [tr, f = std::move(f), inner_execution_context_override](ValT val) {
        if constexpr (std::is_void_v<RslT>) {
          f(std::move(val))
              ->on_resolve([tr]() { tr->resolve(); },
                           inner_execution_context_override);
        } else {
          f(std::move(val))
              ->consume([tr](auto v) { tr->resolve(std::move(v)); },
                        inner_execution_context_override);
        }
      },
      outer_execution_context);
  return tr;
}

template <class ValT>
void Promise<ValT>::maybe_consume() {
  if (remaining_thens_ == 0 && consume_.has_value()) {
    consume_->Scheduler->schedule(Task::Of(
        [fn = std::move(consume_->Fn), this, l = this->shared_from_this()]() {
          fn(std::move(*result_));
        }));
  }
}

}  // namespace igasync
