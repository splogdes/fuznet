#include "netlist.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>

std::string Net::lable(int width) const {
    if (!name.empty()) return name;
    if (width == 0) return "net_" + std::to_string(id);
    int digit_count = static_cast<int>(std::log10(id)) + 1;
    if (width < digit_count) throw std::invalid_argument("Width too small for ID");
    return "_" + std::string(width - digit_count, '0') + std::to_string(id) + "_";
}

void Net::remove_sink(Port* port) {
    auto it = std::remove(sinks.begin(), sinks.end(), port);
    sinks.erase(it, sinks.end());
}

Module::Module(Id module_id, const ModuleSpec& spec_ref, std::mt19937_64& rng)
    : id{module_id}, spec{spec_ref} {

    for (const auto& port_spec : spec_ref.inputs)
        inputs.emplace_back(std::make_unique<Port>(port_spec, this));

    for (const auto& port_spec : spec_ref.outputs)
        outputs.emplace_back(std::make_unique<Port>(port_spec, this));

    for (const auto& param_spec : spec_ref.params) {
        std::string value;
        value.reserve(param_spec.width);
        for (int i = 0; i < param_spec.width; ++i)
            value.push_back((rng() & 1) ? '1' : '0');
        param_values.emplace(param_spec.name, std::move(value));
    }
}

std::string Module::lable(int width) const {
    if (width == 0) return spec.name + "_" + std::to_string(id);
    int digit_count = static_cast<int>(std::log10(id)) + 1;
    if (width < digit_count) throw std::invalid_argument("Width too small for ID");
    return "_" + std::string(width - digit_count, '0') + std::to_string(id) + "_";
}

Netlist::Netlist(Library& library_ref, std::mt19937_64& rng)
    : lib{library_ref}, rng{rng} {

    Net* input_net = make_net();
    Net* clock_net = make_net();

    input_net->net_type = NetType::EXT_IN;
    clock_net->net_type = NetType::EXT_CLK;
    clock_net->name     = "clk";

    add_buffer(input_net, lib.get_module("IBUF"));
    add_buffer(clock_net, lib.get_module("BUFG"));
}

Netlist::~Netlist() = default;

void Netlist::add_external_nets(size_t number) {
    for (size_t i = 0; i < number; ++i) {
        Net* ext_net = make_net();
        ext_net->net_type = NetType::EXT_IN;
        add_buffer(ext_net, lib.get_module("IBUF"));
    }
}

void Netlist::add_random_module() {
    const ModuleSpec& spec_ref = lib.get_random_module();
    make_module(spec_ref);
}

void Netlist::add_undriven_nets(NetType type, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        Net* new_net = make_net();
        new_net->net_type = type;
    }
}

void Netlist::drive_undriven_nets(double seq_probability, bool limit_to_one, NetType type) {
    for (auto& net_ptr : nets) {
        if (net_ptr->driver || net_ptr->net_type != type) continue;

        std::uniform_real_distribution<double> dist(0.0, 1.0);
        bool sequential = dist(rng) < seq_probability;

        auto module_filter = [&](const ModuleSpec& ms) {
            return ms.outputs.size() == 1 &&
                   ms.outputs[0].net_type == type &&
                   !(sequential && ms.combinational);
        };

        const ModuleSpec& driver_spec = lib.get_random_module(module_filter);

        Module* driver_module = make_module(driver_spec, false);
        Port*   driver_port   = driver_module->outputs[0].get();

        driver_port->net = net_ptr.get();
        net_ptr->driver  = driver_port;

        std::set<int> comb_group = get_combinational_group(driver_module, false);

        for (auto& input_port : driver_module->inputs) {

            auto same_type = [&](const Net* n) {return n->net_type == input_port->net_type; };

            if (input_port->net_type == NetType::LOGIC) {
                Net* source = sequential
                                ?  get_random_net(
                                    [&](const Net* n) { return comb_group.contains(n->id) && same_type(n); }
                                ): get_random_net(
                                    [&](const Net* n) { return !comb_group.contains(n->id) && same_type(n); }
                                );
                input_port->net = source;
                source->add_sink(input_port.get());
            } else {
                Net* source = get_random_net(same_type);
                input_port->net = source;
                source->add_sink(input_port.get());
            }
        }

        if (limit_to_one) break;
    }
}

