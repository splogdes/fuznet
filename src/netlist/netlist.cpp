#include "netlist.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <fstream>


std::string Net::lable(int width) const {
    if (!name.empty()) return name;
    if (width == 0) return "net_" + std::to_string(id);
    int digit_count = static_cast<int>(std::log10(id)) + 1;
    if (width < digit_count) throw std::invalid_argument("Width too small for ID");
    return "_" + std::string(width - digit_count, '0') + std::to_string(id) + "_";
}

void Net::remove_sink(PortBit portbit) {
    auto it = std::remove(sinks.begin(), sinks.end(), portbit);
    sinks.erase(it, sinks.end());
}

Module::Module(Id module_id, const ModuleSpec& spec_ref, std::mt19937_64& rng)
    : id{module_id}, spec{spec_ref} {

    for (const auto& port_spec : spec_ref.inputs)
        inputs.emplace_back(std::make_unique<Port>(port_spec, this));

    for (const auto& port_spec : spec_ref.outputs)
        outputs.emplace_back(std::make_unique<Port>(port_spec, this));

    for (const auto& output_port : outputs)
        if (spec_ref.seq_conns.contains(output_port->spec.name))
            for (const auto& seq_name : spec_ref.seq_conns.at(output_port->spec.name)) {
                Port* input_port = get_input(seq_name);
                if (!input_port) 
                    throw std::runtime_error("Sequential connection to non-existent input: " + seq_name);
                seq_conns[output_port.get()].insert(input_port);
            }

    for (const auto& param_spec : spec_ref.params) {
        std::string value;
        value.reserve(param_spec.width);
        for (int i = 0; i < param_spec.width; ++i)
            value.push_back((rng() & 1) ? '1' : '0');
        param_values.emplace(param_spec.name, std::move(value));
    }
}

Port* Module::get_input(const std::string& name) {
    for (auto& input_port : inputs)
        if (input_port->spec.name == name)
            return input_port.get();
    throw std::runtime_error("Input port not found: " + name);
}

std::string Module::lable(int width) const {
    if (width == 0) return spec.name + "_" + std::to_string(id);
    int digit_count = static_cast<int>(std::log10(id)) + 1;
    if (width < digit_count) throw std::invalid_argument("Width too small for ID");
    return "_" + std::string(width - digit_count, '0') + std::to_string(id) + "_";
}

Netlist::Netlist(Library& library_ref, std::mt19937_64& rng)
    : lib{library_ref}, rng{rng} {}

void Netlist::add_initial_nets() {
    Net* input_net = make_net(NetType::EXT_IN);
    Net* clock_net = make_net(NetType::EXT_CLK, "clk");

    add_buffer(
        input_net, lib.get_random_buffer(NetType::EXT_IN, NetType::LOGIC)
    );
    add_buffer(
        clock_net, lib.get_random_buffer(NetType::EXT_CLK, NetType::CLK)
    );
}
    

Netlist::~Netlist() = default;

void Netlist::add_external_nets(size_t number) {
    for (size_t i = 0; i < number; ++i) {
        Net* ext_net = make_net(NetType::EXT_IN);

        add_buffer(
            ext_net, lib.get_random_buffer(NetType::EXT_IN, NetType::LOGIC)
        );
    }
}

void Netlist::add_random_module() {
    const ModuleSpec& spec_ref = lib.get_random_module();
    make_module(spec_ref);
}

void Netlist::add_undriven_nets(NetType type, size_t n) {
    for (size_t i = 0; i < n; ++i)
        make_net(type);
}

