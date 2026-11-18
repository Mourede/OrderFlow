#include "orderflow/inventory.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace orderflow {

namespace {

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace

void Inventory::add_stock(const std::string& sku, int quantity) {
    if (sku.empty()) {
        throw std::invalid_argument("SKU must not be empty");
    }
    if (quantity < 0) {
        throw std::invalid_argument("quantity must not be negative for " + sku);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    stock_[sku] += quantity;
}

bool Inventory::reserve_all(const std::vector<OrderLine>& lines) {
    // Combine duplicate SKUs so an order asking for the same item twice
    // is checked against its total.
    std::unordered_map<std::string, int> needed;
    for (const auto& line : lines) {
        if (line.quantity <= 0) {
            return false;
        }
        needed[line.sku] += line.quantity;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [sku, qty] : needed) {
        auto it = stock_.find(sku);
        if (it == stock_.end() || it->second < qty) {
            return false;
        }
    }
    for (const auto& [sku, qty] : needed) {
        stock_[sku] -= qty;
    }
    return true;
}

int Inventory::stock(const std::string& sku) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stock_.find(sku);
    return it == stock_.end() ? 0 : it->second;
}

long long Inventory::total_units() const {
    std::lock_guard<std::mutex> lock(mutex_);
    long long total = 0;
    for (const auto& [sku, qty] : stock_) {
        total += qty;
    }
    return total;
}

std::vector<std::string> Inventory::skus() const {
    std::vector<std::string> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result.reserve(stock_.size());
        for (const auto& [sku, qty] : stock_) {
            result.push_back(sku);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void Inventory::load_csv(std::istream& in) {
    std::string raw;
    int line_number = 0;
    while (std::getline(in, raw)) {
        ++line_number;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto comma = line.find(',');
        if (comma == std::string::npos) {
            throw std::runtime_error("line " + std::to_string(line_number) +
                                     ": expected \"sku,quantity\"");
        }
        const std::string sku = trim(line.substr(0, comma));
        const std::string qty_text = trim(line.substr(comma + 1));
        if (line_number == 1 && sku == "sku") {
            continue;  // header row
        }

        int quantity = 0;
        std::size_t used = 0;
        try {
            quantity = std::stoi(qty_text, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        if (sku.empty() || used == 0 || used != qty_text.size() || quantity < 0) {
            throw std::runtime_error("line " + std::to_string(line_number) +
                                     ": invalid entry \"" + line + "\"");
        }
        add_stock(sku, quantity);
    }
}

}  // namespace orderflow