void Netlist::switch_up() {
    for (auto& module_ptr : modules) {
        std::set<int> comb_group = get_combinational_group(module_ptr.get(), true);

        std::vector<Net*> swappable;
        for (auto& net_ptr : nets)
            if (net_ptr->net_type == NetType::LOGIC &&
                !comb_group.contains(net_ptr->id))
                swappable.push_back(net_ptr.get());

        if (swappable.empty()) continue;

        std::uniform_int_distribution<std::size_t> dist(0, swappable.size() - 1);

        for (auto& input_port : module_ptr->inputs)
            if (input_port->net_type == NetType::LOGIC) {
                Net* new_net = swappable[dist(rng)];
                input_port->net->remove_sink(input_port.get());
                input_port->net = new_net;
                new_net->add_sink(input_port.get());
            }
    }
}

void Netlist::buffer_unconnected_outputs() {
    std::vector<Net*> logic_nets_without_sinks;
    for (auto& net_ptr : nets)
        if (net_ptr->sinks.empty() && net_ptr->net_type == NetType::LOGIC)
            logic_nets_without_sinks.push_back(net_ptr.get());

    for (Net* net_ptr : logic_nets_without_sinks)
        add_buffer(net_ptr, lib.get_module("OBUF"));
}

void Netlist::add_buffer(Net* drive_net, const ModuleSpec& buffer_spec) {
    if (buffer_spec.inputs.size() != 1 ||
        buffer_spec.outputs.size() != 1)
        throw std::invalid_argument("Buffer must have one input and one output");

    auto buffer_module = std::make_unique<Module>(modules.size(), buffer_spec, rng);
    Module* module_ptr = buffer_module.get();
    module_ptr->id = get_next_id();
    modules.push_back(std::move(buffer_module));

    Port* input_port  = module_ptr->inputs[0].get();
    Port* output_port = module_ptr->outputs[0].get();

    input_port->net = drive_net;
    drive_net->add_sink(input_port);

    Net* new_net = make_net();
    new_net->net_type = output_port->net_type;
    output_port->net  = new_net;
    new_net->driver   = output_port;
}

Module* Netlist::make_module(const ModuleSpec& spec_ref, bool connect_random) {
    auto module_obj = std::make_unique<Module>(get_next_id(), spec_ref, rng);
    Module* module_ptr = module_obj.get();
    modules.push_back(std::move(module_obj));

    if (!connect_random) return module_ptr;

    for (auto& input_port : module_ptr->inputs) {
        Net* source = get_random_net(
            [&](const Net* n) { return n->net_type == input_port->net_type; }
        );
        input_port->net = source;
        source->add_sink(input_port.get());
    }

    for (auto& output_port : module_ptr->outputs) {
        Net* dest = make_net();
        dest->net_type = output_port->net_type;
        output_port->net = dest;
        dest->driver = output_port.get();
    }
    return module_ptr;
}

Net* Netlist::get_random_net(std::function<bool(const Net*)> filter) const {
    std::vector<Net*> candidates;
    for (auto& net_ptr : nets)
        if (!filter || filter(net_ptr.get()))
            candidates.push_back(net_ptr.get());

    if (candidates.empty())
        throw std::runtime_error("No nets of requested type");

    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rng)];
}

std::set<int> Netlist::get_combinational_group(Module* seed, bool stop_at_seq) const {
    std::set<int> visited;
    std::queue<Net*> bfs_queue;

    for (auto& out_port : seed->outputs)
        if (out_port->net_type != NetType::CLK)
            bfs_queue.push(out_port->net);

    while (!bfs_queue.empty()) {
        Net* current = bfs_queue.front();
        bfs_queue.pop();
        if (!visited.insert(current->id).second) continue;

        for (Port* sink_port : current->sinks) {
            if (!sink_port->parent ||
                (stop_at_seq && !sink_port->parent->spec.combinational))
                continue;
            for (auto& next_out : sink_port->parent->outputs)
                if (next_out->net_type != NetType::CLK)
                    bfs_queue.push(next_out->net);
        }
    }
    return visited;
}