void Netlist::drive_undriven_nets(double seq_mod_prob, double seq_port_prob, bool limit_to_one, NetType type) {
    for (auto& net_ptr : nets) {
        if (net_ptr->driver.port || net_ptr->net_type != type) continue;

        std::uniform_real_distribution<double> dist(0.0, 1.0);
        bool seq_mod = dist(rng) < seq_mod_prob;

        auto module_filter = [&](const ModuleSpec& ms) {
            return ms.outputs.size() == 1 &&
                   ms.outputs[0].net_type == type &&
                   ms.outputs[0].width == 1 &&
                   !(seq_mod && ms.combinational);
        };

        const ModuleSpec& driver_spec = lib.get_random_module(module_filter);

        Module* driver_module = make_module(driver_spec, false);
        Port*   driver_port   = driver_module->outputs[0].get();

        driver_port->nets[0] = net_ptr.get();
        net_ptr->driver.port = driver_port;
        net_ptr->driver.bit = 0;
        
        for (auto& input_port : driver_module->inputs) {

            std::set<int> forwad_group = get_combinational_group(input_port.get(), false);
            std::set<int> comb_group   = get_combinational_group(input_port.get(), true);
            std::set<int> seq_group;

            for (auto id : forwad_group)
                if (!comb_group.contains(id))
                    seq_group.insert(id);

            for (int i = 0; i < input_port->width; ++i) {

                bool seq_port = dist(rng) < seq_port_prob;

                auto same_type = [&](const Net* n) {return n->net_type == input_port->net_type; };

                if (input_port->net_type == NetType::LOGIC) {

                    std::function<bool(const Net*)> seq_filter = [&](const Net* n) {
                        return seq_group.contains(n->id) && same_type(n);
                    };

                    std::function<bool(const Net*)> comb_filter = [&](const Net* n) {
                        return !forwad_group.contains(n->id) && same_type(n);
                    };

                    if (seq_group.empty())
                        seq_filter = comb_filter;

                    Net* source = seq_port
                                    ?  get_random_net(seq_filter)
                                    :  get_random_net(comb_filter);

                    input_port->nets[i] = source;
                    source->add_sink(input_port.get(), i);
                } else {
                    Net* source = get_random_net(same_type);
                    input_port->nets[i] = source;
                    source->add_sink(input_port.get(), i);
                }

            }
        }

        if (limit_to_one) break;
    }
}

void Netlist::buffer_unconnected_outputs() {
    std::vector<Net*> logic_nets_without_sinks;
    for (auto& net_ptr : nets)
        if (net_ptr->sinks.empty() && net_ptr->net_type == NetType::LOGIC)
            logic_nets_without_sinks.push_back(net_ptr.get());

    for (Net* net_ptr : logic_nets_without_sinks)
        add_buffer(
            net_ptr, lib.get_random_buffer(net_ptr->net_type, NetType::EXT_OUT)
        );
}

void Netlist::add_buffer(Net* drive_net, const ModuleSpec& buffer_spec) {
    if (buffer_spec.inputs.size() != 1 || buffer_spec.inputs[0].width != 1 
        || buffer_spec.outputs.size() != 1 || buffer_spec.outputs[0].width != 1)
        throw std::invalid_argument("Buffer must have one input and one output");

    auto buffer_module = std::make_unique<Module>(modules.size(), buffer_spec, rng);
    Module* module_ptr = buffer_module.get();
    module_ptr->id = get_next_id();
    modules.push_back(std::move(buffer_module));

    Port* input_port  = module_ptr->inputs[0].get();
    Port* output_port = module_ptr->outputs[0].get();

    input_port->nets[0] = drive_net;
    drive_net->add_sink(input_port, 0);

    Net* new_net = make_net(output_port->net_type);
    output_port->nets[0]  = new_net;
    new_net->driver   = PortBit{output_port, 0};
}

