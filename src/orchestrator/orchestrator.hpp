#pragma once

#include <atomic>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "library.hpp"
#include "netlist.hpp"
#include "commands.hpp"

namespace fuznet {

class Orchestrator {
public:
    Orchestrator(const std::string& lib_yaml    = "hardware/cells/xilinx.yaml",
                 const std::string& config_toml = "config/settings.toml",
                 unsigned           seed        = std::random_device{}());

    void run(const std::string& dot_path, bool animate = false);

    ~Orchestrator();

private:
    void load_config(const std::string& toml_path, bool print = false);

    struct Entry {
        ICommand* cmd;
        double    weight;
    };

    Library                         library;
    Netlist                         netlist;
    std::mt19937_64                 rng;
    std::vector<Entry>              commands;
    std::discrete_distribution<int> weight_dist;
    std::atomic<bool>               stop_flag{false};

    int    max_iter               = 0;
    int    stop_iter_lambda       = 0;
    double seq_probability        = 0.0;
    int    start_undriven_lambda  = 0;
    int    start_input_lambda     = 0;
};

}
