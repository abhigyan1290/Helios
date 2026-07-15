#include "helios/csv.hpp"

#include <sstream>

namespace helios {

std::string simulation_result_to_csv(const SimulationResult& result) {
  std::ostringstream output;
  output << "job_id,arrival_time,start_time,completion_time,duration,wait_time,turnaround_time\n";

  for (const auto& job : result.jobs) {
    output << job.job_id << ',' << job.arrival_time << ',' << job.start_time << ','
           << job.completion_time << ',' << job.duration << ',' << job.wait_time << ','
           << job.turnaround_time << '\n';
  }

  return output.str();
}

} // namespace helios
