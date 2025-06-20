#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <set>

#include "netlist.hpp"

namespace fuznet {

class Reducer {
public:
    Reducer(const std::string& lib_yaml     = "hardware/xilinx/cells.yaml",
            const std::string& input_json   = "output/output_netlist.json",
            unsigned           seed         = std::random_device{}(),
            bool               json_stats   = false,
            bool               verbose      = false);

    void write_outputs(const std::string& output_json) const;
    int  reduce(const int& output_id, bool success = true);
    
private:
    
    int  iterative_reduce(bool success);
    void keep_only_net(const int& output_id);

    std::mt19937_64 rng;
    Library library;
    Netlist netlist;

    nlohmann::json json_data;

    bool json_stats{false};
    bool verbose{false};
};

}