#include "netlist.hpp"
#include <algorithm>
#include <iostream>

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

    auto m = std::make_unique<Module>(modules.size(), buffer);
    auto ptr = m.get();
    modules.emplace_back(std::move(m));

    for (auto& p : ptr->ports) {
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

    auto m = std::make_unique<Module>(modules.size(), ms);
    auto ptr = m.get();
    modules.emplace_back(std::move(m));

    std::set<int> local_group;

    for (auto& p : ptr->ports) {
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

    return ptr;
}

Net* Netlist::make_net(std::string_view name) {
    auto n = std::make_unique<Net>();
    n->id   = nets.size();
    n->name = name.empty() ? ("net_" + std::to_string(n->id)) : std::string(name);
    auto ptr = n.get();
    nets.emplace_back(std::move(n));
    return ptr;
}

Net* Netlist::get_net(int id)
{
    if (id < 0 || id >= static_cast<int>(nets.size())) {
        throw std::out_of_range("Net ID out of range");
    }
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
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
#endif