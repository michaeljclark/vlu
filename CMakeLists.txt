cmake_minimum_required(VERSION 3.0.2)

project(vlu)

include(CheckCXXCompilerFlag)

# We need haswell for TZCNT/LZCNT
check_cxx_compiler_flag("-march=haswell" has_march_haswell "int main() { return 0; }")
if (has_march_haswell)
	list(APPEND CMAKE_CXX_FLAGS -march=haswell)
endif()

# We need <chrono> and <random>
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vlu_test src/vlu_test.cc)
add_executable(vlu_demo src/vlu_demo.cc)
add_executable(vlu_bench src/vlu_bench.cc)