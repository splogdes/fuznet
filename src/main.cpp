#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "module_library.hpp"
#include "netlist.hpp"

using namespace std::chrono_literals;

void write_dot(Netlist& nl) {
    std::ofstream file("output.dot", std::ios::trunc);
    nl.emit_dotfile(file, "top");
    file.close();
    std::this_thread::sleep_for(1s);
}

int main() {
    try {
        std::mt19937_64 prng(std::random_device{}());

        ModuleLibrary library("hardware/cells/xilinx.yaml", prng);
        Netlist netlist(library, prng);

        write_dot(netlist);

        netlist.add_random_module();
        write_dot(netlist);

        netlist.add_external_net();
        write_dot(netlist);

        for (int i = 0; i < 3; ++i) {
            netlist.add_undriven_net();
            write_dot(netlist);
        }
        for (int i = 0; i < 3; ++i) {
            netlist.add_random_module();
            write_dot(netlist);
        }
        for (int i = 0; i < 3; ++i) {
            netlist.drive_undriven_nets(0.3, true);
            write_dot(netlist);
        }

        netlist.buffer_unconnected_outputs();
        write_dot(netlist);

        netlist.print();

        std::ofstream verilog_file("output.v");
        netlist.emit_verilog(verilog_file, "top");

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
