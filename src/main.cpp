#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <random>

#include "orchestrator.hpp"

int main(int argc, char** argv) {
    try {
        CLI::App app{"Fuznet: netlist fuzzing"};

        std::string lib_cfg      = "hardware/cells/xilinx.yaml";
        std::string settings_cfg = "config/settings.toml";
        std::string out_prefix   = "output/output";
        std::string seed_str     = std::to_string(std::random_device{}());

        bool animate = false;
        bool verbose = false;
        bool show_ver = false;

        app.add_option("-l,--lib",     lib_cfg,      "Cell library YAML");
        app.add_option("-c,--config",  settings_cfg, "Settings TOML");
        app.add_option("-s,--seed",    seed_str,     "Random seed");
        app.add_option("-o,--output",  out_prefix,   "Output prefix");
        app.add_flag  ("-a,--animate", animate,      "Write DOT after each step");
        app.add_flag  ("-v,--verbose", verbose,      "Print chosen options");
        app.add_flag  ("--version",    show_ver,     "Show version");

        CLI11_PARSE(app, argc, argv);

        if (show_ver) {
            std::cout << "Fuznet 0.1.0\n";
            return 0;
        }

        if (verbose) {
            std::cout << "lib      : " << lib_cfg      << '\n'
                      << "settings : " << settings_cfg << '\n'
                      << "seed     : " << seed_str     << '\n';
        }

        unsigned seed = std::stoul(seed_str);

        fuznet::Orchestrator orch(lib_cfg, settings_cfg, seed, verbose);
        orch.run(out_prefix, animate);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
