#include <igasync/promise_combiner.h>
using namespace igasync;

PromiseCombiner::Result& PromiseCombiner::Result::operator=(
    PromiseCombiner::Result&& o) {
  combiner_ = std::exchange(o.combiner_, nullptr);
  return *this;
}

PromiseCombiner::Result::~Result() {
  if (combiner_ != nullptr) {
    combiner_->entries_.clear();
  }
  combiner_ = nullptr;
}

PromiseCombiner::PromiseCombiner()
    : next_key_(1u),
      final_promise_(Promise<Result>::Create()),
      is_finished_(false),
      result_(nullptr) {}

std::shared_ptr<PromiseCombiner> PromiseCombiner::Create() {
  return std::shared_ptr<PromiseCombiner>(new PromiseCombiner());
}

void PromiseCombiner::resolve_promise(uint16_t key) {
  // I'm not actually sure why this prevents concurrency bugs, but for some
  //  reason including it got rid of a crash in a test scene that happened
  //  consistently within 1000 frames.
  // Dear future Sessamekesh: If you're hunting down another concurrency bug
  //  and you make it here... go to bed already, you're not finding the issue
  //  tonight, and tomorrow might be a good day for day drinking.
  std::lock_guard l(m_entries_);

  if (key != 0u) {
    for (size_t i = 0; i < entries_.size(); i++) {
      if (entries_[i].Key == key) {
        entries_[i].IsResolved = true;
        break;
      }
    }
  }

  if (!is_finished_) {
    return;
  }

  for (size_t i = 0; i < entries_.size(); i++) {
    if (!entries_[i].IsResolved) {
      return;
    }
  }

  final_promise_->resolve(std::move(result_));
}

void PromiseCombiner::add(std::shared_ptr<Promise<void>> promise,
                          std::shared_ptr<ExecutionContext> execution_context) {
  std::lock_guard l(m_entries_);
  if (is_finished_) {
    // TODO (sessamekesh): Invoke callback for 'cannot add promises after finish
    // already registered'
    return;
  }

  PromiseKey<void, false> key(next_key_++);
  entries_.push_back({key.key_, promise, false, false});

  promise->on_resolve(
      [key, l = weak_from_this()]() {
        auto t = l.lock();
        if (t == nullptr) return;

        t->resolve_promise(key.key_);
      },
      execution_context);
}
