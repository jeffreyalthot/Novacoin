#include "blockchain.hpp"
#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoin-regtest summary\n"
              << "  novacoin-regtest balances\n"
              << "  novacoin-regtest mine <miner> [count]\n"
              << "  novacoin-regtest send <from> <to> <amount_nova> [fee_nova]\n"
              << "  novacoin-regtest history <address> [limit]\n"
              << "  novacoin-regtest debug [recent_blocks]\n";
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

std::string formatAmount(Amount amount) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(8) << Transaction::toNOVA(amount);
    return out.str();
}

void printBalances(const Blockchain& chain, const std::vector<std::string>& addresses) {
    for (const auto& address : addresses) {
        std::cout << "  " << address << ": " << formatAmount(chain.getBalance(address)) << " NOVA\n";
    }
}

void printSummary(const Blockchain& regtest, const std::vector<std::string>& addresses) {
    std::cout << "regtest summary\n"
              << "  blocks: " << regtest.getBlockCount() << "\n"
              << "  supply: " << formatAmount(regtest.getTotalSupply()) << " NOVA\n";
    printBalances(regtest, addresses);
    std::cout << "  valid: " << std::boolalpha << regtest.isValid() << "\n";
}

void printNetworkStats(const Blockchain& regtest) {
    const auto stats = regtest.getNetworkStats();
    std::cout << "network stats\n"
              << "  blocks=" << stats.blockCount << "\n"
              << "  user_txs=" << stats.userTransactionCount << "\n"
              << "  coinbase_txs=" << stats.coinbaseTransactionCount << "\n"
              << "  pending_txs=" << stats.pendingTransactionCount << "\n"
              << "  total_transferred=" << formatAmount(stats.totalTransferred) << " NOVA\n"
              << "  total_fees=" << formatAmount(stats.totalFeesPaid) << " NOVA\n"
              << "  total_mined=" << formatAmount(stats.totalMinedRewards) << " NOVA\n"
              << "  median_user_tx=" << formatAmount(stats.medianUserTransactionAmount) << " NOVA\n";
}

void printMempoolStats(const Blockchain& regtest) {
    const auto stats = regtest.getMempoolStats();
    std::cout << "mempool stats\n"
              << "  count=" << stats.transactionCount << "\n";
    if (stats.transactionCount == 0) {
        std::cout << "  (mempool vide)\n";
        return;
    }
    std::cout << "  total_amount=" << formatAmount(stats.totalAmount) << " NOVA\n"
              << "  total_fees=" << formatAmount(stats.totalFees) << " NOVA\n"
              << "  min_fee=" << formatAmount(stats.minFee) << " NOVA\n"
              << "  max_fee=" << formatAmount(stats.maxFee) << " NOVA\n"
              << "  median_fee=" << formatAmount(stats.medianFee) << " NOVA\n"
              << "  oldest_ts=" << stats.oldestTimestamp << "\n"
              << "  newest_ts=" << stats.newestTimestamp << "\n"
              << "  min_age_s=" << stats.minAgeSeconds << "\n"
              << "  max_age_s=" << stats.maxAgeSeconds << "\n"
              << "  median_age_s=" << stats.medianAgeSeconds << "\n";
}

void printRecentBlocks(const Blockchain& regtest, std::size_t count) {
    const auto blocks = regtest.getRecentBlockSummaries(count);
    std::cout << "recent blocks (" << blocks.size() << ")\n";
    if (blocks.empty()) {
        std::cout << "  (aucun bloc)\n";
        return;
    }
    for (const auto& block : blocks) {
        std::cout << "  #" << block.index
                  << " txs=" << block.transactionCount
                  << " user_txs=" << block.userTransactionCount
                  << " fees=" << formatAmount(block.totalFees) << " NOVA"
                  << " diff=" << block.difficulty
                  << " ts=" << block.timestamp << "\n";
    }
}

Blockchain seedRegtest() {
    Blockchain regtest{1, Transaction::fromNOVA(100.0), 6};

    regtest.minePendingTransactions("minerA");
    regtest.createTransaction(
        Transaction{"minerA", "alice", Transaction::fromNOVA(12.0), nowSeconds(), Transaction::fromNOVA(0.2)});
    regtest.createTransaction(
        Transaction{"minerA", "bob", Transaction::fromNOVA(8.0), nowSeconds(), Transaction::fromNOVA(0.2)});
    regtest.minePendingTransactions("minerA");

    regtest.createTransaction(
        Transaction{"alice", "carol", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    regtest.minePendingTransactions("minerB");

    return regtest;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        const std::vector<std::string> addresses = {"minerA", "minerB", "alice", "bob", "carol"};
        Blockchain regtest = seedRegtest();

        const std::string command = argc > 1 ? argv[1] : "summary";
        if (command == "summary") {
            if (argc != 1 && argc != 2) {
                printUsage();
                return 1;
            }
            printSummary(regtest, addresses);
            return 0;
        }

        if (command == "balances") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "balances\n";
            printBalances(regtest, addresses);
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
                regtest.minePendingTransactions(miner);
            }
            std::cout << "mined " << count << " blocks\n"
                      << "  height: " << regtest.getBlockCount() - 1 << "\n"
                      << "  supply: " << formatAmount(regtest.getTotalSupply()) << " NOVA\n";
            return 0;
        }

        if (command == "send") {
            if (argc < 5 || argc > 6) {
                printUsage();
                return 1;
            }
            const Amount amount = Transaction::fromNOVA(parseDouble(argv[4], "amount_nova"));
            const Amount fee =
                argc == 6 ? Transaction::fromNOVA(parseDouble(argv[5], "fee_nova")) : Transaction::fromNOVA(0.01);
            regtest.createTransaction(Transaction{argv[2], argv[3], amount, nowSeconds(), fee});
            std::cout << "transaction queued\n"
                      << "  mempool_size=" << regtest.getPendingTransactions().size() << "\n";
            return 0;
        }

        if (command == "history") {
            if (argc < 3 || argc > 4) {
                printUsage();
                return 1;
            }
            const std::string address = argv[2];
            if (address.empty()) {
                throw std::invalid_argument("L'adresse ne peut pas etre vide.");
            }
            const std::size_t limit = argc == 4 ? parseSize(argv[3], "limit") : 0;
            const auto history = regtest.getTransactionHistoryDetailed(address, limit, true);
            std::cout << "history: " << address << "\n";
            if (history.empty()) {
                std::cout << "  (aucune transaction)\n";
                return 0;
            }
            for (std::size_t i = 0; i < history.size(); ++i) {
                const auto& entry = history[i];
                std::cout << "  #" << (i + 1) << " id=" << entry.tx.id()
                          << " amount=" << formatAmount(entry.tx.amount) << " NOVA\n";
            }
            return 0;
        }

        if (command == "debug") {
            if (argc > 3) {
                printUsage();
                return 1;
            }
            const std::size_t recentBlocks = argc == 3 ? parseSize(argv[2], "recent_blocks") : 5;
            printSummary(regtest, addresses);
            printNetworkStats(regtest);
            printMempoolStats(regtest);
            printRecentBlocks(regtest, recentBlocks);
            return 0;
        }

        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur regtest: " << ex.what() << "\n";
        return 1;
    }
}
