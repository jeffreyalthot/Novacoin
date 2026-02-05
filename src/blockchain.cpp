#include "blockchain.hpp"

#include <algorithm>
#include <chrono>
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

    if (tx.from == "network") {
        return tx.amount >= 0.0 && tx.fee == 0.0;
    }

    return tx.amount > 0.0 && tx.fee >= 0.0;
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
}

Block Blockchain::createGenesisBlock() const {
    Transaction bootstrap{"network", "genesis", 0.0, nowSeconds(), 0.0};
    Block genesis{0, "0", {bootstrap}};
    genesis.mine(difficulty_);
    return genesis;
}

void Blockchain::createTransaction(const Transaction& tx) {
    if (!isTransactionShapeValid(tx)) {
        throw std::invalid_argument("Transaction invalide (adresses, montant ou frais).");
    }

    if (tx.from != "network" && tx.amount + tx.fee > getAvailableBalance(tx.from)) {
        throw std::invalid_argument("Fonds insuffisants pour créer cette transaction (montant + frais).");
    }

    pendingTransactions_.push_back(tx);
}

void Blockchain::minePendingTransactions(const std::string& minerAddress) {
    if (minerAddress.empty()) {
        throw std::invalid_argument("L'adresse du mineur ne peut pas être vide.");
    }

    if (pendingTransactions_.empty()) {
        return;
    }

    const std::size_t txCount = std::min(maxTransactionsPerBlock_, pendingTransactions_.size());
    std::vector<Transaction> transactionsToMine(
        pendingTransactions_.begin(),
        pendingTransactions_.begin() + txCount);

    double collectedFees = 0.0;
    for (const auto& tx : transactionsToMine) {
        collectedFees += tx.fee;
    }
    const double minedReward = miningReward_ + collectedFees;

    Block blockToMine{chain_.size(), chain_.back().getHash(), std::move(transactionsToMine)};
    blockToMine.mine(difficulty_);
    chain_.push_back(blockToMine);

    pendingTransactions_.erase(
        pendingTransactions_.begin(),
        pendingTransactions_.begin() + txCount);
    pendingTransactions_.push_back(Transaction{"network", minerAddress, minedReward, nowSeconds(), 0.0});
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
    const std::size_t txCount = std::min(maxTransactionsPerBlock_, pendingTransactions_.size());

    double totalFees = 0.0;
    for (std::size_t i = 0; i < txCount; ++i) {
        totalFees += pendingTransactions_[i].fee;
    }
    return miningReward_ + totalFees;
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

    for (std::size_t i = 0; i < chain_.size(); ++i) {
        const auto& current = chain_[i];

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
        }

        if (i > 0 && current.getTransactions().size() > maxTransactionsPerBlock_) {
            return false;
        }

        for (const auto& tx : current.getTransactions()) {
            if (!isTransactionShapeValid(tx)) {
                return false;
            }

            if (tx.from != "network") {
                if (balances[tx.from] + 1e-9 < tx.amount + tx.fee) {
                    return false;
                }
                balances[tx.from] -= (tx.amount + tx.fee);
            }
            balances[tx.to] += tx.amount;
        }
    }

    return true;
}

const std::vector<Block>& Blockchain::getChain() const { return chain_; }
const std::vector<Transaction>& Blockchain::getPendingTransactions() const { return pendingTransactions_; }
