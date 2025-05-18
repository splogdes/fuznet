#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class PortDir { INPUT, OUTPUT };
enum class NetType { EXT_CLK, CLK, EXT_IN, EXT_OUT, LOGIC };

struct PortSpec {
    std::string name;
    PortDir     port_dir;
    int         width;
    NetType     net_type;
};

struct ParamSpec {
    std::string name;
    int         width;
};

struct ModuleSpec {
    std::string                           name;
    std::vector<PortSpec>                 inputs;
    std::vector<PortSpec>                 outputs;
    std::vector<ParamSpec>                params;
    bool                                  combinational;
    int                                   weight;
    std::unordered_map<std::string, int>  resource;
};
