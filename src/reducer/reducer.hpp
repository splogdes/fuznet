#pragma once

#include <string>
#include <fstream>

#include "netlist.hpp"

namespace fuznet {

class Reducer {
public:
    Reducer(const std::string& lib_yaml     = "hardware/xilinx/cells.yaml",
            const std::string& input_json   = "output/output_netlist.json",
            unsigned           seed         = std::random_device{}(),
            bool               verbose      = false);

    void keep_only_net(const int& output_id);
    void write_outputs(const std::string& output_json) const;

private:
    std::mt19937_64 rng;
    Library library;
    Netlist netlist;


    bool verbose{false};
};

}