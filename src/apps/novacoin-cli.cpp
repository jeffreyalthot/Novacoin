#include "blockchain.hpp"
#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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
              << "  novacoin-cli summary\n"
              << "  novacoin-cli address-stats <address>\n"
              << "  novacoin-cli mempool-stats\n"
              << "  novacoin-cli top <limit>\n"
              << "  novacoin-cli headers <start_height> <max_count>\n"
              << "  novacoin-cli locator\n"
              << "  novacoin-cli headers-sync <max_count> [locator_hash ...]\n"
              << "  novacoin-cli headers-sync-stop <max_count> <stop_hash> [locator_hash ...]\n"
              << "  novacoin-cli block <height|hash>\n"
              << "  novacoin-cli blocks <max_count>\n";
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

        if (command == "address-stats") {
            if (argc != 3) {
                printUsage();
                return 1;
            }

            const auto stats = chain.getAddressStats(argv[2]);
            std::cout << "Address stats for " << argv[2] << "\n"
                      << "  total_received=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalReceived) << " NOVA\n"
                      << "  total_sent=" << Transaction::toNOVA(stats.totalSent) << " NOVA\n"
                      << "  fees_paid=" << Transaction::toNOVA(stats.feesPaid) << " NOVA\n"
                      << "  mined_rewards=" << Transaction::toNOVA(stats.minedRewards) << " NOVA\n"
                      << "  pending_outgoing=" << Transaction::toNOVA(stats.pendingOutgoing) << " NOVA\n"
                      << "  outgoing_tx=" << stats.outgoingTransactionCount << "\n"
                      << "  incoming_tx=" << stats.incomingTransactionCount << "\n"
                      << "  mined_blocks=" << stats.minedBlockCount << "\n";
            return 0;
        }

        if (command == "mempool-stats") {
            if (argc != 2) {
                printUsage();
                return 1;
            }

            const auto stats = chain.getMempoolStats();
            std::cout << "Mempool stats\n"
                      << "  tx_count=" << stats.transactionCount << "\n"
                      << "  total_amount=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalAmount) << " NOVA\n"
                      << "  total_fees=" << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
                      << "  min_fee=" << Transaction::toNOVA(stats.minFee) << " NOVA\n"
                      << "  max_fee=" << Transaction::toNOVA(stats.maxFee) << " NOVA\n"
                      << "  median_fee=" << Transaction::toNOVA(stats.medianFee) << " NOVA\n";
            return 0;
        }

        if (command == "headers") {
            if (argc != 4) {
                printUsage();
                return 1;
            }

            const std::size_t startHeight = static_cast<std::size_t>(std::stoull(argv[2]));
            const std::size_t maxCount = static_cast<std::size_t>(std::stoull(argv[3]));
            const auto headers = chain.getHeadersFromHeight(startHeight, maxCount);
            std::cout << "headers=" << headers.size() << "\n";
            for (const auto& header : headers) {
                std::cout << "  h=" << header.index << " diff=" << header.difficulty << " ts=" << header.timestamp
                          << " hash=" << header.hash << " prev=" << header.previousHash << "\n";
            }
            return 0;
        }

        if (command == "headers-sync") {
            if (argc < 3) {
                printUsage();
                return 1;
            }

            const std::size_t maxCount = static_cast<std::size_t>(std::stoull(argv[2]));
            std::vector<std::string> locatorHashes;
            locatorHashes.reserve(static_cast<std::size_t>(argc > 3 ? argc - 3 : 0));
            for (int i = 3; i < argc; ++i) {
                locatorHashes.emplace_back(argv[i]);
            }

            const auto headers = chain.getHeadersForLocator(locatorHashes, maxCount);
            std::cout << "headers_sync=" << headers.size() << "\n";
            for (const auto& header : headers) {
                std::cout << "  h=" << header.index << " diff=" << header.difficulty << " ts=" << header.timestamp
                          << " hash=" << header.hash << " prev=" << header.previousHash << "\n";
            }
            return 0;
        }

        if (command == "headers-sync-stop") {
            if (argc < 4) {
                printUsage();
                return 1;
            }

            const std::size_t maxCount = static_cast<std::size_t>(std::stoull(argv[2]));
            const std::string stopHash = argv[3];
            std::vector<std::string> locatorHashes;
            locatorHashes.reserve(static_cast<std::size_t>(argc > 4 ? argc - 4 : 0));
            for (int i = 4; i < argc; ++i) {
                locatorHashes.emplace_back(argv[i]);
            }

            const auto headers = chain.getHeadersForLocatorWithStop(locatorHashes, maxCount, stopHash);
            std::cout << "headers_sync_stop=" << headers.size() << "\n";
            for (const auto& header : headers) {
                std::cout << "  h=" << header.index << " diff=" << header.difficulty << " ts=" << header.timestamp
                          << " hash=" << header.hash << " prev=" << header.previousHash << "\n";
            }
            return 0;
        }

        if (command == "locator") {
            if (argc != 2) {
                printUsage();
                return 1;
            }

            const auto locator = chain.getBlockLocatorHashes();
            std::cout << "locator_size=" << locator.size() << "\n";
            for (std::size_t i = 0; i < locator.size(); ++i) {
                std::cout << "  [" << i << "] " << locator[i] << "\n";
            }
            return 0;
        }

        if (command == "block") {
            if (argc != 3) {
                printUsage();
                return 1;
            }

            std::optional<BlockSummary> summary;
            try {
                const std::size_t height = static_cast<std::size_t>(std::stoull(argv[2]));
                summary = chain.getBlockSummaryByHeight(height);
            } catch (const std::exception&) {
                summary = chain.getBlockSummaryByHash(argv[2]);
            }

            if (!summary.has_value()) {
                std::cout << "block_not_found\n";
                return 0;
            }

            std::cout << "block h=" << summary->index << " diff=" << summary->difficulty
                      << " ts=" << summary->timestamp << " txs=" << summary->transactionCount
                      << " user_txs=" << summary->userTransactionCount << " fees=" << std::fixed
                      << std::setprecision(8) << Transaction::toNOVA(summary->totalFees) << " NOVA"
                      << " hash=" << summary->hash << " prev=" << summary->previousHash << "\n";
            return 0;
        }

        if (command == "blocks") {
            if (argc != 3) {
                printUsage();
                return 1;
            }

            const std::size_t maxCount = static_cast<std::size_t>(std::stoull(argv[2]));
            const auto summaries = chain.getRecentBlockSummaries(maxCount);
            std::cout << "blocks=" << summaries.size() << "\n";
            for (const auto& summary : summaries) {
                std::cout << "  h=" << summary.index << " diff=" << summary.difficulty << " ts="
                          << summary.timestamp << " txs=" << summary.transactionCount << " user_txs="
                          << summary.userTransactionCount << " fees=" << std::fixed << std::setprecision(8)
                          << Transaction::toNOVA(summary.totalFees) << " NOVA"
                          << " hash=" << summary.hash << " prev=" << summary.previousHash << "\n";
            }
            return 0;
        }

        if (command == "top") {
            if (argc != 3) {
                printUsage();
                return 1;
            }

            const std::size_t limit = static_cast<std::size_t>(std::stoull(argv[2]));
            const auto topBalances = chain.getTopBalances(limit);
            std::cout << "Top balances (limit=" << limit << ")\n";
            for (std::size_t i = 0; i < topBalances.size(); ++i) {
                std::cout << "  #" << (i + 1) << " " << topBalances[i].first << "="
                          << std::fixed << std::setprecision(8) << Transaction::toNOVA(topBalances[i].second)
                          << " NOVA\n";
            }
            return 0;
        }

        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
