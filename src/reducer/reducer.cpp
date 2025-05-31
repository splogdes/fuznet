#include <iostream>

#include "reducer.hpp"


namespace fuznet {

Reducer::Reducer(const std::string& lib_yaml,
              const std::string& input_json,
              unsigned seed,
              bool verbose)
     : rng(seed),
       library(lib_yaml, rng), 
       netlist(library, rng),
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
    dot_file.close();
}

}