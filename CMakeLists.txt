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
  "include/igasync/task.h"
)
set(igasync_sources
  "src/task.cc"
)

add_library(igasync STATIC ${igasync_headers} ${igasync_sources})
target_include_directories(igasync PUBLIC include)
target_link_libraries(igasync PUBLIC concurrentqueue)
set_property(TARGET igasync PROPERTY CXX_STANDARD 17)

#
# Tests
#
if (IGASYNC_BUILD_TESTS)
  set(igasync_test_sources
    "tests/task_test.cc"
  )

  add_executable(igasync_test ${igasync_test_sources})
  target_link_libraries(igasync_test gtest gtest_main igasync)
  target_include_directories(igasync_test PRIVATE tests/include)
  set_property(TARGET igasync_test PROPERTY CXX_STANDARD 17)
  gtest_discover_tests(igasync_test)
endif ()

#
# Examples
#
if (IGASYNC_BUILD_EXAMPLES)
  # add_subdirectory(samples)
endif ()
