cmake_minimum_required(VERSION 3.20)

project(
  Helios
  VERSION 0.1.0
  DESCRIPTION "GPU cluster scheduling simulator for reinforcement learning research"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(helios_core src/job.cpp src/resources.cpp src/version.cpp)
target_include_directories(helios_core PUBLIC include)

add_executable(helios_sim apps/helios_sim/main.cpp)
target_link_libraries(helios_sim PRIVATE helios_core)

enable_testing()

add_executable(helios_smoke_test tests/cpp/smoke_test.cpp)
target_link_libraries(helios_smoke_test PRIVATE helios_core)
add_test(NAME helios_smoke_test COMMAND helios_smoke_test)

add_executable(helios_job_test tests/cpp/job_test.cpp)
target_link_libraries(helios_job_test PRIVATE helios_core)
add_test(NAME helios_job_test COMMAND helios_job_test)

add_executable(helios_resources_test tests/cpp/resources_test.cpp)
target_link_libraries(helios_resources_test PRIVATE helios_core)
add_test(NAME helios_resources_test COMMAND helios_resources_test)
