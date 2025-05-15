#pragma once
#include "module.hpp"
#include <string>
#include <unordered_map>

struct Library {
    std::unordered_map<std::string,ModuleSpec> modules;
};

Library load_library(const std::string& filename);