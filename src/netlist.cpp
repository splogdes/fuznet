#include "netlist.hpp"
#include <algorithm>
#include <iomanip>

std::string Net::get_name(int width) const {
    if (!name.empty()) return name;
    if (width == 0) return std::string("net_") + std::to_string(id);
    if (width - static_cast<int>(std::log10(id)) < 0) throw std::invalid_argument("Width is too small for ID");
    return "_" + std::string(width - static_cast<int>(std::log10(id)) - 1, '0')
                + std::to_string(id) + "_";
}

Module::Module(Id id_, const ModuleSpec* ms, std::mt19937_64* rng) : id{id_}, spec{ms} {
    for (auto& ps : ms->ports) {
        auto p = std::make_unique<Port>(&ps, this);
        ports.emplace_back(std::move(p));
    }
    if (rng) {
        for (const auto& pspec : ms->params) {
            std::string value;
            for (size_t i = 0; i < pspec.width; ++i) {
                value += ((*rng)() % 2) ? '1' : '0';
                param_values[pspec.name] = value;
            }
        }
    }
}

std::string Module::get_name(int width) const {
    if (width == 0) return spec->name + "_" + std::to_string(id);
    if (width - static_cast<int>(std::log10(id)) < 0) throw std::invalid_argument("Width is too small for ID");
    return "_" + std::string(width - static_cast<int>(std::log10(id)) - 1, '0')
                + std::to_string(id) + "_";
}

Netlist::Netlist(Library& lib, std::optional<std::mt19937_64> rng_opt)
    : lib(lib), rng(rng_opt.value_or(std::mt19937_64(std::random_device{}()))) {
    Net* input_net = make_net();
    Net* clock_net = make_net();
    
    input_net->net_type = NetType::EXT_IN;
    clock_net->net_type = NetType::EXT_CLK;
    clock_net->name = "clk";
    
    add_buffer(input_net, lib.get_module("IBUF"));
    add_buffer(clock_net, lib.get_module("BUFG"));

    combinational_groups.resize(1);
    combinational_groups[0].insert(input_net->id);
}


void Netlist::add_external_net() {
    auto ext_in = make_net();   
    ext_in->net_type = NetType::EXT_IN;
}
    

void Netlist::add_random_module() {
    auto ms = lib.random_module(rng);
    if (!ms) {
        throw std::runtime_error("Failed to get random module");
    }
    make_module(ms);
}


void Netlist::add_buffer(Net* net, const ModuleSpec* buffer) {
    if (!net || !buffer) {
        throw std::invalid_argument("Net or buffer cannot be null");
    }
    if (buffer->ports.size() != 2) {
        throw std::invalid_argument("Buffer must have exactly two ports");
    }

    auto module = std::make_unique<Module>(modules.size(), buffer, &rng);
    auto module_ptr = module.get();
    modules.emplace_back(std::move(module));
    module_ptr->id = get_next_id();

    for (auto& p : module_ptr->ports) {
        if (p->is_input()) {
            p->net = net;
            net->add_sink(p.get());
        } else if (p->is_output()) {
            auto new_net = make_net();
            new_net->net_type = p->net_type;
            p->net = new_net;
            new_net->driver = p.get();
        }
    }

}

Net* Netlist::get_random_net(NetType type) {
    std::vector<Net*> filtered_nets;
    for (const auto& net : nets) {
        if (net->net_type == type) {
            filtered_nets.push_back(net.get());
        }
    }
    if (filtered_nets.empty()) {
        throw std::runtime_error("No nets of the specified type found");
    }
    std::uniform_int_distribution<int> dist(0, filtered_nets.size() - 1);
    int idx = dist(rng);
    return filtered_nets[idx];
}


