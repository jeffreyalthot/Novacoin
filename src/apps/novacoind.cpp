#include "blockchain.hpp"
#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoind status\n"
              << "  novacoind mine <miner> [count]\n"
              << "  novacoind submit <from> <to> <amount_nova> [fee_nova]\n"
              << "  novacoind mempool\n"
              << "  novacoind difficulty\n";
}

double parseDouble(const std::string& raw, const std::string& field) {
    std::size_t consumed = 0;
    const double value = std::stod(raw, &consumed);
    if (consumed != raw.size()) {
        throw std::invalid_argument("Valeur invalide pour " + field + ": " + raw);
    }
    return value;
}

std::size_t parseSize(const std::string& raw, const std::string& field) {
    std::size_t consumed = 0;
    const auto value = std::stoull(raw, &consumed);
    if (consumed != raw.size()) {
        throw std::invalid_argument("Valeur invalide pour " + field + ": " + raw);
    }
    return static_cast<std::size_t>(value);
}

void printStatus(const Blockchain& daemonChain) {
    std::cout << "novacoind started (simulation mode)\n"
              << "  height: " << daemonChain.getBlockCount() - 1 << "\n"
              << "  difficulty: " << daemonChain.getCurrentDifficulty() << "\n"
              << "  total_supply: " << std::fixed << std::setprecision(8)
              << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n"
              << "  pending_tx: " << daemonChain.getPendingTransactions().size() << "\n"
              << "  chain_valid: " << std::boolalpha << daemonChain.isValid() << "\n";
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        Blockchain daemonChain{2, Transaction::fromNOVA(50.0), 20};

        daemonChain.minePendingTransactions("node-miner");
        daemonChain.createTransaction(
            Transaction{"node-miner", "faucet", Transaction::fromNOVA(5.0), nowSeconds(), Transaction::fromNOVA(0.1)});
        daemonChain.minePendingTransactions("node-miner");

        const std::string command = argc > 1 ? argv[1] : "status";
        if (command == "status") {
            if (argc != 1 && argc != 2) {
                printUsage();
                return 1;
            }
            printStatus(daemonChain);
            return 0;
        }

        if (command == "mine") {
            if (argc < 3 || argc > 4) {
                printUsage();
                return 1;
            }
            const std::string miner = argv[2];
            if (miner.empty()) {
                throw std::invalid_argument("Le miner ne peut pas etre vide.");
            }
            const std::size_t count = argc == 4 ? parseSize(argv[3], "count") : 1;
            for (std::size_t i = 0; i < count; ++i) {
                daemonChain.minePendingTransactions(miner);
            }
            std::cout << "mined " << count << " blocks\n"
                      << "  height: " << daemonChain.getBlockCount() - 1 << "\n"
                      << "  total_supply: " << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n";
            return 0;
        }

        if (command == "submit") {
            if (argc < 5 || argc > 6) {
                printUsage();
                return 1;
            }
            const Amount amount = Transaction::fromNOVA(parseDouble(argv[4], "amount_nova"));
            const Amount fee =
                argc == 6 ? Transaction::fromNOVA(parseDouble(argv[5], "fee_nova")) : Blockchain::kMinRelayFee;
            daemonChain.createTransaction(Transaction{argv[2], argv[3], amount, nowSeconds(), fee});
            std::cout << "transaction accepted\n"
                      << "  mempool_size=" << daemonChain.getPendingTransactions().size() << "\n";
            return 0;
        }

        if (command == "mempool") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            const auto stats = daemonChain.getMempoolStats();
            std::cout << "mempool\n"
                      << "  tx_count=" << stats.transactionCount << "\n"
                      << "  total_fees=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
                      << "  min_fee=" << Transaction::toNOVA(stats.minFee) << " NOVA\n"
                      << "  max_fee=" << Transaction::toNOVA(stats.maxFee) << " NOVA\n";
            return 0;
        }

        if (command == "difficulty") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "difficulty\n"
                      << "  current=" << daemonChain.getCurrentDifficulty() << "\n"
                      << "  next=" << daemonChain.estimateNextDifficulty() << "\n";
            return 0;
        }

        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur novacoind: " << ex.what() << "\n";
        return 1;
    }
}
