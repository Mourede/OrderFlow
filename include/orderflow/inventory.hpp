#pragma once

#include <istream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace orderflow {

struct OrderLine {
    std::string sku;
    int quantity = 0;
};

// Thread-safe stock levels keyed by SKU.
class Inventory {
public:
    // Adds stock for a SKU (creates the SKU if it does not exist).
    void add_stock(const std::string& sku, int quantity);

    // Reserves every line of an order, or none of them. Returns false and
    // leaves stock untouched if any line cannot be filled. This prevents
    // partially picked orders and overselling under concurrency.
    bool reserve_all(const std::vector<OrderLine>& lines);

    int stock(const std::string& sku) const;
    long long total_units() const;
    std::vector<std::string> skus() const;  // sorted

    // Loads "sku,quantity" lines. Blank lines and lines starting with '#'
    // are skipped; a header line "sku,quantity" is allowed. Throws
    // std::runtime_error with the line number on malformed input.
    void load_csv(std::istream& in);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, int> stock_;
};

}  // namespace orderflow
