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

    uint32_t seed = 0xb15d635f;
    uint32_t cycles = 100;
    std::mt19937 rng(seed);
    auto rnd_bit = [&]() { return rng() & 1; };

    std::cerr << "[TB] seed=" << seed << " cycles=" << cycles << std::endl;

    for (uint32_t i = 0; i < cycles; ++i) {
        top->clk = 0;
        top->eval();
        tfp->dump(i * 10);
        top->_01_ = rnd_bit();
        top->_02_ = rnd_bit();

        top->clk = 1;
        top->eval();
        tfp->dump(i * 10 + 5);
        if (top->trigger) {
            std::cerr << "[TB] Triggered at cycle " << i << std::endl;
            tfp->close();
            return 1;
        }
    }

    std::cerr << "[TB] PASS (" << cycles << " cycles)" << std::endl;
    tfp->close();
    return 0;
}
