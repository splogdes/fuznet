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
        top->_01_ = rnd_bit();
        top->_07_ = rnd_bit();
        top->_10_ = rnd_bit();

        top->clk = 1;
        top->eval();
        tfp->dump(i * 10 + 5);
        if (top->trigger) {
            std::cerr << "[TB] Triggered at cycle " << i << std::endl;
            if (!top->_53_) std::cout << "[TB] Triggered by wire _53_ " << std::endl;
            if (!top->_73_) std::cout << "[TB] Triggered by wire _73_ " << std::endl;
            if (!top->_75_) std::cout << "[TB] Triggered by wire _75_ " << std::endl;
            if (!top->_77_) std::cout << "[TB] Triggered by wire _77_ " << std::endl;
            if (!top->_79_) std::cout << "[TB] Triggered by wire _79_ " << std::endl;
            if (!top->_81_) std::cout << "[TB] Triggered by wire _81_ " << std::endl;
            if (!top->_83_) std::cout << "[TB] Triggered by wire _83_ " << std::endl;
            if (!top->_85_) std::cout << "[TB] Triggered by wire _85_ " << std::endl;
            if (!top->_87_) std::cout << "[TB] Triggered by wire _87_ " << std::endl;
            if (!top->_89_) std::cout << "[TB] Triggered by wire _89_ " << std::endl;
            if (!top->_91_) std::cout << "[TB] Triggered by wire _91_ " << std::endl;
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
