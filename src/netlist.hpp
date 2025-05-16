// -------------------------------------------------- netlist.hpp
#pragma once
#include "module.hpp"
#include "lib.hpp"
#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <stdexcept>
#include <unordered_map>
#include <random>
#include <optional>
#include <set>
#include <iostream>

using Id = std::size_t;

struct Port;
struct Module;

// -------------------------------------------------- Net
struct Net {
    Id                id;
    std::string       name;
    NetType           net_type {NetType::LOGIC};
    Port*             driver {nullptr};            
    std::vector<Port*> sinks;

    void add_sink(Port* p) { assert(p); sinks.push_back(p); }
};

// -------------------------------------------------- Port
struct Port {
    const PortSpec* spec;
    Module*     parent {nullptr};
    Net*        net    {nullptr};
    NetType     net_type;

    Port(const PortSpec* s, Module* mod)
        : spec{s}, parent{mod}, net_type{s->net_type} {}

    bool is_input()  const { return spec->port_dir == PortDir::INPUT;  }
    bool is_output() const { return spec->port_dir == PortDir::OUTPUT; }
};

// -------------------------------------------------- Module
struct Module {
    Id                  id;
    const ModuleSpec*   spec;
    std::vector<std::unique_ptr<Port>> ports;
    std::unordered_map<std::string, std::string> param_values;

    explicit Module(Id id_, const ModuleSpec* ms, std::mt19937_64* rng = nullptr) : id{id_}, spec{ms} {
        for (auto& ps : ms->ports) {
            auto p = std::make_unique<Port>(&ps, this);
            ports.emplace_back(std::move(p));
        }
        if (rng) {
            for (const auto& pspec : ms->params) {
                std::string value;
                for (int i = 0; i < pspec.width; ++i) {
                    value += ((*rng)() % 2) ? '1' : '0';
                    param_values[pspec.name] = value;
                }
            }
        }
    }
};

// -------------------------------------------------- Netlist
class Netlist {
    public:
        Netlist(Library& lib, std::optional<std::mt19937_64> rng_opt  = std::nullopt);
        ~Netlist();
    
        void    add_random_module();
        void    add_external_net();
        void    emit_verilog(std::ostream& os, const std::string& top_name = "top", bool include_names = false);
        void    print();
        Net*    make_net(std::string_view name = "");
        Net*    get_net(int id);

    private:
        void add_buffer(Net* net, const ModuleSpec* buffer);
        Module* make_module(const ModuleSpec* ms);
        Net* get_random_net(NetType net_type);
        void update_combinational_groups(std::set<int>& group);
        void insert_output_buffers();
        
        std::vector<std::unique_ptr<Module>> modules;
        std::vector<std::unique_ptr<Net>>    nets;
        std::vector<std::set<int>>           combinational_groups;
        Library& lib;
        std::mt19937_64 rng;
};
