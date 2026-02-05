#include "blockchain.hpp"
#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
} // namespace

int main() {
    try {
        Blockchain daemonChain{2, Transaction::fromNOVA(50.0), 20};

        daemonChain.minePendingTransactions("node-miner");
        daemonChain.createTransaction(
            Transaction{"node-miner", "faucet", Transaction::fromNOVA(5.0), nowSeconds(), Transaction::fromNOVA(0.1)});
        daemonChain.minePendingTransactions("node-miner");

        std::cout << "novacoind started (simulation mode)\n"
                  << "  height: " << daemonChain.getBlockCount() - 1 << "\n"
                  << "  difficulty: " << daemonChain.getCurrentDifficulty() << "\n"
                  << "  total_supply: " << std::fixed << std::setprecision(8)
                  << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n"
                  << "  pending_tx: " << daemonChain.getPendingTransactions().size() << "\n"
                  << "  chain_valid: " << std::boolalpha << daemonChain.isValid() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur novacoind: " << ex.what() << "\n";
        return 1;
    }
}
