#include <gtest/gtest.h>

#include <atomic>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "orderflow/inventory.hpp"
#include "orderflow/simulator.hpp"
#include "orderflow/thread_safe_queue.hpp"

using namespace orderflow;

// ---------- ThreadSafeQueue ----------

TEST(ThreadSafeQueue, KeepsFifoOrder) {
    ThreadSafeQueue<int> q;
    for (int i = 0; i < 5; ++i) q.push(i);
    q.close();
    for (int i = 0; i < 5; ++i) EXPECT_EQ(q.pop().value(), i);
    EXPECT_FALSE(q.pop().has_value());
}

TEST(ThreadSafeQueue, RejectsPushAfterClose) {
    ThreadSafeQueue<int> q;
    q.close();
    EXPECT_FALSE(q.push(1));
    EXPECT_EQ(q.size(), 0u);
}

TEST(ThreadSafeQueue, ManyProducersAndConsumersDeliverEachItemOnce) {
    ThreadSafeQueue<int> q;
    constexpr int kProducers = 4, kConsumers = 4, kPerProducer = 2500;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) q.push(p * kPerProducer + i);
        });
    }

    std::mutex seen_mutex;
    std::multiset<int> seen;
    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (auto item = q.pop()) {
                std::lock_guard<std::mutex> lock(seen_mutex);
                seen.insert(*item);
            }
        });
    }

    for (auto& t : producers) t.join();
    q.close();
    for (auto& t : consumers) t.join();

    ASSERT_EQ(seen.size(), static_cast<size_t>(kProducers * kPerProducer));
    for (int i = 0; i < kProducers * kPerProducer; ++i) EXPECT_EQ(seen.count(i), 1u);
}

// ---------- Inventory ----------

TEST(Inventory, ReservesWhenStockIsAvailable) {
    Inventory inv;
    inv.add_stock("A", 10);
    EXPECT_TRUE(inv.reserve_all({{"A", 4}}));
    EXPECT_EQ(inv.stock("A"), 6);
}

TEST(Inventory, ReservationIsAllOrNothing) {
    Inventory inv;
    inv.add_stock("A", 10);
    inv.add_stock("B", 1);
    EXPECT_FALSE(inv.reserve_all({{"A", 5}, {"B", 2}}));
    EXPECT_EQ(inv.stock("A"), 10);  // untouched
    EXPECT_EQ(inv.stock("B"), 1);
}

TEST(Inventory, DuplicateSkusInOneOrderAreCombined) {
    Inventory inv;
    inv.add_stock("A", 5);
    EXPECT_FALSE(inv.reserve_all({{"A", 3}, {"A", 3}}));
    EXPECT_EQ(inv.stock("A"), 5);
}

TEST(Inventory, RejectsUnknownSkuAndBadQuantity) {
    Inventory inv;
    inv.add_stock("A", 5);
    EXPECT_FALSE(inv.reserve_all({{"MISSING", 1}}));
    EXPECT_FALSE(inv.reserve_all({{"A", 0}}));
    EXPECT_THROW(inv.add_stock("A", -1), std::invalid_argument);
}

TEST(Inventory, NeverOversellsUnderConcurrency) {
    Inventory inv;
    inv.add_stock("A", 100);
    std::atomic<int> successes{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 50; ++i) {
                if (inv.reserve_all({{"A", 1}})) ++successes;
            }
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(successes.load(), 100);  // 400 attempts, exactly 100 units
    EXPECT_EQ(inv.stock("A"), 0);
}

TEST(Inventory, LoadsCsvWithHeaderAndComments) {
    std::istringstream csv("sku,quantity\n# comment\n\nA, 3\nB,7\nA,2\n");
    Inventory inv;
    inv.load_csv(csv);
    EXPECT_EQ(inv.stock("A"), 5);
    EXPECT_EQ(inv.stock("B"), 7);
    EXPECT_EQ(inv.total_units(), 12);
}

TEST(Inventory, CsvErrorsReportLineNumber) {
    std::istringstream csv("A,3\nB,seven\n");
    Inventory inv;
    try {
        inv.load_csv(csv);
        FAIL() << "expected an exception";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("line 2"), std::string::npos);
    }
}

// ---------- Simulator ----------

namespace {
void stock_sample(Inventory& inv) {
    inv.add_stock("A", 50);
    inv.add_stock("B", 50);
    inv.add_stock("C", 50);
}
}  // namespace

TEST(Simulator, AccountsForEveryOrderAndUnit) {
    Inventory inv;
    stock_sample(inv);
    const long long before = inv.total_units();

    SimConfig cfg;
    cfg.workers = 4;
    cfg.orders = 500;
    cfg.pick_time_us = 0;
    const SimResult r = run_simulation(inv, cfg);

    EXPECT_EQ(r.fulfilled + r.backordered, cfg.orders);
    EXPECT_EQ(r.units_shipped, before - inv.total_units());
    int sum = 0;
    for (int n : r.orders_per_worker) sum += n;
    EXPECT_EQ(sum, cfg.orders);
}

TEST(Simulator, SingleWorkerIsDeterministicForASeed) {
    SimConfig cfg;
    cfg.workers = 1;
    cfg.orders = 300;
    cfg.pick_time_us = 0;

    Inventory first, second;
    stock_sample(first);
    stock_sample(second);
    const SimResult a = run_simulation(first, cfg);
    const SimResult b = run_simulation(second, cfg);

    EXPECT_EQ(a.fulfilled, b.fulfilled);
    EXPECT_EQ(a.units_shipped, b.units_shipped);
    EXPECT_EQ(first.stock("A"), second.stock("A"));
}

TEST(Simulator, RejectsInvalidConfig) {
    Inventory inv;
    stock_sample(inv);
    SimConfig cfg;
    cfg.workers = 0;
    EXPECT_THROW(run_simulation(inv, cfg), std::invalid_argument);

    Inventory empty;
    EXPECT_THROW(run_simulation(empty, SimConfig{}), std::invalid_argument);
}
