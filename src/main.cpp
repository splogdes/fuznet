#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "orchestrator.hpp"

using namespace std::chrono_literals;

int main() {
    try {
        std::mt19937_64 rng(std::random_device{}());

        fuznet::Orchestrator orchestrator = 
            fuznet::Orchestrator();

        orchestrator.run("output.dot");


    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
