#include "netlist.hpp"
#include <algorithm>
#include <iomanip>

Netlist::Netlist(Library& lib, std::optional<std::mt19937_64> rng_opt)
    : lib(lib), rng(rng_opt.value_or(std::mt19937_64(std::random_device{}())))
{
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

    for (auto& p : module_ptr->ports) {
        if (p->is_output()) {
            auto net = make_net();
            net->net_type = p->net_type;
            p->net = net;
            net->driver = p.get();
            local_group.insert(net->id);
        } else if (p->is_input()) {
            auto net = get_random_net(p->net_type);
            p->net = net;
            net->add_sink(p.get());
            local_group.insert(net->id);
        }
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

    for (auto& net : nets)
        if (net->sinks.empty() && net->net_type == NetType::LOGIC)
            add_buffer(net.get(), lib.get_module("OBUF"));
}

void Netlist::emit_verilog(std::ostream& os, const std::string& top_name) {
    insert_output_buffers();

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

        
        os << "_" << module->get_name(width) << "_ " << "(\n";

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

#define NETLIST_TEST
#ifdef NETLIST_TEST
int main() {
    try {
        Library lib("/home/splogdes/Documents/UNI/PNR/fuznet/src/lib.yaml");
        Netlist netlist(lib);
        netlist.add_random_module();
        netlist.add_external_net();
        netlist.add_random_module();
        netlist.add_random_module();
        netlist.add_random_module();

        netlist.print();

        netlist.emit_verilog(std::cout, "top");
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
#endif