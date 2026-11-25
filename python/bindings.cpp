#include "helios/schedulers/easy_backfill_scheduler.hpp"
#include "helios/schedulers/estimated_sjf_scheduler.hpp"
#include "helios/schedulers/fifo_scheduler.hpp"
#include "helios/schedulers/random_scheduler.hpp"
#include "helios/simulator.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace helios;

PYBIND11_MODULE(_core, m) {
  m.doc() = "Bindings for the Helios interactive simulator";
  py::class_<Resources>(m, "Resources")
      .def(py::init<ResourceAmount, ResourceAmount, ResourceAmount>(), py::arg("gpu_count"),
           py::arg("cpu_count"), py::arg("memory_mb"))
      .def_readwrite("gpu_count", &Resources::gpu_count)
      .def_readwrite("cpu_count", &Resources::cpu_count)
      .def_readwrite("memory_mb", &Resources::memory_mb);
  py::class_<Job>(m, "Job")
      .def(py::init<JobId, SimTime, Duration, Resources, std::optional<Duration>>(), py::arg("id"),
           py::arg("arrival_time"), py::arg("duration"), py::arg("request"),
           py::arg("estimated_duration") = std::nullopt)
      .def_readwrite("id", &Job::id)
      .def_readwrite("arrival_time", &Job::arrival_time)
      .def_readwrite("duration", &Job::duration)
      .def_readwrite("request", &Job::request)
      .def_readwrite("estimated_duration", &Job::estimated_duration);
  py::class_<Workload>(m, "Workload")
      .def(py::init([](std::vector<Job> jobs) { return make_workload(std::move(jobs)); }),
           py::arg("jobs"))
      .def_readonly("jobs", &Workload::jobs);
  py::class_<PendingJobView>(m, "PendingJobView")
      .def_readonly("id", &PendingJobView::id)
      .def_readonly("arrival_time", &PendingJobView::arrival_time)
      .def_readonly("estimated_duration", &PendingJobView::estimated_duration)
      .def_readonly("request", &PendingJobView::request);
  py::class_<RunningJobView>(m, "RunningJobView")
      .def_readonly("job", &RunningJobView::job)
      .def_readonly("start_time", &RunningJobView::start_time)
      .def_readonly("elapsed_time", &RunningJobView::elapsed_time);
  py::class_<Observation>(m, "Observation")
      .def_readonly("current_time", &Observation::current_time)
      .def_readonly("total_resources", &Observation::total_resources)
      .def_readonly("available_resources", &Observation::available_resources)
      .def_readonly("pending_jobs", &Observation::pending_jobs)
      .def_readonly("running_jobs", &Observation::running_jobs)
      .def_readonly("completed_job_count", &Observation::completed_job_count)
      .def_readonly("terminated", &Observation::terminated);
  py::class_<ScheduleDecision>(m, "ScheduleDecision")
      .def_readonly("job_id", &ScheduleDecision::job_id);
  m.def("select_job", &select_job, py::arg("job_id"));
  m.def("no_schedule_decision", &no_schedule_decision);
  m.def("fifo_policy", &fifo_policy, py::arg("observation"));
  m.def("estimated_sjf_schedule", &estimated_sjf_schedule, py::arg("observation"));
  m.def("easy_backfill_schedule", &easy_backfill_schedule, py::arg("observation"));
  py::class_<RandomScheduler>(m, "RandomScheduler")
      .def(py::init<unsigned int>(), py::arg("seed"))
      .def("__call__", &RandomScheduler::operator(), py::arg("observation"))
      .def("reset", &RandomScheduler::reset, py::arg("seed"));
  py::class_<StepResult>(m, "StepResult")
      .def_readonly("elapsed_time", &StepResult::elapsed_time)
      .def_readonly("started_job_id", &StepResult::started_job_id)
      .def_readonly("completed_job_ids", &StepResult::completed_job_ids)
      .def_readonly("terminated", &StepResult::terminated);
  py::class_<JobMetrics>(m, "JobMetrics")
      .def_readonly("job_id", &JobMetrics::job_id)
      .def_readonly("arrival_time", &JobMetrics::arrival_time)
      .def_readonly("start_time", &JobMetrics::start_time)
      .def_readonly("completion_time", &JobMetrics::completion_time)
      .def_readonly("duration", &JobMetrics::duration)
      .def_readonly("wait_time", &JobMetrics::wait_time)
      .def_readonly("turnaround_time", &JobMetrics::turnaround_time);
  py::class_<SimulationResult>(m, "SimulationResult")
      .def_readonly("jobs", &SimulationResult::jobs)
      .def_readonly("makespan", &SimulationResult::makespan);
  m.def("total_wait_time", &total_wait_time);
  m.def("average_wait_time", &average_wait_time);
  m.def("p95_wait_time", &p95_wait_time);
  m.def("max_wait_time", &max_wait_time);
  py::class_<Simulator>(m, "Simulator")
      .def(py::init<const Workload&, Resources>(), py::arg("workload"), py::arg("total_resources"))
      .def("reset", &Simulator::reset, py::arg("workload"), py::arg("total_resources"))
      .def("observe", &Simulator::observe)
      .def("step", &Simulator::step, py::arg("decision"))
      .def("done", &Simulator::done)
      .def("result", &Simulator::result);
}
