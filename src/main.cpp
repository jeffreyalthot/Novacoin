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
} // namespace

int main() {
    try {
        Blockchain novacoin{4, 25.0};

        novacoin.createTransaction(Transaction{"alice", "bob", 10.0, nowSeconds()});
        novacoin.createTransaction(Transaction{"bob", "charlie", 3.5, nowSeconds()});

        novacoin.minePendingTransactions("miner1");
        novacoin.minePendingTransactions("miner1");

        std::cout << "Chaîne valide: " << std::boolalpha << novacoin.isValid() << "\n";
        std::cout << "Solde de alice: " << novacoin.getBalance("alice") << "\n";
        std::cout << "Solde de bob: " << novacoin.getBalance("bob") << "\n";
        std::cout << "Solde de charlie: " << novacoin.getBalance("charlie") << "\n";
        std::cout << "Solde de miner1: " << novacoin.getBalance("miner1") << "\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
