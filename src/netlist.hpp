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

    std::string get_name(int width = 0) const {
        if (width == 0) return name;
        if (width - static_cast<int>(std::log10(id)) - 1 < 0) throw std::invalid_argument("Width is too small for ID");
        return "_" + std::string(width - static_cast<int>(std::log10(id)) - 1, '0')
                   + std::to_string(id) + "_";
    }
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

    std::string get_name(int width = 0) const {
        if (width == 0) return spec->name;
        if (width - static_cast<int>(std::log10(id)) - 1 < 0) throw std::invalid_argument("Width is too small for ID");
        return "_" + std::string(width - static_cast<int>(std::log10(id)) - 1, '0')
                   + std::to_string(id) + "_";
    }
};

// -------------------------------------------------- Netlist
class Netlist {
    public:
        Netlist(Library& lib, std::optional<std::mt19937_64> rng_opt  = std::nullopt);
        ~Netlist();
    
        void    add_random_module();
        void    add_external_net();
        void    emit_verilog(std::ostream& os, const std::string& top_name = "top");
        void    print();
        Net*    make_net(std::string name = "");
        Net*    get_net(int id);

    private:
        void add_buffer(Net* net, const ModuleSpec* buffer);
        Module* make_module(const ModuleSpec* ms);
        Net* get_random_net(NetType net_type);
        void update_combinational_groups(std::set<int>& group);
        void insert_output_buffers();
        int  get_next_id() { return id_counter++; }
        int  id_width() const { return static_cast<int>(std::log10(id_counter)) + 1; }

        std::vector<std::unique_ptr<Module>> modules;
        std::vector<std::unique_ptr<Net>>    nets;
        std::vector<std::set<int>>           combinational_groups;
        Library& lib;
        std::mt19937_64 rng;
        int  id_counter {0};
};
