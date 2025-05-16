#include "lib.hpp"
#include <yaml-cpp/yaml.h>

Library::Library(const std::string& filename, std::optional<std::mt19937_64> rng_opt) {
    
    if (rng_opt) {
        this->rng = *rng_opt;
    } else {
        std::random_device rd;
        this->rng = std::mt19937_64(rd());
    }

    YAML::Node root = YAML::LoadFile(filename);
    
    for (const auto& it : root) {
        std::string name = it.first.as<std::string>();
        const YAML::Node& node = it.second;

        ModuleSpec m;
        m.name = name;

        for (const auto& port_entry : node["ports"]) {

            PortSpec p;

            std::string port_name = port_entry.first.as<std::string>();

            const YAML::Node& port = port_entry.second;

            std::string port_dir = port["dir"].as<std::string>();
            int port_width = port["width"].as<int>();
            std::string port_net_type = port["type"].as<std::string>("logic");

            p.name = port_name;
            p.width = port_width;

            switch (port_dir[0]) {
                case 'i': p.port_dir = PortDir::INPUT; break;
                case 'o': p.port_dir = PortDir::OUTPUT; break;
                default: throw std::runtime_error(
                    "Invalid port direction, Module: " + name + ", Port: " + port_name + ", Direction: " + port_dir
                );
            }

            switch (port_net_type[0]) {
                case 'c': p.net_type = NetType::CLK; break;
                case 'e': p.net_type = NetType::EXT_CLK; break;
                case 'o': p.net_type = NetType::EXT_OUT; break;
                case 'i': p.net_type = NetType::EXT_IN; break;
                case 'l': p.net_type = NetType::LOGIC; break;
                default: throw std::runtime_error(
                    "Invalid net type, Module: " + name + ", Port: " + port_name + ", Type: " + port_net_type
                );
            }

            m.ports.push_back(p);

        }

        for (const auto& param_entry : node["params"]) {
            ParamSpec p;

            const std::string& param_name = param_entry.first.as<std::string>();

            const YAML::Node& param = param_entry.second;

            const int param_width = param["width"].as<int>();

            p.name = param_name;
            p.width = param_width;

            m.params.push_back(p);
        }

        m.combinational = node["combinational"].as<bool>(true);

        for (const auto& resource : node["resources"])
            m.resource[resource.first.as<std::string>()] = resource.second.as<int>();

        m.weight = node["weight"].as<int>(1);
        
        this->module_weights.push_back(m.weight);
        this->modules[name] = m;
        this->module_names.push_back(name);
    }
}

ModuleSpec* Library::get_module(const std::string& name) {
    auto it = modules.find(name);
    if (it != modules.end()) {
        return &it->second;
    } else {
        throw std::runtime_error("Module not found: " + name);
    }
}

ModuleSpec* Library::random_module() {
    std::discrete_distribution<int> dist(module_weights.begin(), module_weights.end());
    int idx = dist(rng);
    std::string name = module_names[idx];
    auto it = modules.find(name);
    if (it != modules.end()) {
        return &it->second;
    } else {
        throw std::runtime_error("Module not found: " + name);
    }
}

#ifdef LIBRARY_TEST
#include <iostream>

int main() {
    try {
        std::optional<std::mt19937_64> rng_opt = std::nullopt;
        Library lib("src/lib.yaml");
        
        ModuleSpec* module = lib.random_module();

        std::cout << "Random module name: " << module->name << std::endl;

        for (const auto& port : module->ports) {
            std::cout << "Port name: " << port.name 
                      << ", Direction: " 
                      << (port.port_dir == PortDir::INPUT ? "INPUT" : "OUTPUT") 
                      << ", Width: " << port.width 
                      << ", Type: " 
                      << (port.net_type == NetType::CLK ? "CLK" : 
                          (port.net_type == NetType::EXT_IN ? "EXT_IN" : 
                          (port.net_type == NetType::EXT_OUT ? "EXT_OUT" : "LOGIC")))
                      << std::endl;
        }

        for (const auto& param : module->params) {
            std::cout << "Param name: " << param.name
                      << ", Width: " << param.width 
                      << std::endl;
        }

        for (const auto& resource : module->resource) {
            std::cout << "Resource name: " << resource.first 
                      << ", Count: " << resource.second 
                      << std::endl;
        }


        std::cout << "Combinational: " << (module->combinational ? "true" : "false") << std::endl;
        std::cout << "Weight: " << module->weight << std::endl;

    } catch (const std::exception& e) { 
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
#endif