#ifndef IGASYNC_PROMISE_H
#define IGASYNC_PROMISE_H

#include <igasync/concepts.h>
#include <igasync/execution_context.h>
#include <igasync/inline_execution_context.h>

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>

namespace igasync {

/**
 * @brief Default execution context for promise resolution (an
 *        InlineExecutionContext instance)
 */
inline std::shared_ptr<ExecutionContext> gDefaultExecutionContext =
    std::make_shared<InlineExecutionContext>();

/**
 * @brief Promise implementation for igasync library
 * @tparam ValT Type of value the promise will contain
 *
 * A promise is a container for a value that may or may not be present,
 * but will eventually always be present.
 *
 * Promises are created either with the intention of providing a value in the
 * future with Promise<ValT>::Create. The value can then later be provided
 * with Promise<ValT>::resolve(ValT).
 *
 * @code{.cc}
 * auto some_promise = Promise<int>::Create();
 * // Do the logic required to populate the promise...
 * some_promise->resolve(42);
 * @endcode
 *
 * Promises can also be created with an immediately present value using the
 * Promise<ValT>::Immediate(ValT) constructor.
 *
 * @code{.cc}
 * auto some_promise = Promise<int>::Immediate(42);
 * @endcode
 */
template <class ValT>
class Promise : public std::enable_shared_from_this<Promise<ValT>> {
 public:
  using value_type = ValT;

 private:
  struct ThenOp {
    std::function<void(const ValT&)> Fn;
    std::shared_ptr<ExecutionContext> Scheduler;
  };

  struct ConsumeOp {
    std::function<void(ValT&&)> Fn;
    std::shared_ptr<ExecutionContext> Scheduler;
  };

  Promise() : is_finished_(false), accept_thens_(true), remaining_thens_(0) {}

 public:
  Promise(const Promise<ValT>&) = delete;
  Promise(Promise<ValT>&&) = delete;
  Promise<ValT>& operator=(const Promise<ValT>&) = delete;
  Promise<ValT>& operator=(Promise<ValT>&&) = delete;
  ~Promise<ValT>() = default;

  /**
   * @brief Create a new, unresolved promise
   * @return Non-null promise pointer
   */
  static std::shared_ptr<Promise<ValT>> Create();

  /**
   * @brief Create a new promise that's resolved with the provided value
   * @param val Value of the resolved promise
   * @return Non-null promise pointer
   */
  static std::shared_ptr<Promise<ValT>> Immediate(ValT val);

  /**
   * @brief Finalize this promise with a successful result. This will
   *        immediately queue up all success callbacks.
   * @param val Value to assign to this promise
   * @return Shared pointer reference to this promise (for chaining)
   */
  std::shared_ptr<Promise<ValT>> resolve(ValT val);

  /**
   * @brief Schedule a callback to be invoked when this promise resolves
   * @tparam F Callback type - must take in a single const ValT& parameter
   * @param f Callback implementation
   * @param execution_context Scheduler for callback - defaults to an
   *                          InlineExecutionContext implementation
   * @return Shared pointer reference to this promise (for chaining)
   */
  template <typename F>
    requires(NonVoidPromiseThenCb<ValT, F>)
  std::shared_ptr<Promise<ValT>> on_resolve(
      F&& f, std::shared_ptr<ExecutionContext> execution_context =
                 gDefaultExecutionContext);

  /**
   * @brief Schedule a callback to consume the final value when this promise
            resolves. Consuming a value destroys the promise.
   * @tparam F Callback type - must take in a single ValT parameter of a
               move constructable type.
   * @param f Callback implementation
   * @param execution_context Scheduler for callback - defaults to an
   *                          InlineExecutionContext implementation
   * @return Shared pointer reference to this promise (for chaining)
   */
  template <typename F>
    requires(NonVoidPromiseConsumeCb<ValT, F>)
  std::shared_ptr<Promise<ValT>> consume(
      F&& f, std::shared_ptr<ExecutionContext> execution_context =
                 gDefaultExecutionContext);

  /**
   * @return True if this promise is finished, false otherwise
   */
  bool is_finished();

  /**
   * @brief UNSAFELY peek at the contained promise value
   *
   * This should ONLY be called if is_finished() returns true! This
   * should not be called under normal circumstances!!
   */
  const ValT& unsafe_sync_peek();

  /**
   * @brief UNSAFELY consume the contained promise value
   *
   * This should ONLY be called if is_finished() returns true, AND
   * this promise is known to have no consumers. This should not
   * be called under normal circumstances!!
   */
  ValT unsafe_sync_move();

  /**
   * @brief Create a new promise containing the result of a function invoked
   *        with the value of this promise, once resolved.
   * @tparam F Functor that consumes a const ValT& argument once this promise
   *           resolves
   * @tparam RslT
   * @param f
   * @param execution_context Scheduling mechanism to invoke the functor
   * against, defaults to gDefaultExecutionContext
   * @return A new promise
   */
  template <typename F,
            typename RslT = typename std::invoke_result_t<F, const ValT&>>
    requires(CanApplyFunctor<F, const ValT&>)
  auto then(F&& f, std::shared_ptr<ExecutionContext> execution_context =
                       gDefaultExecutionContext)
      -> std::shared_ptr<Promise<RslT>>;

  /**
   * @brief Create a new promise containing the result of a function invoked
   *        with the consumed value of this promise, once resolved.
   * @tparam F
   * @tparam RslT
   * @param f
   * @param execution_context
   * @return A new promise
   */
  template <typename F, typename RslT = typename std::invoke_result_t<F, ValT>>
    requires(CanApplyFunctor<F, ValT>)
  auto then_consuming(F&& f,
                      std::shared_ptr<ExecutionContext> execution_context =
                          gDefaultExecutionContext)
      -> std::shared_ptr<Promise<RslT>>;

