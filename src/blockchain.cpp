#include "blockchain.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

bool safeAdd(Amount lhs, Amount rhs, Amount& out) {
    if (rhs > 0 && lhs > std::numeric_limits<Amount>::max() - rhs) {
        return false;
    }
    if (rhs < 0 && lhs < std::numeric_limits<Amount>::min() - rhs) {
        return false;
    }
    out = lhs + rhs;
    return true;
}

bool isTransactionShapeValid(const Transaction& tx) {
    if (tx.from.empty() || tx.to.empty()) {
        return false;
    }

    if (tx.from == "network") {
        return tx.amount >= 0 && tx.fee == 0;
    }

    return tx.amount > 0 && tx.fee >= 0;
}

bool hasNonDecreasingTimestamps(const std::vector<Transaction>& transactions) {
    for (std::size_t i = 1; i < transactions.size(); ++i) {
        if (transactions[i].timestamp < transactions[i - 1].timestamp) {
            return false;
        }
    }
    return true;
}

void applyBalanceUpdates(const std::vector<Transaction>& transactions,
                         const std::string& address,
                         Amount& balance,
                         const char* debitError,
                         const char* creditError) {
    for (const auto& tx : transactions) {
        if (tx.from == address) {
            Amount debit = 0;
            if (!safeAdd(tx.amount, tx.fee, debit) || !safeAdd(balance, -debit, balance)) {
                throw std::overflow_error(debitError);
            }
        }
        if (tx.to == address) {
            if (!safeAdd(balance, tx.amount, balance)) {
                throw std::overflow_error(creditError);
            }
        }
    }
}

std::size_t boundedEndHeight(std::size_t startHeight, std::size_t maxCount, std::size_t chainSize) {
    const std::size_t remaining = chainSize - startHeight;
    return startHeight + std::min(maxCount, remaining);
}

} // namespace

Blockchain::Blockchain(unsigned int difficulty, Amount miningReward, std::size_t maxTransactionsPerBlock)
    : initialDifficulty_(std::clamp(difficulty, kMinDifficulty, kMaxDifficulty)),
      miningReward_(miningReward),
      maxTransactionsPerBlock_(maxTransactionsPerBlock),
      chain_({createGenesisBlock()}),
      pendingTransactions_(),
      hashToHeight_(),
      lastReorgDepth_(0),
      lastForkHeight_(0),
      lastForkHash_(),
      reorgCount_(0) {
    if (maxTransactionsPerBlock_ == 0) {
        throw std::invalid_argument("La taille maximale d'un bloc doit être > 0.");
    }
    if (miningReward_ < 0) {
        throw std::invalid_argument("La récompense de minage doit être >= 0.");
    }
    rebuildHashIndex();
}

Block Blockchain::createGenesisBlock() const {
    Transaction bootstrap{"network", "genesis", 0, nowSeconds(), 0};
    Block genesis{0, "0", {bootstrap}, initialDifficulty_};
    genesis.mine();
    return genesis;
}

Amount Blockchain::blockSubsidyAtHeight(std::size_t height) const {
    const std::size_t halvings = height / kHalvingInterval;
    if (halvings >= 63) {
        return 0;
    }

    const Amount reward = miningReward_ >> halvings;
    return reward;
}

bool Blockchain::isTimestampAcceptable(std::uint64_t timestamp) const {
    return timestamp <= nowSeconds() + kMaxFutureDriftSeconds;
}

bool Blockchain::isMempoolTransactionExpired(const Transaction& tx, std::uint64_t now) const {
    if (tx.timestamp >= now) {
        return false;
    }
    return now - tx.timestamp > kMempoolExpirySeconds;
}

void Blockchain::pruneExpiredPendingTransactions() {
    const std::uint64_t now = nowSeconds();
    pendingTransactions_.erase(
        std::remove_if(pendingTransactions_.begin(), pendingTransactions_.end(),
                       [this, now](const Transaction& tx) { return isMempoolTransactionExpired(tx, now); }),
        pendingTransactions_.end());
}

unsigned int Blockchain::expectedDifficultyAtHeight(std::size_t height,
                                                   const std::vector<Block>& referenceChain) const {
    if (height == 0 || referenceChain.empty()) {
        return initialDifficulty_;
    }

    const unsigned int previousDifficulty = referenceChain[height - 1].getDifficulty();
    if (height % kDifficultyAdjustmentInterval != 0 || height < kDifficultyAdjustmentInterval) {
        return previousDifficulty;
    }

    const std::uint64_t firstTimestamp = referenceChain[height - kDifficultyAdjustmentInterval].getTimestamp();
    const std::uint64_t lastTimestamp = referenceChain[height - 1].getTimestamp();
    const std::uint64_t actualTimespan = std::max<std::uint64_t>(1, lastTimestamp - firstTimestamp);
    const std::uint64_t targetTimespan = kDifficultyAdjustmentInterval * kTargetBlockTimeSeconds;

    if (actualTimespan < targetTimespan / 2) {
        return std::min(kMaxDifficulty, previousDifficulty + 1);
    }
    if (actualTimespan > targetTimespan * 2) {
        return std::max(kMinDifficulty, previousDifficulty - 1);
    }

    return previousDifficulty;
}

std::uint64_t Blockchain::medianTimePastAtHeight(std::size_t height,
                                                  const std::vector<Block>& referenceChain) const {
    if (referenceChain.empty()) {
        return 0;
    }

    const std::size_t clampedHeight = std::min(height, referenceChain.size() - 1);
    const std::size_t windowSize = 11;
    const std::size_t begin = clampedHeight + 1 >= windowSize ? clampedHeight + 1 - windowSize : 0;

    std::vector<std::uint64_t> timestamps;
    timestamps.reserve(clampedHeight - begin + 1);
    for (std::size_t i = begin; i <= clampedHeight; ++i) {
        timestamps.push_back(referenceChain[i].getTimestamp());
    }

    std::sort(timestamps.begin(), timestamps.end());
    return timestamps[timestamps.size() / 2];
}


