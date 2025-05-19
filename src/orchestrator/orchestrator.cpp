#include "orchestrator.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

#include <toml++/toml.h>

using namespace std::chrono_literals;

namespace fuznet {

Orchestrator::Orchestrator(const std::string& lib_yaml,
                           const std::string& config_toml,
                           unsigned           seed,
                           bool               verbose)
    : rng(seed),
      library(lib_yaml, rng),
      netlist(library, rng),
      verbose(verbose),
      animate(animate) {

    commands = {
        { new AddRandomModule(netlist),            1.0 },
        { new AddExternalNet(netlist),             1.0 },
        { new AddUndriveNet(netlist),              1.0 },
        { new DriveUndrivenNet(netlist, 0.3),      1.0 },
        { new DriveUndrivenNets(netlist, 0.3),     1.0 },
        { new BufferUnconnectedOutputs(netlist),   1.0 }
    };

    load_config(config_toml);

    std::poisson_distribution<int> undriven_dist (start_undriven_lambda);
    std::poisson_distribution<int> input_dist    (start_input_lambda);

    netlist.add_undriven_nets(NetType::LOGIC, undriven_dist(rng));
    netlist.add_external_nets(input_dist(rng));

    std::vector<double> weights;
    for (const auto& entry : commands) weights.push_back(entry.weight);
    weight_dist = std::discrete_distribution<int>(weights.begin(), weights.end());
}

void Orchestrator::load_config(const std::string& toml_path) {
    auto cfg = toml::parse_file(toml_path);

    for (auto& entry : commands) {
        auto node = entry.cmd->name();
        if (!cfg["priorities"][node]) {
            std::cerr << "Warning: command '" << node << "' not in config, weight 0\n";
            continue;
        }
        entry.weight = cfg["priorities"][node].value_or(0.0);
    }

    auto set              = cfg["settings"];
    max_iter              = set["max_iter"].value_or(10'000);
    stop_iter_lambda      = set["stop_iter_lambda"].value_or(20);
    start_input_lambda    = set["start_input_lambda"].value_or(5);
    start_undriven_lambda = set["start_undriven_lambda"].value_or(5);
    seq_probability       = set["prob_sequential"].value_or(0.2);

    DriveUndrivenNet*  drive_one   = nullptr;
    DriveUndrivenNets* drive_many  = nullptr;

    for (auto& entry : commands) {
        if (entry.cmd->name() == "DriveUndrivenNet")   drive_one  = static_cast<DriveUndrivenNet*>(entry.cmd);
        if (entry.cmd->name() == "DriveUndrivenNets")  drive_many = static_cast<DriveUndrivenNets*>(entry.cmd);
    }
    if (drive_one)  drive_one->seq_probability  = seq_probability;
    if (drive_many) drive_many->seq_probability = seq_probability;

    if (!verbose) return;

    std::cout << "\n=== settings: " << toml_path << " ===\n";
    std::cout << "max_iter:                 " << max_iter              << '\n';
    std::cout << "stop_iter_lambda:         " << stop_iter_lambda      << '\n';
    std::cout << "start_input_lambda:       " << start_input_lambda    << '\n';
    std::cout << "start_undriven_lambda:    " << start_undriven_lambda << '\n';
    std::cout << "prob_sequential:          " << seq_probability       << "\n\n";
    std::cout << "      --- command weights ---\n";
    for (const auto& entry : commands)
    std::cout << std::left << std::setw(26) << entry.cmd->name() << " : " << entry.weight << '\n';
    std::cout << "======== Configuration Loaded ========\n";
    std::cout << "======================================\n\n";
}

void Orchestrator::run(const std::string& output_prefix, bool animate) {
    std::poisson_distribution<int> stop_dist(stop_iter_lambda);
    int iterations = std::min(stop_dist(rng), max_iter);

    std::string dot_path = output_prefix + ".dot";
    std::string verilog_path = output_prefix + ".v";

    auto dump_dot = [&]() {
        std::ofstream file(dot_path, std::ios::trunc);
        netlist.emit_dotfile(file, "top");
        file.close();
        if (animate) std::this_thread::sleep_for(1s);
    };

    for (int i = 0; i < iterations; ++i) {
        commands[weight_dist(rng)].cmd->execute();
        if (animate) dump_dot();
    }
    
    netlist.drive_undriven_nets(seq_probability);
    netlist.buffer_unconnected_outputs();
    
    std::ofstream v(verilog_path);
    netlist.emit_verilog(v, "top");
    
    dump_dot();

    if(!verbose) return;
    
    std::cout << "======== Netlist Generated =========\n";
    netlist.print();
    std::cout << "====================================\n\n";
}

Orchestrator::~Orchestrator() {
    for (auto& entry : commands) delete entry.cmd;
}

}
