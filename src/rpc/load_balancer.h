#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <random>
#include <mutex>

enum class LoadBalanceType { ROUND_ROBIN, RANDOM };

class LoadBalancer {
public:
    explicit LoadBalancer(LoadBalanceType type = LoadBalanceType::ROUND_ROBIN)
        : type_(type), index_(0), rng_(std::random_device{}()) {}

    std::string select(const std::vector<std::string>& addresses);

private:
    LoadBalanceType type_;
    std::atomic<uint32_t> index_;
    std::mt19937 rng_;
    std::mutex rngMtx_;
};

inline std::string LoadBalancer::select(const std::vector<std::string>& addresses) {
    if (addresses.empty()) return "";
    if (type_ == LoadBalanceType::ROUND_ROBIN) {
        uint32_t idx = index_.fetch_add(1, std::memory_order_relaxed) % addresses.size();
        return addresses[idx];
    } else {
	std::lock_guard<std::mutex> lock(rngMtx_);
        std::uniform_int_distribution<size_t> dis(0, addresses.size() - 1);
        return addresses[dis(rng_)];
    }
}