void Blockchain::createTransaction(const Transaction& tx) {
    pruneExpiredPendingTransactions();
    if (!isTransactionShapeValid(tx)) {
        throw std::invalid_argument("Transaction invalide (adresses, montant ou frais).");
    }
    if (tx.from == "network") {
        throw std::invalid_argument("Les transactions network sont réservées au consensus (coinbase).");
    }
    if (!isTimestampAcceptable(tx.timestamp)) {
        throw std::invalid_argument("Horodatage transaction trop dans le futur.");
    }
    if (tx.fee < kMinRelayFee) {
        throw std::invalid_argument("Frais insuffisants pour entrer en mempool.");
    }

    Amount debit = 0;
    if (!safeAdd(tx.amount, tx.fee, debit) || debit > getAvailableBalance(tx.from)) {
        throw std::invalid_argument("Fonds insuffisants pour créer cette transaction (montant + frais).");
    }

    const std::string txId = tx.id();
    for (const auto& block : chain_) {
        for (const auto& confirmedTx : block.getTransactions()) {
            if (confirmedTx.from != "network" && confirmedTx.id() == txId) {
                throw std::invalid_argument("Transaction deja confirmee dans la chaine.");
            }
        }
    }

    const auto isDuplicate = std::any_of(
        pendingTransactions_.begin(), pendingTransactions_.end(),
        [&txId](const Transaction& candidate) { return candidate.id() == txId; });
    if (isDuplicate) {
        throw std::invalid_argument("Transaction en double detectee dans la mempool locale.");
    }

    if (pendingTransactions_.size() >= kMaxMempoolTransactions) {
        auto evictIt = std::min_element(
            pendingTransactions_.begin(), pendingTransactions_.end(), [](const Transaction& lhs, const Transaction& rhs) {
                if (lhs.fee != rhs.fee) {
                    return lhs.fee < rhs.fee;
                }
                return lhs.timestamp < rhs.timestamp;
            });

        if (evictIt == pendingTransactions_.end() || tx.fee <= evictIt->fee) {
            throw std::invalid_argument("Mempool pleine: frais trop faibles pour eviction.");
        }

        pendingTransactions_.erase(evictIt);
    }

    pendingTransactions_.push_back(tx);
}

void Blockchain::minePendingTransactions(const std::string& minerAddress) {
    pruneExpiredPendingTransactions();
    if (minerAddress.empty()) {
        throw std::invalid_argument("L'adresse du mineur ne peut pas être vide.");
    }

    const std::size_t nextHeight = chain_.size();
    const Amount baseReward = blockSubsidyAtHeight(nextHeight);
    const Amount remainingSupply = std::max<Amount>(0, kMaxSupply - getTotalSupply());
    if (pendingTransactions_.empty() && baseReward <= 0) {
        return;
    }

    const std::size_t maxUserTransactions = maxTransactionsPerBlock_ > 0 ? maxTransactionsPerBlock_ - 1 : 0;

    std::vector<std::size_t> order(pendingTransactions_.size());
    for (std::size_t i = 0; i < pendingTransactions_.size(); ++i) {
        order[i] = i;
    }

    std::stable_sort(order.begin(), order.end(), [this](std::size_t lhs, std::size_t rhs) {
        return pendingTransactions_[lhs].fee > pendingTransactions_[rhs].fee;
    });

    std::vector<Transaction> transactionsToMine;
    transactionsToMine.reserve(maxUserTransactions + 1);

    std::unordered_map<std::string, Amount> projectedBalance;
    Amount collectedFees = 0;

    const std::uint64_t now = nowSeconds();
    for (const std::size_t idx : order) {
        if (transactionsToMine.size() >= maxUserTransactions) {
            break;
        }

        const Transaction& candidate = pendingTransactions_[idx];
        if (!isTimestampAcceptable(candidate.timestamp) || isMempoolTransactionExpired(candidate, now)) {
            continue;
        }

        if (projectedBalance.find(candidate.from) == projectedBalance.end()) {
            projectedBalance[candidate.from] = getBalance(candidate.from);
        }
        if (projectedBalance.find(candidate.to) == projectedBalance.end()) {
            projectedBalance[candidate.to] = getBalance(candidate.to);
        }

        Amount debit = 0;
        if (!safeAdd(candidate.amount, candidate.fee, debit) || projectedBalance[candidate.from] < debit) {
            continue;
        }

        projectedBalance[candidate.from] -= debit;
        Amount newRecipient = 0;
        if (!safeAdd(projectedBalance[candidate.to], candidate.amount, newRecipient)) {
            continue;
        }
        projectedBalance[candidate.to] = newRecipient;

        Amount newFees = 0;
        if (!safeAdd(collectedFees, candidate.fee, newFees)) {
            continue;
        }
        collectedFees = newFees;
        transactionsToMine.push_back(candidate);
    }

    Amount scheduledReward = 0;
    if (!safeAdd(baseReward, collectedFees, scheduledReward)) {
        throw std::overflow_error("Overflow reward+fees.");
    }
    const Amount mintedReward = std::min(scheduledReward, remainingSupply);
    transactionsToMine.push_back(Transaction{"network", minerAddress, mintedReward, nowSeconds(), 0});

    Block blockToMine{chain_.size(),
                      chain_.back().getHash(),
                      transactionsToMine,
                      expectedDifficultyAtHeight(chain_.size(), chain_)};
    blockToMine.mine();
    chain_.push_back(blockToMine);
    hashToHeight_.emplace(blockToMine.getHash(), chain_.size() - 1);

    std::unordered_map<std::string, bool> minedIds;
    for (const auto& tx : transactionsToMine) {
        if (tx.from != "network") {
            minedIds.emplace(tx.id(), true);
        }
    }

    pendingTransactions_.erase(
        std::remove_if(pendingTransactions_.begin(), pendingTransactions_.end(), [&minedIds](const Transaction& tx) {
            return minedIds.find(tx.id()) != minedIds.end();
        }),
        pendingTransactions_.end());
}

Amount Blockchain::getBalance(const std::string& address) const {
    Amount balance = 0;
    for (const auto& block : chain_) {
        applyBalanceUpdates(block.getTransactions(), address, balance, "Overflow balance debit.",
                            "Overflow balance credit.");
    }

    return balance;
}

