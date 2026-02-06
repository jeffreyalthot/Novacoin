#include "blockchain.hpp"
#include "core/build_info.hpp"

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
        std::cout << "  " << tx.from << " -> " << tx.to << " : " << Transaction::toNOVA(tx.amount)
                  << " (frais=" << Transaction::toNOVA(tx.fee) << ") @" << tx.timestamp << '\n';
    }
}
} // namespace

int main() {
    try {
        Blockchain novacoin{4, Transaction::fromNOVA(25.0), 2};

        std::cout << novacoin::projectLayoutSummary() << "\n";

        novacoin.minePendingTransactions("miner1");
        novacoin.createTransaction(
            Transaction{"miner1", "alice", Transaction::fromNOVA(10.0), nowSeconds(), Transaction::fromNOVA(0.15)});
        novacoin.minePendingTransactions("miner1");

        novacoin.createTransaction(
            Transaction{"alice", "bob", Transaction::fromNOVA(6.0), nowSeconds(), Transaction::fromNOVA(0.05)});
        novacoin.createTransaction(Transaction{"bob", "charlie", Transaction::fromNOVA(3.5), nowSeconds(),
                                               Transaction::fromNOVA(0.05)});

        std::cout << "Récompense estimée du prochain bloc: "
                  << Transaction::toNOVA(novacoin.estimateNextMiningReward()) << "\n";
        novacoin.minePendingTransactions("miner1");

        novacoin.createTransaction(
            Transaction{"miner1", "alice", Transaction::fromNOVA(5.0), nowSeconds(), Transaction::fromNOVA(0.1)});

        std::cout << "Nombre de blocs: " << novacoin.getBlockCount() << "\n";
        std::cout << "Masse monétaire totale: " << Transaction::toNOVA(novacoin.getTotalSupply()) << "\n";
        std::cout << "Chaîne valide: " << std::boolalpha << novacoin.isValid() << "\n";
        std::cout << "Solde de alice: " << Transaction::toNOVA(novacoin.getBalance("alice")) << "\n";
        std::cout << "Solde disponible de miner1: " << Transaction::toNOVA(novacoin.getAvailableBalance("miner1"))
                  << "\n";
        std::cout << "Transactions en attente: " << novacoin.getPendingTransactions().size() << "\n";
        std::cout << "Template prochain bloc (tx user): "
                  << novacoin.getPendingTransactionsForBlockTemplate().size() << "\n";
        std::cout << novacoin.getChainSummary();

        novacoin.minePendingTransactions("miner2");
        std::cout << "Après minage fractionné (capacité=2), blocs: " << novacoin.getBlockCount()
                  << ", en attente: " << novacoin.getPendingTransactions().size() << "\n";

        printHistory(novacoin, "alice");

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