Module* Netlist::make_module(const ModuleSpec* ms) {

    if (!ms) {
        throw std::invalid_argument("ModuleSpec cannot be null");
    }

    auto module = std::make_unique<Module>(modules.size(), ms, &rng);
    auto module_ptr = module.get();
    modules.emplace_back(std::move(module));
    module_ptr->id = get_next_id();

    std::set<int> local_group;
    std::vector<Port*> output_ports;

    for (auto& p : module_ptr->ports) {
        if (p->is_output()) {
            output_ports.push_back(p.get());
        } else if (p->is_input()) {
            auto net = get_random_net(p->net_type);
            p->net = net;
            net->add_sink(p.get());
            local_group.insert(net->id);
        }
    }

    for (auto& p : output_ports) {
            auto net = make_net();
            net->net_type = p->net_type;
            p->net = net;
            net->driver = p;
            local_group.insert(net->id);
    }

    if (ms->combinational) update_combinational_groups(local_group);

    return module_ptr;
}

Net* Netlist::make_net(std::string name) {
    auto net = std::make_unique<Net>();
    net->id = get_next_id();
    auto net_ptr = net.get();
    nets.emplace_back(std::move(net));
    return net_ptr;
}

Net* Netlist::get_net(int id) {
    return nets[id].get();
}

void Netlist::update_combinational_groups(std::set<int>& group) {
    std::vector<std::set<int>> new_groups;
    std::set<int> merged = group;

    for (auto& g : combinational_groups) {
        if (std::any_of(group.begin(), group.end(), [&g](int id) { return g.count(id); })) {
            merged.insert(g.begin(), g.end());
        } else {
            new_groups.push_back(g);
        }
    }

    new_groups.push_back(merged);
    combinational_groups = std::move(new_groups);
}

void Netlist::insert_output_buffers() {
    std::vector<Net*> targets;

    for (auto& net : nets)
        if (net->sinks.empty() && net->net_type == NetType::LOGIC)
            targets.push_back(net.get());

    for (auto& net : targets)
        add_buffer(net, lib.get_module("OBUF"));
                
}

void Netlist::emit_verilog(std::ostream& os, const std::string& top_name) {

    std::vector<const Net*> ext_in_nets;
    std::vector<const Net*> ext_out_nets;

    int width = id_width();

    for (const auto& net : nets) {
        if (net->net_type == NetType::EXT_IN || net->net_type == NetType::EXT_CLK)
            ext_in_nets.push_back(net.get());
        else if (net->net_type == NetType::EXT_OUT)
            ext_out_nets.push_back(net.get());
    }

    os << "module " << top_name << "(";
    for (size_t i = 0; i < ext_in_nets.size(); ++i) {
        os << ext_in_nets[i]->get_name(width) << (i + 1 < ext_in_nets.size() ? ", " : "");
    }
    if (!ext_out_nets.empty() && !ext_out_nets.empty())
        os << ", ";
    for (size_t i = 0; i < ext_out_nets.size(); ++i) {
        os << ext_out_nets[i]->get_name(width) << (i + 1 < ext_out_nets.size() ? ", " : "");
    }
    os << ");\n";


    for (const auto* net : ext_in_nets)
        os << "  input " << net->get_name(width) << ";\n";
    for (const auto* net : ext_out_nets)
        os << "  output " << net->get_name(width) << ";\n";

    for (const auto& net : nets)
        if (net->net_type == NetType::LOGIC)  
            os << "  wire " << net->get_name(width) << ";\n";

    for (const auto& module : modules) {


        if (!module->param_values.empty()) {
            os << "  " << module->spec->name << " #(\n";
            for (size_t i = 0; i < module->spec->params.size(); ++i) {
                auto& param = module->spec->params[i];
                os << "    ." << param.name << "(" << param.width << "'b" << module->param_values.at(param.name) << ")\n";
                if ((i + 1) < module->param_values.size())
                    os << ",\n";
            }
            os << "  ) ";
        } else {
            os << "  " << module->spec->name << " ";
        }

        
        os << module->get_name(width) << " (\n";

        for (size_t i = 0; i < module->ports.size(); ++i) {
            auto& port = module->ports[i];
            os << "    ." << port->spec->name << "(" << port->net->get_name(width) << ")";
            if (i + 1 < module->ports.size())
                os << ",";
            os << "\n";
        }
        os << "  );\n";
    }

    os << "endmodule\n";
}

