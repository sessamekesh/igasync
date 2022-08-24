#ifndef IGASYNC_PROMISE_COMBINER_H
#define IGASYNC_PROMISE_COMBINER_H

#include <iostream>
#include <memory>

#include "promise.h"

namespace igasync {

/**
 * PromiseCombiner - fills the use case of the JavaScript Promise.all method
 *
 * A trivially copyable "PromiseKey" object is returned for each promise added.
 * The result of that promise can be obtained by querying with the returned key.
 *
 * Once all promises have been added, the combiner can be finished with a method
 * that is scheduled when all added promises have been finished. A single
 * CombinerResult parameter is passed to this callback, which can be queried for
 * values of the underlying promises.
 *
 * Example usage:
 *
 * ```
 * auto combiner = PromiseCombiner::Create(options);
 * auto foo_key = combiner->add(foo_promise);
 * auto bar_key = combiner->add_consuming(bar_promise);
 *
 * combiner->finish([foo_key, bar_key](const auto& result) {
 *   const auto& foo = result.get(foo_key);
 *   auto bar = result.move(bar_key);
 *
 *   someMethod(foo, bar);
 * });
 * ```
 */
class PromiseCombiner : public std::enable_shared_from_this<PromiseCombiner> {
 public:
  /**
   * Options for using this promise combiner - default state is reasonable,
   * though task lists should be provided for the best usage experience.
   */
  struct Options {
    /** True if errors should be logged to the stderr stream */
    bool LogOnError = true;

    /** Default task list for marking promise resolution and bookkeeping ops */
    std::shared_ptr<TaskList> DefaultTaskList = nullptr;
  };
  static Options default_options(std::shared_ptr<TaskList> task_list);

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
    const T& get(const PromiseKey<T, is_consuming>& key) const {
      // Concurrency guards not needed - this method should not be called until
      // all promises have resolved, and no more promises are being
      // added/removed
      for (int i = 0; i < combiner_->entries_.size(); i++) {
        if (combiner_->entries_[i].Key == key.key_) {
          std::shared_ptr<Promise<T>> pp = std::static_pointer_cast<Promise<T>>(
              combiner_->entries_[i].PromiseRaw);
          if (!pp) {
            // Failed pointer cast for some reason
            if (combiner_->options_.LogOnError) {
              std::cerr
                  << "[PromiseCombiner::Result::get] Pointer cast failed on "
                     "get for some reason"
                  << std::endl;
            }
            break;
          }

          return pp->unsafe_sync_get();
        }
      }

      if (combiner_->options_.LogOnError) {
        std::cerr
            << "[PromiseCombiner::Result::get] Promise with given key not "
               "found - something is probably horribly wrong in the igasync "
               "library"
            << std::endl;
      }

      // This line keeps the compiler happy, but it's nonsense. The application
      // is in a very bad state if this line is ever reached.
      assert(false && "PromiseCombiner::Result::get failed spectacularly");
      return *((T*)nullptr);
    }

    template <typename T, bool is_consuming,
              typename = std::enable_if<is_consuming>::type>
    T move(const PromiseKey<T, is_consuming>& key) const {
      static_assert(
          is_consuming,
          "Cannot use non-movable key with PromiseCombiner::Result::move");

      // Concurrency guards not needed - this method should not be called until
      // all promises have resolved, and no more promises are being
      // added/removed
      for (int i = 0; i < combiner_->entries_.size(); i++) {
        if (combiner_->entries_[i].Key == key.key_) {
          std::shared_ptr<Promise<T>> pp = std::static_pointer_cast<Promise<T>>(
              combiner_->entries_[i].PromiseRaw);
          if (!pp) {
            // Failed pointer cast for some reason
            if (combiner_->options_.LogOnError) {
              std::cerr << "[PromiseCombiner::Result] Pointer cast failed on "
                           "move for some reason"
                        << std::endl;
            }
            assert(false);
            break;
          }

          if (!combiner_->entries_[i].IsOwning) {
            if (combiner_->options_.LogOnError) {
              std::cerr << "[PromiseCombiner::Result] Attempted to move a "
                           "non-owned value"
                        << std::endl;
            }
            assert(false);
            break;
          }

          return pp->unsafe_sync_move();
        }
      }

      if (combiner_->options_.LogOnError) {
        std::cerr
            << "[PromiseCombiner::Result::move] Promise with given key not "
               "found - something is probably horribly wrong in the igasync "
               "library"
            << std::endl;
      }

      // This line keeps the compiler happy, but it's nonsense. The application
      // is in a very bad state if this line is ever reached.
      assert(false && "PromiseCombiner::Result::move failed spectacularly");
      return (T &&) * ((T*)nullptr);
    }

