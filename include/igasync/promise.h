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
  requires(!std::is_void_v<ValT>)
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

  Promise() : is_finished_(false), accept_thens_(true) {}

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

 private:
  std::shared_mutex m_result_;
  std::optional<ValT> result_;

  std::mutex m_then_queue_;
  std::queue<ThenOp> then_queue_;

  std::mutex m_consume_;
  std::optional<ConsumeOp> consume_;

  std::atomic_bool is_finished_;
  std::atomic_bool accept_thens_;
};

}  // namespace igasync

#include <igasync/promise.inl>

#endif