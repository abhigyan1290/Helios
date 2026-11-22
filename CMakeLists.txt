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

add_library(
  helios_core
  src/benchmark_suite.cpp
  src/csv.cpp
  src/demo_workloads.cpp
  src/event.cpp
  src/job.cpp
  src/job_state.cpp
  src/metrics.cpp
  src/resources.cpp
  src/runtime_estimates.cpp
  src/schedule_decision.cpp
  src/schedulers/fifo_scheduler.cpp
  src/schedulers/sjf_scheduler.cpp
  src/schedulers/estimated_sjf_scheduler.cpp
  src/schedulers/random_scheduler.cpp
  src/schedulers/easy_backfill_scheduler.cpp
  src/simulation_runner.cpp
  src/simulator.cpp
  src/simulator_state.cpp
  src/version.cpp
  src/workload.cpp
  src/workload_generator.cpp
)
target_include_directories(helios_core PUBLIC include)

option(HELIOS_BUILD_PYTHON "Build the Python simulator bindings" OFF)
if(HELIOS_BUILD_PYTHON)
  find_package(Python 3.11 COMPONENTS Interpreter Development.Module REQUIRED)
  find_package(pybind11 CONFIG REQUIRED)
  set_target_properties(helios_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
  pybind11_add_module(_core python/bindings.cpp)
  target_link_libraries(_core PRIVATE helios_core)
  install(TARGETS _core LIBRARY DESTINATION helios RUNTIME DESTINATION helios)
endif()

add_executable(helios_sim apps/helios_sim/main.cpp)
target_link_libraries(helios_sim PRIVATE helios_core)

enable_testing()

add_executable(helios_observation_policies_test tests/cpp/observation_policies_test.cpp)
target_link_libraries(helios_observation_policies_test PRIVATE helios_core)
add_test(NAME helios_observation_policies_test COMMAND helios_observation_policies_test)

add_executable(helios_simulator_test tests/cpp/simulator_test.cpp)
target_link_libraries(helios_simulator_test PRIVATE helios_core)
add_test(NAME helios_simulator_test COMMAND helios_simulator_test)

add_executable(helios_runtime_estimates_test tests/cpp/runtime_estimates_test.cpp)
target_link_libraries(helios_runtime_estimates_test PRIVATE helios_core)
add_test(NAME helios_runtime_estimates_test COMMAND helios_runtime_estimates_test)

option(HELIOS_ENABLE_SCALE_TESTS "Enable slower tests with up to 12000 jobs per scheduler" OFF)
if(HELIOS_ENABLE_SCALE_TESTS)
  add_executable(helios_scale_test tests/cpp/scale_test.cpp)
  target_link_libraries(helios_scale_test PRIVATE helios_core)
  add_test(NAME helios_scale_test COMMAND helios_scale_test)
  set_tests_properties(helios_scale_test PROPERTIES LABELS scale TIMEOUT 600)
endif()

add_executable(helios_benchmark_suite_test tests/cpp/benchmark_suite_test.cpp)
target_link_libraries(helios_benchmark_suite_test PRIVATE helios_core)
add_test(NAME helios_benchmark_suite_test COMMAND helios_benchmark_suite_test)

add_executable(helios_smoke_test tests/cpp/smoke_test.cpp)
target_link_libraries(helios_smoke_test PRIVATE helios_core)
add_test(NAME helios_smoke_test COMMAND helios_smoke_test)

add_executable(helios_job_test tests/cpp/job_test.cpp)
target_link_libraries(helios_job_test PRIVATE helios_core)
add_test(NAME helios_job_test COMMAND helios_job_test)

add_executable(helios_resources_test tests/cpp/resources_test.cpp)
target_link_libraries(helios_resources_test PRIVATE helios_core)
add_test(NAME helios_resources_test COMMAND helios_resources_test)

add_executable(helios_job_state_test tests/cpp/job_state_test.cpp)
target_link_libraries(helios_job_state_test PRIVATE helios_core)
add_test(NAME helios_job_state_test COMMAND helios_job_state_test)

add_executable(helios_event_test tests/cpp/event_test.cpp)
target_link_libraries(helios_event_test PRIVATE helios_core)
add_test(NAME helios_event_test COMMAND helios_event_test)

add_executable(helios_simulator_state_test tests/cpp/simulator_state_test.cpp)
target_link_libraries(helios_simulator_state_test PRIVATE helios_core)
add_test(NAME helios_simulator_state_test COMMAND helios_simulator_state_test)

add_executable(helios_schedule_decision_test tests/cpp/schedule_decision_test.cpp)
target_link_libraries(helios_schedule_decision_test PRIVATE helios_core)
add_test(NAME helios_schedule_decision_test COMMAND helios_schedule_decision_test)

add_executable(helios_fifo_scheduler_test tests/cpp/fifo_scheduler_test.cpp)
target_link_libraries(helios_fifo_scheduler_test PRIVATE helios_core)
add_test(NAME helios_fifo_scheduler_test COMMAND helios_fifo_scheduler_test)

add_executable(helios_sjf_scheduler_test tests/cpp/sjf_scheduler_test.cpp)
target_link_libraries(helios_sjf_scheduler_test PRIVATE helios_core)
add_test(NAME helios_sjf_scheduler_test COMMAND helios_sjf_scheduler_test)

add_executable(helios_workload_test tests/cpp/workload_test.cpp)
target_link_libraries(helios_workload_test PRIVATE helios_core)
add_test(NAME helios_workload_test COMMAND helios_workload_test)

add_executable(helios_workload_generator_test tests/cpp/workload_generator_test.cpp)
target_link_libraries(helios_workload_generator_test PRIVATE helios_core)
add_test(NAME helios_workload_generator_test COMMAND helios_workload_generator_test)

add_executable(helios_metrics_test tests/cpp/metrics_test.cpp)
target_link_libraries(helios_metrics_test PRIVATE helios_core)
add_test(NAME helios_metrics_test COMMAND helios_metrics_test)

add_executable(helios_simulation_runner_test tests/cpp/simulation_runner_test.cpp)
target_link_libraries(helios_simulation_runner_test PRIVATE helios_core)
add_test(NAME helios_simulation_runner_test COMMAND helios_simulation_runner_test)

add_executable(helios_csv_test tests/cpp/csv_test.cpp)
target_link_libraries(helios_csv_test PRIVATE helios_core)
add_test(NAME helios_csv_test COMMAND helios_csv_test)

add_executable(helios_demo_workloads_test tests/cpp/demo_workloads_test.cpp)
target_link_libraries(helios_demo_workloads_test PRIVATE helios_core)
add_test(NAME helios_demo_workloads_test COMMAND helios_demo_workloads_test)

# The C++ tests use assert; keep their checks active in Release builds too.
get_property(helios_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(helios_test_target IN LISTS helios_test_targets)
  if(helios_test_target MATCHES "^helios_.*_test$")
    if(MSVC)
      target_compile_options(${helios_test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${helios_test_target} PRIVATE -UNDEBUG)
    endif()
  endif()
endforeach()
