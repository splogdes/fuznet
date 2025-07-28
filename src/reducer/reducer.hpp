#pragma once

#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <set>

#include "netlist.hpp"

namespace fuznet {

enum class Result {
    SUCCESS, 
    FAILURE, 
    ALREADY_SEEN, 
    NEW_HASH_ADDED
};

class Reducer {
public:
    Reducer(const std::string& lib_yaml      = "hardware/xilinx/cells.yaml",
            const std::string& input_json    = "output/output_netlist.json",
            const std::string& hash_file     = "output/seen_netlists.txt",
            unsigned           seed          = std::random_device{}(),
            bool               json_stats    = false,
            bool               verbose       = false);

    void write_outputs(const std::string& output_json) const;
    Result reduce(const int& output_id, bool success = true);
    
private:
    
    Result  iterative_reduce(bool success);
    void    keep_only_net(const int& output_id);
    Result  check_hash() const;

    const std::string hash_file;

    std::mt19937_64 rng;
    Library library;
    Netlist netlist;

    nlohmann::json json_data;

    bool json_stats{false};
    bool verbose{false};
};

}