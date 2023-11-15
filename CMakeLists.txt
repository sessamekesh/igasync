# CMakeList.txt : CMake project for igasync, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)

include(FetchContent)

project (
	"igasync"
	VERSION 0.1
	DESCRIPTION "Asynchronous programming utilities for writing WASM-friendly C++ games"
	LANGUAGES CXX
)

#
# Configuration
#
set(IGASYNC_BUILD_TESTS "ON" CACHE BOOL "Build unit tests")
set(IGASYNC_BUILD_EXAMPLES "ON" CACHE BOOL "Build examples")
set(IGASYNC_ENABLE_WASM_THREADS "ON" CACHE BOOL "Include threading support in WASM builds")

#
# Testing support
#
if (IGASYNC_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY "https://github.com/google/googletest"
    GIT_TAG "6b74da4757a549563d7c37c8fae3e704662a043b"
  )

  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)
endif ()

#
# Third-party dependencies
#
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY "https://github.com/cameron314/concurrentqueue"
  GIT_TAG "65d6970912fc3f6bb62d80edf95ca30e0df85137"
)
FetchContent_MakeAvailable(concurrentqueue)

#
# Main igasync library
#
set(igasync_headers
  "include/igasync/concepts.h"
  "include/igasync/execution_context.h"
  "include/igasync/promise.h"
  "include/igasync/promise.inl"
  "include/igasync/promise_combiner.h"
  "include/igasync/promise_combiner.inl"
  "include/igasync/task.h"
  "include/igasync/task.inl"
  "include/igasync/task_list.h"
  "include/igasync/thread_pool.h"
  "include/igasync/void_promise.inl"
)
set(igasync_sources
  "src/promise_combiner.cc"
  "src/task.cc"
  "src/task_list.cc"
  "src/thread_pool.cc"
  "src/void_promise.cc"
)

add_library(igasync STATIC ${igasync_headers} ${igasync_sources})
target_include_directories(igasync PUBLIC include)
target_link_libraries(igasync PUBLIC concurrentqueue)
set_property(TARGET igasync PROPERTY CXX_STANDARD 20)

#
# Tests
#
if (IGASYNC_BUILD_TESTS)
  set(igasync_test_sources
    "tests/concepts_test.cc"
	"tests/promise_combiner_test.cc"
	"tests/promise_test.cc"
    "tests/task_test.cc"
	"tests/task_list_test.cc"
	"tests/thread_pool_test.cc"
	"tests/void_promise_test.cc"
  )

  add_executable(igasync_test ${igasync_test_sources})
  target_link_libraries(igasync_test gtest gtest_main igasync)
  target_include_directories(igasync_test PRIVATE tests/include)
  set_property(TARGET igasync_test PROPERTY CXX_STANDARD 20)

  if (NOT EMSCRIPTEN)
    gtest_discover_tests(igasync_test)
  endif ()

  if (EMSCRIPTEN AND IGASYNC_ENABLE_WASM_THREADS)
    target_compile_options(igasync PUBLIC "-pthread")
	target_compile_options(igasync_test PUBLIC "-pthread")
	target_compile_options(gtest PUBLIC "-pthread")

	target_link_options(igasync_test PUBLIC "SHELL:-s USE_PTHREADS=1 -pthread -sPTHREAD_POOL_SIZE=\"Math.max(((navigator&&navigator.hardwareConcurrency)||4),6))\"")
    set_target_properties(igasync_test PROPERTIES SUFFIX ".html")
  endif ()
endif ()

#
# Examples
#
if (IGASYNC_BUILD_EXAMPLES)
  add_subdirectory(samples)
endif ()
