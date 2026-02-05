#pragma once

#include "block.hpp"
#include "consensus.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Blockchain {
public:
    static constexpr Amount kMaxSupply = consensus::kMaxSupply;
    static constexpr std::size_t kHalvingInterval = consensus::kHalvingInterval;
    static constexpr std::uint64_t kMaxFutureDriftSeconds = consensus::kMaxFutureDriftSeconds;
    static constexpr std::uint64_t kTargetBlockTimeSeconds = consensus::kTargetBlockTimeSeconds;
    static constexpr std::size_t kDifficultyAdjustmentInterval = consensus::kDifficultyAdjustmentInterval;
    static constexpr unsigned int kMinDifficulty = consensus::kMinDifficulty;
    static constexpr unsigned int kMaxDifficulty = consensus::kMaxDifficulty;
    static constexpr Amount kMinRelayFee = consensus::kMinRelayFee;

    explicit Blockchain(unsigned int difficulty = 4,
                        Amount miningReward = consensus::kInitialBlockReward,
                        std::size_t maxTransactionsPerBlock = 100);

    void createTransaction(const Transaction& tx);
    void minePendingTransactions(const std::string& minerAddress);

    [[nodiscard]] Amount getBalance(const std::string& address) const;
    [[nodiscard]] Amount getAvailableBalance(const std::string& address) const;
    [[nodiscard]] Amount estimateNextMiningReward() const;
    [[nodiscard]] Amount getTotalSupply() const;
    [[nodiscard]] unsigned int getCurrentDifficulty() const;
    [[nodiscard]] unsigned int estimateNextDifficulty() const;
    [[nodiscard]] std::size_t getBlockCount() const;
    [[nodiscard]] std::vector<Transaction> getTransactionHistory(const std::string& address) const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::uint64_t getCumulativeWork() const;
    [[nodiscard]] std::string getChainSummary() const;
    [[nodiscard]] std::vector<Transaction> getPendingTransactionsForBlockTemplate() const;
    [[nodiscard]] bool tryAdoptChain(const std::vector<Block>& candidateChain);
    [[nodiscard]] const std::vector<Block>& getChain() const;
    [[nodiscard]] const std::vector<Transaction>& getPendingTransactions() const;

private:
    [[nodiscard]] Amount blockSubsidyAtHeight(std::size_t height) const;
    [[nodiscard]] bool isTimestampAcceptable(std::uint64_t timestamp) const;
    [[nodiscard]] unsigned int expectedDifficultyAtHeight(std::size_t height,
                                                          const std::vector<Block>& referenceChain) const;
    [[nodiscard]] bool isChainValid(const std::vector<Block>& candidateChain) const;
    [[nodiscard]] std::uint64_t computeCumulativeWork(const std::vector<Block>& chain) const;

    unsigned int initialDifficulty_;
    Amount miningReward_;
    std::size_t maxTransactionsPerBlock_;
    std::vector<Block> chain_;
    std::vector<Transaction> pendingTransactions_;

    Block createGenesisBlock() const;
};
