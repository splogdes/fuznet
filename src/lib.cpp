#include "lib.hpp"
#include <yaml-cpp/yaml.h>

Library::Library(const std::string& filename, std::mt19937_64& r)
    : rng(std::move(r)) {
    YAML::Node root = YAML::LoadFile(filename);

    for (const auto& it : root) {
        const std::string name = it.first.as<std::string>();
        const YAML::Node& node = it.second;

        ModuleSpec m;
        m.name = name;

        for (const auto& port_entry : node["ports"]) {
            PortSpec p;
            p.name  = port_entry.first.as<std::string>();

            const YAML::Node& port = port_entry.second;

            const std::string dir  = port["dir"].as<std::string>();
            p.width                = port["width"].as<int>();
            const std::string type = port["type"].as<std::string>("logic");
            
            if      (type == "clk")      p.net_type = NetType::CLK;
            else if (type == "ext_clk")  p.net_type = NetType::EXT_CLK;
            else if (type == "ext_out")  p.net_type = NetType::EXT_OUT;
            else if (type == "ext_in")   p.net_type = NetType::EXT_IN;
            else if (type == "logic")    p.net_type = NetType::LOGIC;
            else throw std::runtime_error("Invalid net type: " + type);

            if (dir == "input") {
                p.port_dir = PortDir::INPUT;
                m.input_ports.push_back(p);
            } else if (dir == "output") {
                p.port_dir = PortDir::OUTPUT;
                m.output_ports.push_back(p);
            } else throw std::runtime_error("Invalid port direction: " + dir);
        }

        if (node["params"])
            for (const auto& param_entry : node["params"]) {
                ParamSpec p;
                p.name  = param_entry.first.as<std::string>();
                p.width = param_entry.second["width"].as<int>();
                m.params.push_back(p);
            }

        m.combinational = node["combinational"].as<bool>(true);

        for (const auto& res : node["resources"])
            m.resource[res.first.as<std::string>()] = res.second.as<int>();

        m.weight = node["weight"].as<int>(1);

        module_weights.push_back(m.weight);
        modules.emplace(name, m);
        module_names.push_back(name);
    }
}

const ModuleSpec& Library::get_module(const std::string& name) const {
    auto it = modules.find(name);
    if (it == modules.end()) throw std::runtime_error("Module not found: " + name);
    return it->second;
}

const ModuleSpec& Library::random_module() const {
    std::discrete_distribution<int> dist(module_weights.begin(), module_weights.end());
    return get_module(module_names[dist(rng)]);
}
