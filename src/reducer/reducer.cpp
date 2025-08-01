#include <iostream>
#include <fstream>

#include "reducer.hpp"


namespace fuznet {

Reducer::Reducer(const std::string& lib_yaml,
                 const std::string& input_json,
                 const std::string& hash_file,
                 unsigned seed,
                 bool json_stats,
                 bool verbose)
     : hash_file(hash_file),
       rng(seed),
       library(lib_yaml, rng), 
       netlist(library, rng),
       json_stats(json_stats),
       verbose(verbose)
{
    std::ifstream json_file(input_json);
    if (!json_file.is_open()) {
        std::cerr << "Error: Could not open input JSON file: " << input_json << "\n";
        throw std::runtime_error("Failed to open input JSON file");
    }
    
    json_file >> json_data;
    json_file.close();
}

Result Reducer::reduce(const int& output_id, bool success) {
    if (verbose)
        std::cout << "Starting reduction process.\n";

    int iterations = json_data.value("iterations", 0);
    json_data["iterations"] = iterations + 1;

    if (iterations == 0)
        json_data["old"] = json_data["new"];

    if (output_id >= 0 && iterations == 0) {
        keep_only_net(output_id);
        return Result::SUCCESS;
    }

    if (verbose)
        std::cout << "Iterative reduction started with last success: " << success << "\n";
    
    return iterative_reduce(success);
}
    
void Reducer::keep_only_net(const int& output_id) {
    if (verbose)
        std::cout << "Reducing netlist to keep only net with ID: " << output_id << "\n";

    netlist.load_from_json(json_data["new"]);

    if (verbose) {
        std::cout << "Netlist has:" << "\n";
        netlist.print();
    }

    netlist.remove_other_nets(output_id);
    
    if (verbose) {
        std::cout << "After removing other nets, the netlist has:\n";
        netlist.print();
    }

    json_data["new"] = netlist.json();
}

Result Reducer::iterative_reduce(bool success) {
    if (verbose)
        std::cout << "Starting iterative reduction of the netlist.\n";

    if (success) {
        if (verbose)
            std::cout << "Reducer initialized with last reduction success.\n";
        netlist.load_from_json(json_data["new"]);
        json_data["old"] = json_data["new"];
    } else {
        if (verbose)
            std::cout << "Reducer initialized with last reduction failure.\n";
        netlist.load_from_json(json_data["old"]);
    }

    if (verbose) {
        std::cout << "Netlist has:" << "\n";
        netlist.print();
    }

    std::set<int> tried_to_remove_net_ids = json_data.value(
        "tried_to_remove_net_ids", nlohmann::json::array()
    ).get<std::set<int>>();

    int removed_id = netlist.remove_random_module(
        [&](const Module* mod) {
            return !tried_to_remove_net_ids.contains(mod->id) && !mod->is_buffer();
        }
    );

    if (removed_id < 0) {
        if (verbose)
            std::cout << "No more modules to remove.\n";
        return check_hash();
    }

    if (verbose)
        std::cout << "Removed module with ID: " << removed_id << "\n";

    tried_to_remove_net_ids.insert(removed_id);
    netlist.remove_duplicate_outputs();
    netlist.remove_input_output_chains();

    json_data["new"] = netlist.json();

    json_data["tried_to_remove_net_ids"] = nlohmann::json::array();
    for (const auto& id : tried_to_remove_net_ids)
        json_data["tried_to_remove_net_ids"].push_back(id);

    return Result::SUCCESS;
}

void Reducer::write_outputs(const std::string& output) const {
    if (verbose)
        std::cout << "Dumping netlist file to prefix: " << output << "\n";

    std::ofstream json_file(output + ".json", std::ios::trunc);
    json_file << std::setw(4) << json_data << std::endl;
    json_file.close();
    
    int iterations = json_data.value("iterations", 0);
    std::ofstream dot_file(output + "_iter" + std::to_string(iterations) + ".dot");
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

    std::ofstream json_stats(output + "_stats.json");
    json_stats << std::setw(4) << json_data << std::endl;
    json_stats.close();
    
    if (verbose)
        std::cout << "Netlist stats written to: " << output << "_stats.json\n";
}

Result Reducer::check_hash() const {
    if (verbose)
        std::cout << "Checking hash for the current netlist.\n";

    if (!std::filesystem::exists(hash_file)) {
        std::ofstream new_hash_file(hash_file);
        if (!new_hash_file.is_open()) {
            std::cerr << "Error: Could not create hash file: " << hash_file << "\n";
            return Result::FAILURE;
        }
        new_hash_file.close();
    }
    
    std::ifstream hash_file_stream(hash_file);
    if (!hash_file_stream.is_open()) {
        std::cerr << "Error: Could not open hash file: " << hash_file << "\n";
        return Result::FAILURE;
    }

    std::set<std::string> seen_hashes;
    std::string line;
    while (std::getline(hash_file_stream, line)) {
        seen_hashes.insert(line);
    }
    hash_file_stream.close();

    int fingerprint = netlist.get_fingerprint();
    std::string current_hash = std::to_string(fingerprint);

    std::cout << "Current netlist fingerprint: " << current_hash << "\n";

    if (seen_hashes.contains(current_hash)) {
        std::cout << "Netlist already seen\n";
        return Result::ALREADY_SEEN;
    }

    std::ofstream hash_file_out(hash_file, std::ios::app);
    if (!hash_file_out.is_open()) {
        std::cerr << "Error: Could not open hash file for writing: " << hash_file << "\n";
        return Result::FAILURE;
    }
    hash_file_out << current_hash << "\n";
    hash_file_out.close();

    std::cout << "New netlist hash added to the file.\n";
    return Result::NEW_HASH_ADDED;
}

}