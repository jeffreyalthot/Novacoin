#pragma once

#include <cstdint>
#include <string>

struct Transaction {
    std::string from;
    std::string to;
    double amount{0.0};
    std::uint64_t timestamp{0};

    std::string serialize() const;
};
