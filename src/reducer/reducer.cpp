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
    dump_json(output_json);
}

void Reducer::dump_json(const std::string& output_json) const {
    library.print();
    std::cout << "Loaded netlist from JSON: " << output_json << '\n';
    netlist.emit_json(output_json);
}


}