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

} // namespace

Blockchain::Blockchain(unsigned int difficulty, Amount miningReward, std::size_t maxTransactionsPerBlock)
    : initialDifficulty_(std::clamp(difficulty, kMinDifficulty, kMaxDifficulty)),
      miningReward_(miningReward),
      maxTransactionsPerBlock_(maxTransactionsPerBlock),
      chain_({createGenesisBlock()}),
      pendingTransactions_() {
    if (maxTransactionsPerBlock_ == 0) {
        throw std::invalid_argument("La taille maximale d'un bloc doit être > 0.");
    }
    if (miningReward_ < 0) {
        throw std::invalid_argument("La récompense de minage doit être >= 0.");
    }
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

void Blockchain::createTransaction(const Transaction& tx) {
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
    const auto isDuplicate = std::any_of(
        pendingTransactions_.begin(), pendingTransactions_.end(),
        [&txId](const Transaction& candidate) { return candidate.id() == txId; });
    if (isDuplicate) {
        throw std::invalid_argument("Transaction en double detectee dans la mempool locale.");
    }

    pendingTransactions_.push_back(tx);
}

void Blockchain::minePendingTransactions(const std::string& minerAddress) {
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

    for (const std::size_t idx : order) {
        if (transactionsToMine.size() >= maxUserTransactions) {
            break;
        }

        const Transaction& candidate = pendingTransactions_[idx];
        if (!isTimestampAcceptable(candidate.timestamp)) {
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
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == address) {
                Amount debit = 0;
                if (!safeAdd(tx.amount, tx.fee, debit) || !safeAdd(balance, -debit, balance)) {
                    throw std::overflow_error("Overflow balance debit.");
                }
            }
            if (tx.to == address) {
                if (!safeAdd(balance, tx.amount, balance)) {
                    throw std::overflow_error("Overflow balance credit.");
                }
            }
        }
    }

    return balance;
}

Amount Blockchain::getAvailableBalance(const std::string& address) const {
    Amount balance = getBalance(address);

    for (const auto& tx : pendingTransactions_) {
        if (tx.from == address) {
            Amount debit = 0;
            if (!safeAdd(tx.amount, tx.fee, debit) || !safeAdd(balance, -debit, balance)) {
                throw std::overflow_error("Overflow pending debit.");
            }
        }
        if (tx.to == address) {
            if (!safeAdd(balance, tx.amount, balance)) {
                throw std::overflow_error("Overflow pending credit.");
            }
        }
    }

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

unsigned int Blockchain::getCurrentDifficulty() const {
    return chain_.empty() ? initialDifficulty_ : chain_.back().getDifficulty();
}

unsigned int Blockchain::estimateNextDifficulty() const { return expectedDifficultyAtHeight(chain_.size(), chain_); }

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
            if (current.getDifficulty() != expectedDifficultyAtHeight(i, candidateChain)) {
                return false;
            }
        }

        if (i > 0 && current.getTransactions().size() > maxTransactionsPerBlock_) {
            return false;
        }

        if (!hasNonDecreasingTimestamps(current.getTransactions())) {
            return false;
        }

        Amount blockFees = 0;
        Amount mintedInBlock = 0;
        std::size_t coinbaseCount = 0;

        for (const auto& tx : current.getTransactions()) {
            if (!isTransactionShapeValid(tx) || !isTimestampAcceptable(tx.timestamp)) {
                return false;
            }

            if (tx.from != "network") {
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

    pendingTransactions_ = std::move(rebuilt);
}

bool Blockchain::tryAdoptChain(const std::vector<Block>& candidateChain) {
    if (!isChainValid(candidateChain)) {
        return false;
    }

    const std::uint64_t currentWork = computeCumulativeWork(chain_);
    const std::uint64_t candidateWork = computeCumulativeWork(candidateChain);
    if (candidateWork <= currentWork) {
        return false;
    }

    const std::vector<Block> previousChain = chain_;
    chain_ = candidateChain;
    rebuildPendingTransactionsAfterReorg(previousChain, chain_);

    return true;
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

    for (const std::size_t idx : order) {
        if (selected.size() >= maxUserTransactions) {
            break;
        }

        const Transaction& candidate = pendingTransactions_[idx];
        if (!isTimestampAcceptable(candidate.timestamp)) {
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

std::string Blockchain::getChainSummary() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(8);
    out << "Novacoin summary\n";
    out << "- blocks=" << getBlockCount() << "\n";
    out << "- total_supply=" << Transaction::toNOVA(getTotalSupply()) << " / " << Transaction::toNOVA(kMaxSupply)
        << "\n";
    out << "- next_reward_estimate=" << Transaction::toNOVA(estimateNextMiningReward()) << "\n";
    out << "- current_difficulty=" << getCurrentDifficulty() << "\n";
    out << "- next_difficulty_estimate=" << estimateNextDifficulty() << "\n";
    out << "- cumulative_work=" << getCumulativeWork() << "\n";
    out << "- pending_transactions=" << pendingTransactions_.size() << "\n";
    return out.str();
}

const std::vector<Block>& Blockchain::getChain() const { return chain_; }
const std::vector<Transaction>& Blockchain::getPendingTransactions() const { return pendingTransactions_; }