Net* Netlist::make_net(std::string explicit_name) {
    auto net_obj = std::make_unique<Net>();
    net_obj->id   = get_next_id();
    net_obj->name = std::move(explicit_name);
    Net* net_ptr  = net_obj.get();
    nets.push_back(std::move(net_obj));
    return net_ptr;
}

Net* Netlist::get_net(int id) {
    for (auto& net_ptr : nets)
        if (net_ptr->id == id) return net_ptr.get();
    throw std::runtime_error("Net not found");
}

void Netlist::emit_verilog(std::ostream& os, const std::string& top_name) const {
    std::vector<const Net*> top_inputs;
    std::vector<const Net*> top_outputs;

    for (const auto& net_ptr : nets) {
        if (net_ptr->net_type == NetType::EXT_IN ||
            net_ptr->net_type == NetType::EXT_CLK)
            top_inputs.push_back(net_ptr.get());
        else if (net_ptr->net_type == NetType::EXT_OUT)
            top_outputs.push_back(net_ptr.get());
    }

    int width = id_width();

    os << "module " << top_name << "(";
    for (size_t i = 0; i < top_inputs.size(); ++i)
        os << top_inputs[i]->lable(width)
           << (i + 1 < top_inputs.size() ? ", " : "");
    if (!top_outputs.empty()) os << ", ";
    for (size_t i = 0; i < top_outputs.size(); ++i)
        os << top_outputs[i]->lable(width)
           << (i + 1 < top_outputs.size() ? ", " : "");
    os << ");\n";

    for (auto* net_ptr : top_inputs)
        os << "  input  " << net_ptr->lable(width) << ";\n";
    for (auto* net_ptr : top_outputs)
        os << "  output " << net_ptr->lable(width) << ";\n";

    for (const auto& net_ptr : nets)
        if (net_ptr->net_type == NetType::LOGIC)
            os << "  wire   " << net_ptr->lable(width) << ";\n";

    for (const auto& module_ptr : modules) {
        if (!module_ptr->param_values.empty()) {
            os << "  " << module_ptr->spec.name << " #(\n";
            for (size_t i = 0; i < module_ptr->spec.params.size(); ++i) {
                const auto& param_spec = module_ptr->spec.params[i];
                os << "    ." << param_spec.name << "("
                   << param_spec.width << "'b" << module_ptr->param_values.at(param_spec.name) << ")";
                if (i + 1 < module_ptr->spec.params.size()) os << ",";
                os << "\n";
            }
            os << "  ) ";
        } else {
            os << "  " << module_ptr->spec.name << " ";
        }

        os << module_ptr->lable(width) << " (\n";

        std::vector<Port*> ordered_ports;
        for (const auto& port_ptr : module_ptr->inputs)  ordered_ports.push_back(port_ptr.get());
        for (const auto& port_ptr : module_ptr->outputs) ordered_ports.push_back(port_ptr.get());

        for (size_t i = 0; i < ordered_ports.size(); ++i) {
            const Port* port_ref = ordered_ports[i];
            os << "    ." << port_ref->spec.name << "("
               << port_ref->net->lable(width) << ")";
            if (i + 1 < ordered_ports.size()) os << ",";
            os << "\n";
        }
        os << "  );\n";
    }
    os << "endmodule\n";
}

