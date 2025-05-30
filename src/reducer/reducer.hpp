#pragma once

#include <string>
#include <fstream>

#include "netlist.hpp"

namespace fuznet {

class Reducer {
public:
    Reducer(const std::string& lib_yaml     = "hardware/xilinx/cells.yaml",
            const std::string& input_json   = "output/output_netlist.json",
            const std::string& output       = "output/reduced_netlist",
            unsigned           seed         = std::random_device{}(),
            bool               verbose      = false);

    void remove_other_nets(const int& output_id);
    void dump_json(const std::string& output_json) const;

private:
    std::mt19937_64 rng;
    Library library;
    Netlist netlist;


    bool verbose{false};
};

}