Module* Netlist::make_module(const ModuleSpec& spec_ref, bool connect_random, int id) {
    if (id < 0)
        id = get_next_id();
    auto module_obj = std::make_unique<Module>(id, spec_ref, rng);
    Module* module_ptr = module_obj.get();
    modules.push_back(std::move(module_obj));

    if (!connect_random) return module_ptr;

    for (auto& input_port : module_ptr->inputs)
        for (int i = 0; i < input_port->width; ++i) {
            Net* source = get_random_net(
                [&](const Net* n) { return n->net_type == input_port->net_type; }
            );
            input_port->nets[i] = source;
            source->add_sink(input_port.get(), i);
        }

    for (auto& output_port : module_ptr->outputs)
        for (int i = 0; i < output_port->width; ++i) {
            Net* dest = make_net(output_port->net_type);
            output_port->nets[i] = dest;
            dest->driver = PortBit{output_port.get(), i};
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

std::set<int> Netlist::get_combinational_group(Port* input_port, bool stop_at_seq) const {
    std::set<int> visited;
    std::queue<Net*> bfs_queue;

    Module* module = input_port->parent;
    for (auto& next_out : module->outputs) {
        for (int i = 0; i < next_out->width; ++i) {
            const bool edge_is_sequential =
                module->seq_conns.contains(next_out.get()) &&
                module->seq_conns.at(next_out.get()).contains(input_port);
            
            if (stop_at_seq && edge_is_sequential)
                continue; 
            
            bfs_queue.push(next_out->nets[i]);
        }
    }

    while (!bfs_queue.empty()) {
        Net* current = bfs_queue.front();
        bfs_queue.pop();
        if (!visited.insert(current->id).second) continue;

        for (const PortBit& sink : current->sinks) {
            Module* module = sink.port->parent;

            for (auto& next_out : module->outputs) {
                for (int i = 0; i < next_out->width; ++i) {
                    const bool edge_is_sequential =
                        module->seq_conns.contains(next_out.get()) &&
                        module->seq_conns.at(next_out.get()).contains(sink.port);
                    
                    if (stop_at_seq && edge_is_sequential)
                       continue; 
                    
                    bfs_queue.push(next_out->nets[i]);
                }
            }      
        }
    }
    return visited;
}

Net* Netlist::make_net(NetType type,const std::string& name, int id) {
    if (id < 0)
        id = get_next_id();
    auto net_obj = std::make_unique<Net>();
    net_obj->id = id;
    net_obj->net_type = type;
    net_obj->name = std::move(name);
    Net* net_ptr  = net_obj.get();
    nets.push_back(std::move(net_obj));
    return net_ptr;
}

Net* Netlist::get_net(int id) {
    for (auto& net_ptr : nets)
        if (net_ptr->id == static_cast<size_t>(id)) return net_ptr.get();
    throw std::runtime_error("Net not found");
}

void Netlist::remove_other_nets(const int& output_id) {
    Net* out_net = get_net(output_id);

    if (out_net->net_type != NetType::EXT_OUT)
        throw std::invalid_argument("Output net must be of type EXT_OUT");

    std::unordered_set<int> keep_nets;
    std::unordered_set<int> keep_modules;
    std::queue<Net*> work;

    keep_nets.insert(out_net->id);
    work.push(out_net);

    while (!work.empty()) {
        Net* current = work.front();
        work.pop();

        if (!current->driver.port) 
            continue;

        Module* module = current->driver.port->parent;

        if (keep_modules.insert(module->id).second)
            for (const auto& input_port : module->inputs)
                for (int i = 0; i < input_port->width; ++i) 
                    if (keep_nets.insert(input_port->nets[i]->id).second)
                        work.push(input_port->nets[i]);

    }

    for (const auto& net_ptr : nets)
        if (net_ptr->name == "clk")
            keep_nets.insert(net_ptr->id);

    modules.erase(
        std::remove_if(modules.begin(), modules.end(),
                       [&](const std::unique_ptr<Module>& m) {
                           return !keep_modules.contains(m->id);
                       }),
        modules.end()
    );

    nets.erase(
        std::remove_if(nets.begin(), nets.end(),
                       [&](const std::unique_ptr<Net>& n) {
                           return !keep_nets.contains(n->id);
                       }),
        nets.end()
    );

    for (auto& net_ptr : nets)
        net_ptr->sinks.erase(
            std::remove_if(net_ptr->sinks.begin(), net_ptr->sinks.end(),
                           [&](PortBit pb) { return !keep_modules.contains(pb.port->parent->id); }),
            net_ptr->sinks.end()
        );

    for (auto& module_ptr : modules) {
        for (auto& port_ptr : module_ptr->inputs)
            for (int i = 0; i < port_ptr->width; ++i) 
                if (!keep_nets.contains(port_ptr->nets[i]->id))
                    std::runtime_error("Input port net not found in keep set");

        for (auto& port_ptr : module_ptr->outputs) {
            for (int i = 0; i < port_ptr->width; ++i) {     
                if (!keep_nets.contains(port_ptr->nets[i]->id)) {
                    Net* net = make_net(port_ptr->net_type);
                    port_ptr->nets[i] = net;
                    net->driver = PortBit{port_ptr.get(), i};
                }
            }
        }
    }

    buffer_unconnected_outputs();
    
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
            os << "    ." << port_ref->spec.name << "(";
            if (port_ref->width > 1)
                os << "{";
            for (int j = 0; j < port_ref->width; ++j) {
                if (port_ref->nets[j])
                    os << port_ref->nets[j]->lable(width);
                else
                    os << "1'b0"; 
                if (j + 1 < port_ref->width) os << ", ";
            }
            if (port_ref->width > 1)
                os << "}";
            os << ")";
            if (i + 1 < ordered_ports.size()) os << ",";
            os << "\n";
        }
        os << "  );\n";
    }
    os << "endmodule\n";
}


