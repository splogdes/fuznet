#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <random>

#include "orchestrator.hpp"
#include "reducer.hpp"

int main(int argc, char** argv) {
    try {
        CLI::App app{"Fuznet: netlist fuzzing"};

        std::string lib_cfg      = "hardware/xilinx/cells.yaml";
        std::string settings_cfg = "config/settings.toml";
        std::string hash_file    = "output/seen_netlists.txt";
        std::string out_prefix   = "output/output";
        std::string seed_str     = std::to_string(std::random_device{}());

        std::string json_netlist = "output/output_netlist.json";
        int         keep_only    = -1;

        bool animate    = false;
        bool verbose    = false;
        bool show_ver   = false;
        bool json_stats = false;
        bool last_success = false;


        app.add_option("-l,--lib",     lib_cfg,      "Cell library YAML");
        app.add_option("-s,--seed",    seed_str,     "Random seed");
        app.add_flag  ("-v,--verbose", verbose,      "Print chosen options");
        app.add_flag  ("--version",    show_ver,     "Show version");
        app.add_flag  ("-j,--json",    json_stats,   "Write JSON stats");

        app.fallthrough();

        auto generate_mode = app.add_subcommand("generate", "Generate a new netlist");
        generate_mode->add_flag  ("-a,--animate", animate,      "Write DOT after each step");
        generate_mode->add_option("-c,--config",  settings_cfg, "Settings TOML");
        generate_mode->add_option("-o,--output",  out_prefix,   "Output prefix");
        
        auto reducer_mode = app.add_subcommand("reduce", "Reduce netlist to a single output net");
        reducer_mode->add_option("-i,--input",     json_netlist, "Input JSON netlist")->required();
        reducer_mode->add_option("--hash-file",    hash_file, "File to store seen netlists hashes")->required();
        reducer_mode->add_option("-o,--output",    out_prefix,   "Output prefix");
        reducer_mode->add_option("-r,--keep-only", keep_only,    "Keep only this outout net and remove othets");
        reducer_mode->add_flag("--last-success",   last_success, "Flag if the last reduction iteration was a success");


        CLI11_PARSE(app, argc, argv);

        if (show_ver) {
            std::cout << "Fuznet 0.5.3\n";
            return 0;
        }

        if (verbose) {
            std::cout << "lib      : " << lib_cfg      << '\n'
                      << "seed     : " << seed_str     << '\n';
        }

        unsigned seed = std::stoul(seed_str);

        if (*generate_mode) {
            fuznet::Orchestrator orch(lib_cfg, settings_cfg, seed, verbose, animate, json_stats);
            orch.run(out_prefix);
        }

        if (*reducer_mode) {
            fuznet::Reducer reducer(lib_cfg, json_netlist, hash_file, seed, json_stats, verbose);
            fuznet::Result result = reducer.reduce(keep_only, last_success);
            reducer.write_outputs(out_prefix);
            
            switch (result) {
                case fuznet::Result::SUCCESS:
                    std::cout << "Reduction successful.\n";
                    return 0;
                case fuznet::Result::FAILURE:
                    std::cout << "Reduction failed.\n";
                    return 1;
                case fuznet::Result::ALREADY_SEEN:
                    std::cout << "Netlist already seen, skipping reduction.\n";
                    return 2;
                case fuznet::Result::NEW_HASH_ADDED:
                    std::cout << "New netlist hash added to the file.\n";
                    return 3;
            }
                
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
