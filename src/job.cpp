#include "helios/job.hpp"

#include <stdexcept>

namespace helios {

void validate_job(const Job& job) {
  validate_resources(job.request);

  if (job.id < 0) {
    throw std::invalid_argument("Job id must be non-negative");
  }
  if (job.arrival_time < 0) {
    throw std::invalid_argument("Job arrival time must be non-negative");
  }
  if (job.duration <= 0) {
    throw std::invalid_argument("Job duration must be positive");
  }
  if (job.request.gpu_count <= 0) {
    throw std::invalid_argument("Job GPU request must be positive");
  }
}

}  // namespace helios
