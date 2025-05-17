#pragma once

#include "module.hpp"

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class Library {
public:
    Library(const std::string& filename, std::mt19937_64& rng);

    const ModuleSpec& get_module   (const std::string& name) const;
    const ModuleSpec& random_module() const;

private:
    std::unordered_map<std::string,ModuleSpec> modules;
    std::vector<std::string>                   module_names;
    std::vector<int>                           module_weights;
    mutable std::mt19937_64                    rng;
};
