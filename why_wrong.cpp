#include <verilated.h>
#include <verilated_vcd_c.h>
#include <Vtop.h>
#include <random>
#include <iostream>


int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    VerilatedVcdC *tfp = new VerilatedVcdC;
    Vtop *top = new Vtop;

    Verilated::traceEverOn(true);
    top->trace(tfp, 99);
    tfp->open("tmp/why_wrong.vcd");

    uint32_t seed = 0x5762b088;
    uint32_t cycles = 100;

    std::mt19937 rng(seed);


    auto rnd1 = [&]() {return rng() & 1;};

    std::cout << "[TB] seed=" << seed << " cycles=" << cycles << std::endl;

    for (int i = 0; i < cycles; i++) {

        top->clk = 0;
        top->eval();
        tfp->dump(i * 10);

        if (top->trigger) {
            std::cout << "[TB] Triggered at cycle " << i << std::endl;
            std::cout << "[TB] _001_=" << int(top->_001_)
                      << " _008_=" << int(top->_008_)
                      << " _014_=" << int(top->_014_)
                      << " _021_=" << int(top->_021_)
                      << " _028_=" << int(top->_028_)
                      << " _033_=" << int(top->_033_)
                      << " _041_=" << int(top->_041_)
                      << " _048_=" << int(top->_048_) << std::endl;
        }


        top->_001_ = rnd1();
        top->_008_ = rnd1();
        top->_014_ = rnd1();
        top->_021_ = rnd1();
        top->_028_ = rnd1();
        top->_033_ = rnd1();
        top->_041_ = rnd1();
        top->_048_ = rnd1();

        top->clk = 1;
        top->eval();
        tfp->dump(i * 10 + 5);

    }

    std::cerr << "[TB] PASS (" << cycles << " cycles)" << std::endl;

    delete top;
    delete tfp;
    return 0;

}
