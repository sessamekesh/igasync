#include <igasync/inline_execution_context.h>

using namespace igasync;

void InlineExecutionContext::schedule(std::unique_ptr<Task> task) {
  task->run();
}
