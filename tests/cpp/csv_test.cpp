#include "helios/csv.hpp"

#include <cassert>
#include <string>

namespace {

helios::SimulationResult sample_result() {
  return helios::SimulationResult{
      .jobs =
          {
              helios::JobMetrics{
                  .job_id = 1,
                  .arrival_time = 0,
                  .start_time = 0,
                  .completion_time = 5,
                  .duration = 5,
                  .wait_time = 0,
                  .turnaround_time = 5,
              },
              helios::JobMetrics{
                  .job_id = 2,
                  .arrival_time = 3,
                  .start_time = 5,
                  .completion_time = 9,
                  .duration = 4,
                  .wait_time = 2,
                  .turnaround_time = 6,
              },
          },
      .makespan = 9,
  };
}

void csv_includes_header() {
  const auto csv = helios::simulation_result_to_csv(sample_result());

  assert(csv.rfind(
             "job_id,arrival_time,start_time,completion_time,duration,wait_time,turnaround_time\n",
             0) == 0);
}

void csv_includes_one_row_per_job_metric() {
  const auto csv = helios::simulation_result_to_csv(sample_result());

  assert(csv.find("1,0,0,5,5,0,5\n") != std::string::npos);
  assert(csv.find("2,3,5,9,4,2,6\n") != std::string::npos);
}

void csv_output_is_deterministic() {
  assert(helios::simulation_result_to_csv(sample_result()) ==
         helios::simulation_result_to_csv(sample_result()));
}

void empty_result_emits_header_with_trailing_newline() {
  const auto csv = helios::simulation_result_to_csv(helios::SimulationResult{
      .jobs = {},
      .makespan = 0,
  });

  assert(csv ==
         "job_id,arrival_time,start_time,completion_time,duration,wait_time,turnaround_time\n");
}

void values_are_written_in_documented_column_order() {
  const auto csv = helios::simulation_result_to_csv(sample_result());

  assert(csv ==
         "job_id,arrival_time,start_time,completion_time,duration,wait_time,turnaround_time\n"
         "1,0,0,5,5,0,5\n"
         "2,3,5,9,4,2,6\n");
}

void csv_output_ends_with_trailing_newline() {
  const auto csv = helios::simulation_result_to_csv(sample_result());

  assert(!csv.empty());
  assert(csv.back() == '\n');
}

} // namespace

int main() {
  csv_includes_header();
  csv_includes_one_row_per_job_metric();
  csv_output_is_deterministic();
  empty_result_emits_header_with_trailing_newline();
  values_are_written_in_documented_column_order();
  csv_output_ends_with_trailing_newline();

  return 0;
}
