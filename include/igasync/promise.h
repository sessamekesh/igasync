#ifndef IGASYNC_PROMISE_H
#define IGASYNC_PROMISE_H

#include <igasync/concepts.h>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <type_traits>

#include "task_list.h"

namespace igasync {

/**
 * Streamlined Promise implementation
 *
 * A Promise is basically an Optional / Maybe type that has two additional
 * properties and a bunch of built-in behavior that make it very useful for
 * writing asynchronous code with or without concurrency, with little blocking.
 *
 * (1) A promise is initialized with no value (empty state)
 * (2) A promise SHOULD always have a value set exactly once (this is the
 *     responsibility of the API user)
 * (3) Once set, the promise will have the same value until move semantics
 *     are optionally invoked, or the promise is destroyed.
 *
 * Promises do not expose their values directly, but rather can be provided a
 * callback that should be executed when the promise value becomes available.
 * If the promise value is already present, the callback will be scheduled
 * immediately.
 *
 * Promises do not handle actual task execution, and instead schedule callback
 * tasks against a task list that is provided on callback registration.
 *
 * ---------------------------------- Usage ----------------------------------
 *
 * Promises must always be wrapped in a shared_ptr, create a promise with the
 * static Create command. The value held by that promise is set with the
 * "resolve" method at any time, but only once.
 *
 * A shorthand for this is to use `Immediate` to create a new promise.
 *
 * ```
 * auto my_promise = Promise<int>::Create();
 * my_promise->resolve(42);
 *
 * auto immediate_promise = Promise<int>::Immediate(9001);
 * ```
 *
 * Register callbacks with the `on_resolve` method. A const auto reference to
 * the data held in the promise will be provided as the only parameter, and no
 * return value from the callback will be used. Pass in a task list that the
 * callback method should be executed against.
 *
 * ```
 * my_promise->on_resolve(
 *     [](const int& number) { std::cout << "Resolved with: " << number; },
 *     main_thread_task_list);
 * ```
 *
 * You can also chain promises by providing a transformation.
 *
 * ```
 * auto string_promise = my_promise->then<std::string>(
 *     [](const int& number) -> std::string { return std::to_string(number); },
 *     main_thread_task_list);
 * ```
 *
 * Promises can be chained with other promise-producing methods using then_chain
 *
 * ```
 * std::shared_ptr<Promise<int>> foo(int val) { ... }
 *
 * auto chained_promise = my_promise->then_chain<int>(
 *    foo, main_thread_task_list);
 * ```
 *
 * Promise methods are thread-safe
 *
 * ----------------------------------- Tips -----------------------------------
 * (1) If you're coming from a JavaScript Promise background, be aware that...
 *   (a) Error states are not expressed in igasync::Promise. Instead, use a held
 *       type that is a variant of both success and failure states.
 *   (b) Move semantics exist that might remove a held value - once invoked, the
 *       promise becomes invalid but the reference still exists.
 */

template <>
class Promise<void> : public std::enable_shared_from_this<Promise<void>> {
 private:
  struct ThenOp {
    std::function<void()> Fn;
    std::shared_ptr<TaskList> DestList;
  };

  Promise() : accepts_thens_(true), remaining_thens_(0u), is_finished_(false) {}

 public:
  /****************************************************************************
   *
   * Core API
   *
   ****************************************************************************/

  inline static std::shared_ptr<Promise<void>> Create() {
    return std::shared_ptr<Promise<void>>(new Promise<void>());
  }

  inline static std::shared_ptr<Promise<void>> Immediate() {
    auto p = Create();
    p->resolve();
    return p;
  }

  Promise(const Promise<void>&) = delete;
  Promise(Promise<void>&&) = delete;
  Promise<void>& operator=(const Promise<void>&) = delete;
  Promise<void>& operator=(Promise<void>&&) = delete;
  ~Promise<void>() = default;

  /**
   * Finalize this promise with a successful result - this will immediately
   * queue up all success callbacks
   */
  std::shared_ptr<Promise<void>> resolve() {
    {
      std::scoped_lock l(m_result_);

      if (is_finished_) {
        // TODO (sessamekesh): Error handling here, this promise is already
        // resolved
        return nullptr;
      }

      is_finished_ = true;
    }

    // Flush queue of pending operations
    {
      std::scoped_lock l(m_then_queue_);
      while (!then_queue_.empty()) {
        auto v = std::move(then_queue_.front());
        then_queue_.pop();
        auto fn = std::move(v.Fn);
        auto& task_list = v.DestList;

        task_list->add_task(
            Task::of([fn = std::move(fn), this,
                      lifetime = this->shared_from_this()]() { fn(); }));
      }
    }

    return this->shared_from_this();
  }

