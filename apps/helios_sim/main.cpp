#include "helios/benchmark_suite.hpp"
#include "helios/csv.hpp"
#include "helios/demo_workloads.hpp"
#include "helios/metrics.hpp"
#include "helios/runtime_estimates.hpp"
#include "helios/schedulers/fifo_scheduler.hpp"
#include "helios/simulation_runner.hpp"
#include "helios/simulator.hpp"
#include "helios/version.hpp"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void print_demo_workloads() {
  for (const auto& spec : helios::demo_workload_specs()) {
    std::cout << spec.name << " - " << spec.description << '\n';
  }
}

void print_comparison_row(std::string_view scheduler_name, const helios::SimulationResult& result) {
  std::cout << scheduler_name << ',' << result.makespan << ',' << helios::total_wait_time(result)
            << ',' << helios::average_wait_time(result) << ',' << helios::p95_wait_time(result)
            << ',' << helios::max_wait_time(result) << '\n';
}

unsigned int parse_unsigned(std::string_view value) {
  unsigned int number = 0;
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
  if (error != std::errc{} || end != value.data() + value.size()) {
    throw std::invalid_argument("expected an unsigned integer");
  }
  return number;
}

int parse_estimate_percentage(std::string_view value) {
  const auto percentage = parse_unsigned(value);
  if (percentage == 0 || percentage > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("estimate percentage must be a positive int");
  }
  return static_cast<int>(percentage);
}

void print_seeded_rows(const std::vector<helios::SeededSummary>& rows) {
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
  std::cout << "workload,seed,scheduler,job_count,makespan,total_wait_time,average_wait_time,"
               "p95_wait_time,max_wait_time\n";
  for (const auto& row : rows) {
    const auto& summary = row.summary;
    std::cout << summary.workload_name << ',' << row.seed << ',' << summary.scheduler_name << ','
              << summary.job_count << ',' << summary.makespan << ',' << summary.total_wait_time
              << ',' << summary.average_wait_time << ',' << summary.p95_wait_time << ','
              << summary.max_wait_time << '\n';
  }
}

} // namespace

