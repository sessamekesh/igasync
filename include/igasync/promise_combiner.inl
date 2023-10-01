#include <igasync/promise_combiner.h>

namespace igasync {

template <typename T, bool is_consuming>
  requires(!IsVoid<T>)
const T& PromiseCombiner::Result::get(
    const PromiseCombiner::PromiseKey<T, is_consuming>& key) const {
  // Concurrency guards are not needed - this method should not be called until
  // all promises have resolved, and no more promises are being added/removed.
  for (size_t i = 0; i < combiner_->entries_.size(); i++) {
    if (combiner_->entries_[i].Key == key.key_) {
      std::shared_ptr<Promise<T>> pp = std::static_pointer_cast<Promise<T>>(
          combiner_->entries_[i].PromiseRaw);

      if (!pp) {
        // TODO (sessamekesh): Invoke very bad error callback here

        // Failed pointer cast for some reason
        break;
      }

      return pp->unsafe_sync_peek();
    }
  }

  // TODO (sessamekesh): Invoke error callback here for no result present
  // (indicates a bug in the igasync library)

  // This line keeps the compiler happy, but it's nonsense. The application
  // is in a very bad state if this line is ever reached.
  return *((T*)nullptr);
}

template <typename T, bool is_consuming>
  requires(is_consuming && !IsVoid<T>)
T PromiseCombiner::Result::move(
    const PromiseCombiner::PromiseKey<T, is_consuming>& key) const {
  // Concurrency guards are not needed - this method should not be called until
  // all promises have resolved, and no more promises are being added/removed.
  for (size_t i = 0; i < combiner_->entries_.size(); i++) {
    if (combiner_->entries_[i].Key == key.key_) {
      std::shared_ptr<Promise<T>> pp = std::static_pointer_cast<Promise<T>>(
          combiner_->entries_[i].PromiseRaw);

      if (!pp) {
        // TODO (sessamekesh): Invoke very bad error callback here

        // Failed pointer cast for some reason
        break;
      }

      if (!combiner_->entries_[i].IsOwning) {
        // TODO (sessamekesh): Invoke very bad error callback here (non-owning
        // result called)
        break;
      }

      return pp->unsafe_sync_move();
    }
  }

  // This line keeps the compiler happy, but it's nonsense. The application
  // is in a very bad state if this line is ever reached.
  return (T&&)*((T*)nullptr);
}

template <typename T>
  requires(!IsVoid<T>)
[[nodiscard]] PromiseCombiner::PromiseKey<T, false> PromiseCombiner::add(
    std::shared_ptr<Promise<T>> promise,
    std::shared_ptr<ExecutionContext> execution_context) {
  std::lock_guard l(m_entries_);
  if (is_finished_) {
    // TODO (sessamekesh): Invoke callback for 'cannot add promises after finish
    // already registered'
    return PromiseCombiner::PromiseKey<T, false>(0);
  }

  PromiseKey<T, false> key(next_key_++);
  entries_.push_back({key.key_, promise, false, false});

  promise->on_resolve(
      [key, l = weak_from_this()](const auto&) {
        auto t = l.lock();
        if (t == nullptr) return;

        t->resolve_promise(key.key_);
      },
      execution_context);

  return key;
}

template <typename T>
  requires(!IsVoid<T>)
PromiseCombiner::PromiseKey<T, true> [[nodiscard]] PromiseCombiner::
    add_consuming(std::shared_ptr<Promise<T>> promise,
                  std::shared_ptr<ExecutionContext> execution_context) {
  std::lock_guard l(m_entries_);
  if (is_finished_) {
    // TODO (sessamekesh): Invoke callback for 'finish already registered'
    return PromiseCombiner::PromiseKey<T, true>(0);
  }

  // Pass through a second promise so that this one can do a "consume"
  auto p2 = Promise<T>::Create();
  PromiseKey<T, true> key(next_key_++);
  entries_.push_back({key.key_, p2, false, true});

  promise->consume(
      [p2, execution_context](T val) { p2->resolve(std::move(val)); },
      execution_context);

  p2->on_resolve([key, l = weak_from_this()](const auto&) {
    auto t = l.lock();
    if (!t) return;

    t->resolve_promise(key.key_);
  });

  return key;
}

template <typename F, typename RslT>
  requires(CanApplyFunctor<F, PromiseCombiner::Result>)
std::shared_ptr<Promise<RslT>> PromiseCombiner::combine(
    F&& f, std::shared_ptr<ExecutionContext> execution_context) {
  {
    std::lock_guard l(m_entries_);
    if (is_finished_) {
      // TODO (sessamekesh): Invoke callback for "combiner is already scheduled"
      return nullptr;
    }

    // CAREFUL - this creates a self-reference!
    result_ = Result(shared_from_this());
    is_finished_ = true;
  }

  // Run the promise resolution check again, but with an invalid ID (just in
  // case all promises are already resolved)
  resolve_promise(0u);

  return final_promise_->then_consuming(
      [f = std::move(f)](Result rsl) { return f(std::move(rsl)); },
      execution_context);
}

template <typename F, typename RslT>
  requires(HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F>)
std::shared_ptr<Promise<RslT>> PromiseCombiner::combine_chaining(
    F&& f, std::shared_ptr<ExecutionContext> outer_execution_context,
    std::shared_ptr<ExecutionContext> inner_execution_context_override) {
  {
    std::lock_guard l(m_entries_);
    if (is_finished_) {
      // TODO (sessamekesh): Invoke callback for "combiner is already scheduled"
      return nullptr;
    }

    // CAREFUL - this creates a self-reference!
    result_ = Result(shared_from_this());
    is_finished_ = true;
  }

  // Run the promise resolution check again, but with an invalid ID (just in
  // case all promises are already resolved)
  resolve_promise(0u);

  return final_promise_->then_chain_consuming(
      [f = std::move(f)](Result rsl) { return f(std::move(rsl)); },
      outer_execution_context, inner_execution_context_override);
}

}  // namespace igasync
