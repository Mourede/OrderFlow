#include "orderflow/simulator.hpp"

#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

#include "orderflow/thread_safe_queue.hpp"

namespace orderflow {

namespace {

struct Order {
    int id = 0;
    std::vector<OrderLine> lines;
};

void validate(const SimConfig& c) {
    if (c.workers < 1) throw std::invalid_argument("workers must be >= 1");
    if (c.orders < 0) throw std::invalid_argument("orders must be >= 0");
    if (c.max_lines < 1) throw std::invalid_argument("max_lines must be >= 1");
    if (c.max_quantity < 1) throw std::invalid_argument("max_quantity must be >= 1");
    if (c.pick_time_us < 0) throw std::invalid_argument("pick_time_us must be >= 0");
}

}  // namespace

SimResult run_simulation(Inventory& inventory, const SimConfig& config) {
    validate(config);
    const std::vector<std::string> skus = inventory.skus();
    if (skus.empty()) {
        throw std::invalid_argument("inventory has no SKUs");
    }

    ThreadSafeQueue<Order> queue;
    std::atomic<int> fulfilled{0};
    std::atomic<int> backordered{0};
    std::atomic<long long> units_shipped{0};
    // Each worker writes only its own slot, so no lock is needed.
    std::vector<int> per_worker(config.workers, 0);

    const auto start = std::chrono::steady_clock::now();

    // Producer: generates orders deterministically from the seed.
    std::thread producer([&] {
        std::mt19937 rng(config.seed);
        std::uniform_int_distribution<int> line_count(1, config.max_lines);
        std::uniform_int_distribution<int> quantity(1, config.max_quantity);
        std::uniform_int_distribution<std::size_t> sku_index(0, skus.size() - 1);

        for (int id = 1; id <= config.orders; ++id) {
            Order order;
            order.id = id;
            const int n = line_count(rng);
            for (int i = 0; i < n; ++i) {
                order.lines.push_back({skus[sku_index(rng)], quantity(rng)});
            }
            queue.push(std::move(order));
        }
        queue.close();  // tells workers no more orders are coming
    });

    // Consumers: pick orders until the queue is closed and drained.
    std::vector<std::thread> workers;
    workers.reserve(config.workers);
    for (int w = 0; w < config.workers; ++w) {
        workers.emplace_back([&, w] {
            while (auto order = queue.pop()) {
                if (config.pick_time_us > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(config.pick_time_us));
                }
                if (inventory.reserve_all(order->lines)) {
                    long long units = 0;
                    for (const auto& line : order->lines) units += line.quantity;
                    units_shipped += units;
                    ++fulfilled;
                } else {
                    ++backordered;
                }
                ++per_worker[w];
            }
        });
    }

    producer.join();
    for (auto& t : workers) t.join();

    const auto end = std::chrono::steady_clock::now();

    SimResult result;
    result.fulfilled = fulfilled.load();
    result.backordered = backordered.load();
    result.units_shipped = units_shipped.load();
    result.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
    result.orders_per_worker = std::move(per_worker);
    return result;
}

}  // namespace orderflow
