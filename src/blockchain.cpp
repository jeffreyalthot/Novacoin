#include "blockchain.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

bool isTransactionShapeValid(const Transaction& tx) {
    if (tx.from.empty() || tx.to.empty()) {
        return false;
    }

    if (!std::isfinite(tx.amount) || !std::isfinite(tx.fee)) {
        return false;
    }

    if (tx.from == "network") {
        return tx.amount >= 0.0 && tx.fee == 0.0;
    }

    return tx.amount > 0.0 && tx.fee >= 0.0;
}

bool hasNonDecreasingTimestamps(const std::vector<Transaction>& transactions) {
    if (transactions.empty()) {
        return true;
    }

    for (std::size_t i = 1; i < transactions.size(); ++i) {
        if (transactions[i].timestamp < transactions[i - 1].timestamp) {
            return false;
        }
    }

    return true;
}

} // namespace

Blockchain::Blockchain(unsigned int difficulty,
                       double miningReward,
                       std::size_t maxTransactionsPerBlock)
    : difficulty_(difficulty),
      miningReward_(miningReward),
      maxTransactionsPerBlock_(maxTransactionsPerBlock),
      chain_({createGenesisBlock()}),
      pendingTransactions_() {
    if (maxTransactionsPerBlock_ == 0) {
        throw std::invalid_argument("La taille maximale d'un bloc doit être > 0.");
    }
    if (miningReward_ < 0.0) {
        throw std::invalid_argument("La récompense de minage doit être >= 0.");
    }
}

Block Blockchain::createGenesisBlock() const {
    Transaction bootstrap{"network", "genesis", 0.0, nowSeconds(), 0.0};
    Block genesis{0, "0", {bootstrap}};
    genesis.mine(difficulty_);
    return genesis;
}

double Blockchain::blockSubsidyAtHeight(std::size_t height) const {
    const std::size_t halvings = height / kHalvingInterval;
    if (halvings >= std::numeric_limits<double>::digits) {
        return 0.0;
    }

    const double divisor = std::pow(2.0, static_cast<double>(halvings));
    const double reward = miningReward_ / divisor;
    return reward < 1e-8 ? 0.0 : reward;
}

bool Blockchain::isTimestampAcceptable(std::uint64_t timestamp) const {
    const std::uint64_t now = nowSeconds();
    return timestamp <= now + kMaxFutureDriftSeconds;
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

    if (tx.amount + tx.fee > getAvailableBalance(tx.from)) {
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
    const double baseReward = blockSubsidyAtHeight(nextHeight);
    const double remainingSupply = std::max(0.0, kMaxSupply - getTotalSupply());

    if (pendingTransactions_.empty() && baseReward <= 0.0) {
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

    std::unordered_map<std::string, double> projectedBalance;
    double collectedFees = 0.0;

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

        const double debit = candidate.amount + candidate.fee;
        if (projectedBalance[candidate.from] + 1e-9 < debit) {
            continue;
        }

        projectedBalance[candidate.from] -= debit;
        projectedBalance[candidate.to] += candidate.amount;

        collectedFees += candidate.fee;
        transactionsToMine.push_back(candidate);
    }

    const double scheduledReward = baseReward + collectedFees;
    const double mintedReward = std::min(scheduledReward, remainingSupply);
    transactionsToMine.push_back(Transaction{"network", minerAddress, mintedReward, nowSeconds(), 0.0});

    Block blockToMine{chain_.size(), chain_.back().getHash(), std::move(transactionsToMine)};
    blockToMine.mine(difficulty_);
    chain_.push_back(blockToMine);

    const std::unordered_map<std::string, bool> minedIds = [&transactionsToMine]() {
        std::unordered_map<std::string, bool> ids;
        for (const auto& tx : transactionsToMine) {
            if (tx.from != "network") {
                ids.emplace(tx.id(), true);
            }
        }
        return ids;
    }();

    pendingTransactions_.erase(
        std::remove_if(pendingTransactions_.begin(), pendingTransactions_.end(), [&minedIds](const Transaction& tx) {
            return minedIds.find(tx.id()) != minedIds.end();
        }),
        pendingTransactions_.end());
}

double Blockchain::getBalance(const std::string& address) const {
    double balance = 0.0;

    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == address) {
                balance -= (tx.amount + tx.fee);
            }
            if (tx.to == address) {
                balance += tx.amount;
            }
        }
    }

    return balance;
}

