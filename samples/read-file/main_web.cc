#include <igasync/promise_combiner.h>
#include <igasync/thread_pool.h>

#include <iostream>

#include "file_promise.h"
#include "sha256/picosha2.h"

namespace {
auto read_file_or_default(std::string file_name, std::string default_value) {
  return igasync::sample::FilePromise::Create(file_name)->then(
      [file_name, default_value = std::move(default_value)](const auto& rsl) {
        if (std::holds_alternative<std::string>(rsl)) {
          return std::get<std::string>(rsl);
        }

        std::cout << "Failed to read file '" << file_name
                  << "' - replacing with default value" << std::endl;
        return default_value;
      });
}

std::string hash(const std::string& s) {
  return picosha2::hash256_hex_string(s);
}
}  // namespace

int main() {
  auto dataFilePromise = ::read_file_or_default("data_file.txt", "EMPTY TEXT");
  auto dataFileHashPromise = dataFilePromise->then(::hash);

  auto missingFilePromise =
      ::read_file_or_default("missing_file.txt", "Missing File Text");
  auto missingFileHashPromise = missingFilePromise->then(::hash);

  auto dataFilePromiseCombiner = igasync::PromiseCombiner::Create();
  auto data_file_key = dataFilePromiseCombiner->add(dataFilePromise);
  auto data_hash_key = dataFilePromiseCombiner->add(dataFileHashPromise);
  auto data_file_done = dataFilePromiseCombiner->combine(
      [data_file_key, data_hash_key](igasync::PromiseCombiner::Result rsl) {
        const std::string& data_file_contents = rsl.get(data_file_key);
        const std::string& data_hash_contents = rsl.get(data_hash_key);

        std::cout << "---- data_file.txt ----\n"
                  << data_file_contents << "\n\n"
                  << "SHA256: " << data_hash_contents << std::endl;
      });

  auto missingFilePromiseCombiner = igasync::PromiseCombiner::Create();
  auto missing_file_key = missingFilePromiseCombiner->add(missingFilePromise);
  auto missing_hash_key =
      missingFilePromiseCombiner->add(missingFileHashPromise);
  auto missing_file_done = missingFilePromiseCombiner->combine(
      [missing_file_key,
       missing_hash_key](igasync::PromiseCombiner::Result rsl) {
        const std::string& data_file_contents = rsl.get(missing_file_key);
        const std::string& data_hash_contents = rsl.get(missing_hash_key);

        std::cout << "---- missing_file.txt ----\n"
                  << data_file_contents << "\n\n"
                  << "SHA256: " << data_hash_contents << std::endl;
      });

  std::cout << "FINISHED (with sync portion)" << std::endl;

  return 0;
}
