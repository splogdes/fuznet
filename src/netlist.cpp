#include "netlist.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>

std::string Net::get_name(int width) const {
    if (!name.empty()) return name;
    if (width == 0) return "net_" + std::to_string(id);
    int digits = static_cast<int>(std::log10(id)) + 1;
    if (width < digits) throw std::invalid_argument("Width too small for ID");
    return "_" + std::string(width - digits, '0') + std::to_string(id) + "_";
}

void Net::remove_sink(Port* p) {
    auto it = std::remove(sinks.begin(), sinks.end(), p);
    sinks.erase(it, sinks.end());
}

Module::Module(Id id_, const ModuleSpec& ms, std::mt19937_64& rng) : id{id_}, spec{ms} {
    for (const auto& ps : ms.input_ports)
        input_ports.emplace_back(std::make_unique<Port>(ps, this));

    for (const auto& ps : ms.output_ports)
        output_ports.emplace_back(std::make_unique<Port>(ps, this));

    for (const auto& pspec : ms.params) {
        std::string value;
        value.reserve(pspec.width);
        for (int i = 0; i < pspec.width; ++i)
            value.push_back((rng() & 1) ? '1' : '0');
        param_values.emplace(pspec.name, std::move(value));
    }
}

std::string Module::get_name(int width) const {
    if (width == 0) return spec.name + "_" + std::to_string(id);
    int digits = static_cast<int>(std::log10(id)) + 1;
    if (width < digits) throw std::invalid_argument("Width too small for ID");
    return "_" + std::string(width - digits, '0') + std::to_string(id) + "_";
}

Netlist::Netlist(Library& lib, std::mt19937_64& rng) : lib{lib}, rng{std::move(rng)} {
    Net* input_net = make_net();
    Net* clock_net = make_net();

    input_net->net_type = NetType::EXT_IN;
    clock_net->net_type = NetType::EXT_CLK;
    clock_net->name     = "clk";

    add_buffer(input_net, lib.get_module("IBUF"));
    add_buffer(clock_net, lib.get_module("BUFG"));
}

Netlist::~Netlist() = default;

void Netlist::add_external_net() {
    Net* ext_in = make_net();
    ext_in->net_type = NetType::EXT_IN;
    add_buffer(ext_in, lib.get_module("IBUF"));
}

void Netlist::add_random_module() {
    const ModuleSpec& ms = lib.random_module();
    make_module(ms);
}

void Netlist::switch_up() {
    for (auto& mod : modules) {
        std::set<int> comb_nets = get_combinational_group(mod.get());
        std::vector<Net*> switchable;
        for (auto& net : nets)
            if (net->net_type == NetType::LOGIC && comb_nets.count(net->id) == 0)
                switchable.push_back(net.get());

        if (switchable.empty()) continue;

        std::uniform_int_distribution<std::size_t> dist(0, switchable.size() - 1);

        for (auto& port : mod->input_ports)
            if (port->net_type == NetType::LOGIC) {
                Net* new_net = switchable[dist(rng)];
                port->net->remove_sink(port.get());
                port->net = new_net;
                new_net->add_sink(port.get());
            }
    }
}

void Netlist::insert_output_buffers() {
    std::vector<Net*> targets;
    for (auto& net : nets)
        if (net->sinks.empty() && net->net_type == NetType::LOGIC)
            targets.push_back(net.get());
    for (Net* n : targets)
        add_buffer(n, lib.get_module("OBUF"));
}

void Netlist::add_buffer(Net* drive_net, const ModuleSpec& buf_spec) {
    if (buf_spec.input_ports.size() != 1 && buf_spec.output_ports.size() != 1)
        throw std::invalid_argument("Buffer must have exactly two ports");

    auto buf_mod = std::make_unique<Module>(modules.size(), buf_spec, rng);
    Module* m = buf_mod.get();
    m->id = get_next_id();
    modules.push_back(std::move(buf_mod));

    auto& p_in = m->input_ports[0];  
    p_in->net = drive_net;
    drive_net->add_sink(p_in.get());

    auto& p_out = m->output_ports[0];
    Net* out_net = make_net();
    out_net->net_type = p_out->net_type;
    p_out->net = out_net;
    out_net->driver = p_out.get();
}

