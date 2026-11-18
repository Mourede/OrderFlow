#pragma once

#include <vector>

#include "orderflow/inventory.hpp"

namespace orderflow {

struct SimConfig {
    int workers = 4;          // number of picker threads
    int orders = 1000;        // orders to generate
    unsigned seed = 42;       // random seed for order generation
    int max_lines = 3;        // max distinct SKUs per order
    int max_quantity = 5;     // max units per line
    int pick_time_us = 200;   // simulated time to pick one order
};

struct SimResult {
    int fulfilled = 0;
    int backordered = 0;
    long long units_shipped = 0;
    double elapsed_ms = 0.0;
    std::vector<int> orders_per_worker;
};

// Runs one producer thread that generates random orders and `workers`
// consumer threads that pick them against the shared inventory.
// Throws std::invalid_argument on a bad config or empty inventory.
SimResult run_simulation(Inventory& inventory, const SimConfig& config);

}  // namespace orderflow
