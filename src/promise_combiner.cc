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
