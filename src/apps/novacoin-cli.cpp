#include "blockchain.hpp"
#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoin-cli mine <miner_address>\n"
              << "  novacoin-cli send <from> <to> <amount_nova> [fee_nova]\n"
              << "  novacoin-cli balance <address>\n"
              << "  novacoin-cli summary\n";
}

double parseDouble(const std::string& raw, const std::string& field) {
    std::size_t consumed = 0;
    const double value = std::stod(raw, &consumed);
    if (consumed != raw.size()) {
        throw std::invalid_argument("Valeur invalide pour " + field + ": " + raw);
    }
    return value;
}

Blockchain makeDemoChain() {
    Blockchain chain{2, Transaction::fromNOVA(50.0), 8};
    chain.minePendingTransactions("miner");
    chain.createTransaction(Transaction{"miner", "alice", Transaction::fromNOVA(10.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    chain.minePendingTransactions("miner");
    return chain;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        Blockchain chain = makeDemoChain();
        const std::string command = argv[1];

        if (command == "mine") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            chain.minePendingTransactions(argv[2]);
            std::cout << "Bloc mine. Height=" << chain.getBlockCount() - 1
                      << ", total_supply=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(chain.getTotalSupply()) << " NOVA\n";
            return 0;
        }

        if (command == "send") {
            if (argc < 5 || argc > 6) {
                printUsage();
                return 1;
            }
            const Amount amount = Transaction::fromNOVA(parseDouble(argv[4], "amount_nova"));
            const Amount fee = argc == 6 ? Transaction::fromNOVA(parseDouble(argv[5], "fee_nova"))
                                         : Blockchain::kMinRelayFee;

            chain.createTransaction(Transaction{argv[2], argv[3], amount, nowSeconds(), fee});
            std::cout << "Transaction ajoutee. mempool_size=" << chain.getPendingTransactions().size() << "\n";
            return 0;
        }

        if (command == "balance") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const std::string address = argv[2];
            std::cout << address << ": confirmed=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(chain.getBalance(address)) << " NOVA, available="
                      << Transaction::toNOVA(chain.getAvailableBalance(address)) << " NOVA\n";
            return 0;
        }

        if (command == "summary") {
            std::cout << chain.getChainSummary();
            return 0;
        }

        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
