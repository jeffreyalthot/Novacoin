#pragma once

#include "transaction.hpp"

#include <cstdint>
#include <string>
#include <vector>

class Block {
public:
    Block(std::uint64_t index, std::string previousHash, std::vector<Transaction> transactions);

    void mine(unsigned int difficulty);
    bool hasValidHash(unsigned int difficulty) const;

    [[nodiscard]] std::uint64_t getIndex() const;
    [[nodiscard]] const std::string& getPreviousHash() const;
    [[nodiscard]] const std::string& getHash() const;
    [[nodiscard]] std::uint64_t getNonce() const;
    [[nodiscard]] std::uint64_t getTimestamp() const;
    [[nodiscard]] const std::vector<Transaction>& getTransactions() const;

private:
    std::uint64_t index_;
    std::string previousHash_;
    std::vector<Transaction> transactions_;
    std::uint64_t timestamp_;
    std::uint64_t nonce_;
    std::string hash_;

    std::string computeHash() const;
};
