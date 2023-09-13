#ifndef IGASYNC_SAMPLES_READ_FILE_FILE_PROMISE_H
#define IGASYNC_SAMPLES_READ_FILE_FILE_PROMISE_H

#include <igasync/promise.h>

#include <variant>

namespace igasync::sample {

enum class FileReadError {
  FileNotFound,
  FileNotRead,
};

class FilePromise {
 public:
  using result_t = std::variant<std::string, FileReadError>;

  static std::shared_ptr<Promise<result_t>> Create(
      const std::string& file_name);
};

}  // namespace igasync::sample

#endif
