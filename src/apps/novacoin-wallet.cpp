#include "blockchain.hpp"
#include "transaction.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoin-wallet <address>\n";
}

void printBalance(const Blockchain& chain, const std::string& address) {
    std::cout << "Wallet: " << address << "\n"
              << "  confirmed: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(chain.getBalance(address))
              << " NOVA\n"
              << "  available: " << std::fixed << std::setprecision(8)
              << Transaction::toNOVA(chain.getAvailableBalance(address)) << " NOVA\n";
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            printUsage();
            return 1;
        }

        const std::string address = argv[1];
        if (address.empty()) {
            throw std::invalid_argument("L'adresse wallet ne peut pas etre vide.");
        }

        Blockchain chain{2, Transaction::fromNOVA(50.0), 10};
        chain.minePendingTransactions("miner");
        chain.createTransaction(Transaction{"miner", address, Transaction::fromNOVA(3.5), 1, Transaction::fromNOVA(0.1)});
        chain.minePendingTransactions("miner");

        printBalance(chain, address);
        std::cout << "  tx_count: " << chain.getTransactionHistory(address).size() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
