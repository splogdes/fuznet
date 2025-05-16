// #pragma once
// #include "lib.hpp"
// #include "module.hpp"
// #include "netlist.hpp"
// #include "spec.hpp"

// #include <string>
// #include <unordered_map>
// #include <random>
// #include <vector>

// class NetlistFuzzer {
//     public:
//         NetlistFuzzer(Library& lib, uint64_t seed, size_t max_steps);
//         void run();

//     private:
//         void init();
//         void add_external_nets();
//         void add_random_module();
//         bool add_random_net();

//         Library& lib;
//         Netlist netlist;
//         std::mt19937_64 rng;
//         size_t max_steps;

// };


