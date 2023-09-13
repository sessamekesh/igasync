#
# read-file
#
set(sample-read-file-common-srcs "file_promise.h" "sha256/picosha2.h")

if (EMSCRIPTEN)
  set(sample-read-file-platform-srcs "main_web.cc")
else ()
  set(sample-read-file-platform-srcs "file_promise_native.cc" "main_native.cc")
endif ()

add_executable(sample-read-file ${sample-read-file-common-srcs} ${sample-read-file-platform-srcs})
target_link_libraries(sample-read-file igasync)
target_include_directories(sample-read-file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET sample-read-file PROPERTY CXX_STANDARD 20)
configure_file("data_file.txt" "${CMAKE_CURRENT_BINARY_DIR}/data_file.txt" COPYONLY)