  /**
   * @brief Create a new promise containing the result of a promise returned
   *        from the given function, which takes the inner value of this
   *        promise by const ref as a parameter
   * @tparam F
   * @tparam RslT
   * @param f
   * @param outer_execution_context Scheduling mechanism for resolving this
   *        promise before passing to the callback function
   * @param inner_execution_context_override Scheduling mechanism for
   *        resolving the promise returned by f
   * @return
   */
  template <typename F, typename RslT = typename std::invoke_result_t<
                            F, const ValT&>::element_type::value_type>
    requires(
        HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F, const ValT&>)
  auto then_chain(F&& f,
                  std::shared_ptr<ExecutionContext> outer_execution_context =
                      gDefaultExecutionContext,
                  std::shared_ptr<ExecutionContext>
                      inner_execution_context_override = nullptr)
      -> std::shared_ptr<Promise<RslT>>;

  /**
   * @brief Chain a promise-producing method with this promise, consuming the
   *        value of this promise in the process
   * @tparam F
   * @tparam RslT
   * @param f
   * @param outer_execution_context
   * @param inner_execution_context_override
   * @return
   */
  template <typename F, typename RslT = typename std::invoke_result_t<
                            F, ValT>::element_type::value_type>
    requires(HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F, ValT>)
  auto then_chain_consuming(
      F&& f,
      std::shared_ptr<ExecutionContext> outer_execution_context =
          gDefaultExecutionContext,
      std::shared_ptr<ExecutionContext> inner_execution_context_override =
          nullptr) -> std::shared_ptr<Promise<RslT>>;

 private:
  void maybe_consume();

 private:
  std::shared_mutex m_result_;
  std::optional<ValT> result_;

  std::mutex m_then_queue_;
  std::queue<ThenOp> then_queue_;

  std::mutex m_consume_;
  std::optional<ConsumeOp> consume_;

  std::atomic_bool is_finished_;
  std::atomic_bool accept_thens_;

  std::atomic_int remaining_thens_;
};

/**
 * @brief Template specialization for void promises.
 *
 * The biggest differences are:
 * 1. Resolve takes no parameters, nor do resolution callbacks
 * 2. There's not point to specifying consuming functions, since there's no data
 *    to consume
 */
template <>
class Promise<void> : public std::enable_shared_from_this<Promise<void>> {
 public:
  using value_type = void;

 private:
  struct ThenOp {
    std::function<void()> Fn;
    std::shared_ptr<ExecutionContext> Scheduler;
  };

  Promise() : is_finished_(false) {}

 public:
  Promise(const Promise<void>&) = delete;
  Promise(Promise<void>&&) = delete;
  Promise<void>& operator=(const Promise<void>&) = delete;
  Promise<void>& operator=(Promise<void>&&) = delete;
  ~Promise<void>() = default;

 public:
  /**
   * @brief Create a new, unresolved void promise
   * @return Non-null promise pointer
   */
  static std::shared_ptr<Promise<void>> Create();

  /**
   * @brief Create a new, already resolved void promise
   * @return Non-null promise pointer
   */
  static std::shared_ptr<Promise<void>> Immediate();

  /**
   * @brief Resolve this void promise, marking it as finished
   * @return A self-reference Promise pointer (good for chaining)
   */
  std::shared_ptr<Promise<void>> resolve();

  /**
   * @brief Schedule a callback to be invoked when this promise resolves
   * @tparam F Callback type - must provide no-parameter void function
   * @param f Callback implementation
   * @param execution_context Scheduler for callback - defaults to an
   *                          InlineExecutionContext implementation
   * @return Shared pointer reference to this promise (for chaining)
   */
  template <typename F>
    requires(VoidPromiseThenCb<F>)
  std::shared_ptr<Promise<void>> on_resolve(
      F&& f, std::shared_ptr<ExecutionContext> execution_context =
                 gDefaultExecutionContext);

  /**
   * @brief Schedule a callback to be invoked when this promise resolves, and
   *        return a promise with the result value of that callback
   * @tparam F
   * @tparam RslT
   * @param f
   * @param execution_context
   * @return
   */
  template <typename F, typename RslT = typename std::invoke_result_t<F>>
    requires(CanApplyFunctor<F>)
  auto then(F&& f, std::shared_ptr<ExecutionContext> execution_context =
                       gDefaultExecutionContext)
      -> std::shared_ptr<Promise<RslT>>;

  /**
   * @brief Create a new promise containing the result of a promise returned
   *        from the given function, once this promise resolves
   * @tparam F
   * @tparam RslT
   * @param f
   * @param outer_execution_context Scheduling mechanism for resolving this
   *        promise before passing to the callback function
   * @param inner_execution_context_override Scheduling mechanism for
   *        resolving the promise returned by f
   * @return
   */
  template <typename F, typename RslT = typename std::invoke_result_t<
                            F>::element_type::value_type>
    requires(HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F>)
  auto then_chain(F&& f,
                  std::shared_ptr<ExecutionContext> outer_execution_context =
                      gDefaultExecutionContext,
                  std::shared_ptr<ExecutionContext>
                      inner_execution_context_override = nullptr)
      -> std::shared_ptr<Promise<RslT>>;

  /**
   * @return True if this promise is finished, false otherwise
   */
  bool is_finished();

 private:
  std::mutex m_then_queue_;
  std::queue<ThenOp> then_queue_;

  std::atomic_bool is_finished_;
};

}  // namespace igasync

#include <igasync/promise.inl>
#include <igasync/void_promise.inl>

#endif