Module* Netlist::make_module(const ModuleSpec& ms) {
    auto mod = std::make_unique<Module>(modules.size(), ms, rng);
    Module* m = mod.get();
    m->id = get_next_id();
    modules.push_back(std::move(mod));

    std::vector<Port*> outputs;

    for (const auto& p : m->input_ports) {
        Net* src = get_random_net(p->net_type);
        p->net = src;
        src->add_sink(p.get());
    }

    for (const auto& p : m->output_ports) {
        Net* net = make_net();
        net->net_type = p->net_type;
        p->net = net;
        net->driver = p.get();
    }

    return m;
}

Net* Netlist::get_random_net(NetType t) {
    std::vector<Net*> pool;
    for (auto& net : nets)
        if (net->net_type == t) pool.push_back(net.get());
    if (pool.empty()) throw std::runtime_error("No nets of requested type");
    std::uniform_int_distribution<std::size_t> dist(0, pool.size() - 1);
    return pool[dist(rng)];
}

std::set<int> Netlist::get_combinational_group(Module* seed) {
    std::set<int> group;
    std::queue<Net*> q;

    for (auto& p : seed->output_ports)
        if (p->net_type != NetType::CLK)
            q.push(p->net);

    while (!q.empty()) {
        Net* cur = q.front();
        q.pop();
        if (!group.insert(cur->id).second) continue;
        for (Port* sink : cur->sinks) {
            if (!sink->parent || !sink->parent->spec.combinational) continue;
            for (auto& p : sink->parent->output_ports)
                if (p->net_type != NetType::CLK)
                    q.push(p->net);
        }
    }
    return group;
}

Net* Netlist::make_net(std::string explicit_name) {
    auto n = std::make_unique<Net>();
    n->id = get_next_id();
    n->name = std::move(explicit_name);
    Net* ptr = n.get();
    nets.push_back(std::move(n));
    return ptr;
}

Net* Netlist::get_net(int id) {
    for (auto& net : nets)
        if (net->id == id) return net.get();
    throw std::runtime_error("Net not found");
}

void Netlist::emit_verilog(std::ostream& os, const std::string& top_name) const {
    std::vector<const Net*> ext_in, ext_out;
    for (const auto& n : nets) {
        if (n->net_type == NetType::EXT_IN || n->net_type == NetType::EXT_CLK)
            ext_in.push_back(n.get());
        else if (n->net_type == NetType::EXT_OUT)
            ext_out.push_back(n.get());
    }

    int w = id_width();

    os << "module " << top_name << "(";
    for (size_t i = 0; i < ext_in.size(); ++i)
        os << ext_in[i]->get_name(w) << (i + 1 < ext_in.size() ? ", " : "");
    if (!ext_out.empty()) os << ", ";
    for (size_t i = 0; i < ext_out.size(); ++i)
        os << ext_out[i]->get_name(w) << (i + 1 < ext_out.size() ? ", " : "");
    os << ");\n";

    for (auto* n : ext_in)  os << "  input  " << n->get_name(w) << ";\n";
    for (auto* n : ext_out) os << "  output " << n->get_name(w) << ";\n";

    for (const auto& n : nets)
        if (n->net_type == NetType::LOGIC)
            os << "  wire   " << n->get_name(w) << ";\n";

    for (const auto& m : modules) {
        if (!m->param_values.empty()) {
            os << "  " << m->spec.name << " #(\n";
            for (size_t i = 0; i < m->spec.params.size(); ++i) {
                const auto& ps = m->spec.params[i];
                os << "    ." << ps.name << "(" << ps.width << "'b" << m->param_values.at(ps.name) << ")";
                if (i + 1 < m->spec.params.size()) os << ",";
                os << "\n";
            }
            os << "  ) ";
        } else {
            os << "  " << m->spec.name << " ";
        }

        os << m->get_name(w) << " (\n";

        std::vector<Port*> all_ports;
        for (const auto& p : m->input_ports) all_ports.push_back(p.get());
        for (const auto& p : m->output_ports) all_ports.push_back(p.get());
        
        for (size_t i = 0; i < all_ports.size(); ++i) {
            const Port* p = all_ports[i];
            os << "    ." << p->spec.name << "(" << p->net->get_name(w) << ")";
            if (i + 1 < all_ports.size()) os << ",";
            os << "\n";
        }
        os << "  );\n";
    }
    os << "endmodule\n";
}

