#ifndef IGASYNC_PROMISE_COMBINER_H
#define IGASYNC_PROMISE_COMBINER_H

#include <igasync/concepts.h>
#include <igasync/promise.h>

#include <memory>

namespace igasync {

class PromiseCombiner : public std::enable_shared_from_this<PromiseCombiner> {
 public:
  template <typename T, bool is_consuming>
  class PromiseKey {
   public:
    friend class PromiseCombiner;

    ~PromiseKey() = default;
    PromiseKey(const PromiseKey&) = default;
    PromiseKey(PromiseKey&&) = default;
    PromiseKey& operator=(const PromiseKey&) = default;
    PromiseKey& operator=(PromiseKey&&) = default;

    PromiseKey() = delete;

    bool is_valid() const { return key_ > 0; }
    operator bool() const { return is_valid(); }

    uint16_t key() const { return key_; }

   private:
    // Private constructor w/ PromiseCombiner access to prevent API users from
    // creating an invalid PromiseKey - they must only be created within the
    // context of a PromiseCombiner
    explicit PromiseKey(uint16_t key) : key_(key) {}
    uint16_t key_;
  };

  class Result {
   public:
    friend class PromiseCombiner;

    template <typename T, bool is_consuming>
      requires(!IsVoid<T>)
    const T& get(const PromiseKey<T, is_consuming>& key) const;

    template <typename T, bool is_consuming>
      requires(is_consuming && !IsVoid<T>)
    T move(const PromiseKey<T, is_consuming>& key) const;

   public:
    // Prevent all copy - contents of a result should only be processed
    // within the handling body itself because of the nasty lifetime bugs that
    // stand to be made with all the self-reference and whatnot.
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // Strictly required, but avoid using outside of igasync library code!
    // Required here to allow std::optional assignment in Promise object
    Result(Result&& o) : combiner_(std::exchange(o.combiner_, nullptr)) {}
    Result& operator=(Result&& o);
    ~Result();

   private:
    Result(std::shared_ptr<PromiseCombiner> combiner) : combiner_(combiner) {}

   private:
    std::shared_ptr<PromiseCombiner> combiner_;
  };

 public:
  static std::shared_ptr<PromiseCombiner> Create();

  template <typename T>
  PromiseKey<T, false> add(std::shared_ptr<Promise<T>> promise,
                           std::shared_ptr<ExecutionContext> execution_context =
                               gDefaultExecutionContext);

  template <typename T>
    requires(!IsVoid<T>)
  PromiseKey<T, true> add_consuming(
      std::shared_ptr<Promise<T>> promise,
      std::shared_ptr<ExecutionContext> execution_context =
          gDefaultExecutionContext);

  /**
   * @brief Call once all promises have been added to schedule a callback once
   *        they're all finished executing.
   * @tparam F Functor template type that takes a single Result parameter and
   * returns
   * @tparam RslT Result type of the functor
   * @param execution_context
   * @param f
   * @param ...args
   * @return A promise that resolves to the value of the supplied callback once
   */
  template <typename F,
            typename RslT = std::invoke_result_t<F, PromiseCombiner::Result>>
    requires(CanApplyFunctor<F, PromiseCombiner::Result>)
  std::shared_ptr<Promise<RslT>> combine(
      F&& f, std::shared_ptr<ExecutionContext> execution_context =
                 gDefaultExecutionContext);

  /**
   * @brief Chaining overload of PromiseCombiner::combine method.
   * @tparam F
   * @tparam RslT
   * @param f
   * @param outer_execution_context
   * @param inner_execution_context_override
   * @return
   */
  template <typename F,
            typename RslT = std::invoke_result_t<F>::element_type::value_type>
    requires(HasAppropriateFunctor<std::shared_ptr<Promise<RslT>>, F>)
  std::shared_ptr<Promise<RslT>> combine_chaining(
      F&& f,
      std::shared_ptr<ExecutionContext> outer_execution_context =
          gDefaultExecutionContext,
      std::shared_ptr<ExecutionContext> inner_execution_context_override =
          nullptr);

 private:
  struct PromiseEntry {
    uint16_t Key;
    std::shared_ptr<void> PromiseRaw;
    bool IsResolved;
    bool IsOwning;
  };

 private:
  void resolve_promise(uint16_t key);
  PromiseCombiner();

 private:
  std::atomic_int16_t next_key_;

  std::mutex m_entries_;
  std::vector<PromiseEntry> entries_;

  bool is_finished_;
  Result result_;
  std::shared_ptr<Promise<Result>> final_promise_;
};

}  // namespace igasync

#include <igasync/promise_combiner.inl>

#endif