Amount Blockchain::getAvailableBalance(const std::string& address) const {
    Amount balance = getBalance(address);
    applyBalanceUpdates(pendingTransactions_, address, balance, "Overflow pending debit.",
                        "Overflow pending credit.");

    return balance;
}

Amount Blockchain::estimateNextMiningReward() const {
    Amount totalFees = 0;
    for (const auto& tx : getPendingTransactionsForBlockTemplate()) {
        if (!safeAdd(totalFees, tx.fee, totalFees)) {
            throw std::overflow_error("Overflow total fees.");
        }
    }

    const Amount baseReward = blockSubsidyAtHeight(chain_.size());
    const Amount remainingSupply = std::max<Amount>(0, kMaxSupply - getTotalSupply());
    Amount scheduled = 0;
    if (!safeAdd(baseReward, totalFees, scheduled)) {
        throw std::overflow_error("Overflow estimate reward.");
    }
    return std::min(scheduled, remainingSupply);
}

Amount Blockchain::getTotalSupply() const {
    Amount supply = 0;

    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == "network" && !safeAdd(supply, tx.amount, supply)) {
                throw std::overflow_error("Overflow total supply.");
            }
        }
    }

    return supply;
}

Amount Blockchain::estimateSupplyAtHeight(std::size_t height) const {
    Amount projected = 0;

    for (std::size_t h = 1; h <= height; ++h) {
        Amount next = 0;
        if (!safeAdd(projected, blockSubsidyAtHeight(h), next)) {
            throw std::overflow_error("Overflow projected supply.");
        }
        projected = std::min(next, kMaxSupply);
        if (projected >= kMaxSupply) {
            break;
        }
    }

    return projected;
}

MonetaryProjection Blockchain::getMonetaryProjection(std::size_t height) const {
    MonetaryProjection projection{};
    projection.height = height;
    projection.currentSubsidy = blockSubsidyAtHeight(height);
    projection.projectedSupply = estimateSupplyAtHeight(height);
    projection.remainingIssuable = std::max<Amount>(0, kMaxSupply - projection.projectedSupply);

    const std::size_t nextHalving = ((height / kHalvingInterval) + 1) * kHalvingInterval;
    projection.nextHalvingHeight = nextHalving;
    projection.nextSubsidy = blockSubsidyAtHeight(nextHalving);

    return projection;
}

unsigned int Blockchain::getCurrentDifficulty() const {
    return chain_.empty() ? initialDifficulty_ : chain_.back().getDifficulty();
}

unsigned int Blockchain::estimateNextDifficulty() const { return expectedDifficultyAtHeight(chain_.size(), chain_); }

std::uint64_t Blockchain::getMedianTimePast() const {
    if (chain_.empty()) {
        return 0;
    }
    return medianTimePastAtHeight(chain_.size() - 1, chain_);
}

std::uint64_t Blockchain::estimateNextMinimumTimestamp() const {
    if (chain_.empty()) {
        return 0;
    }
    return medianTimePastAtHeight(chain_.size() - 1, chain_);
}

std::size_t Blockchain::getBlockCount() const { return chain_.size(); }

std::vector<Transaction> Blockchain::getTransactionHistory(const std::string& address) const {
    std::vector<Transaction> history;

    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == address || tx.to == address) {
                history.push_back(tx);
            }
        }
    }

    for (const auto& tx : pendingTransactions_) {
        if (tx.from == address || tx.to == address) {
            history.push_back(tx);
        }
    }

    return history;
}

std::vector<TransactionHistoryEntry> Blockchain::getTransactionHistoryDetailed(const std::string& address,
                                                                                std::size_t limit,
                                                                                bool includePending) const {
    std::vector<TransactionHistoryEntry> history;

    for (std::size_t remaining = chain_.size(); remaining > 0; --remaining) {
        const std::size_t height = remaining - 1;
        const auto& txs = chain_[height].getTransactions();
        for (std::size_t txRemaining = txs.size(); txRemaining > 0; --txRemaining) {
            const auto& tx = txs[txRemaining - 1];
            if (tx.from != address && tx.to != address) {
                continue;
            }

            TransactionHistoryEntry entry;
            entry.tx = tx;
            entry.isConfirmed = true;
            entry.blockHeight = height;
            entry.confirmations = chain_.size() - height;
            history.push_back(entry);

            if (limit != 0 && history.size() >= limit) {
                return history;
            }
        }
    }

    if (includePending) {
        for (std::size_t remaining = pendingTransactions_.size(); remaining > 0; --remaining) {
            const auto& tx = pendingTransactions_[remaining - 1];
            if (tx.from != address && tx.to != address) {
                continue;
            }

            TransactionHistoryEntry entry;
            entry.tx = tx;
            entry.isConfirmed = false;
            entry.blockHeight = std::nullopt;
            entry.confirmations = 0;
            history.push_back(entry);

            if (limit != 0 && history.size() >= limit) {
                return history;
            }
        }
    }

    return history;
}

bool Blockchain::isValid() const { return isChainValid(chain_); }


std::uint64_t Blockchain::computeCumulativeWork(const std::vector<Block>& chain) const {
    std::uint64_t total = 0;
    for (const auto& block : chain) {
        const std::uint64_t blockWork = 1ULL << std::min(63U, block.getDifficulty());
        if (std::numeric_limits<std::uint64_t>::max() - total < blockWork) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        total += blockWork;
    }
    return total;
}

std::uint64_t Blockchain::getCumulativeWork() const { return computeCumulativeWork(chain_); }

