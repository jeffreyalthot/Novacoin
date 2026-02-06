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
              << "  novacoind mempool-stats\n"
              << "  novacoind mempool-ids\n"
              << "  novacoind mempool-summary\n"
              << "  novacoind mempool-age\n"
              << "  novacoind network-stats\n"
              << "  novacoind difficulty\n"
              << "  novacoind time\n"
              << "  novacoind supply\n"
              << "  novacoind monetary [height]\n"
              << "  novacoind supply-audit <start_height> <max_count>\n"
              << "  novacoind consensus\n"
              << "  novacoind reorgs\n"
              << "  novacoind chain-health\n"
              << "  novacoind height\n"
              << "  novacoind tip\n"
              << "  novacoind params\n"
              << "  novacoind version\n";
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

        if (command == "mempool-stats") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            const auto stats = daemonChain.getMempoolStats();
            std::cout << "mempool_stats\n"
                      << "  tx_count=" << stats.transactionCount << "\n"
                      << "  total_amount=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalAmount) << " NOVA\n"
                      << "  total_fees=" << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
                      << "  min_fee=" << Transaction::toNOVA(stats.minFee) << " NOVA\n"
                      << "  max_fee=" << Transaction::toNOVA(stats.maxFee) << " NOVA\n"
                      << "  median_fee=" << Transaction::toNOVA(stats.medianFee) << " NOVA\n"
                      << "  oldest_ts=" << stats.oldestTimestamp << "\n"
                      << "  newest_ts=" << stats.newestTimestamp << "\n";
            return 0;
        }

        if (command == "mempool-ids") {
            if (argc != 2) {
                printUsage();
                return 1;
            }

            const auto blockTemplate = daemonChain.getPendingTransactionsForBlockTemplate();
            std::cout << "mempool_ids=" << blockTemplate.size() << "\n";
            for (std::size_t i = 0; i < blockTemplate.size(); ++i) {
                const auto& tx = blockTemplate[i];
                std::cout << "  #" << (i + 1) << " id=" << tx.id() << " fee=" << std::fixed
                          << std::setprecision(8) << Transaction::toNOVA(tx.fee) << " NOVA"
                          << " amount=" << Transaction::toNOVA(tx.amount) << " NOVA\n";
            }
            return 0;
        }

        if (command == "mempool-summary") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            const auto stats = daemonChain.getMempoolStats();
            const Amount total = stats.totalAmount + stats.totalFees;
            std::cout << "mempool_summary\n"
                      << "  tx_count=" << stats.transactionCount << "\n"
                      << "  total_amount=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalAmount) << " NOVA\n"
                      << "  total_fees=" << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
                      << "  total_including_fees=" << Transaction::toNOVA(total) << " NOVA\n";
            return 0;
        }

        if (command == "mempool-age") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            const auto stats = daemonChain.getMempoolStats();
            std::cout << "mempool_age\n"
                      << "  tx_count=" << stats.transactionCount << "\n"
                      << "  oldest_ts=" << stats.oldestTimestamp << "\n"
                      << "  newest_ts=" << stats.newestTimestamp << "\n"
                      << "  min_age_s=" << stats.minAgeSeconds << "\n"
                      << "  median_age_s=" << stats.medianAgeSeconds << "\n"
                      << "  max_age_s=" << stats.maxAgeSeconds << "\n";
            return 0;
        }

        if (command == "network-stats") {
            if (argc != 2) {
                printUsage();
                return 1;
            }

            const auto stats = daemonChain.getNetworkStats();
            std::cout << "network_stats\n"
                      << "  block_count=" << stats.blockCount << "\n"
                      << "  user_tx_count=" << stats.userTransactionCount << "\n"
                      << "  coinbase_tx_count=" << stats.coinbaseTransactionCount << "\n"
                      << "  pending_tx_count=" << stats.pendingTransactionCount << "\n"
                      << "  total_transferred=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalTransferred) << " NOVA\n"
                      << "  total_fees_paid=" << Transaction::toNOVA(stats.totalFeesPaid) << " NOVA\n"
                      << "  total_mined_rewards=" << Transaction::toNOVA(stats.totalMinedRewards) << " NOVA\n"
                      << "  median_user_tx_amount=" << Transaction::toNOVA(stats.medianUserTransactionAmount)
                      << " NOVA\n";
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

        if (command == "time") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "time\n"
                      << "  median_time_past=" << daemonChain.getMedianTimePast() << "\n"
                      << "  next_min_timestamp=" << daemonChain.estimateNextMinimumTimestamp() << "\n";
            return 0;
        }

        if (command == "supply") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "supply\n"
                      << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
                      << "  total=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n"
                      << "  max=" << Transaction::toNOVA(Blockchain::kMaxSupply) << " NOVA\n";
            return 0;
        }

        if (command == "monetary") {
            if (argc > 3) {
                printUsage();
                return 1;
            }

            const std::size_t height = argc == 3
                ? parseSize(argv[2], "height")
                : (daemonChain.getBlockCount() > 0 ? daemonChain.getBlockCount() - 1 : 0);
            const auto projection = daemonChain.getMonetaryProjection(height);
            std::cout << "monetary\n"
                      << "  height=" << projection.height << "\n"
                      << "  subsidy_current=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(projection.currentSubsidy) << " NOVA\n"
                      << "  projected_supply=" << Transaction::toNOVA(projection.projectedSupply)
                      << " NOVA\n"
                      << "  issuance_remaining=" << Transaction::toNOVA(projection.remainingIssuable)
                      << " NOVA\n"
                      << "  next_halving_height=" << projection.nextHalvingHeight << "\n"
                      << "  next_subsidy=" << Transaction::toNOVA(projection.nextSubsidy) << " NOVA\n";
            return 0;
        }

        if (command == "supply-audit") {
            if (argc != 4) {
                printUsage();
                return 1;
            }

            const std::size_t startHeight = parseSize(argv[2], "start_height");
            const std::size_t maxCount = parseSize(argv[3], "max_count");
            const auto audit = daemonChain.getSupplyAudit(startHeight, maxCount);
            std::cout << "supply_audit=" << audit.size() << "\n";
            for (const auto& entry : audit) {
                std::cout << "  h=" << entry.height
                          << " subsidy=" << std::fixed << std::setprecision(8)
                          << Transaction::toNOVA(entry.blockSubsidy) << " NOVA"
                          << " fees=" << Transaction::toNOVA(entry.totalFees) << " NOVA"
                          << " minted=" << Transaction::toNOVA(entry.mintedReward) << " NOVA"
                          << " max_allowed=" << Transaction::toNOVA(entry.maxAllowedReward) << " NOVA"
                          << " supply=" << Transaction::toNOVA(entry.cumulativeSupply) << " NOVA"
                          << " reward_ok=" << (entry.rewardWithinLimit ? "yes" : "no")
                          << " cap_ok=" << (entry.supplyWithinCap ? "yes" : "no")
                          << " hash=" << entry.hash << "\n";
            }
            return 0;
        }

        if (command == "consensus") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "consensus\n"
                      << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
                      << "  current_difficulty=" << daemonChain.getCurrentDifficulty() << "\n"
                      << "  next_difficulty=" << daemonChain.estimateNextDifficulty() << "\n"
                      << "  cumulative_work=" << daemonChain.getCumulativeWork() << "\n"
                      << "  median_time_past=" << daemonChain.getMedianTimePast() << "\n"
                      << "  next_min_timestamp=" << daemonChain.estimateNextMinimumTimestamp() << "\n"
                      << "  next_reward=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(daemonChain.estimateNextMiningReward()) << " NOVA\n";
            return 0;
        }

        if (command == "reorgs") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "reorgs\n"
                      << "  reorg_count=" << daemonChain.getReorgCount() << "\n"
                      << "  last_reorg_depth=" << daemonChain.getLastReorgDepth() << "\n"
                      << "  last_fork_height=" << daemonChain.getLastForkHeight() << "\n"
                      << "  last_fork_hash=" << daemonChain.getLastForkHash() << "\n";
            return 0;
        }

        if (command == "chain-health") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "chain_health\n"
                      << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
                      << "  chain_valid=" << std::boolalpha << daemonChain.isValid() << "\n"
                      << "  cumulative_work=" << daemonChain.getCumulativeWork() << "\n"
                      << "  reorg_count=" << daemonChain.getReorgCount() << "\n"
                      << "  last_reorg_depth=" << daemonChain.getLastReorgDepth() << "\n"
                      << "  last_fork_height=" << daemonChain.getLastForkHeight() << "\n"
                      << "  last_fork_hash=" << daemonChain.getLastForkHash() << "\n"
                      << "  pending_tx=" << daemonChain.getPendingTransactions().size() << "\n";
            return 0;
        }

        if (command == "height") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            const auto& chainData = daemonChain.getChain();
            if (chainData.empty()) {
                std::cout << "height=0\n";
                return 0;
            }
            const auto& tip = chainData.back();
            std::cout << "height=" << tip.getIndex() << "\n"
                      << "hash=" << tip.getHash() << "\n";
            return 0;
        }

        if (command == "tip") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            const auto& chainData = daemonChain.getChain();
            if (chainData.empty()) {
                std::cout << "tip=none\n";
                return 0;
            }
            const auto& tip = chainData.back();
            std::cout << "tip\n"
                      << "  height=" << tip.getIndex() << "\n"
                      << "  hash=" << tip.getHash() << "\n"
                      << "  prev_hash=" << tip.getPreviousHash() << "\n";
            return 0;
        }

        if (command == "params") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "params\n"
                      << "  max_supply=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(Blockchain::kMaxSupply) << " NOVA\n"
                      << "  halving_interval=" << Blockchain::kHalvingInterval << "\n"
                      << "  target_block_time_s=" << Blockchain::kTargetBlockTimeSeconds << "\n"
                      << "  max_future_drift_s=" << Blockchain::kMaxFutureDriftSeconds << "\n"
                      << "  difficulty_adjust_interval=" << Blockchain::kDifficultyAdjustmentInterval << "\n"
                      << "  min_difficulty=" << Blockchain::kMinDifficulty << "\n"
                      << "  max_difficulty=" << Blockchain::kMaxDifficulty << "\n"
                      << "  mempool_expiry_s=" << Blockchain::kMempoolExpirySeconds << "\n"
                      << "  max_mempool_txs=" << Blockchain::kMaxMempoolTransactions << "\n"
                      << "  min_relay_fee=" << Transaction::toNOVA(Blockchain::kMinRelayFee) << " NOVA\n";
            return 0;
        }

        if (command == "version") {
            if (argc != 2) {
                printUsage();
                return 1;
            }
            std::cout << "novacoind version=0.1.0\n"
                      << "  network=regtest\n";
            return 0;
        }

        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur novacoind: " << ex.what() << "\n";
        return 1;
    }
}
