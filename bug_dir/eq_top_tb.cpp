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

    uint32_t seed = 1703316128;
    uint32_t cycles = 100;
    bool trigger = false;

    std::mt19937 rng(seed);
    auto rnd_bit = [&]() { return rng() & 1; };

    std::cerr << "[TB] seed=" << seed << " cycles=" << cycles << std::endl;

    for (uint32_t i = 0; i < cycles; ++i) {
        top->clk = 0;
        top->eval();
        tfp->dump(i * 10);
        top->_005_ = rnd_bit();
        top->_011_ = rnd_bit();
        top->_014_ = rnd_bit();
        top->_015_ = rnd_bit();
        top->_999_ = rnd_bit();

        top->clk = 1;
        top->eval();
        tfp->dump(i * 10 + 5);
        if (top->trigger) {
            std::cerr << "[TB] Triggered at cycle " << i << std::endl;
            if (!top->_091_) std::cout << "[TB] Triggered by wire _091_ " << std::endl;
            if (!top->_097_) std::cout << "[TB] Triggered by wire _097_ " << std::endl;
            if (!top->_101_) std::cout << "[TB] Triggered by wire _101_ " << std::endl;
            trigger = true;
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