bool Blockchain::isChainValid(const std::vector<Block>& candidateChain) const {
    if (candidateChain.empty() || maxTransactionsPerBlock_ == 0) {
        return false;
    }

    std::unordered_map<std::string, Amount> balances;
    std::unordered_set<std::string> seenUserTransactionIds;
    Amount cumulativeSupply = 0;

    for (std::size_t i = 0; i < candidateChain.size(); ++i) {
        const auto& current = candidateChain[i];

        if (!isTimestampAcceptable(current.getTimestamp())) {
            return false;
        }

        if (i == 0) {
            if (!current.hasValidHash() || current.getDifficulty() != initialDifficulty_) {
                return false;
            }
        } else {
            const auto& previous = candidateChain[i - 1];
            if (current.getPreviousHash() != previous.getHash() || !current.hasValidHash()) {
                return false;
            }
            if (current.getTimestamp() + 1 < previous.getTimestamp()) {
                return false;
            }
            if (current.getTimestamp() < medianTimePastAtHeight(i - 1, candidateChain)) {
                return false;
            }
            if (current.getDifficulty() != expectedDifficultyAtHeight(i, candidateChain)) {
                return false;
            }
        }

        if (i > 0 && current.getTransactions().size() > maxTransactionsPerBlock_) {
            return false;
        }

        if (i > 0) {
            if (current.getTransactions().empty()) {
                return false;
            }
            if (current.getTransactions().back().from != "network") {
                return false;
            }
        }

        if (!hasNonDecreasingTimestamps(current.getTransactions())) {
            return false;
        }

        Amount blockFees = 0;
        Amount mintedInBlock = 0;
        std::size_t coinbaseCount = 0;

        for (std::size_t txIndex = 0; txIndex < current.getTransactions().size(); ++txIndex) {
            const auto& tx = current.getTransactions()[txIndex];
            if (!isTransactionShapeValid(tx) || !isTimestampAcceptable(tx.timestamp)) {
                return false;
            }

            if (tx.from != "network") {
                const std::string txId = tx.id();
                if (seenUserTransactionIds.find(txId) != seenUserTransactionIds.end()) {
                    return false;
                }
                seenUserTransactionIds.insert(txId);

                if (tx.fee < kMinRelayFee) {
                    return false;
                }
                Amount debit = 0;
                if (!safeAdd(tx.amount, tx.fee, debit) || balances[tx.from] < debit) {
                    return false;
                }
                balances[tx.from] -= debit;
                if (!safeAdd(blockFees, tx.fee, blockFees)) {
                    return false;
                }
            } else {
                ++coinbaseCount;
                if (i > 0 && txIndex + 1 != current.getTransactions().size()) {
                    return false;
                }
                if (!safeAdd(mintedInBlock, tx.amount, mintedInBlock) ||
                    !safeAdd(cumulativeSupply, tx.amount, cumulativeSupply)) {
                    return false;
                }
                if (cumulativeSupply > kMaxSupply) {
                    return false;
                }
            }

            if (!safeAdd(balances[tx.to], tx.amount, balances[tx.to])) {
                return false;
            }
        }

        if (i > 0) {
            Amount expectedMaxReward = 0;
            if (!safeAdd(blockSubsidyAtHeight(i), blockFees, expectedMaxReward)) {
                return false;
            }
            if (coinbaseCount != 1 || mintedInBlock > expectedMaxReward) {
                return false;
            }
        }
    }

    return true;
}

std::unordered_set<std::string> Blockchain::buildUserTransactionIdSet(const std::vector<Block>& source) const {
    std::unordered_set<std::string> ids;
    for (const auto& block : source) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from != "network") {
                ids.insert(tx.id());
            }
        }
    }
    return ids;
}

std::vector<Transaction> Blockchain::collectDetachedTransactions(const std::vector<Block>& oldChain,
                                                                 const std::vector<Block>& newChain) const {
    const auto newChainIds = buildUserTransactionIdSet(newChain);
    std::vector<Transaction> detached;

    for (const auto& block : oldChain) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == "network") {
                continue;
            }

            if (newChainIds.find(tx.id()) == newChainIds.end()) {
                detached.push_back(tx);
            }
        }
    }

    return detached;
}

void Blockchain::rebuildPendingTransactionsAfterReorg(const std::vector<Block>& oldChain,
                                                      const std::vector<Block>& newChain) {
    std::vector<Transaction> rebuilt;
    rebuilt.reserve(pendingTransactions_.size() + oldChain.size());

    const auto newChainIds = buildUserTransactionIdSet(newChain);

    auto appendIfValid = [this, &rebuilt, &newChainIds](const Transaction& tx) {
        if (tx.from == "network" || !isTransactionShapeValid(tx) || tx.fee < kMinRelayFee ||
            !isTimestampAcceptable(tx.timestamp) || newChainIds.find(tx.id()) != newChainIds.end()) {
            return;
        }

        const bool duplicate = std::any_of(rebuilt.begin(), rebuilt.end(), [&tx](const Transaction& existing) {
            return existing.id() == tx.id();
        });
        if (!duplicate) {
            rebuilt.push_back(tx);
        }
    };

    for (const auto& tx : pendingTransactions_) {
        appendIfValid(tx);
    }

    for (const auto& tx : collectDetachedTransactions(oldChain, newChain)) {
        appendIfValid(tx);
    }

    // Revalide l'ensemble de la mempool sur le nouvel etat de chaine. Une reorg profonde
    // peut invalider des transactions autrefois valides (soldes insuffisants apres fork).
    std::vector<std::size_t> order(rebuilt.size());
    for (std::size_t i = 0; i < rebuilt.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&rebuilt](std::size_t lhs, std::size_t rhs) {
        if (rebuilt[lhs].fee != rebuilt[rhs].fee) {
            return rebuilt[lhs].fee > rebuilt[rhs].fee;
        }
        return rebuilt[lhs].timestamp < rebuilt[rhs].timestamp;
    });

    std::unordered_map<std::string, Amount> projectedBalance;
    std::vector<Transaction> revalidated;
    revalidated.reserve(rebuilt.size());

    for (const std::size_t idx : order) {
        const Transaction& candidate = rebuilt[idx];

        if (projectedBalance.find(candidate.from) == projectedBalance.end()) {
            projectedBalance[candidate.from] = getBalance(candidate.from);
        }
        if (projectedBalance.find(candidate.to) == projectedBalance.end()) {
            projectedBalance[candidate.to] = getBalance(candidate.to);
        }

        Amount debit = 0;
        if (!safeAdd(candidate.amount, candidate.fee, debit) || projectedBalance[candidate.from] < debit) {
            continue;
        }

        projectedBalance[candidate.from] -= debit;
        if (!safeAdd(projectedBalance[candidate.to], candidate.amount, projectedBalance[candidate.to])) {
            continue;
        }

        revalidated.push_back(candidate);
    }

    pendingTransactions_ = std::move(revalidated);
}

