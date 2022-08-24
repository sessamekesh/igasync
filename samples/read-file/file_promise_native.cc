#include <fstream>
#include <thread>

#include "file_promise.h"

using namespace igasync;
using namespace sample;

std::shared_ptr<Promise<std::variant<std::string, FileReadError>>>
FilePromise::Create(const std::string& file_name) {
  auto rsl = Promise<std::variant<std::string, FileReadError>>::Create();

  std::thread t([rsl, file_name]() {
    std::ifstream fin(file_name, std::ios::binary | std::ios::ate);

    if (!fin) {
      rsl->resolve(FileReadError::FileNotFound);
      return;
    }

    auto size = fin.tellg();
    fin.seekg(0, std::ios::beg);

    std::string data(size, '\0');
    if (!fin.read(&data[0], size)) {
      rsl->resolve(FileReadError::FileNotRead);
      return;
    }

    rsl->resolve(std::move(data));
  });
  t.detach();

  return rsl;
}
