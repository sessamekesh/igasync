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
  static std::shared_ptr<Promise<std::variant<std::string, FileReadError>>>
  Create(const std::string& file_name);
};

}  // namespace igasync::sample

#endif
