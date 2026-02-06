#pragma once

#include <string>
#include <vector>

class SeedRegistry {
public:
    explicit SeedRegistry(std::size_t maxSeeds = 64);

    [[nodiscard]] bool addSeed(const std::string& endpoint);
    [[nodiscard]] bool removeSeed(const std::string& endpoint);
    [[nodiscard]] bool hasSeed(const std::string& endpoint) const;

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t maxSeeds() const;
    [[nodiscard]] bool isFull() const;

    [[nodiscard]] std::vector<std::string> listSeeds() const;

private:
    std::size_t maxSeeds_ = 64;
    std::vector<std::string> seeds_;

    static bool isEndpointValid(const std::string& endpoint);
};
