cmake_minimum_required(VERSION 3.14)
project(bkd LANGUAGES CXX)

# Header-only: bkd.hh is the whole implementation.
add_library(bkd INTERFACE)
target_include_directories(bkd INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(bkd INTERFACE cxx_std_20)
add_library(bkd::bkd ALIAS bkd)

# Tests build only when this repo is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(bkd_test test/test.cc)
  target_link_libraries(bkd_test PRIVATE bkd)
  add_test(NAME bkd COMMAND bkd_test)

  # libFuzzer target (Clang): cmake -DBKD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
  option(BKD_FUZZ "Build the libFuzzer target" OFF)
  if(BKD_FUZZ)
    add_executable(bkd_fuzz fuzz/fuzz_tree.cc)
    target_link_libraries(bkd_fuzz PRIVATE bkd)
    target_compile_options(bkd_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    target_link_options(bkd_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()
endif()
