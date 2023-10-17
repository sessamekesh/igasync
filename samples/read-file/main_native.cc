#include <igasync/promise_combiner.h>
#include <igasync/thread_pool.h>

#include <iostream>

#include "file_promise.h"
#include "sha256/picosha2.h"

namespace {
auto read_file_or_default(std::string file_name, std::string default_value,
                          std::shared_ptr<igasync::TaskList> task_list) {
  return igasync::sample::FilePromise::Create(file_name)->then(
      [file_name, default_value = std::move(default_value)](const auto& rsl) {
        if (std::holds_alternative<std::string>(rsl)) {
          return std::get<std::string>(rsl);
        }

        std::cout << "Failed to read file '" << file_name
                  << "' - replacing with default value" << std::endl;
        return default_value;
      },
      task_list);
}

std::string hash(const std::string& s) {
  return picosha2::hash256_hex_string(s);
}
}  // namespace

int main() {
  auto thread_pool = igasync::ThreadPool::Create();
  auto async_task_list = igasync::TaskList::Create();
  auto main_thread_list = igasync::TaskList::Create();
  thread_pool->add_task_list(async_task_list);

  auto dataFilePromise =
      ::read_file_or_default("data_file.txt", "EMPTY TEXT", main_thread_list);
  auto dataFileHashPromise = dataFilePromise->then(::hash, main_thread_list);

  auto missingFilePromise = ::read_file_or_default(
      "missing_file.txt", "Missing File Text", main_thread_list);
  auto missingFileHashPromise =
      missingFilePromise->then(::hash, main_thread_list);

  auto dataFilePromiseCombiner = igasync::PromiseCombiner::Create();
  auto data_file_key =
      dataFilePromiseCombiner->add(dataFilePromise, main_thread_list);
  auto data_hash_key =
      dataFilePromiseCombiner->add(dataFileHashPromise, main_thread_list);
  auto data_file_done = dataFilePromiseCombiner->combine(
      [data_file_key, data_hash_key](igasync::PromiseCombiner::Result rsl) {
        const std::string& data_file_contents = rsl.get(data_file_key);
        const std::string& data_hash_contents = rsl.get(data_hash_key);

        std::cout << "---- data_file.txt ----\n"
                  << data_file_contents << "\n\n"
                  << "SHA256: " << data_hash_contents << std::endl;
      },
      main_thread_list);

  auto missingFilePromiseCombiner = igasync::PromiseCombiner::Create();
  auto missing_file_key =
      missingFilePromiseCombiner->add(missingFilePromise, main_thread_list);
  auto missing_hash_key =
      missingFilePromiseCombiner->add(missingFileHashPromise, main_thread_list);
  auto missing_file_done = missingFilePromiseCombiner->combine(
      [missing_file_key,
       missing_hash_key](igasync::PromiseCombiner::Result rsl) {
        const std::string& data_file_contents = rsl.get(missing_file_key);
        const std::string& data_hash_contents = rsl.get(missing_hash_key);

        std::cout << "---- missing_file.txt ----\n"
                  << data_file_contents << "\n\n"
                  << "SHA256: " << data_hash_contents << std::endl;
      },
      main_thread_list);

  // For ten seconds, sleep for a bit and hope for finishing
  for (int i = 0; i < 200; i++) {
    if (missing_file_done->is_finished() && data_file_done->is_finished()) {
      break;
    }
    while (main_thread_list->execute_next())
      ;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "FINISHED" << std::endl;

  return 0;
}
