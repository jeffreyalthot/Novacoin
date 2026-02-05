#pragma once

#include <cstdint>
#include <string>

using Amount = std::int64_t;

struct Transaction {
    std::string from;
    std::string to;
    Amount amount{0};
    std::uint64_t timestamp{0};
    Amount fee{0};

    std::string serialize() const;
    std::string id() const;

    static constexpr Amount kCoin = 100'000'000;
    static Amount fromNOVA(double value);
    static double toNOVA(Amount value);
};