bool Blockchain::tryAdoptChain(const std::vector<Block>& candidateChain) {
    pruneExpiredPendingTransactions();
    if (candidateChain.empty() || chain_.empty()) {
        return false;
    }

    // Un noeud ne doit pas adopter une chaine d'un autre reseau/genesis.
    if (candidateChain.front().getHash() != chain_.front().getHash()) {
        return false;
    }

    if (!isChainValid(candidateChain)) {
        return false;
    }

    const std::uint64_t currentWork = computeCumulativeWork(chain_);
    const std::uint64_t candidateWork = computeCumulativeWork(candidateChain);
    if (candidateWork < currentWork) {
        return false;
    }

    if (candidateWork == currentWork) {
        if (candidateChain.back().getHash() >= chain_.back().getHash()) {
            return false;
        }
    }

    if (candidateChain.size() == chain_.size() &&
        commonPrefixLength(candidateChain, chain_) == chain_.size()) {
        return false;
    }

    const std::vector<Block> previousChain = chain_;
    const std::size_t sharedPrefix = commonPrefixLength(previousChain, candidateChain);
    chain_ = candidateChain;
    rebuildHashIndex();
    rebuildPendingTransactionsAfterReorg(previousChain, chain_);

    const std::size_t detachedBlocks = previousChain.size() > sharedPrefix ? previousChain.size() - sharedPrefix : 0;
    lastReorgDepth_ = detachedBlocks;
    lastForkHeight_ = sharedPrefix > 0 ? sharedPrefix - 1 : 0;
    lastForkHash_ = sharedPrefix > 0 ? previousChain[sharedPrefix - 1].getHash() : "";
    ++reorgCount_;

    return true;
}

void Blockchain::rebuildHashIndex() {
    hashToHeight_.clear();
    hashToHeight_.reserve(chain_.size());
    for (std::size_t i = 0; i < chain_.size(); ++i) {
        hashToHeight_.emplace(chain_[i].getHash(), i);
    }
}

std::size_t Blockchain::commonPrefixLength(const std::vector<Block>& lhs, const std::vector<Block>& rhs) const {
    const std::size_t limit = std::min(lhs.size(), rhs.size());
    std::size_t prefix = 0;
    while (prefix < limit && lhs[prefix].getHash() == rhs[prefix].getHash()) {
        ++prefix;
    }
    return prefix;
}

std::vector<Transaction> Blockchain::getPendingTransactionsForBlockTemplate() const {
    const std::size_t maxUserTransactions = maxTransactionsPerBlock_ > 0 ? maxTransactionsPerBlock_ - 1 : 0;

    std::vector<std::size_t> order(pendingTransactions_.size());
    for (std::size_t i = 0; i < pendingTransactions_.size(); ++i) {
        order[i] = i;
    }

    std::stable_sort(order.begin(), order.end(), [this](std::size_t lhs, std::size_t rhs) {
        return pendingTransactions_[lhs].fee > pendingTransactions_[rhs].fee;
    });

    std::unordered_map<std::string, Amount> projectedBalance;
    std::vector<Transaction> selected;
    selected.reserve(maxUserTransactions);

    const std::uint64_t now = nowSeconds();
    for (const std::size_t idx : order) {
        if (selected.size() >= maxUserTransactions) {
            break;
        }

        const Transaction& candidate = pendingTransactions_[idx];
        if (!isTimestampAcceptable(candidate.timestamp) || isMempoolTransactionExpired(candidate, now)) {
            continue;
        }

        if (projectedBalance.find(candidate.from) == projectedBalance.end()) {
            projectedBalance[candidate.from] = getBalance(candidate.from);
        }
        if (projectedBalance.find(candidate.to) == projectedBalance.end()) {
            projectedBalance[candidate.to] = getBalance(candidate.to);
        }

        Amount debit = 0;
        if (!safeAdd(candidate.amount, candidate.fee, debit) || projectedBalance[candidate.from] < debit) {
            continue;
        }

        projectedBalance[candidate.from] -= debit;
        if (!safeAdd(projectedBalance[candidate.to], candidate.amount, projectedBalance[candidate.to])) {
            continue;
        }
        selected.push_back(candidate);
    }

    return selected;
}

AddressStats Blockchain::getAddressStats(const std::string& address) const {
    if (address.empty()) {
        throw std::invalid_argument("L'adresse analysee ne peut pas etre vide.");
    }

    AddressStats stats;

    for (const auto& block : chain_) {
        const auto& txs = block.getTransactions();
        bool minerRewardedInBlock = false;

        for (const auto& tx : txs) {
            if (tx.from == "network") {
                if (tx.to == address) {
                    if (!safeAdd(stats.minedRewards, tx.amount, stats.minedRewards)) {
                        throw std::overflow_error("Overflow minedRewards.");
                    }
                    minerRewardedInBlock = true;
                }
                continue;
            }

            if (tx.from == address) {
                if (!safeAdd(stats.totalSent, tx.amount, stats.totalSent) ||
                    !safeAdd(stats.feesPaid, tx.fee, stats.feesPaid)) {
                    throw std::overflow_error("Overflow address sent/fees.");
                }
                ++stats.outgoingTransactionCount;
            }

            if (tx.to == address) {
                if (!safeAdd(stats.totalReceived, tx.amount, stats.totalReceived)) {
                    throw std::overflow_error("Overflow address received.");
                }
                ++stats.incomingTransactionCount;
            }
        }

        if (minerRewardedInBlock) {
            ++stats.minedBlockCount;
        }
    }

    for (const auto& tx : pendingTransactions_) {
        if (tx.from == address) {
            Amount debit = 0;
            if (!safeAdd(tx.amount, tx.fee, debit) || !safeAdd(stats.pendingOutgoing, debit, stats.pendingOutgoing)) {
                throw std::overflow_error("Overflow pendingOutgoing.");
            }
        }
    }

    return stats;
}

