#include "orchestrator.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <toml++/toml.h>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;

namespace fuznet {

Orchestrator::Orchestrator(const std::string& lib_yaml,
                           const std::string& config_toml,
                           unsigned           seed,
                           bool               verbose,
                           bool               animate,
                           bool               json_stats)
    : library_yaml(lib_yaml),
      config_toml(config_toml),
      seed(seed),
      rng(seed),
      library(lib_yaml, rng),
      netlist(library, rng),
      verbose(verbose),
      animate(animate),
      json_stats(json_stats)
    {

    commands = {
        { new AddRandomModule(netlist),              1.0 },
        { new AddExternalNet(netlist),               1.0 },
        { new AddUndriveNet(netlist),                1.0 },
        { new DriveUndrivenNet(netlist, 0.3, 0.3),   1.0 },
        { new DriveUndrivenNets(netlist, 0.3, 0.3),  1.0 },
        { new BufferUnconnectedOutputs(netlist),     1.0 }
    };

    load_config();

    std::poisson_distribution<int> undriven_dist (start_undriven_lambda);
    std::poisson_distribution<int> input_dist    (start_input_lambda);

    netlist.add_undriven_nets(NetType::LOGIC, undriven_dist(rng));
    netlist.add_external_nets(input_dist(rng));

    std::vector<double> weights;
    for (const auto& entry : commands) weights.push_back(entry.weight);
    weight_dist = std::discrete_distribution<int>(weights.begin(), weights.end());
}

void Orchestrator::load_config() {
    auto cfg = toml::parse_file(config_toml);

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
    seq_mod_prob          = set["prob_sequential_module"].value_or(0.2);
    seq_port_prob         = set["prob_sequential_port"].value_or(0.2);

    DriveUndrivenNet*  drive_one   = nullptr;
    DriveUndrivenNets* drive_many  = nullptr;

    for (auto& entry : commands) {
        if (std::string(entry.cmd->name()) == "DriveUndrivenNet")   drive_one  = static_cast<DriveUndrivenNet*>(entry.cmd);
        if (std::string(entry.cmd->name()) == "DriveUndrivenNets")  drive_many = static_cast<DriveUndrivenNets*>(entry.cmd);
    }
    if (drive_one)  {
        drive_one->seq_mod_prob  = seq_mod_prob;
        drive_one->seq_port_prob = seq_port_prob;
    }
    if (drive_many) {
        drive_many->seq_mod_prob = seq_mod_prob;
        drive_many->seq_port_prob = seq_port_prob;
    }

    if (!verbose) return;

    std::cout << "\n=== settings: " << config_toml << " ===\n";
    std::cout << "max_iter:                 " << max_iter              << '\n';
    std::cout << "stop_iter_lambda:         " << stop_iter_lambda      << '\n';
    std::cout << "start_input_lambda:       " << start_input_lambda    << '\n';
    std::cout << "start_undriven_lambda:    " << start_undriven_lambda << '\n';
    std::cout << "prob_sequential_module:   " << seq_mod_prob          << "\n";
    std::cout << "prob_sequential_port:     " << seq_port_prob         << "\n\n";
    std::cout << "      --- command weights ---\n";
    for (const auto& entry : commands)
    std::cout << std::left << std::setw(26) << entry.cmd->name() << " : " << entry.weight << '\n';
    std::cout << "======== Configuration Loaded ========\n";
    std::cout << "======================================\n\n";
}

void Orchestrator::run(const std::string& output_prefix) {
    std::poisson_distribution<int> stop_dist(stop_iter_lambda);
    int iterations = std::min(stop_dist(rng), max_iter);

    std::string verilog_path = output_prefix + ".v";

    auto dump_dot = [&](int iter = 0) {
        std::ofstream file(output_prefix + "_iter" + std::to_string(iter) + ".dot", std::ios::trunc);
        netlist.emit_dotfile(file, "top");
        file.close();
    };

    netlist.add_initial_nets();
    if (animate) dump_dot(0);

    for (int i = 0; i < iterations; ++i) {
        commands[weight_dist(rng)].cmd->execute();
        if (animate) dump_dot(i + 1);
    }
    
    netlist.drive_undriven_nets(seq_mod_prob, seq_port_prob);
    netlist.buffer_unconnected_outputs();
    
    std::ofstream v(verilog_path);
    netlist.emit_verilog(v, "top");
    
    dump_dot(iterations + 1);

    nlohmann::json json_save;
    json_save["new"] = netlist.json();
    std::ofstream json_file(output_prefix + ".json");
    json_file << std::setw(4) << json_save << std::endl;
    json_file.close();

    if(verbose) {
        std::cout << "======== Netlist Generated =========\n";
        netlist.print();
        std::cout << "====================================\n\n";
    }

    if (json_stats)
        json_dump(output_prefix);
}

void Orchestrator::json_dump(const std::string& output_prefix) const {
    nlohmann::json json_data;

    json_data["library"] = library_yaml;
    json_data["config"] = config_toml;
    json_data["seed"] = seed;
    
    
    json_data["settings"] = {
        {"max_iter", max_iter},
        {"stop_iter_lambda", stop_iter_lambda},
        {"start_input_lambda", start_input_lambda},
        {"start_undriven_lambda", start_undriven_lambda},
        {"seq_mod_prob", seq_mod_prob},
        {"seq_port_prob", seq_port_prob}
    };
    
    for (const auto& entry : commands) {
        json_data["commands"].push_back({
            {"name", entry.cmd->name()},
            {"weight", entry.weight}
        });
    }

    auto stats = netlist.get_stats();

    json_data["netlist_stats"] = {
        {"input_nets", stats.input_nets},
        {"output_nets", stats.output_nets},
        {"total_nets", stats.total_nets},
        {"comb_modules", stats.comb_modules},
        {"seq_modules", stats.seq_modules},
        {"total_modules", stats.total_modules}
    };

    std::ofstream json_file(output_prefix + "_stats.json");
    json_file << std::setw(4) << json_data << std::endl;
    json_file.close();
    if (verbose) {
        std::cout << "JSON stats written to: " << output_prefix << ".json\n";
    }
}

Orchestrator::~Orchestrator() {
    for (auto& entry : commands) delete entry.cmd;
}

}