   public:
    // Prevent all copy - contents of a result should only be processed
    // within the handling body itself because of the nasty lifetime bugs that
    // stand to be made with all the self-reference and whatnot.
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // Strictly required, but avoid using outside of igasync library code!
    // Required here to allow std::optional assignment in Promise object.
    Result(Result&& o) : combiner_(std::exchange(o.combiner_, nullptr)) {}
    Result& operator=(Result&& o) {
      combiner_ = std::exchange(o.combiner_, nullptr);
      return *this;
    }

    ~Result() {
      if (combiner_ != nullptr) {
        combiner_->entries_.clear();
      }
      combiner_ = nullptr;
    }

   private:
    Result(std::shared_ptr<PromiseCombiner> combiner) : combiner_(combiner) {}

   private:
    std::shared_ptr<PromiseCombiner> combiner_;
  };

 public:
  static std::shared_ptr<PromiseCombiner> Create(Options options);

  template <typename T>
  PromiseKey<T, false> add(
      std::shared_ptr<Promise<T>> promise,
      std::shared_ptr<TaskList> task_list_override = nullptr) {
    std::shared_ptr<TaskList> task_list = (task_list_override != nullptr)
                                              ? task_list_override
                                              : options_.DefaultTaskList;

    if (task_list == nullptr) {
      if (options_.LogOnError) {
        std::cerr << "[PromiseCombiner::add] Missing task list implementation "
                     "in both default options and override param - cannot add "
                     "promise to queue!"
                  << std::endl;
      }

      assert(false);
      return PromiseKey<T, false>(0);
    }

    std::lock_guard l(m_entries_);
    if (is_finished_) {
      if (options_.LogOnError) {
        std::cerr << "[PromiseCombiner::add] Finish already registered - "
                     "cannot add more promises"
                  << std::endl;
      }

      assert(false);
      return PromiseKey<T, false>(0);
    }

    PromiseKey<T, false> key(next_key_++);
    entries_.push_back({key.key_, promise, false, false});

    promise->on_resolve(
        [key, l = weak_from_this()](const auto&) {
          auto t = l.lock();
          assert(t != nullptr);

          t->resolve_promise(key.key_);
        },
        task_list);

    return key;
  }

  template <typename T>
  PromiseKey<T, true> add_consuming(
      std::shared_ptr<Promise<T>> promise,
      std::shared_ptr<TaskList> task_list_override = nullptr) {
    std::shared_ptr<TaskList> task_list = (task_list_override != nullptr)
                                              ? task_list_override
                                              : options_.DefaultTaskList;

    if (task_list == nullptr) {
      if (options_.LogOnError) {
        std::cerr << "[PromiseCombiner::add_consuming] Missing task list "
                     "implementation "
                     "in both default options and override param - cannot add "
                     "promise to queue!"
                  << std::endl;
      }

      assert(false);
      return PromiseKey<T, true>(0);
    }

    std::lock_guard l(m_entries_);
    if (is_finished_) {
      if (options_.LogOnError) {
        std::cerr << "[PromiseCombiner::add] Finish already registered - "
                     "cannot add more promises"
                  << std::endl;
      }

      assert(false);
      return PromiseKey<T, true>(0);
    }

    PromiseKey<T, true> key(next_key_++);
    entries_.push_back({key.key_, promise, false, true});

    // Pass through a second promise so that this one can do a "consume"
    auto p2 = Promise<T>::Create();
    promise->consume([p2, task_list](T val) { p2->resolve(std::move(val)); },
                     task_list);

    p2->on_resolve(
        [key, l = weak_from_this()](const auto&) {
          auto t = l.lock();
          assert(t != nullptr);

          t->resolve_promise(key.key_);
        },
        task_list);

    return key;
  }

