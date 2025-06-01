#pragma once

#include "netlist.hpp"

struct ICommand {
    virtual const char* name() const = 0;
    virtual void execute() = 0;
    virtual ~ICommand() = default;
};

struct AddRandomModule : ICommand {
    Netlist& netlist;
    explicit AddRandomModule(Netlist& nl) : netlist(nl) {}
    const char* name() const override {
        return "AddRandomModule";
    }
    void execute() override {
        netlist.add_random_module();
    }
};

struct AddExternalNet : ICommand {
    Netlist& netlist;
    explicit AddExternalNet(Netlist& nl) : netlist(nl) {}
    const char* name() const override {
        return "AddExternalNet";
    }
    void execute() override {
        netlist.add_external_nets();
    }
};

struct AddUndriveNet : ICommand {
    Netlist& netlist;
    NetType type;
    explicit AddUndriveNet(Netlist& nl, NetType t = NetType::LOGIC)
        : netlist(nl), type(t) {}
    const char* name() const override {
        return "AddUndriveNet";
    }
    void execute() override {
        netlist.add_undriven_nets(type);
    }
};

struct DriveUndrivenNet : ICommand {
    Netlist& netlist;
    double seq_mod_prob;
    double seq_port_prob;
    NetType type;
    DriveUndrivenNet(Netlist& nl, double sm, double sp, NetType t = NetType::LOGIC)
        : netlist(nl), seq_mod_prob(sm), seq_port_prob(sp), type(t) {}
    const char* name() const override {
        return "DriveUndrivenNet";
    }
    void execute() override {
        netlist.drive_undriven_nets(seq_mod_prob, seq_port_prob, true, type);
    }
};

struct DriveUndrivenNets : ICommand {
    Netlist& netlist;
    double seq_mod_prob;
    double seq_port_prob;
    NetType type;
    DriveUndrivenNets(Netlist& nl, double sm, double sp, NetType t = NetType::LOGIC)
        : netlist(nl), seq_mod_prob(sm), seq_port_prob(sp), type(t) {}
    const char* name() const override {
        return "DriveUndrivenNets";
    }
    void execute() override {
        netlist.drive_undriven_nets(seq_mod_prob, seq_port_prob, false, type);
    }
};

struct BufferUnconnectedOutputs : ICommand {
    Netlist& netlist;
    explicit BufferUnconnectedOutputs(Netlist& nl) : netlist(nl) {}
    const char* name() const override {
        return "BufferUnconnectedOutputs";
    }
    void execute() override {
        netlist.buffer_unconnected_outputs();
    }
};