NetworkStats Blockchain::getNetworkStats() const {
    NetworkStats stats;
    stats.blockCount = chain_.size();
    stats.pendingTransactionCount = pendingTransactions_.size();

    std::vector<Amount> userAmounts;

    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == "network") {
                ++stats.coinbaseTransactionCount;
                if (!safeAdd(stats.totalMinedRewards, tx.amount, stats.totalMinedRewards)) {
                    throw std::overflow_error("Overflow network mined rewards.");
                }
                continue;
            }

            ++stats.userTransactionCount;
            if (!safeAdd(stats.totalTransferred, tx.amount, stats.totalTransferred) ||
                !safeAdd(stats.totalFeesPaid, tx.fee, stats.totalFeesPaid)) {
                throw std::overflow_error("Overflow network transferred/fees.");
            }
            userAmounts.push_back(tx.amount);
        }
    }

    if (!userAmounts.empty()) {
        std::sort(userAmounts.begin(), userAmounts.end());
        const std::size_t mid = userAmounts.size() / 2;
        if (userAmounts.size() % 2 == 1) {
            stats.medianUserTransactionAmount = userAmounts[mid];
        } else {
            stats.medianUserTransactionAmount = userAmounts[mid - 1] + (userAmounts[mid] - userAmounts[mid - 1]) / 2;
        }
    }

    return stats;
}


Amount Blockchain::estimateRequiredFeeForInclusion(std::size_t targetBlocks) const {
    if (targetBlocks == 0) {
        throw std::invalid_argument("targetBlocks doit etre >= 1.");
    }

    const std::size_t maxUserTransactions = maxTransactionsPerBlock_ > 0 ? maxTransactionsPerBlock_ - 1 : 0;
    if (maxUserTransactions == 0 || pendingTransactions_.empty()) {
        return kMinRelayFee;
    }

    const std::size_t projectedSlots = maxUserTransactions * targetBlocks;
    if (projectedSlots == 0 || projectedSlots > pendingTransactions_.size()) {
        return kMinRelayFee;
    }

    std::vector<Amount> fees;
    fees.reserve(pendingTransactions_.size());
    const std::uint64_t now = nowSeconds();
    for (const auto& tx : pendingTransactions_) {
        if (!isTimestampAcceptable(tx.timestamp) || isMempoolTransactionExpired(tx, now)) {
            continue;
        }
        fees.push_back(tx.fee);
    }

    if (fees.empty()) {
        return kMinRelayFee;
    }

    std::sort(fees.begin(), fees.end(), std::greater<Amount>());
    if (projectedSlots > fees.size()) {
        return kMinRelayFee;
    }

    const Amount cutoffFee = fees[projectedSlots - 1];
    return std::max(kMinRelayFee, cutoffFee);
}

MempoolStats Blockchain::getMempoolStats() const {
    MempoolStats stats;
    const std::uint64_t now = nowSeconds();
    std::vector<const Transaction*> eligible;
    eligible.reserve(pendingTransactions_.size());

    for (const auto& tx : pendingTransactions_) {
        if (!isTimestampAcceptable(tx.timestamp) || isMempoolTransactionExpired(tx, now)) {
            continue;
        }
        eligible.push_back(&tx);
    }

    stats.transactionCount = eligible.size();
    if (eligible.empty()) {
        return stats;
    }

    std::vector<Amount> fees;
    fees.reserve(eligible.size());
    std::vector<std::uint64_t> ages;
    ages.reserve(eligible.size());

    stats.minFee = eligible.front()->fee;
    stats.maxFee = eligible.front()->fee;
    stats.oldestTimestamp = eligible.front()->timestamp;
    stats.newestTimestamp = eligible.front()->timestamp;
    const std::uint64_t firstAge = eligible.front()->timestamp >= now ? 0 : now - eligible.front()->timestamp;
    stats.minAgeSeconds = firstAge;
    stats.maxAgeSeconds = firstAge;
    ages.push_back(firstAge);

    for (std::size_t i = 0; i < eligible.size(); ++i) {
        const auto& tx = *eligible[i];
        if (!safeAdd(stats.totalAmount, tx.amount, stats.totalAmount) ||
            !safeAdd(stats.totalFees, tx.fee, stats.totalFees)) {
            throw std::overflow_error("Overflow mempool totals.");
        }

        stats.minFee = std::min(stats.minFee, tx.fee);
        stats.maxFee = std::max(stats.maxFee, tx.fee);
        fees.push_back(tx.fee);
        stats.oldestTimestamp = std::min(stats.oldestTimestamp, tx.timestamp);
        stats.newestTimestamp = std::max(stats.newestTimestamp, tx.timestamp);
        if (i != 0) {
            const std::uint64_t age = tx.timestamp >= now ? 0 : now - tx.timestamp;
            stats.minAgeSeconds = std::min(stats.minAgeSeconds, age);
            stats.maxAgeSeconds = std::max(stats.maxAgeSeconds, age);
            ages.push_back(age);
        }
    }

    std::sort(fees.begin(), fees.end());
    const std::size_t mid = fees.size() / 2;
    if (fees.size() % 2 == 1) {
        stats.medianFee = fees[mid];
    } else {
        stats.medianFee = fees[mid - 1] + (fees[mid] - fees[mid - 1]) / 2;
    }

    std::sort(ages.begin(), ages.end());
    const std::size_t ageMid = ages.size() / 2;
    if (ages.size() % 2 == 1) {
        stats.medianAgeSeconds = ages[ageMid];
    } else {
        stats.medianAgeSeconds = ages[ageMid - 1] + (ages[ageMid] - ages[ageMid - 1]) / 2;
    }

    return stats;
}



std::vector<BlockHeaderInfo> Blockchain::getHeadersFromHeight(std::size_t startHeight,
                                                              std::size_t maxCount) const {
    if (maxCount == 0 || startHeight >= chain_.size()) {
        return {};
    }

    const std::size_t end = boundedEndHeight(startHeight, maxCount, chain_.size());
    std::vector<BlockHeaderInfo> headers;
    headers.reserve(end - startHeight);

    for (std::size_t height = startHeight; height < end; ++height) {
        const Block& block = chain_[height];
        headers.push_back(BlockHeaderInfo{block.getIndex(),
                                          block.getHash(),
                                          block.getPreviousHash(),
                                          block.getTimestamp(),
                                          block.getDifficulty()});
    }

    return headers;
}

