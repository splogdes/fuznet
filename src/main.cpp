#include <yaml-cpp/yaml.h>
#include <iostream>

int main() {
    YAML::Node config = YAML::LoadFile("lib.yaml");
    std::cout << "Loaded YAML: " << config << "\n";
    return 0;
}
