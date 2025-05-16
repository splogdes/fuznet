// ----------------------------------------------------------------- module.hpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cstdint>

// ---------- spec from library file ----------
enum class PortDir { INPUT, OUTPUT };
enum class NetType { CLK, EXT_IN, EXT_OUT, LOGIC };

struct PortSpec  { std::string name; PortDir port_dir; int width; NetType net_type; };
struct ParamSpec { std::string name; int width; };

struct ModuleSpec {
    std::string                       name;
    std::vector<PortSpec>             ports;
    std::vector<ParamSpec>            params;
    std::unordered_map<std::string,int> resource;   // e.g. {"lut":1}
};