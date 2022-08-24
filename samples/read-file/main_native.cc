#include <iostream>

#include "file_promise.h"

using namespace igasync;
using namespace sample;

namespace {

void handle_file_result(std::string file_name,
                        const std::variant<std::string, FileReadError>& rsl) {
  if (std::holds_alternative<FileReadError>(rsl)) {
    std::string error_text = "<<unknown error>>";
    switch (std::get<FileReadError>(rsl)) {
      case FileReadError::FileNotFound:
        error_text = "File not found";
        break;
      case FileReadError::FileNotRead:
        error_text = "File not read";
        break;
    }

    std::cerr << "Could not load file " << file_name << ": " << error_text
              << std::endl;

    return;
  }

  std::string file_text = std::get<std::string>(rsl);
  std::cout << "----- BEGIN " << file_name << " ------" << std::endl
            << file_text << std::endl
            << "----- END " << file_name << " ------" << std::endl
            << std::endl;
}

void test_file(std::string file_name, std::shared_ptr<TaskList> task_list) {
  std::cout << "Attempting to read file " << file_name << "..." << std::endl;

  FilePromise::Create(file_name)->on_resolve(
      std::bind(&handle_file_result, file_name, std::placeholders::_1),
      task_list);
}

}  // namespace

int main() {
  auto task_list = TaskList::Create();

  test_file("data_file.txt", task_list);
  test_file("does_not_exist.txt", task_list);

  // For ten seconds, flush task list and sleep for a bit.
  for (int i = 0; i < 100; i++) {
    while (task_list->execute_next()) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "======== FINISHED ========" << std::endl;
}
