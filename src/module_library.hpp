#pragma once

#include "module_spec.hpp"

#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

class ModuleLibrary {
public:
    ModuleLibrary(const std::string& filename, std::mt19937_64& rng);

    const ModuleSpec& get_module        (const std::string& name) const;
    const ModuleSpec& get_random_module (std::function<bool (const ModuleSpec& ms)> filter = nullptr) const;

private:
    std::unordered_map<std::string, ModuleSpec> modules;
    std::vector<std::string>                    module_names;
    std::vector<int>                            module_weights;
    mutable std::mt19937_64                     rng;
};