void Netlist::emit_dotfile(std::ostream& os, const std::string& top_name) {
    os << "digraph " << top_name << " {\n";
    os << "  rankdir=LR;\n";
    for (const auto& module : modules) {
        os << "  " << module->get_name() << " [shape=record,";
        std::vector<std::string> input_ports;
        std::vector<std::string> output_ports;
        for (const auto& port : module->ports) {
            if (port->is_input()) {
                input_ports.push_back(port->spec->name);
            } else if (port->is_output()) {
                output_ports.push_back(port->spec->name);
            }
        }
        os << " label=\"{{";
        for (size_t i = 0; i < input_ports.size(); ++i) {
            os << " <" << input_ports[i] << "> " << input_ports[i];
            if (i + 1 < input_ports.size()) os << " |";
        }
        os << "}| " << module->spec->name << " | {";
        for (size_t i = 0; i < output_ports.size(); ++i) {
            os << " <" << output_ports[i] << "> " << output_ports[i];
            if (i + 1 < output_ports.size()) os << " |";
        }
        os << "}}\"";
        os << "];\n";
    }
    for (const auto& net : nets) {
        if (net->net_type == NetType::EXT_IN || net->net_type == NetType::EXT_CLK) {
            os << "  " << net->get_name() << " [shape=octagon, label=\"" << net->get_name() << "\"];\n";
            for (const auto& sink : net->sinks) {
                os << "  " << net->get_name() << ":e -> " << sink->parent->get_name() << ":<" << sink->spec->name << ">;\n";
            }
        } else if (net->net_type == NetType::EXT_OUT) {
            os << "  " << net->get_name() << " [shape=octagon, label=\"" << net->get_name() << "\"];\n";
            os << "  " << net->driver->parent->get_name() << ":<" << net->driver->spec->name << "> -> " 
               << net->get_name() << ";\n";
        } else {
            os << "  " << net->get_name() << " [shape=diamond, label=\"" << net->get_name() << "\"];\n";
            auto driver = net->driver;
            os << "  " << driver->parent->get_name() << ":<" << driver->spec->name << "> -> " 
               << net->get_name() << ":w ;\n";
            for (const auto& sink : net->sinks) {
                os << "  " << net->get_name() << ":e -> " << sink->parent->get_name() 
                   << ":<" << sink->spec->name << ">;\n";
            }
        }
    }

    os << "}\n";
}


void Netlist::print() {
    printf("Netlist:\n");
    printf("Number of modules: %zu\n", modules.size());
    printf("Number of nets: %zu\n", nets.size());
    for (const auto& m : modules) {
        printf("Module ID: %zu, Name: %s\n", m->id, m->spec->name.c_str());
        for (const auto& p : m->ports) {
            printf("  Port: %s, I/O: %s, Type: %d, Net: %s, Type: %d\n", p->spec->name.c_str(), p->is_input() ? "Input" : "Output", 
                   p->net_type, p->net->name.c_str(), p->net->net_type);
        }
    }
    for (const auto& n : nets) {
        printf("Net ID: %zu, Name: %s\n", n->id, n->name.c_str());
    }
    for (const auto& group : combinational_groups) {
        printf("Combinational group: ");
        for (const auto& id : group) {
            printf("%d ", id);
        }
        printf("\n");
    }
}

Netlist::~Netlist() {
    // Destructor will automatically clean up unique_ptrs
}

#ifdef NETLIST_TEST
#include <fstream>

int main() {
    try {
        std::mt19937_64 rng;
        rng.seed(32);
        Library lib("/home/splogdes/Documents/UNI/PNR/fuznet/src/lib.yaml");
        Netlist netlist(lib, rng);
        netlist.add_random_module();
        netlist.add_external_net();
        for (int i = 0; i < 49; ++i) {
            netlist.add_random_module();
        }
        netlist.insert_output_buffers();

        std::ofstream verilog_file("output.v");
        if (!verilog_file) {
            throw std::runtime_error("Failed to open file for writing");
        }
        netlist.emit_verilog(verilog_file, "top");
        verilog_file.close();

        std::ofstream dot_file("output.dot");
        if (!dot_file) {
            throw std::runtime_error("Failed to open file for writing");
        }
        netlist.emit_dotfile(dot_file, "top");
        dot_file.close();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
#endif