#include "blockchain.hpp"

#include <chrono>
#include <iostream>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch())
                                      .count());
}

void printHistory(const Blockchain& chain, const std::string& address) {
    const auto history = chain.getTransactionHistory(address);
    std::cout << "Historique de " << address << " (" << history.size() << " transaction(s)):\n";
    for (const auto& tx : history) {
        std::cout << "  " << tx.from << " -> " << tx.to << " : " << tx.amount << " @" << tx.timestamp << '\n';
    }
}
} // namespace

int main() {
    try {
        Blockchain novacoin{4, 25.0};

        novacoin.createTransaction(Transaction{"network", "alice", 20.0, nowSeconds()});
        novacoin.minePendingTransactions("miner1");

        novacoin.createTransaction(Transaction{"alice", "bob", 10.0, nowSeconds()});
        novacoin.createTransaction(Transaction{"bob", "charlie", 3.5, nowSeconds()});
        novacoin.minePendingTransactions("miner1");

        novacoin.createTransaction(Transaction{"miner1", "alice", 5.0, nowSeconds()});

        std::cout << "Chaîne valide: " << std::boolalpha << novacoin.isValid() << "\n";
        std::cout << "Solde de alice: " << novacoin.getBalance("alice") << "\n";
        std::cout << "Solde disponible de miner1: " << novacoin.getAvailableBalance("miner1") << "\n";
        std::cout << "Transactions en attente: " << novacoin.getPendingTransactions().size() << "\n";

        printHistory(novacoin, "alice");

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
