#pragma once

#include "module.hpp"

#include <random>
#include <string>
#include <map>
#include <vector>
#include <functional>

class Library {
public:
    Library(const std::string& filename, std::mt19937_64& rng);

    const ModuleSpec& get_module        (const std::string& name) const;
    const ModuleSpec& get_random_module (std::function<bool (const ModuleSpec& ms)> filter = nullptr) const;
    const ModuleSpec& get_random_buffer (NetType input_type, NetType output_type) const;

private:
    std::map<std::string, ModuleSpec>           modules;
    std::vector<std::string>                    module_names;
    std::vector<int>                            module_weights;
    std::mt19937_64&                            rng;
};
