cmake_minimum_required(VERSION 3.20)

project(cpu-wavelet LANGUAGES CXX)

add_library(
  cpu_wavelet
  src/lifting/scheme_json.cpp
  src/lifting/plan.cpp
  src/lifting/executor.cpp
  src/lifting/cpu_transform.cpp)

target_compile_features(cpu_wavelet PUBLIC cxx_std_20)
target_include_directories(cpu_wavelet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(cpu_wavelet_cli apps/cpu_dwt.cpp)
target_link_libraries(cpu_wavelet_cli PRIVATE cpu_wavelet)

add_executable(cpu_wavelet_bench apps/cpu_benchmark.cpp)
target_link_libraries(cpu_wavelet_bench PRIVATE cpu_wavelet)

option(CPU_WAVELET_USE_OPENMP "Enable OpenMP in the CPU wavelet executor" OFF)

if(CPU_WAVELET_USE_OPENMP)
  find_package(OpenMP REQUIRED)
  target_link_libraries(cpu_wavelet PUBLIC OpenMP::OpenMP_CXX)
  target_compile_definitions(cpu_wavelet PUBLIC CPU_WAVELET_USE_OPENMP=1)
endif()