void Netlist::emit_dotfile(std::ostream& os, const std::string& top) const {
    os << "digraph \"" << top << "\" {\n"
       << "label=\"" << top << "\";\n"
       << "rankdir=\"LR\";\n"
       << "remincross=true;\n";

    for (const auto& m : modules) {

        std::vector<std::string> in_labels;
        std::vector<std::string> out_labels;
        for (const auto& p : m->inputs) {

            in_labels.push_back(p->spec.name);

            if (p->width > 1) {
                const std::string bus = m->lable() + "_" + p->spec.name;

                os << "  " << bus << R"( [shape=record,style=rounded,label=")";
                for (int b = p->width - 1; b >= 0; --b) {
                    os << "<s" << b << "> " << b << ':' << b;
                    if (b) os << " | ";
                }
                os << R"(",color="black",fontcolor="black"])" << ";\n";

                os << "  " << bus << ":e -> "
                   << m->lable() << ":<" << p->spec.name << ">:w "
                   << R"([arrowhead=odiamond,arrowtail=odiamond,dir=both,)"
                   << "style=\"setlinewidth(" << p->width
                   << ")\",color=\"black\",fontcolor=\"black\"];\n";
            }
        }

        for (const auto& p : m->outputs) {
            out_labels.push_back(p->spec.name);

            if (p->width > 1) {
                const std::string bus = m->lable() + "_" + p->spec.name;

                os << "  " << bus << R"( [shape=record,style=rounded,label=")";
                for (int b = p->width - 1; b >= 0; --b) {
                    os << "<s" << b << "> " << b << ':' << b;
                    if (b) os << " | ";
                }
                os << R"(",color="black",fontcolor="black"])" << ";\n";

                os << "  " << m->lable() << ":<" << p->spec.name << ">:e -> "
                   << bus << ":w "
                   << R"([arrowhead=odiamond,arrowtail=odiamond,dir=both,)"
                   << "style=\"setlinewidth(" << p->width
                   << ")\",color=\"black\",fontcolor=\"black\"];\n";
            }
        }

        os << "  " << m->lable()
           << R"( [shape=record,label="{{)";
        for (std::size_t i = 0; i < in_labels.size(); ++i) {
            os << '<' << in_labels[i] << "> " << in_labels[i];
            if (i + 1 < in_labels.size()) os << " | ";
        }
        os << "} | " << m->spec.name << " | {";
        for (std::size_t i = 0; i < out_labels.size(); ++i) {
            os << '<' << out_labels[i] << "> " << out_labels[i];
            if (i + 1 < out_labels.size()) os << " | ";
        }
        os << R"(}}",color="black",fontcolor="black"])" << ";\n";
    }

    auto edge = [&](const std::string& s, const std::string& d) {
        os << "  " << s << " -> " << d
           << R"( [color="black",fontcolor="black"])" << ";\n";
    };

    for (const auto& n : nets) {
        const std::string nid = n->lable();

        auto net_node = [&](const char* shape) {
            os << "  " << nid << " [shape=" << shape
               << ",label=\"" << nid
               << R"(",color="black",fontcolor="black"])" << ";\n";
        };

        switch (n->net_type) {
        case NetType::EXT_IN:
        case NetType::EXT_CLK: net_node("octagon"); break;
        case NetType::EXT_OUT: net_node("octagon"); break;
        default:               net_node("diamond"); break;
        }

        if (n->driver.port) {
            const Port* dp = n->driver.port;
            std::string src = (dp->width == 1)
                ? dp->parent->lable() + ":<" + dp->spec.name + '>'
                : dp->parent->lable() + '_' + dp->spec.name
                    + ":<s" + std::to_string(n->driver.bit) + '>';

            edge(src, nid + ":w");
        }

        for (const PortBit& s : n->sinks) {
            const Port* sp = s.port;
            std::string dst = (sp->width == 1)
                ? sp->parent->lable() + ":<" + sp->spec.name + '>'
                : sp->parent->lable() + '_' + sp->spec.name
                    + ":<s" + std::to_string(s.bit) + '>';

            edge(nid + ":e", dst);
        }
    }

    os << "}\n";
}