int run_cli(int argc, char* argv[]) {
  if (argc >= 2 && std::string_view{argv[1]} == "--compare-baselines") {
    if (argc != 5) {
      throw std::invalid_argument(
          "usage: --compare-baselines <workload> <estimate-percent> <random-seed>");
    }
    const std::string_view name = argv[2];
    const auto percentage = parse_estimate_percentage(argv[3]);
    const auto seed = parse_unsigned(argv[4]);
    const auto workload =
        helios::with_runtime_estimates(helios::make_demo_workload(name), percentage);
    const auto rows =
        helios::compare_baselines(name, workload, helios::demo_cluster_resources(), seed);
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
    std::cout
        << "workload,scheduler,estimate_percent,random_seed,job_count,makespan,total_wait_time,"
           "average_wait_time,p95_wait_time,max_wait_time\n";
    for (const auto& row : rows) {
      std::cout << row.workload_name << ',' << row.scheduler_name << ',' << percentage << ','
                << seed << ',' << row.job_count << ',' << row.makespan << ',' << row.total_wait_time
                << ',' << row.average_wait_time << ',' << row.p95_wait_time << ','
                << row.max_wait_time << '\n';
    }
    return 0;
  }
  if (argc >= 2 && std::string_view{argv[1]} == "--demo-step") {
    if (argc != 3) {
      throw std::invalid_argument("usage: --demo-step <workload>");
    }
    helios::Simulator simulator(helios::make_demo_workload(argv[2]),
                                helios::demo_cluster_resources());
    std::cout << "from_time,to_time,action,job_id,completed_count,available_gpus,terminated\n";
    while (!simulator.done()) {
      const auto view = simulator.observe();
      const auto action = helios::fifo_policy(view);
      const auto result = simulator.step(action);
      const auto after = simulator.observe();
      std::cout << view.current_time << ',' << after.current_time << ','
                << (action.job_id ? "start" : "wait") << ',';
      if (action.job_id) {
        std::cout << *action.job_id;
      }
      std::cout << ',' << result.completed_job_ids.size() << ','
                << after.available_resources.gpu_count << ',' << result.terminated << '\n';
    }
    return 0;
  }
  if (argc >= 2 && std::string_view{argv[1]} == "--demo-random") {
    if (argc != 4) {
      throw std::invalid_argument("usage: --demo-random <workload> <seed>");
    }
    std::cout << helios::simulation_result_to_csv(
        helios::run_random_simulation(helios::make_demo_workload(argv[2]),
                                      helios::demo_cluster_resources(), parse_unsigned(argv[3])));
    return 0;
  }
  if (argc >= 2 && (std::string_view{argv[1]} == "--demo-estimated-sjf" ||
                    std::string_view{argv[1]} == "--demo-backfill")) {
    if (argc != 4) {
      throw std::invalid_argument(
          "usage: --demo-estimated-sjf/--demo-backfill <workload> <estimate-percent>");
    }
    const auto workload = helios::with_runtime_estimates(helios::make_demo_workload(argv[2]),
                                                         parse_estimate_percentage(argv[3]));
    const auto resources = helios::demo_cluster_resources();
    const auto result = std::string_view{argv[1]} == "--demo-backfill"
                            ? helios::run_easy_backfill_simulation(workload, resources)
                            : helios::run_estimated_sjf_simulation(workload, resources);
    std::cout << helios::simulation_result_to_csv(result);
    return 0;
  }
  if (argc >= 2 && (std::string_view{argv[1]} == "--compare-seeds" ||
                    std::string_view{argv[1]} == "--compare-scale")) {
    if (argc > 3) {
      throw std::invalid_argument(
          "usage: --compare-seeds [seed-count] or --compare-scale [seed-count]");
    }
    const auto count = argc == 3 ? parse_unsigned(argv[2]) : 10;
    if (count == 0) {
      throw std::invalid_argument("seed count must be positive");
    }
    std::vector<unsigned int> seeds;
    for (unsigned int seed = 0; seed < count; ++seed) {
      seeds.push_back(seed);
    }
    const bool scale = std::string_view{argv[1]} == "--compare-scale";
    std::vector<std::string> profiles;
    for (const auto& spec :
         scale ? helios::scale_workload_specs() : helios::demo_workload_specs()) {
      if (scale || spec.name.starts_with("generated-")) {
        profiles.emplace_back(spec.name);
      }
    }
    print_seeded_rows(helios::compare_seeded_workloads(profiles, seeds));
    return 0;
  }
  if (argc >= 2 && std::string_view{argv[1]} == "--compare-seeded") {
    if (argc != 4) {
      throw std::invalid_argument("usage: --compare-seeded <profile> <seed>");
    }
    print_seeded_rows(helios::compare_seeded_workloads({argv[2]}, {parse_unsigned(argv[3])}));
    return 0;
  }
  if (argc == 2 && std::string_view{argv[1]} == "--compare-suite") {
    const auto summaries = helios::compare_generated_suite();
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
    std::cout << "workload,scheduler,job_count,makespan,total_wait_time,average_wait_time,"
                 "p95_wait_time,max_wait_time\n";
    for (const auto& summary : summaries) {
      std::cout << summary.workload_name << ',' << summary.scheduler_name << ','
                << summary.job_count << ',' << summary.makespan << ',' << summary.total_wait_time
                << ',' << summary.average_wait_time << ',' << summary.p95_wait_time << ','
                << summary.max_wait_time << '\n';
    }
    return 0;
  }
  if (argc == 2 && std::string_view{argv[1]} == "--list-demo-workloads") {
    print_demo_workloads();
    return 0;
  }

  if ((argc == 2 || argc == 3) && std::string_view{argv[1]} == "--demo-fifo") {
    const std::string_view workload_name = argc == 3 ? std::string_view{argv[2]} : "light-overlap";
    const auto result = helios::run_fifo_simulation(helios::make_demo_workload(workload_name),
                                                    helios::demo_cluster_resources());
    std::cout << helios::simulation_result_to_csv(result);
    return 0;
  }

  if ((argc == 2 || argc == 3) && std::string_view{argv[1]} == "--demo-sjf") {
    const std::string_view workload_name = argc == 3 ? std::string_view{argv[2]} : "light-overlap";
    const auto result = helios::run_sjf_simulation(helios::make_demo_workload(workload_name),
                                                   helios::demo_cluster_resources());
    std::cout << helios::simulation_result_to_csv(result);
    return 0;
  }

  if (argc == 3 && std::string_view{argv[1]} == "--compare") {
    const std::string_view workload_name = argv[2];
    const auto workload = helios::make_demo_workload(workload_name);
    const auto resources = helios::demo_cluster_resources();

    std::cout
        << "scheduler,makespan,total_wait_time,average_wait_time,p95_wait_time,max_wait_time\n";
    print_comparison_row("fifo", helios::run_fifo_simulation(workload, resources));
    print_comparison_row("sjf", helios::run_sjf_simulation(workload, resources));
    return 0;
  }

  std::cout << "Helios simulator scaffold v" << helios::version() << '\n';
  std::cout << "Run with --list-demo-workloads to see named demo workloads.\n";
  std::cout << "Run with --demo-fifo [workload-name] to print a deterministic FIFO CSV.\n";
  std::cout << "Run with --demo-sjf [workload-name] to print a deterministic SJF CSV.\n";
  std::cout << "Run with --compare <workload-name> to compare FIFO and SJF metrics.\n";
  std::cout << "Run with --compare-suite to compare all generated profiles as CSV.\n";
  std::cout
      << "Run with --compare-seeds [seed-count] for paired multi-seed comparisons (default 10).\n";
  std::cout << "Run with --compare-scale [seed-count] for 1000 to 12000 jobs (default 10).\n";
  std::cout << "Run with --compare-seeded <profile> <seed> for one reproducible pair.\n";
  std::cout << "Run with --compare-baselines <workload> <estimate-percent> <random-seed> for five "
               "policies.\n";
  std::cout << "Run with --demo-step <workload> for an event-by-event FIFO trace.\n";
  std::cout << "Run with --demo-random <workload> <seed> for random scheduling CSV.\n";
  std::cout << "Run with --demo-estimated-sjf/--demo-backfill <workload> <estimate-percent> for "
               "estimated policies.\n";
  return 0;
}

int main(int argc, char* argv[]) {
  try {
    return run_cli(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "helios_sim: " << error.what() << '\n';
    return 1;
  }
}