void Netlist::emit_dotfile(std::ostream& os, const std::string& top_name) const {
    os << "digraph " << top_name << " {\n";
    os << "  rankdir=LR;\n";

    for (const auto& m : modules) {
        std::vector<std::string> ins, outs;
        for (const auto& p : m->input_ports) ins.push_back(p->spec.name);
        for (const auto& p : m->output_ports) outs.push_back(p->spec.name);

        os << "  " << m->get_name() << " [shape=record, label=\"{{";
        for (size_t i = 0; i < ins.size(); ++i) {
            os << "<" << ins[i] << "> " << ins[i];
            if (i + 1 < ins.size()) os << " | ";
        }
        os << "} | " << m->spec.name << " | {";
        for (size_t i = 0; i < outs.size(); ++i) {
            os << "<" << outs[i] << "> " << outs[i];
            if (i + 1 < outs.size()) os << " | ";
        }
        os << "}}\"];\n";
    }

    for (const auto& n : nets) {
        const std::string n_label = n->get_name();
        auto emit_edge = [&](const std::string& src, const std::string& dst) {
            os << "  " << src << " -> " << dst << ";\n";
        };

        switch (n->net_type) {
            case NetType::EXT_IN:
            case NetType::EXT_CLK:
                os << "  " << n_label << " [shape=octagon, label=\"" << n_label << "\"];\n";
                for (Port* s : n->sinks)
                    emit_edge(n_label, s->parent->get_name() + ":<" + s->spec.name + ">");
                break;

            case NetType::EXT_OUT:
                os << "  " << n_label << " [shape=octagon, label=\"" << n_label << "\"];\n";
                emit_edge(n->driver->parent->get_name() + ":<" + n->driver->spec.name + ">", n_label);
                break;

            default:
                os << "  " << n_label << " [shape=diamond, label=\"" << n_label << "\"];\n";
                emit_edge(n->driver->parent->get_name() + ":<" + n->driver->spec.name + ">", n_label + ":w");
                for (Port* s : n->sinks)
                    emit_edge(n_label + ":e", s->parent->get_name() + ":<" + s->spec.name + ">");
                break;
        }
    }
    os << "}\n";
}

void Netlist::print() const {
    std::cout << "Netlist:\n"
              << "  modules : " << modules.size() << '\n'
              << "  nets    : " << nets.size() << '\n';

    for (const auto& m : modules) {
        std::cout << "Module #" << m->id << " (" << m->spec.name << ")\n";
        for (const auto& p : m->input_ports)
            std::cout << "    in  "
                      << std::setw(10) << p->spec.name
                      << "  net " << std::setw(3) << p->net->id
                      << " (" << static_cast<int>(p->net_type) << ")\n";
        for (const auto& p : m->output_ports)
            std::cout << "    out "
                      << std::setw(10) << p->spec.name
                      << "  net " << std::setw(3) << p->net->id
                      << " (" << static_cast<int>(p->net_type) << ")\n";
    }

    for (const auto& g : combinational_groups) {
        std::cout << "  comb group:";
        for (int id : g) std::cout << ' ' << id;
        std::cout << '\n';
    }
}
