#pragma once
#include "module.hpp"
#include <string>
#include <unordered_map>
#include <random>
#include <vector>
#include <optional>

class Library {

    public:
        Library(const std::string& filename, std::optional<std::mt19937_64> rng_opt = std::nullopt);
        ModuleSpec* random_module();
        ModuleSpec* get_module(const std::string& name);
    
    private:
        std::unordered_map<std::string,ModuleSpec> modules;
        std::vector<std::string> module_names;
        std::vector<int> module_weights;
        std::mt19937_64 rng;

};