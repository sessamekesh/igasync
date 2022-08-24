#include <igasync/promise_combiner.h>

using namespace igasync;

PromiseCombiner::Options PromiseCombiner::default_options(
    std::shared_ptr<TaskList> task_list) {
  Options options;
  options.DefaultTaskList = task_list;
  return options;
}

std::shared_ptr<PromiseCombiner> PromiseCombiner::Create(Options options) {
  return std::shared_ptr<PromiseCombiner>(new PromiseCombiner(options));
}

PromiseCombiner::PromiseCombiner(Options options)
    : options_(options),
      next_key_(1u),
      final_promise_(Promise<Result>::Create()),
      is_finished_(false),
      result_(nullptr) {}

std::shared_ptr<Promise<EmptyPromiseRsl>> PromiseCombiner::combine(
    std::function<void(PromiseCombiner::Result rsl)> cb,
    std::shared_ptr<TaskList> task_list_override) {
  {
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
        [cb](Result rsl) {
          cb(std::move(rsl));
          return EmptyPromiseRsl{};
        },
        task_list);
  }
}

void PromiseCombiner::resolve_promise(uint16_t key) {
  std::lock_guard l(m_entries_);

  if (key != 0u) {
    for (int i = 0; i < entries_.size(); i++) {
      if (entries_[i].Key == key) {
        entries_[i].IsResolved = true;
        break;
      }
    }
  }

  if (!is_finished_) {
    return;
  }

  for (int i = 0; i < entries_.size(); i++) {
    if (!entries_[i].IsResolved) {
      return;
    }
  }

  final_promise_->resolve(std::move(result_));
}