nlohmann::json Netlist::json() const {

    nlohmann::json json_netlist;
    json_netlist["version"] = "0.1";


    json_netlist["nets"] = nlohmann::json::array();
    
    for (const auto& net_ptr : nets) {
        nlohmann::json net_json;
        net_json["id"] = net_ptr->id;
        net_json["name"] = net_ptr->name;
        net_json["type"] = static_cast<int>(net_ptr->net_type);
        json_netlist["nets"].push_back(net_json);
    }

    json_netlist["modules"] = nlohmann::json::array();
    for (const auto& module_ptr : modules) {
        nlohmann::json module_json;
        module_json["id"] = module_ptr->id;
        module_json["name"] = module_ptr->spec.name;

        auto port_to_json = [](const Port* port) {
            nlohmann::json port_json;
            port_json["width"] = port->spec.width;
            port_json["net_type"] = static_cast<int>(port->net_type);
            port_json["net_ids"] = nlohmann::json::array();
            for (int i = 0; i < port->width; ++i) {
                if (port->nets[i])
                    port_json["net_ids"].push_back(port->nets[i]->id);
                else
                    port_json["net_ids"].push_back(-1);
            } 
            return port_json;
        };

        for (const auto& port_ptr : module_ptr->inputs) 
            module_json["ports"][port_ptr->spec.name] = port_to_json(port_ptr.get());

        for (const auto& port_ptr : module_ptr->outputs) 
            module_json["ports"][port_ptr->spec.name] = port_to_json(port_ptr.get());

        module_json["params"] = nlohmann::json::object();
        for (const auto& param : module_ptr->param_values) {
            module_json["params"][param.first] = param.second;
        }

        json_netlist["modules"].push_back(module_json);

    }

    return json_netlist;
}

