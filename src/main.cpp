#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "orderflow/inventory.hpp"
#include "orderflow/simulator.hpp"

namespace {

void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Simulates a warehouse: one thread generates customer orders and\n"
        << "several picker threads fill them from shared inventory.\n\n"
        << "Options:\n"
        << "  --inventory <file>  CSV of sku,quantity (default: data/inventory.csv)\n"
        << "  --workers <n>       picker threads (default: 4)\n"
        << "  --orders <n>        orders to generate (default: 1000)\n"
        << "  --seed <n>          random seed (default: 42)\n"
        << "  --pick-us <n>       simulated microseconds per pick (default: 200)\n"
        << "  -h, --help          show this help\n";
}

int parse_int(const std::string& flag, const std::string& value) {
    try {
        std::size_t used = 0;
        const int n = std::stoi(value, &used);
        if (used != value.size()) throw std::invalid_argument("trailing text");
        return n;
    } catch (const std::exception&) {
        throw std::invalid_argument(flag + " expects a whole number, got \"" + value + "\"");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    orderflow::SimConfig config;
    std::string inventory_path = "data/inventory.csv";

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            }
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for " + arg);
            }
            const std::string value = argv[++i];
            if (arg == "--inventory") inventory_path = value;
            else if (arg == "--workers") config.workers = parse_int(arg, value);
            else if (arg == "--orders") config.orders = parse_int(arg, value);
            else if (arg == "--seed") config.seed = static_cast<unsigned>(parse_int(arg, value));
            else if (arg == "--pick-us") config.pick_time_us = parse_int(arg, value);
            else throw std::invalid_argument("unknown option " + arg);
        }

        std::ifstream file(inventory_path);
        if (!file) {
            throw std::runtime_error("cannot open inventory file: " + inventory_path);
        }
        orderflow::Inventory inventory;
        inventory.load_csv(file);
        const long long starting_units = inventory.total_units();

        const orderflow::SimResult r = orderflow::run_simulation(inventory, config);

        const int total = r.fulfilled + r.backordered;
        const double fill_rate = total ? 100.0 * r.fulfilled / total : 0.0;
        const double throughput = r.elapsed_ms > 0 ? total / (r.elapsed_ms / 1000.0) : 0.0;

        std::cout << std::fixed << std::setprecision(1)
                  << "=== OrderFlow simulation ===\n"
                  << "Workers:          " << config.workers << "\n"
                  << "Orders processed: " << total << "\n"
                  << "Fulfilled:        " << r.fulfilled << " (" << fill_rate << "% fill rate)\n"
                  << "Backordered:      " << r.backordered << "\n"
                  << "Units shipped:    " << r.units_shipped << " of " << starting_units << "\n"
                  << "Elapsed:          " << r.elapsed_ms << " ms\n"
                  << "Throughput:       " << throughput << " orders/sec\n\n"
                  << "Orders per worker:\n";
        for (std::size_t w = 0; w < r.orders_per_worker.size(); ++w) {
            std::cout << "  worker " << w + 1 << ": " << r.orders_per_worker[w] << "\n";
        }
        std::cout << "\nRemaining stock:\n";
        const auto skus = inventory.skus();
        std::size_t width = 0;
        for (const auto& sku : skus) {
            width = std::max(width, sku.size());
        }
        for (const auto& sku : skus) {
            std::cout << "  " << std::left << std::setw(static_cast<int>(width + 2)) << sku
                      << inventory.stock(sku) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        std::cerr << "Run with --help for usage.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
