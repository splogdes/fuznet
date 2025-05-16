#pragma once
#include "module.hpp"
#include <string>
#include <unordered_map>
#include <random>
#include <vector>
#include <optional>

class Library {

    public:
        Library(const std::string& filename);
        ModuleSpec* random_module(std::optional<std::mt19937_64> rng_opt);
        ModuleSpec* get_module(const std::string& name);
    
    private:
        std::unordered_map<std::string,ModuleSpec> modules;
        std::vector<std::string> module_names;

};