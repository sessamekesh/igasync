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

template <typename F, typename RslT>
  requires(CanApplyFunctor<F>)
auto Promise<void>::then(F&& f,
                         std::shared_ptr<ExecutionContext> execution_context)
    -> std::shared_ptr<Promise<RslT>> {
  auto tr = Promise<RslT>::Create();

  on_resolve(
      [tr, f = std::move(f)]() {
        if constexpr (std::is_void_v<RslT>) {
          f();
          tr->resolve();
        } else {
          tr->resolve(f());
        }
      },
      execution_context);
  return tr;
}

template <typename F, typename RslT>
  requires(HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F>)
auto Promise<void>::then_chain(
    F&& f, std::shared_ptr<ExecutionContext> outer_execution_context,
    std::shared_ptr<ExecutionContext> inner_execution_context_override)
    -> std::shared_ptr<Promise<RslT>> {
  if (inner_execution_context_override == nullptr) {
    inner_execution_context_override = outer_execution_context;
  }

  auto tr = Promise<RslT>::Create();
  on_resolve(
      [tr, f = std::move(f), inner_execution_context_override]() {
        if constexpr (std::is_void_v<RslT>) {
          f()->on_resolve([tr]() { tr->resolve(); },
                          inner_execution_context_override);
        } else {
          f()->consume([tr](auto v) { tr->resolve(std::move(v)); },
                       inner_execution_context_override);
        }
      },
      outer_execution_context);
  return tr;
}

}  // namespace igasync