#pragma once

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
    Orchestrator(const std::string& lib_yaml    = "hardware/xilinx/cells.yaml",
                 const std::string& config_toml = "config/settings.toml",
                 unsigned           seed        = std::random_device{}(),
                 bool               verbose     = false,
                 bool               animate     = false,
                 bool               json_stats  = false);

    void run(const std::string& output_prefix);

    ~Orchestrator();

private:
    void load_config();
    void json_dump(const std::string& output_prefix) const;

    struct Entry {
        ICommand* cmd;
        double    weight;
    };

    std::string library_yaml;
    std::string config_toml;

    unsigned    seed;
    
    std::mt19937_64                 rng;
    Library                         library;
    Netlist                         netlist;
    std::vector<Entry>              commands;
    std::discrete_distribution<int> weight_dist;

    int         max_iter               = 0;
    int         stop_iter_lambda       = 0;
    double      seq_mod_prob           = 0.0;
    double      seq_port_prob          = 0.0;
    int         start_undriven_lambda  = 0;
    int         start_input_lambda     = 0;
    bool        verbose                = false;
    bool        animate                = false;
    bool        json_stats             = false;
};

}
