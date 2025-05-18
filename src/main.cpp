#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "lib.hpp"
#include "netlist.hpp"

void print_dot(std::ofstream& file, Netlist& netlist) {
    file.open("output.dot", std::ios::trunc);
    netlist.emit_dotfile(file, "top");
    file.close();
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main() {
    try {
        std::mt19937_64 rng;
        rng.seed(std::random_device{}());

        Library lib("src/lib.yaml", rng);
        Netlist netlist(lib, rng);

        std::ofstream dot("output.dot");
        dot.close();
        print_dot(dot, netlist);

        netlist.add_random_module();

        print_dot(dot, netlist); 
        netlist.add_external_net();

        print_dot(dot, netlist);

        for (int i = 0; i < 3; ++i) {
            netlist.add_undriven_net();
            print_dot(dot, netlist);
        }
        for (int i = 0; i < 3; ++i) {
            netlist.add_random_module();
            print_dot(dot, netlist);
        }
        for (int i = 0; i < 3; ++i) {
            netlist.drive_undriven_nets(1);
            print_dot(dot, netlist);
        }

        // netlist.switch_up();
        netlist.insert_output_buffers();
        print_dot(dot, netlist);
        netlist.print();

        std::ofstream verilog("output.v");
        netlist.emit_verilog(verilog, "top");

        print_dot(dot, netlist);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
