#pragma 0
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <random>
#include <thread>

#include "netlist.hpp"
#include "library.hpp"
#include "commands.hpp"


namespace fuznet {

class Orchestrator {
    public:
        explicit Orchestrator(const std::string& lib_yaml,
                              const std::string& config_toml = "config/probabilities.toml",
                              unsigned seed = std::random_device{}());

        void run(int max_iters = 10'000, int ms_sleep = 10);
        void stop()                     { stop_flag.store(true); }

        ~Orchestrator();

    private:  
        ICommand& pick_command();
        void load_config(std::string_view toml_path);

        struct Entry {
            std::unique_ptr<ICommand> cmd;
            double                    weight;
        };

        Library             library;
        Netlist             netlist;
        std::mt19937_64     rng;
        std::vector<Entry>  commands;
        std::discrete_distribution<int> dist;
        std::atomic<bool>   stop_flag{false};
};

}