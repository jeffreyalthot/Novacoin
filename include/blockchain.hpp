#pragma once

#include "block.hpp"

#include <cstddef>
#include <string>
#include <vector>

class Blockchain {
public:
    static constexpr double kMaxSupply = 29'000'000.0;
    static constexpr std::size_t kHalvingInterval = 100;

    explicit Blockchain(unsigned int difficulty = 4,
                        double miningReward = 12.5,
                        std::size_t maxTransactionsPerBlock = 100);

    void createTransaction(const Transaction& tx);
    void minePendingTransactions(const std::string& minerAddress);

    [[nodiscard]] double getBalance(const std::string& address) const;
    [[nodiscard]] double getAvailableBalance(const std::string& address) const;
    [[nodiscard]] double estimateNextMiningReward() const;
    [[nodiscard]] double getTotalSupply() const;
    [[nodiscard]] std::size_t getBlockCount() const;
    [[nodiscard]] std::vector<Transaction> getTransactionHistory(const std::string& address) const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::string getChainSummary() const;
    [[nodiscard]] std::vector<Transaction> getPendingTransactionsForBlockTemplate() const;
    [[nodiscard]] const std::vector<Block>& getChain() const;
    [[nodiscard]] const std::vector<Transaction>& getPendingTransactions() const;

private:
    [[nodiscard]] double blockSubsidyAtHeight(std::size_t height) const;

    unsigned int difficulty_;
    double miningReward_;
    std::size_t maxTransactionsPerBlock_;
    std::vector<Block> chain_;
    std::vector<Transaction> pendingTransactions_;

    Block createGenesisBlock() const;
};
