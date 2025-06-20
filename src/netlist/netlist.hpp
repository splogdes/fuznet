#pragma once

#include "library.hpp"
#include "module.hpp"

#include <cassert>
#include <cmath>
#include <memory>
#include <random>
#include <set>
#include <map>
#include <functional>
#include <vector>
#include <nlohmann/json.hpp>

using Id = std::size_t;

struct Port;
struct Module;

struct PortBit {
    Port* port{nullptr};
    int   bit{0};
    bool operator==(const PortBit& other) const {
        return port == other.port && bit == other.bit;
    }
};

struct Net {
    Id                   id;
    std::string          name;
    NetType              net_type{NetType::LOGIC};
    PortBit              driver;
    std::vector<PortBit> sinks;

    std::string lable(int width = 0) const;

    void add_sink   (Port* p, int b) { sinks.push_back(PortBit{p, b}); }
    void remove_sink(PortBit p);
};

struct Port {
    const PortSpec&         spec;
    Module*                 parent{nullptr};
    std::vector<Net*>       nets; 
    int                     width{1};
    NetType                 net_type;

    Port(const PortSpec& s, Module* m) : spec{s}, parent{m}, nets(s.width, nullptr), width(s.width), net_type{s.net_type} {
        assert(m && s.width > 0);
    };

    bool is_input () const { return spec.port_dir == PortDir::INPUT; }
    bool is_output() const { return spec.port_dir == PortDir::OUTPUT; }
};

struct Module {
    Id                                           id;
    const ModuleSpec&                            spec;
    std::vector<std::unique_ptr<Port>>           inputs;
    std::vector<std::unique_ptr<Port>>           outputs;
    std::map<Port*, std::set<Port*>>             seq_conns;
    std::map<std::string, std::string>           param_values;

    Module(Id id_, const ModuleSpec& ms, std::mt19937_64& rng);
    Port* get_input(const std::string& name); 
    std::string lable(int width = 0) const;
};

struct NetlistStats {
    int input_nets      = 0;
    int output_nets     = 0;
    int total_nets      = 0;
    int comb_modules    = 0;
    int seq_modules     = 0;
    int total_modules   = 0;
};

class Netlist {
public:
    Netlist(Library& lib, std::mt19937_64& rng);
    ~Netlist();

    void add_initial_nets();
    void add_random_module();
    void add_external_nets(size_t number = 1);
    void add_undriven_nets(NetType type = NetType::LOGIC, size_t number = 1);
    void drive_undriven_nets(double seq_mod_prob = 0.5, double seq_port_prob = 0.5, bool limit_to_one = false, NetType type = NetType::LOGIC);
    void buffer_unconnected_outputs();
    void remove_other_nets(const int& output_id);

    void emit_verilog(std::ostream& os, const std::string& top_name = "top") const;
    void emit_dotfile(std::ostream& os, const std::string& top_name = "top") const;

    nlohmann::json  json() const;
    void            load_from_json(const nlohmann::json& json_netlist);

    void print(bool only_stats = true) const;
    NetlistStats get_stats() const;
    
private:
    void          add_buffer(Net* net, const ModuleSpec& buffer);
    Net*          get_random_net(std::function<bool(const Net*)> filter = nullptr) const;
    Net*          make_net(NetType type, const std::string& name = "", int id = -1);
    std::set<int> get_combinational_group(Port* input_port, bool stop_at_seq = true) const;
    Module*       make_module(const ModuleSpec& ms, bool connect_random = true, int id = -1);

    int  get_next_id() { return id_counter++; }
    int  id_width() const { return static_cast<int>(std::log10(id_counter)) + 1; }
    Net* get_net(int id);

    std::vector<std::unique_ptr<Module>> modules;
    std::vector<std::unique_ptr<Net>>    nets;

    Library&                    lib;
    std::mt19937_64&            rng;
    int                         id_counter{1};
};