  std::shared_ptr<Promise<EmptyPromiseRsl>> combine(
      std::function<void(Result rsl)> cb,
      std::shared_ptr<TaskList> task_list_override = nullptr);

  template <typename RslT>
  std::shared_ptr<Promise<RslT>> combine(
      std::function<RslT(Result rsl)> cb,
      std::shared_ptr<TaskList> task_list_override = nullptr) {
    std::shared_ptr<TaskList> task_list = (task_list_override != nullptr)
                                              ? task_list_override
                                              : options_.DefaultTaskList;

    if (task_list == nullptr) {
      if (options_.LogOnError) {
        std::cerr
            << "[PromiseCombiner::combine] Missing task list implementation "
               "in both default options and override param - cannot add "
               "promise to queue!"
            << std::endl;
      }

      assert(false);
      return nullptr;
    }

    {
      std::lock_guard l(m_entries_);
      if (is_finished_) {
        if (options_.LogOnError) {
          std::cerr << "[PromiseCombiner::combine] Combiner is already "
                       "combined, cannot combine again"
                    << std::endl;
        }

        assert(false);
        return nullptr;
      }

      // CAREFUL - this creates a self-reference!!!
      result_ = Result(shared_from_this());
      is_finished_ = true;
    }

    resolve_promise(0u);

    return final_promise_->then_consuming(
        [cb](Result rsl) { return cb(std::move(rsl)); }, task_list);
  }

  template <typename RslT>
  std::shared_ptr<Promise<RslT>> combine_chaining(
      std::function<std::shared_ptr<Promise<RslT>>(Result rsl)> cb,
      std::shared_ptr<TaskList> task_list_override = nullptr) {
    std::shared_ptr<TaskList> task_list = (task_list_override != nullptr)
                                              ? task_list_override
                                              : options_.DefaultTaskList;

    if (task_list == nullptr) {
      if (options_.LogOnError) {
        std::cerr << "[PromiseCombiner::combine_chaining] Missing task list "
                     "implementation in both default options and override "
                     "param - cannot add promise to queue!"
                  << std::endl;
      }

      assert(false);
      return nullptr;
    }

    {
      std::lock_guard l(m_entries_);
      if (is_finished_) {
        if (options_.LogOnError) {
          std::cerr
              << "[PromiseCombiner::combine_chaining] Combiner is already "
                 "combined, cannot combine again"
              << std::endl;
        }

        assert(false);
        return nullptr;
      }

      // CAREFUL - this creates a self-reference!!!
      result_ = Result(shared_from_this());
      is_finished_ = true;
    }

    resolve_promise(0u);

    return final_promise_->then_chain_consuming<RslT>(
        std::function<std::shared_ptr<Promise<RslT>>(Result)>(
            [cb](Result rsl) -> std::shared_ptr<Promise<RslT>> {
              return cb(std::move(rsl));
            }),
        task_list);
  }

 private:
  struct PromiseEntry {
    uint16_t Key;
    std::shared_ptr<void> PromiseRaw;
    bool IsResolved;
    bool IsOwning;
  };

  void resolve_promise(uint16_t key);

 private:
  PromiseCombiner(Options options);

  std::atomic_uint16_t next_key_;
  const Options options_;

  std::mutex m_entries_;
  std::vector<PromiseEntry> entries_;
  bool is_finished_;
  Result result_;
  std::shared_ptr<Promise<Result>> final_promise_;
};

}  // namespace igasync

#endif
