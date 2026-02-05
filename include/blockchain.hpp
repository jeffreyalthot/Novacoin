#pragma once

#include "block.hpp"

#include <string>
#include <vector>

class Blockchain {
public:
    explicit Blockchain(unsigned int difficulty = 4, double miningReward = 12.5);

    void createTransaction(const Transaction& tx);
    void minePendingTransactions(const std::string& minerAddress);

    [[nodiscard]] double getBalance(const std::string& address) const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] const std::vector<Block>& getChain() const;
    [[nodiscard]] const std::vector<Transaction>& getPendingTransactions() const;

private:
    unsigned int difficulty_;
    double miningReward_;
    std::vector<Block> chain_;
    std::vector<Transaction> pendingTransactions_;

    Block createGenesisBlock() const;
};
