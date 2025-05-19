#include "orchestrator.hpp"

#include <atomic>
#include <thread>
#include <chrono>
#include <random>
#include <toml++/toml.h>

namespace fuznet {

Orchestrator::Orchestrator(const std::string& lib_yaml,
                           const std::string& config_toml,
                           unsigned seed)
    : rng(seed),
      library(lib_yaml, rng),
      netlist(library, rng)
{
    commands.emplace_back(std::make_unique<AddRandomModule>(netlist), 1);
    commands.emplace_back(std::make_unique<AddExternalNet>(netlist), 1);
    commands.emplace_back(std::make_unique<AddUndriveNet>(netlist), 1);
    commands.emplace_back(std::make_unique<DriveUndrivenNet>(netlist, 0.5), 1);
    commands.emplace_back(std::make_unique<SwitchUp>(netlist), 1);
    commands.emplace_back(std::make_unique<BufferUnconnectedOutputs>(netlist), 1);
}

}