std::vector<std::string> Blockchain::getBlockLocatorHashes() const {
    std::vector<std::string> locator;
    if (chain_.empty()) {
        return locator;
    }

    std::size_t step = 1;
    std::size_t index = chain_.size() - 1;

    while (true) {
        locator.push_back(chain_[index].getHash());
        if (index == 0) {
            break;
        }

        if (index <= step) {
            index = 0;
        } else {
            index -= step;
        }

        if (locator.size() > 10) {
            step *= 2;
        }
    }

    return locator;
}

std::optional<std::size_t> Blockchain::findHighestLocatorMatch(
    const std::vector<std::string>& locatorHashes) const {
    if (locatorHashes.empty()) {
        return std::nullopt;
    }

    std::optional<std::size_t> best;
    for (const auto& hash : locatorHashes) {
        const auto it = hashToHeight_.find(hash);
        if (it == hashToHeight_.end()) {
            continue;
        }

        if (!best.has_value() || it->second > best.value()) {
            best = it->second;
        }
    }

    return best;
}

std::optional<std::size_t> Blockchain::findBlockHeightByHash(const std::string& hash) const {
    if (hash.empty()) {
        return std::nullopt;
    }

    const auto it = hashToHeight_.find(hash);
    if (it != hashToHeight_.end()) {
        return it->second;
    }

    return std::nullopt;
}

BlockSummary Blockchain::makeBlockSummary(const Block& block) const {
    BlockSummary summary;
    summary.index = block.getIndex();
    summary.hash = block.getHash();
    summary.previousHash = block.getPreviousHash();
    summary.timestamp = block.getTimestamp();
    summary.difficulty = block.getDifficulty();
    summary.transactionCount = block.getTransactions().size();

    for (const auto& tx : block.getTransactions()) {
        if (tx.from != "network") {
            ++summary.userTransactionCount;
            if (!safeAdd(summary.totalFees, tx.fee, summary.totalFees)) {
                throw std::overflow_error("Overflow block summary fees.");
            }
        }
    }

    return summary;
}

std::vector<BlockSummary> Blockchain::getBlocksFromHeight(std::size_t startHeight,
                                                         std::size_t maxCount) const {
    if (maxCount == 0 || startHeight >= chain_.size()) {
        return {};
    }

    const std::size_t end = boundedEndHeight(startHeight, maxCount, chain_.size());
    std::vector<BlockSummary> blocks;
    blocks.reserve(end - startHeight);

    for (std::size_t height = startHeight; height < end; ++height) {
        blocks.push_back(makeBlockSummary(chain_[height]));
    }

    return blocks;
}

std::vector<BlockSummary> Blockchain::getBlocksForLocator(const std::vector<std::string>& locatorHashes,
                                                          std::size_t maxCount) const {
    return getBlocksForLocatorWithStop(locatorHashes, maxCount, "");
}

std::vector<BlockSummary> Blockchain::getBlocksForLocatorWithStop(const std::vector<std::string>& locatorHashes,
                                                                  std::size_t maxCount,
                                                                  const std::string& stopHash) const {
    const auto syncStatus = getSyncStatus(locatorHashes, maxCount, stopHash);
    return getBlocksFromHeight(syncStatus.nextHeight, syncStatus.responseBlockCount);
}

std::optional<BlockSummary> Blockchain::getBlockSummaryByHeight(std::size_t height) const {
    if (height >= chain_.size()) {
        return std::nullopt;
    }

    return makeBlockSummary(chain_[height]);
}

std::optional<BlockSummary> Blockchain::getBlockSummaryByHash(const std::string& hash) const {
    const auto height = findBlockHeightByHash(hash);
    if (!height.has_value()) {
        return std::nullopt;
    }

    return makeBlockSummary(chain_[height.value()]);
}


std::optional<TransactionLookup> Blockchain::findTransactionById(const std::string& txId) const {
    if (txId.empty()) {
        return std::nullopt;
    }

    for (std::size_t height = chain_.size(); height > 0; --height) {
        const std::size_t idx = height - 1;
        const auto& txs = chain_[idx].getTransactions();
        for (const auto& tx : txs) {
            if (tx.id() == txId) {
                TransactionLookup lookup;
                lookup.tx = tx;
                lookup.isConfirmed = true;
                lookup.blockHeight = idx;
                lookup.confirmations = chain_.size() - idx;
                return lookup;
            }
        }
    }

    for (const auto& tx : pendingTransactions_) {
        if (tx.id() == txId) {
            TransactionLookup lookup;
            lookup.tx = tx;
            lookup.isConfirmed = false;
            lookup.confirmations = 0;
            lookup.blockHeight = std::nullopt;
            return lookup;
        }
    }

    return std::nullopt;
}

SyncStatus Blockchain::getSyncStatus(const std::vector<std::string>& locatorHashes,
                                     std::size_t maxCount,
                                     const std::string& stopHash) const {
    SyncStatus status;
    if (chain_.empty()) {
        status.isAtTip = true;
        return status;
    }

    status.localHeight = chain_.size() - 1;
    status.maxResponseBlocks = maxCount;

    const auto match = findHighestLocatorMatch(locatorHashes);
    status.locatorHeight = match;

    std::size_t startHeight = 0;
    if (match.has_value()) {
        startHeight = match.value() + 1;
    }

    status.nextHeight = startHeight;
    status.remainingBlocks = startHeight < chain_.size() ? chain_.size() - startHeight : 0;

    std::size_t allowedCount = maxCount;
    if (!stopHash.empty()) {
        const auto stopHeight = findBlockHeightByHash(stopHash);
        status.stopHeight = stopHeight;
        if (stopHeight.has_value()) {
            if (stopHeight.value() < startHeight) {
                status.responseBlockCount = 0;
                status.isAtTip = true;
                status.isStopHashLimiting = true;
                return status;
            }

            const std::size_t upToStop = stopHeight.value() - startHeight + 1;
            status.isStopHashLimiting = upToStop <= allowedCount;
            allowedCount = std::min(allowedCount, upToStop);
        }
    }

    status.responseBlockCount = std::min(status.remainingBlocks, allowedCount);
    status.isAtTip = status.responseBlockCount == 0;
    return status;
}

