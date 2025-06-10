#include <verilated.h>
#include <verilated_vcd_c.h>
#include <Veq_top.h>
#include <random>
#include <iostream>

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    Veq_top* top = new Veq_top;

    Verilated::traceEverOn(true);
    top->trace(tfp, 99);
    tfp->open("bug_dir/eq_top.vcd");

    uint32_t seed = 4160123549;
    uint32_t cycles = 1000;
    std::mt19937 rng(seed);
    auto rnd_bit = [&]() { return rng() & 1; };

    std::cerr << "[TB] seed=" << seed << " cycles=" << cycles << std::endl;

    bool trigger = false;

    for (uint32_t i = 0; i < cycles; ++i) {
        top->clk = 0;
        top->eval();
        tfp->dump(i * 10);
        top->_04_ = rnd_bit();
        top->_12_ = rnd_bit();

        top->clk = 1;
        top->eval();
        tfp->dump(i * 10 + 5);
        if (top->trigger) {
            std::cerr << "[TB] Triggered at cycle " << i << std::endl;
            if (!top->_31_) std::cout << "[TB] Triggered by wire _31_ " << std::endl;
            if (!top->_48_) std::cout << "[TB] Triggered by wire _48_ " << std::endl;
            if (!top->_50_) std::cout << "[TB] Triggered by wire _50_ " << std::endl;
            if (!top->_52_) std::cout << "[TB] Triggered by wire _52_ " << std::endl;
            if (!top->_54_) std::cout << "[TB] Triggered by wire _54_ " << std::endl;
            if (!top->_56_) std::cout << "[TB] Triggered by wire _56_ " << std::endl;
            if (!top->_58_) std::cout << "[TB] Triggered by wire _58_ " << std::endl;
            if (!top->_60_) std::cout << "[TB] Triggered by wire _60_ " << std::endl;
            trigger=true;
        }
    }

    std::cerr << "[TB] PASS (" << cycles << " cycles)" << std::endl;
    tfp->close();
    delete top;
    delete tfp;
    if (trigger)
        return 1;
    return 0;
}