double Blockchain::getAvailableBalance(const std::string& address) const {
    double balance = getBalance(address);

    for (const auto& tx : pendingTransactions_) {
        if (tx.from == address) {
            balance -= (tx.amount + tx.fee);
        }
        if (tx.to == address) {
            balance += tx.amount;
        }
    }

    return balance;
}

double Blockchain::estimateNextMiningReward() const {
    double totalFees = 0.0;
    for (const auto& tx : getPendingTransactionsForBlockTemplate()) {
        totalFees += tx.fee;
    }

    const double baseReward = blockSubsidyAtHeight(chain_.size());
    const double remainingSupply = std::max(0.0, kMaxSupply - getTotalSupply());
    return std::min(baseReward + totalFees, remainingSupply);
}

double Blockchain::getTotalSupply() const {
    double supply = 0.0;

    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == "network") {
                supply += tx.amount;
            }
        }
    }

    for (const auto& tx : pendingTransactions_) {
        if (tx.from == "network") {
            supply += tx.amount;
        }
    }

    return supply;
}

std::size_t Blockchain::getBlockCount() const {
    return chain_.size();
}

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

bool Blockchain::isValid() const {
    if (chain_.empty() || maxTransactionsPerBlock_ == 0) {
        return false;
    }

    std::unordered_map<std::string, double> balances;
    double cumulativeSupply = 0.0;

    for (std::size_t i = 0; i < chain_.size(); ++i) {
        const auto& current = chain_[i];

        if (!isTimestampAcceptable(current.getTimestamp())) {
            return false;
        }

        if (i == 0) {
            if (!current.hasValidHash(difficulty_)) {
                return false;
            }
        } else {
            const auto& previous = chain_[i - 1];
            if (current.getPreviousHash() != previous.getHash()) {
                return false;
            }
            if (!current.hasValidHash(difficulty_)) {
                return false;
            }
            if (current.getTimestamp() + 1 < previous.getTimestamp()) {
                return false;
            }
        }

        if (i > 0 && current.getTransactions().size() > maxTransactionsPerBlock_) {
            return false;
        }

        if (!hasNonDecreasingTimestamps(current.getTransactions())) {
            return false;
        }

        double blockFees = 0.0;
        double mintedInBlock = 0.0;
        std::size_t coinbaseCount = 0;

        for (const auto& tx : current.getTransactions()) {
            if (!isTransactionShapeValid(tx) || !isTimestampAcceptable(tx.timestamp)) {
                return false;
            }

            if (tx.from != "network") {
                if (tx.fee < kMinRelayFee) {
                    return false;
                }
                if (balances[tx.from] + 1e-9 < tx.amount + tx.fee) {
                    return false;
                }
                balances[tx.from] -= (tx.amount + tx.fee);
                blockFees += tx.fee;
            } else {
                ++coinbaseCount;
                mintedInBlock += tx.amount;
                cumulativeSupply += tx.amount;
                if (cumulativeSupply > kMaxSupply + 1e-9) {
                    return false;
                }
            }
            balances[tx.to] += tx.amount;
        }

        if (i > 0) {
            const double expectedMaxReward = blockSubsidyAtHeight(i) + blockFees;
            if (coinbaseCount != 1 || mintedInBlock > expectedMaxReward + 1e-9) {
                return false;
            }
        }
    }

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

    std::unordered_map<std::string, double> projectedBalance;
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

        const double debit = candidate.amount + candidate.fee;
        if (projectedBalance[candidate.from] + 1e-9 < debit) {
            continue;
        }

        projectedBalance[candidate.from] -= debit;
        projectedBalance[candidate.to] += candidate.amount;
        selected.push_back(candidate);
    }

    return selected;
}

std::string Blockchain::getChainSummary() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(8);
    out << "Novacoin summary\n";
    out << "- blocks=" << getBlockCount() << "\n";
    out << "- total_supply=" << getTotalSupply() << " / " << kMaxSupply << "\n";
    out << "- next_reward_estimate=" << estimateNextMiningReward() << "\n";
    out << "- pending_transactions=" << pendingTransactions_.size() << "\n";
    return out.str();
}

const std::vector<Block>& Blockchain::getChain() const { return chain_; }
const std::vector<Transaction>& Blockchain::getPendingTransactions() const { return pendingTransactions_; }