  /**
   * Invoke callback when this promise resolves
   */
  std::shared_ptr<Promise<void>> on_resolve(
      std::function<void()> fn, std::shared_ptr<TaskList> task_list) {
    std::lock_guard l(m_then_queue_);
    if (!accepts_thens_) {
      // TODO (sessamekesh): Error handling here, this promise is already closed
      return nullptr;
    }

    std::shared_lock l2(m_result_);
    if (!is_finished_) {
      task_list->add_task(Task::of([fn = std::move(fn), this,
                                    l = this->shared_from_this()]() { fn(); }));
      return this->shared_from_this();
    }

    // Promise is still pending in this case - add as a callback
    ThenOp op = {std::move(fn), std::move(task_list)};
    { remaining_thens_++; }
    then_queue_.emplace(std::move(op));

    return this->shared_from_this();
  }

  /****************************************************************************
   *
   * Chaining API
   *
   ****************************************************************************/
  template <typename FnT, typename OutT = std::invoke_result<FnT>::type>
  auto then(FnT&& cb, std::shared_ptr<TaskList> task_list)
      -> std::shared_ptr<Promise<OutT>> {
    auto tr = Promise<OutT>::Create();
    on_resolve([tr, cb = std::move(cb)]() { tr->resolve(cb()); }, task_list);
    return tr;
  }

  /****************************************************************************
   *
   * DANGEROUS API - DO NOT USE THE FOLLOWING METHODS UNLESS YOU KNOW WHAT YOU
   * ARE DOING
   *
   ****************************************************************************/
  bool unsafe_is_finished() {
    std::shared_lock l(m_result_);
    return is_finished_;
  }

 private:
  std::shared_mutex m_result_;

  std::mutex m_then_queue_;
  std::queue<ThenOp> then_queue_;

  std::atomic_bool accepts_thens_;
  std::atomic_int remaining_thens_;
  std::atomic_bool is_finished_;
};

template <typename ValT>
class Promise : public std::enable_shared_from_this<Promise<ValT>> {
 public:
  using value_type = ValT;

 private:
  struct ThenOp {
    std::function<void(const ValT&)> Fn;
    std::shared_ptr<TaskList> DestList;
  };

  struct MoveOp {
    std::function<void(const ValT)> Fn;
    std::shared_ptr<TaskList> DestList;
  };

  Promise() : accepts_thens_(true), remaining_thens_(0u), is_finished_(false) {}

 public:
  /****************************************************************************
   *
   * Core API
   *
   ****************************************************************************/

  inline static std::shared_ptr<Promise<ValT>> Create() {
    return std::shared_ptr<Promise<ValT>>(new Promise<ValT>());
  }

  inline static std::shared_ptr<Promise<ValT>> Immediate(ValT val) {
    auto p = Create();
    p->resolve(std::move(val));
    return p;
  }

  Promise(const Promise<ValT>&) = delete;
  Promise(Promise<ValT>&&) = delete;
  Promise<ValT>& operator=(const Promise<ValT>&) = delete;
  Promise<ValT>& operator=(Promise<ValT>&&) = delete;
  ~Promise<ValT>() = default;

  /**
   * Finalize this promise with a successful result - this will immediately
   * queue up all success callbacks
   */
  std::shared_ptr<Promise<ValT>> resolve(ValT val) {
    {
      std::scoped_lock l(m_result_);

      if (result_.has_value()) {
        // TODO (sessamekesh): Error handling here, this promise is already
        // resolved
        return nullptr;
      }

      result_ = std::move(val);
      is_finished_ = true;
    }

    // Flush queue of pending operations
    {
      std::scoped_lock l(m_then_queue_);
      while (!then_queue_.empty()) {
        auto v = std::move(then_queue_.front());
        then_queue_.pop();
        auto fn = std::move(v.Fn);
        auto& task_list = v.DestList;

        task_list->add_task(Task::of(
            [fn = std::move(fn), this, lifetime = this->shared_from_this()]() {
              this->resolve_inner(std::move(fn));
            }));
      }
    }

    maybe_consume_rsl();
    return this->shared_from_this();
  }

  /**
   * Invoke callback when this promise resolves
   */
  std::shared_ptr<Promise<ValT>> on_resolve(
      std::function<void(const ValT&)> fn,
      std::shared_ptr<TaskList> task_list) {
    std::lock_guard l(m_then_queue_);
    if (!accepts_thens_) {
      // TODO (sessamekesh): Error handling here, this promise is already closed
      return nullptr;
    }

    std::shared_lock l2(m_result_);
    if (result_.has_value()) {
      task_list->add_task(
          Task::of([fn = std::move(fn), this, l = this->shared_from_this()]() {
            fn(*result_);
          }));
      return this->shared_from_this();
    }

    // Promise is still pending in this case - add as a callback
    ThenOp op = {std::move(fn), std::move(task_list)};
    {
      std::lock_guard l3(m_consume_);
      remaining_thens_++;
    }
    then_queue_.emplace(std::move(op));

    return this->shared_from_this();
  }

