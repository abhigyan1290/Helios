#include "helios/csv.hpp"
#include "helios/demo_workloads.hpp"
#include "helios/metrics.hpp"
#include "helios/simulation_runner.hpp"
#include "helios/version.hpp"

#include <iostream>
#include <string_view>

namespace {

void print_demo_workloads() {
  for (const auto& spec : helios::demo_workload_specs()) {
    std::cout << spec.name << " - " << spec.description << '\n';
  }
}

void print_comparison_row(std::string_view scheduler_name, const helios::SimulationResult& result) {
  std::cout << scheduler_name << ',' << result.makespan << ',' << helios::total_wait_time(result)
            << ',' << helios::average_wait_time(result) << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
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

    std::cout << "scheduler,makespan,total_wait_time,average_wait_time\n";
    print_comparison_row("fifo", helios::run_fifo_simulation(workload, resources));
    print_comparison_row("sjf", helios::run_sjf_simulation(workload, resources));
    return 0;
  }

  std::cout << "Helios simulator scaffold v" << helios::version() << '\n';
  std::cout << "Run with --list-demo-workloads to see named demo workloads.\n";
  std::cout << "Run with --demo-fifo [workload-name] to print a deterministic FIFO CSV.\n";
  std::cout << "Run with --demo-sjf [workload-name] to print a deterministic SJF CSV.\n";
  std::cout << "Run with --compare <workload-name> to compare FIFO and SJF metrics.\n";
  return 0;
}
