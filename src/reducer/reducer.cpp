#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

#include "reducer.hpp"


namespace fuznet {

Reducer::Reducer(const std::string& lib_yaml,
              const std::string& input_json,
              unsigned seed,
              bool json_stats,
              bool verbose)
     : rng(seed),
       library(lib_yaml, rng), 
       netlist(library, rng),
       json_stats(json_stats),
       verbose(verbose) 
{       
    netlist.load_from_json(input_json);

    if (verbose) {
        std::cout << "Loaded netlist from: " << input_json << "\n"
                  << "Netlist has:" << "\n";
        netlist.print();
    }
}

void Reducer::keep_only_net(const int& output_id) {
    if (verbose)
        std::cout << "Reducing netlist to keep only net with ID: " << output_id << "\n";

    netlist.remove_other_nets(output_id);
    
    if (verbose) {
        std::cout << "After removing other nets, the netlist has:\n";
        netlist.print();
    }
}

void Reducer::write_outputs(const std::string& output) const {
    if (verbose)
        std::cout << "Dumping netlist file to prefix: " << output << "\n";

    netlist.emit_json(output + ".json");
    
    std::ofstream dot_file(output + ".dot");
    netlist.emit_dotfile(dot_file, "top");
    dot_file.close();
    
    std::ofstream verilog_file(output + ".v");
    netlist.emit_verilog(verilog_file, "top");
    verilog_file.close();
    
    if (!json_stats)
        return;

    nlohmann::json json_data;

    auto stats = netlist.get_stats();

    json_data["input_nets"] = stats.input_nets;
    json_data["output_nets"] = stats.output_nets;
    json_data["total_nets"] = stats.total_nets;
    json_data["comb_modules"] = stats.comb_modules;
    json_data["seq_modules"] = stats.seq_modules;
    json_data["total_modules"] = stats.total_modules;

    std::ofstream json_file(output + "_stats.json");
    json_file << std::setw(4) << json_data << std::endl;
    json_file.close();
    if (verbose) {
        std::cout << "Netlist stats written to: " << output << "_stats.json\n";
    }
}

}