  /**
   * Invoke a final callback when this promise resolves
   */
  void consume(std::function<void(ValT)> cb,
               std::shared_ptr<TaskList> task_list) {
    std::lock_guard l(m_then_queue_);
    if (!accepts_thens_) {
      // TODO (sessamekesh): Error handling here, this promise is already
      // consumed
      return;
    }

    accepts_thens_ = false;

    std::shared_lock l2(m_result_);
    if (result_.has_value()) {
      task_list->add_task(Task::of(
          [cb = std::move(cb), this, lifetime = this->shared_from_this()]() {
            cb(std::move(*result_));
          }));
      return;
    }

    // Promise is still pending - add as a callback
    consume_ = MoveOp{std::move(cb), std::move(task_list)};
  }

  /****************************************************************************
   *
   * Chaining API
   *
   ****************************************************************************/
  template <typename FnT,
            typename OutT = std::invoke_result<FnT, const ValT&>::type>
  auto then(FnT&& cb, std::shared_ptr<TaskList> task_list)
      -> std::shared_ptr<Promise<OutT>> {
    auto tr = Promise<OutT>::Create();
    on_resolve([tr, cb = std::move(cb)](const ValT& v) { tr->resolve(cb(v)); },
               task_list);
    return tr;
  }

  template <typename FnT, typename OutT = std::invoke_result<FnT, ValT>::type>
    requires(!IsVoidFn<FnT, ValT>)
  auto then_consuming(FnT&& cb, std::shared_ptr<TaskList> task_list)
      -> std::shared_ptr<Promise<OutT>> {
    auto tr = Promise<OutT>::Create();
    consume([tr, cb = std::move(cb)](ValT v) { tr->resolve(cb(std::move(v))); },
            task_list);
    return tr;
  }

  template <typename FnT>
    requires(IsVoidFn<FnT, ValT>)
  auto then_consuming(FnT&& cb, std::shared_ptr<TaskList> task_list)
      -> std::shared_ptr<Promise<void>> {
    auto tr = Promise<void>::Create();
    consume(
        [tr, cb = std::move(cb)](ValT v) {
          cb(std::move(v));
          tr->resolve();
        },
        task_list);
    return tr;
  }

  template <typename FnT, typename OutT = std::invoke_result<
                              FnT, const ValT&>::type::element_type::value_type>
  auto then_chain(FnT&& cb, std::shared_ptr<TaskList> task_list) {
    auto tr = Promise<OutT>::Create();
    on_resolve(
        [tr, cb = std::move(cb), task_list](const ValT& val) {
          cb(val)->on_resolve(
              [tr, task_list](const OutT& vv) { tr->resolve(vv); }, task_list);
        },
        task_list);
    return tr;
  }

  template <typename FnT, typename OutT = std::invoke_result<
                              FnT, ValT>::type::element_type::value_type>
  auto then_chain_consuming(FnT&& cb, std::shared_ptr<TaskList> task_list)
      -> std::shared_ptr<Promise<OutT>> {
    auto tr = Promise<OutT>::Create();
    consume(
        [tr, cb = std::move(cb), task_list](ValT val) {
          cb(std::move(val))
              ->consume(
                  [tr, task_list](OutT vv) { tr->resolve(std::move(vv)); },
                  task_list);
        },
        task_list);
    return tr;
  }

  /****************************************************************************
   *
   * DANGEROUS API - DO NOT USE THE FOLLOWING METHODS UNLESS YOU KNOW WHAT YOU
   * ARE DOING
   *
   ****************************************************************************/
  bool unsafe_is_finished() {
    std::shared_lock l(m_result_);
    return is_finished_;
  }

  const ValT& unsafe_sync_get() {
    std::shared_lock l(m_result_);
    return *result_;
  }

  ValT unsafe_sync_move() {
    std::shared_lock l(m_result_);
    return *(std::move(result_));
  }

 private:
  void resolve_inner(std::function<void(const ValT&)> fn) {
    fn(*result_);

    {
      std::lock_guard l(m_consume_);
      remaining_thens_--;
      maybe_consume_rsl();
    }
  }

  void maybe_consume_rsl() {
    std::lock_guard l(m_then_queue_);

    if (remaining_thens_ == 0 && consume_.has_value()) {
      MoveOp op = std::move(*consume_);
      op.DestList->add_task(Task::of(
          [fn = std::move(op.Fn), this, lifetime = this->shared_from_this()]() {
            fn(std::move(*std::move(result_)));
          }));
      remaining_thens_ = -1;
    }
  }

 private:
  std::shared_mutex m_result_;
  std::optional<ValT> result_;

  std::mutex m_then_queue_;
  std::queue<ThenOp> then_queue_;

  std::mutex m_consume_;
  std::optional<MoveOp> consume_;

  std::atomic_bool accepts_thens_;
  std::atomic_int remaining_thens_;
  std::atomic_bool is_finished_;
};

}  // namespace igasync

#endif