void Netlist::emit_dotfile(std::ostream& os, const std::string& top_name) const {
    os << "digraph " << top_name << " {\n";
    os << "  rankdir=LR;\n";

    for (const auto& module_ptr : modules) {
        std::vector<std::string> input_labels;
        std::vector<std::string> output_labels;

        for (const auto& port_ptr : module_ptr->inputs)
            input_labels.push_back(port_ptr->spec.name);
        for (const auto& port_ptr : module_ptr->outputs)
            output_labels.push_back(port_ptr->spec.name);

        os << "  " << module_ptr->lable() << " [shape=record, label=\"{{";
        for (size_t i = 0; i < input_labels.size(); ++i) {
            os << "<" << input_labels[i] << "> " << input_labels[i];
            if (i + 1 < input_labels.size()) os << " | ";
        }
        os << "} | " << module_ptr->spec.name << " | {";
        for (size_t i = 0; i < output_labels.size(); ++i) {
            os << "<" << output_labels[i] << "> " << output_labels[i];
            if (i + 1 < output_labels.size()) os << " | ";
        }
        os << "}}\"];\n";
    }

    for (const auto& net_ptr : nets) {
        std::string label = net_ptr->lable();
        auto edge = [&](const std::string& src, const std::string& dst) {
            os << "  " << src << " -> " << dst << ";\n";
        };

        switch (net_ptr->net_type) {
            case NetType::EXT_IN:
            case NetType::EXT_CLK:
                os << "  " << label << " [shape=octagon, label=\"" << label << "\"];\n";
                for (Port* sink_port : net_ptr->sinks)
                    edge(label, sink_port->parent->lable() + ":<" + sink_port->spec.name + ">");
                break;

            case NetType::EXT_OUT:
                os << "  " << label << " [shape=octagon, label=\"" << label << "\"];\n";
                edge(net_ptr->driver->parent->lable() + ":<" + net_ptr->driver->spec.name + ">", label);
                break;

            default:
                os << "  " << label << " [shape=diamond, label=\"" << label << "\"];\n";
                if (net_ptr->driver)
                    edge(net_ptr->driver->parent->lable() + ":<" + net_ptr->driver->spec.name + ">", label + ":w");
                for (Port* sink_port : net_ptr->sinks)
                    edge(label + ":e", sink_port->parent->lable() + ":<" + sink_port->spec.name + ">");
                break;
        }
    }
    os << "}\n";
}

void Netlist::print(bool only_stats) const {
    int in_nets  = 0;
    int out_nets = 0;

    for (const auto& net : nets)
        if      (net->net_type == NetType::EXT_IN)  ++in_nets;
        else if (net->net_type == NetType::EXT_OUT) ++out_nets;

    int comb_modules = 0;
    int seq_modules  = 0;

    for (const auto& module : modules)
        if      (module->spec.combinational) ++comb_modules;
        else                                 ++seq_modules;

    std::cout << "+--------------------+-------+\n"
              << "| Metric             | Count |\n"
              << "+--------------------+-------+\n"
              << "| Input nets         | " << std::setw(5) << in_nets        << " |\n"
              << "| Output nets        | " << std::setw(5) << out_nets       << " |\n"
              << "| Total nets         | " << std::setw(5) << nets.size()    << " |\n"
              << "| Combinational mods | " << std::setw(5) << comb_modules   << " |\n"
              << "| Sequential mods    | " << std::setw(5) << seq_modules    << " |\n"
              << "| Total modules      | " << std::setw(5) << modules.size() << " |\n"
              << "+--------------------+-------+\n";

    if (only_stats) return;

    for (const auto& module_ptr : modules) {
        std::cout << "Module #" << module_ptr->id << " (" << module_ptr->spec.name << ")\n";
        for (const auto& port_ptr : module_ptr->inputs)
            std::cout << "    in  "
                      << std::setw(12) << port_ptr->spec.name
                      << "  net " << std::setw(4) << port_ptr->net->id
                      << " (" << static_cast<int>(port_ptr->net_type) << ")\n";
        for (const auto& port_ptr : module_ptr->outputs)
            std::cout << "    out "
                      << std::setw(12) << port_ptr->spec.name
                      << "  net " << std::setw(4) << port_ptr->net->id
                      << " (" << static_cast<int>(port_ptr->net_type) << ")\n";
    }
}
