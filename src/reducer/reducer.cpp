#include <iostream>

#include "reducer.hpp"


namespace fuznet {

Reducer::Reducer(const std::string& lib_yaml,
              const std::string& input_json,
              const std::string& output_json,
              unsigned seed,
              bool verbose)
     : rng(seed),
       library(lib_yaml, rng), 
       netlist(library, rng),
       verbose(verbose) 
{       
    netlist.load_from_json(input_json);
    remove_other_nets(207);
    dump_json(output_json);
}

void Reducer::remove_other_nets(const int& output_id) {
    netlist.remove_other_nets(output_id);
}

void Reducer::dump_json(const std::string& output) const {
    netlist.emit_json(output + ".json");
    std::ofstream dot_file(output + ".dot");
    netlist.emit_dotfile(dot_file, "top");
    dot_file.close();
}

}