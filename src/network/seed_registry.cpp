#include "network/seed_registry.hpp"

#include <algorithm>

SeedRegistry::SeedRegistry(std::size_t maxSeeds) : maxSeeds_(std::max<std::size_t>(1, maxSeeds)) {}

bool SeedRegistry::addSeed(const std::string& endpoint) {
    if (!isEndpointValid(endpoint) || isFull() || hasSeed(endpoint)) {
        return false;
    }

    seeds_.push_back(endpoint);
    return true;
}

bool SeedRegistry::removeSeed(const std::string& endpoint) {
    const auto it = std::remove(seeds_.begin(), seeds_.end(), endpoint);
    if (it == seeds_.end()) {
        return false;
    }

    seeds_.erase(it, seeds_.end());
    return true;
}

bool SeedRegistry::hasSeed(const std::string& endpoint) const {
    return std::find(seeds_.begin(), seeds_.end(), endpoint) != seeds_.end();
}

std::size_t SeedRegistry::size() const {
    return seeds_.size();
}

std::size_t SeedRegistry::maxSeeds() const {
    return maxSeeds_;
}

bool SeedRegistry::isFull() const {
    return seeds_.size() >= maxSeeds_;
}

std::vector<std::string> SeedRegistry::listSeeds() const {
    std::vector<std::string> values = seeds_;
    std::sort(values.begin(), values.end());
    return values;
}

bool SeedRegistry::isEndpointValid(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    return endpoint.find(':') != std::string::npos;
}
