#include <yaml-cpp/yaml.h>
#include <iostream>

#include "library.hpp"

Library::Library(const std::string& filename, std::mt19937_64& rng)
    : rng(rng) {

    YAML::Node root = YAML::LoadFile(filename);

    for (const auto& node_pair : root) {
        const std::string module_name = node_pair.first.as<std::string>();
        const YAML::Node& module_node = node_pair.second;

        ModuleSpec module_spec;
        module_spec.name = module_name;

        for (const auto& port_pair : module_node["ports"]) {
            PortSpec port_spec;
            port_spec.name = port_pair.first.as<std::string>();

            const YAML::Node& port_node = port_pair.second;
            const std::string dir_str   = port_node["dir"].as<std::string>();
            const std::string type_str  = port_node["type"].as<std::string>("logic");

            port_spec.width = port_node["width"].as<int>(1);

            if      (type_str == "clk")      port_spec.net_type = NetType::CLK;
            else if (type_str == "ext_clk")  port_spec.net_type = NetType::EXT_CLK;
            else if (type_str == "ext_out")  port_spec.net_type = NetType::EXT_OUT;
            else if (type_str == "ext_in")   port_spec.net_type = NetType::EXT_IN;
            else if (type_str == "logic")    port_spec.net_type = NetType::LOGIC;
            // At the moment, we treat these as logic nets
            else if (type_str == "reset")    port_spec.net_type = NetType::LOGIC;
            else if (type_str == "set")      port_spec.net_type = NetType::LOGIC;
            else if (type_str == "enable")   port_spec.net_type = NetType::LOGIC;

            else throw std::runtime_error("Invalid port type: " + type_str);

            if (dir_str == "input") {
                port_spec.port_dir = PortDir::INPUT;
                module_spec.inputs.push_back(port_spec);
            } else if (dir_str == "output") {
                port_spec.port_dir = PortDir::OUTPUT;
                module_spec.outputs.push_back(port_spec);
                if (port_node["seq_inputs"]) {
                    module_spec.combinational = false;
                    const std::vector<std::string> seq_inputs =
                        port_node["seq_inputs"].as<std::vector<std::string>>();
                    for (const auto& seq_port_name : seq_inputs)
                        module_spec.seq_conns[port_spec.name].insert(seq_port_name);
                }

            } else {
                throw std::runtime_error("Invalid port direction: " + dir_str);
            }
        }

        if (module_node["params"])
            for (const auto& param_pair : module_node["params"]) {
                ParamSpec param_spec;
                param_spec.name  = param_pair.first.as<std::string>();
                param_spec.width = param_pair.second["width"].as<int>();
                module_spec.params.push_back(param_spec);
            }

        for (const auto& res_pair : module_node["resources"])
            module_spec.resource[res_pair.first.as<std::string>()] =
                res_pair.second.as<int>();

        module_spec.weight = module_node["weight"].as<int>(1);

        module_weights.push_back(module_spec.weight);
        modules.emplace(module_name, module_spec);
        module_names.push_back(module_name);
    }
}

const ModuleSpec& Library::get_module(const std::string& name) const {
    auto it = modules.find(name);
    if (it == modules.end()) throw std::runtime_error("Module not found: " + name);
    return it->second;
}

const ModuleSpec& Library::get_random_module(std::function<bool (const ModuleSpec& ms)> filter) const {
    if(!filter) {
        std::discrete_distribution<int> dist(module_weights.begin(), module_weights.end());
        return get_module(module_names[dist(rng)]);
    }

    std::vector<int>          weights;
    std::vector<std::string>  names;

    for (const auto& name : module_names) {
        const ModuleSpec& spec = get_module(name);
        if (filter(spec)) {
            weights.push_back(spec.weight);
            names.push_back(name);
        }
    }

    if (weights.empty())
        throw std::runtime_error("No modules for requested net type");

    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return get_module(names[dist(rng)]);
}

const ModuleSpec& Library::get_random_buffer(NetType input_type, NetType output_type) const {
    auto filter = [&](const ModuleSpec& ms) {
        return ms.inputs.size() == 1 &&
               ms.outputs.size() == 1 &&
               ms.inputs[0].net_type == input_type &&
               ms.outputs[0].net_type == output_type;
    };
    return get_random_module(filter);
}

void Library::print() const {
    std::cout << "Library contains " << modules.size() << " modules:\n";
    for (const auto& [name, spec] : modules) {
        std::cout << "  - " << name << ": " << spec.inputs.size() << " inputs, "
                  << spec.outputs.size() << " outputs, "
                  << spec.params.size() << " params, "
                  << "weight: " << spec.weight << "\n";
    }
}