std::vector<BlockSummary> Blockchain::getRecentBlockSummaries(std::size_t maxCount) const {
    if (maxCount == 0 || chain_.empty()) {
        return {};
    }

    const std::size_t count = std::min(maxCount, chain_.size());
    std::vector<BlockSummary> summaries;
    summaries.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t height = chain_.size() - 1 - i;
        summaries.push_back(makeBlockSummary(chain_[height]));
    }

    return summaries;
}

std::vector<BlockHeaderInfo> Blockchain::getHeadersForLocator(const std::vector<std::string>& locatorHashes,
                                                                 std::size_t maxCount) const {
    return getHeadersForLocatorWithStop(locatorHashes, maxCount, "");
}

std::vector<BlockHeaderInfo> Blockchain::getHeadersForLocatorWithStop(const std::vector<std::string>& locatorHashes,
                                                                      std::size_t maxCount,
                                                                      const std::string& stopHash) const {
    const auto syncStatus = getSyncStatus(locatorHashes, maxCount, stopHash);
    return getHeadersFromHeight(syncStatus.nextHeight, syncStatus.responseBlockCount);
}

std::vector<std::pair<std::string, Amount>> Blockchain::getTopBalances(std::size_t limit) const {
    if (limit == 0) {
        return {};
    }

    std::unordered_map<std::string, Amount> balances;
    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from != "network") {
                Amount debit = 0;
                if (!safeAdd(tx.amount, tx.fee, debit)) {
                    throw std::overflow_error("Overflow top balances debit.");
                }
                auto it = balances.find(tx.from);
                if (it == balances.end()) {
                    balances.emplace(tx.from, -debit);
                } else if (!safeAdd(it->second, -debit, it->second)) {
                    throw std::overflow_error("Overflow top balances sender update.");
                }
            }

            auto itRecipient = balances.find(tx.to);
            if (itRecipient == balances.end()) {
                balances.emplace(tx.to, tx.amount);
            } else if (!safeAdd(itRecipient->second, tx.amount, itRecipient->second)) {
                throw std::overflow_error("Overflow top balances recipient update.");
            }
        }
    }

    std::vector<std::pair<std::string, Amount>> ranking;
    ranking.reserve(balances.size());
    for (const auto& entry : balances) {
        if (entry.second > 0) {
            ranking.push_back(entry);
        }
    }

    std::sort(ranking.begin(), ranking.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        }
        return lhs.first < rhs.first;
    });

    if (ranking.size() > limit) {
        ranking.resize(limit);
    }

    return ranking;
}

std::string Blockchain::getChainSummary() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(8);
    out << "Novacoin summary\n";
    out << "- blocks=" << getBlockCount() << "\n";
    out << "- total_supply=" << Transaction::toNOVA(getTotalSupply()) << " / " << Transaction::toNOVA(kMaxSupply)
        << "\n";
    const auto monetary = getMonetaryProjection(chain_.empty() ? 0 : chain_.size() - 1);
    out << "- issuance_remaining=" << Transaction::toNOVA(monetary.remainingIssuable) << "\n";
    out << "- subsidy_current=" << Transaction::toNOVA(monetary.currentSubsidy) << "\n";
    out << "- next_reward_estimate=" << Transaction::toNOVA(estimateNextMiningReward()) << "\n";
    out << "- current_difficulty=" << getCurrentDifficulty() << "\n";
    out << "- next_difficulty_estimate=" << estimateNextDifficulty() << "\n";
    out << "- cumulative_work=" << getCumulativeWork() << "\n";
    out << "- reorg_count=" << getReorgCount() << "\n";
    out << "- last_reorg_depth=" << getLastReorgDepth() << "\n";
    out << "- last_fork_height=" << getLastForkHeight() << "\n";
    out << "- pending_transactions=" << pendingTransactions_.size() << "\n";

    const MempoolStats mempoolStats = getMempoolStats();
    out << "- mempool_total_amount=" << Transaction::toNOVA(mempoolStats.totalAmount) << "\n";
    out << "- mempool_total_fees=" << Transaction::toNOVA(mempoolStats.totalFees) << "\n";
    out << "- mempool_min_fee=" << Transaction::toNOVA(mempoolStats.minFee) << "\n";
    out << "- mempool_max_fee=" << Transaction::toNOVA(mempoolStats.maxFee) << "\n";
    out << "- mempool_median_fee=" << Transaction::toNOVA(mempoolStats.medianFee) << "\n";
    out << "- mempool_oldest_ts=" << mempoolStats.oldestTimestamp << "\n";
    out << "- mempool_newest_ts=" << mempoolStats.newestTimestamp << "\n";
    out << "- mempool_min_age_s=" << mempoolStats.minAgeSeconds << "\n";
    out << "- mempool_max_age_s=" << mempoolStats.maxAgeSeconds << "\n";
    out << "- mempool_median_age_s=" << mempoolStats.medianAgeSeconds << "\n";

    const NetworkStats networkStats = getNetworkStats();
    out << "- network_user_transactions=" << networkStats.userTransactionCount << "\n";
    out << "- network_coinbase_transactions=" << networkStats.coinbaseTransactionCount << "\n";
    out << "- network_total_transferred=" << Transaction::toNOVA(networkStats.totalTransferred) << "\n";
    out << "- network_total_fees=" << Transaction::toNOVA(networkStats.totalFeesPaid) << "\n";
    out << "- network_median_tx=" << Transaction::toNOVA(networkStats.medianUserTransactionAmount) << "\n";
    return out.str();
}

const std::vector<Block>& Blockchain::getChain() const { return chain_; }
const std::vector<Transaction>& Blockchain::getPendingTransactions() const { return pendingTransactions_; }

std::size_t Blockchain::getLastReorgDepth() const { return lastReorgDepth_; }

std::size_t Blockchain::getLastForkHeight() const { return lastForkHeight_; }

std::string Blockchain::getLastForkHash() const { return lastForkHash_; }

std::size_t Blockchain::getReorgCount() const { return reorgCount_; }