void Netlist::load_from_json(const nlohmann::json& json_netlist) {

    nets.clear();
    modules.clear();

    for (const auto& net_json : json_netlist["nets"]) {
        std::string name = net_json.value("name", "");
        int id = net_json.value("id", -1);
        id_counter = std::max(id_counter, id + 1);
        if (id < 0) {
            throw std::runtime_error("Invalid net ID in JSON: " + std::to_string(id));
        }
        NetType type = static_cast<NetType>(net_json.value("type", -1));
        make_net(type, name, id);
    }

    for (const auto& module_json : json_netlist["modules"]) {
        int id = module_json.value("id", -1);
        id_counter = std::max(id_counter, id + 1);
        if (id < 0) {
            throw std::runtime_error("Invalid module ID in JSON: " + std::to_string(id));
        }
        std::string name = module_json.value("name", "");
        const ModuleSpec& spec = lib.get_module(name);
        Module* module_ptr = make_module(spec, false, id);

        for (auto& port_ptr : module_ptr->inputs) {
            auto port_json = module_json["ports"][port_ptr->spec.name];
            for (int i = 0; i < port_ptr->width; ++i) {           
                int net_id = port_json["net_ids"][i];
                if (net_id >= 0) {
                    Net* net = get_net(net_id);
                    port_ptr->nets[i] = net;
                    net->add_sink(port_ptr.get(), i);
                }
            }
        }

        for (auto& port_ptr : module_ptr->outputs) {
            auto port_json = module_json["ports"][port_ptr->spec.name];
            for (int i = 0; i < port_ptr->width; ++i) {
                int net_id = port_json["net_ids"][i];
                if (net_id >= 0) {
                    Net* net = get_net(net_id);
                    port_ptr->nets[i] = net;
                    net->driver = PortBit{port_ptr.get(), i};
                }
            }
        }

        for (const auto& param : module_json["params"].items()) {
            module_ptr->param_values[param.key()] = param.value();
        }
    }
}

    

void Netlist::print(bool only_stats) const
{
    const NetlistStats stats = get_stats();

    std::cout << "+--------------------+-------+\n"
              << "| Metric             | Count |\n"
              << "+--------------------+-------+\n"
              << "| Input nets         | " << std::setw(5) << stats.input_nets     << " |\n"
              << "| Output nets        | " << std::setw(5) << stats.output_nets    << " |\n"
              << "| Total nets         | " << std::setw(5) << stats.total_nets     << " |\n"
              << "| Combinational mods | " << std::setw(5) << stats.comb_modules   << " |\n"
              << "| Sequential mods    | " << std::setw(5) << stats.seq_modules    << " |\n"
              << "| Total modules      | " << std::setw(5) << stats.total_modules  << " |\n"
              << "+--------------------+-------+\n";

    if (only_stats) return;

    /* Detailed listing -- every port, every bit */
    for (const auto& mod : modules) {
        std::cout << "Module #" << mod->id << " (" << mod->spec.name << ")\n";

        auto dump_port =
            [](const char* dir, const Port* p) {
                std::cout << "    " << dir << ' '
                          << std::setw(12) << p->spec.name << "  ";

                if (p->width == 1) {
                    const Net* n = p->nets[0];
                    std::cout << "net " << std::setw(4)
                              << (n ? std::to_string(n->id) : "-")
                              << " (" << static_cast<int>(p->net_type) << ")\n";
                } else {
                    std::cout << "nets [ ";
                    for (int i = 0; i < p->width; ++i) {
                        const Net* n = p->nets[i];
                        std::cout << (n ? std::to_string(n->id) : "-");
                        if (i + 1 < p->width) std::cout << ' ';
                    }
                    std::cout << "] (" << static_cast<int>(p->net_type) << ")\n";
                }
            };

        for (const auto& in  : mod->inputs)  dump_port("in ", in .get());
        for (const auto& out : mod->outputs) dump_port("out", out.get());
    }
}

NetlistStats Netlist::get_stats() const {
    NetlistStats stats;
    for (const auto& net_ptr : nets)
        if      (net_ptr->net_type == NetType::EXT_IN)  ++stats.input_nets;
        else if (net_ptr->net_type == NetType::EXT_OUT) ++stats.output_nets;
        
    stats.total_nets = static_cast<int>(nets.size());

    for (const auto& module_ptr : modules)
        if   (module_ptr->spec.combinational) ++stats.comb_modules;
        else                                  ++stats.seq_modules;

    stats.total_modules = static_cast<int>(modules.size());

    return stats;
}
