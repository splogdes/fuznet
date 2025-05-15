#include <yaml-cpp/yaml.h>
#include <iostream>
#include "lib.hpp"

Library load_library(const std::string& filename) {
    
    YAML::Node root = YAML::LoadFile(filename);
    Library lib;
    
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

        for (const auto& resource : node["resources"])
            m.resource[resource.first.as<std::string>()] = resource.second.as<int>();

        lib.modules[name] = m;
    }
    return lib;
}