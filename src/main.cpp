#include <fstream>
#include <iostream>
#include <random>

#include "lib.hpp"
#include "netlist.hpp"

int main() {
    try {
        std::mt19937_64 rng;
        rng.seed(std::random_device{}());

        Library lib("src/lib.yaml", rng);
        Netlist netlist(lib, rng);

        netlist.add_random_module();
        netlist.add_external_net();
        for (int i = 0; i < 4; ++i) netlist.add_random_module();

        netlist.switch_up();
        netlist.insert_output_buffers();
        netlist.print();

        std::ofstream verilog("output.v");
        netlist.emit_verilog(verilog, "top");

        std::ofstream dot("output.dot");
        netlist.emit_dotfile(dot, "top");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
