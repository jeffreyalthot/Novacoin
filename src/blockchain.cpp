#include "blockchain.hpp"

#include <chrono>
#include <stdexcept>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
} // namespace

Blockchain::Blockchain(unsigned int difficulty, double miningReward)
    : difficulty_(difficulty),
      miningReward_(miningReward),
      chain_({createGenesisBlock()}),
      pendingTransactions_() {}

Block Blockchain::createGenesisBlock() const {
    Transaction bootstrap{"network", "genesis", 0.0, nowSeconds()};
    Block genesis{0, "0", {bootstrap}};
    genesis.mine(difficulty_);
    return genesis;
}

void Blockchain::createTransaction(const Transaction& tx) {
    if (tx.amount <= 0.0) {
        throw std::invalid_argument("Le montant d'une transaction doit être strictement positif.");
    }
    if (tx.from.empty() || tx.to.empty()) {
        throw std::invalid_argument("Les adresses source et destination sont obligatoires.");
    }

    if (tx.from != "network" && tx.amount > getAvailableBalance(tx.from)) {
        throw std::invalid_argument("Fonds insuffisants pour créer cette transaction.");
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

    Block blockToMine{chain_.size(), chain_.back().getHash(), pendingTransactions_};
    blockToMine.mine(difficulty_);
    chain_.push_back(blockToMine);

    pendingTransactions_.clear();
    pendingTransactions_.push_back(Transaction{"network", minerAddress, miningReward_, nowSeconds()});
}

double Blockchain::getBalance(const std::string& address) const {
    double balance = 0.0;

    for (const auto& block : chain_) {
        for (const auto& tx : block.getTransactions()) {
            if (tx.from == address) {
                balance -= tx.amount;
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
            balance -= tx.amount;
        }
        if (tx.to == address) {
            balance += tx.amount;
        }
    }

    return balance;
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
    if (chain_.empty()) {
        return false;
    }

    for (std::size_t i = 1; i < chain_.size(); ++i) {
        const auto& current = chain_[i];
        const auto& previous = chain_[i - 1];

        if (current.getPreviousHash() != previous.getHash()) {
            return false;
        }

        if (!current.hasValidHash(difficulty_)) {
            return false;
        }
    }

    return chain_.front().hasValidHash(difficulty_);
}

const std::vector<Block>& Blockchain::getChain() const { return chain_; }
const std::vector<Transaction>& Blockchain::getPendingTransactions() const { return pendingTransactions_; }
