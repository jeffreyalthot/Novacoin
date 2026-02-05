#pragma once

#include "block.hpp"
#include "consensus.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <string>
#include <vector>

struct AddressStats {
    Amount totalReceived = 0;
    Amount totalSent = 0;
    Amount feesPaid = 0;
    Amount minedRewards = 0;
    Amount pendingOutgoing = 0;
    std::size_t outgoingTransactionCount = 0;
    std::size_t incomingTransactionCount = 0;
    std::size_t minedBlockCount = 0;
};

struct NetworkStats {
    std::size_t blockCount = 0;
    std::size_t userTransactionCount = 0;
    std::size_t coinbaseTransactionCount = 0;
    std::size_t pendingTransactionCount = 0;
    Amount totalTransferred = 0;
    Amount totalFeesPaid = 0;
    Amount totalMinedRewards = 0;
    Amount medianUserTransactionAmount = 0;
};

struct MempoolStats {
    std::size_t transactionCount = 0;
    Amount totalAmount = 0;
    Amount totalFees = 0;
    Amount minFee = 0;
    Amount maxFee = 0;
    Amount medianFee = 0;
};

struct BlockHeaderInfo {
    std::uint64_t index = 0;
    std::string hash;
    std::string previousHash;
    std::uint64_t timestamp = 0;
    unsigned int difficulty = 0;
};

struct BlockSummary {
    std::uint64_t index = 0;
    std::string hash;
    std::string previousHash;
    std::uint64_t timestamp = 0;
    unsigned int difficulty = 0;
    std::size_t transactionCount = 0;
    std::size_t userTransactionCount = 0;
    Amount totalFees = 0;
};

struct TransactionLookup {
    Transaction tx;
    bool isConfirmed = false;
    std::size_t confirmations = 0;
    std::optional<std::size_t> blockHeight;
};

class Blockchain {
public:
    static constexpr Amount kMaxSupply = consensus::kMaxSupply;
    static constexpr std::size_t kHalvingInterval = consensus::kHalvingInterval;
    static constexpr std::uint64_t kMaxFutureDriftSeconds = consensus::kMaxFutureDriftSeconds;
    static constexpr std::uint64_t kTargetBlockTimeSeconds = consensus::kTargetBlockTimeSeconds;
    static constexpr std::uint64_t kMempoolExpirySeconds = consensus::kMempoolExpirySeconds;
    static constexpr std::size_t kMaxMempoolTransactions = consensus::kMaxMempoolTransactions;
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
    [[nodiscard]] std::uint64_t getMedianTimePast() const;
    [[nodiscard]] std::uint64_t estimateNextMinimumTimestamp() const;
    [[nodiscard]] std::size_t getBlockCount() const;
    [[nodiscard]] std::vector<Transaction> getTransactionHistory(const std::string& address) const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::uint64_t getCumulativeWork() const;
    [[nodiscard]] std::string getChainSummary() const;
    [[nodiscard]] std::size_t getLastReorgDepth() const;
    [[nodiscard]] std::size_t getLastForkHeight() const;
    [[nodiscard]] std::size_t getReorgCount() const;
    [[nodiscard]] std::vector<Transaction> getPendingTransactionsForBlockTemplate() const;
    [[nodiscard]] AddressStats getAddressStats(const std::string& address) const;
    [[nodiscard]] NetworkStats getNetworkStats() const;
    [[nodiscard]] MempoolStats getMempoolStats() const;
    [[nodiscard]] Amount estimateRequiredFeeForInclusion(std::size_t targetBlocks) const;
    [[nodiscard]] std::vector<std::pair<std::string, Amount>> getTopBalances(std::size_t limit) const;
    [[nodiscard]] std::vector<BlockHeaderInfo> getHeadersFromHeight(std::size_t startHeight,
                                                                    std::size_t maxCount) const;
    [[nodiscard]] std::vector<std::string> getBlockLocatorHashes() const;
    [[nodiscard]] std::optional<std::size_t> findHighestLocatorMatch(
        const std::vector<std::string>& locatorHashes) const;
    [[nodiscard]] std::vector<BlockHeaderInfo> getHeadersForLocator(const std::vector<std::string>& locatorHashes,
                                                                     std::size_t maxCount) const;
    [[nodiscard]] std::vector<BlockHeaderInfo> getHeadersForLocatorWithStop(
        const std::vector<std::string>& locatorHashes,
        std::size_t maxCount,
        const std::string& stopHash) const;
    [[nodiscard]] std::vector<BlockSummary> getBlocksFromHeight(std::size_t startHeight,
                                                                std::size_t maxCount) const;
    [[nodiscard]] std::vector<BlockSummary> getBlocksForLocator(const std::vector<std::string>& locatorHashes,
                                                                 std::size_t maxCount) const;
    [[nodiscard]] std::vector<BlockSummary> getBlocksForLocatorWithStop(
        const std::vector<std::string>& locatorHashes,
        std::size_t maxCount,
        const std::string& stopHash) const;
    [[nodiscard]] std::optional<BlockSummary> getBlockSummaryByHeight(std::size_t height) const;
    [[nodiscard]] std::optional<BlockSummary> getBlockSummaryByHash(const std::string& hash) const;
    [[nodiscard]] std::vector<BlockSummary> getRecentBlockSummaries(std::size_t maxCount) const;
    [[nodiscard]] std::optional<TransactionLookup> findTransactionById(const std::string& txId) const;
    [[nodiscard]] bool tryAdoptChain(const std::vector<Block>& candidateChain);
    [[nodiscard]] const std::vector<Block>& getChain() const;
    [[nodiscard]] const std::vector<Transaction>& getPendingTransactions() const;

private:
    [[nodiscard]] Amount blockSubsidyAtHeight(std::size_t height) const;
    [[nodiscard]] bool isTimestampAcceptable(std::uint64_t timestamp) const;
    [[nodiscard]] bool isMempoolTransactionExpired(const Transaction& tx, std::uint64_t now) const;
    void pruneExpiredPendingTransactions();
    [[nodiscard]] unsigned int expectedDifficultyAtHeight(std::size_t height,
                                                          const std::vector<Block>& referenceChain) const;
    [[nodiscard]] std::uint64_t medianTimePastAtHeight(std::size_t height,
                                                       const std::vector<Block>& referenceChain) const;
    [[nodiscard]] bool isChainValid(const std::vector<Block>& candidateChain) const;
    [[nodiscard]] std::uint64_t computeCumulativeWork(const std::vector<Block>& chain) const;
    [[nodiscard]] std::unordered_set<std::string> buildUserTransactionIdSet(const std::vector<Block>& source) const;
    [[nodiscard]] std::vector<Transaction> collectDetachedTransactions(const std::vector<Block>& oldChain,
                                                                       const std::vector<Block>& newChain) const;
    void rebuildPendingTransactionsAfterReorg(const std::vector<Block>& oldChain,
                                              const std::vector<Block>& newChain);
    [[nodiscard]] std::size_t commonPrefixLength(const std::vector<Block>& lhs,
                                                 const std::vector<Block>& rhs) const;
    [[nodiscard]] std::optional<std::size_t> findBlockHeightByHash(const std::string& hash) const;
    [[nodiscard]] BlockSummary makeBlockSummary(const Block& block) const;

    unsigned int initialDifficulty_;
    Amount miningReward_;
    std::size_t maxTransactionsPerBlock_;
    std::vector<Block> chain_;
    std::vector<Transaction> pendingTransactions_;
    std::size_t lastReorgDepth_ = 0;
    std::size_t lastForkHeight_ = 0;
    std::size_t reorgCount_ = 0;

    Block createGenesisBlock() const;
};
