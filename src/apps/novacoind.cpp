#include "blockchain.hpp"
#include "rpc/rpc_context.hpp"
#include "rpc/rpc_dispatcher.hpp"
#include "rpc/rpc_server.hpp"
#include "rpc/rpc_types.hpp"
#include "network/p2p_node.hpp"
#include "transaction.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr const char* kDefaultNodeId = "novacoind";
constexpr const char* kDefaultEndpoint = "127.0.0.1:9333";
constexpr const char* kDefaultNetworkId = "regtest";
constexpr std::chrono::milliseconds kDefaultSyncInterval{1000};
constexpr std::chrono::milliseconds kDefaultMineInterval{10000};
constexpr std::chrono::milliseconds kDefaultStatusInterval{5000};
constexpr std::chrono::milliseconds kDefaultLoopSleep{50};
std::atomic<bool> gShouldStop{false};

std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoind daemon [--node-id <id>] [--endpoint <host:port>] [--network <id>]\n"
              << "                   [--miner <address>] [--sync-interval-ms <ms>]\n"
              << "                   [--mine-interval-ms <ms>] [--status-interval-ms <ms>]\n"
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
              << "  novacoind work\n"
              << "  novacoind reorgs\n"
              << "  novacoind chain-health\n"
              << "  novacoind height\n"
              << "  novacoind tip\n"
              << "  novacoind params\n"
              << "  novacoind version\n"
              << "  novacoind rpc <method> [params]\n";
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

void handleSignal(int) {
    gShouldStop.store(true);
}

struct DaemonConfig {
    std::string nodeId = kDefaultNodeId;
    std::string endpoint = kDefaultEndpoint;
    std::string networkId = kDefaultNetworkId;
    std::optional<std::string> minerAddress;
    std::chrono::milliseconds syncInterval = kDefaultSyncInterval;
    std::chrono::milliseconds mineInterval = kDefaultMineInterval;
    std::chrono::milliseconds statusInterval = kDefaultStatusInterval;
    std::chrono::milliseconds loopSleep = kDefaultLoopSleep;
};

bool parseDaemonArgs(const std::vector<std::string>& args, DaemonConfig& config, std::string& error) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--node-id") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --node-id";
                return false;
            }
            config.nodeId = args[++i];
            continue;
        }
        if (arg == "--endpoint") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --endpoint";
                return false;
            }
            config.endpoint = args[++i];
            continue;
        }
        if (arg == "--network") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --network";
                return false;
            }
            config.networkId = args[++i];
            continue;
        }
        if (arg == "--miner") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --miner";
                return false;
            }
            config.minerAddress = args[++i];
            continue;
        }
        if (arg == "--sync-interval-ms") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --sync-interval-ms";
                return false;
            }
            config.syncInterval = std::chrono::milliseconds(parseSize(args[++i], "sync-interval-ms"));
            continue;
        }
        if (arg == "--mine-interval-ms") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --mine-interval-ms";
                return false;
            }
            config.mineInterval = std::chrono::milliseconds(parseSize(args[++i], "mine-interval-ms"));
            continue;
        }
        if (arg == "--status-interval-ms") {
            if (i + 1 >= args.size()) {
                error = "Missing value for --status-interval-ms";
                return false;
            }
            config.statusInterval = std::chrono::milliseconds(parseSize(args[++i], "status-interval-ms"));
            continue;
        }
        error = "Unknown daemon argument: " + arg;
        return false;
    }
    return true;
}

void printDaemonStatus(const Blockchain& daemonChain, const P2PNode& node) {
    std::cout << "daemon_status\n"
              << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
              << "  difficulty=" << daemonChain.getCurrentDifficulty() << "\n"
              << "  total_supply=" << std::fixed << std::setprecision(8)
              << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n"
              << "  mempool_size=" << daemonChain.getPendingTransactions().size() << "\n"
              << "  peer_count=" << node.peerCount() << "\n"
              << "  chain_valid=" << std::boolalpha << daemonChain.isValid() << "\n";
}

int runDaemon(const std::vector<std::string>& args, Blockchain& daemonChain) {
    DaemonConfig config;
    std::string error;
    if (!parseDaemonArgs(args, config, error)) {
        std::cerr << "Erreur daemon: " << error << "\n";
        printUsage();
        return 1;
    }

    gShouldStop.store(false);
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    P2PNode node(config.nodeId, config.endpoint, config.networkId, daemonChain, nullptr);

    std::cout << "novacoind daemon started\n"
              << "  node_id=" << config.nodeId << "\n"
              << "  endpoint=" << config.endpoint << "\n"
              << "  network=" << config.networkId << "\n";
    if (config.minerAddress) {
        std::cout << "  miner=" << *config.minerAddress << "\n";
    }

    auto nextSync = std::chrono::steady_clock::now();
    auto nextMine = std::chrono::steady_clock::now();
    auto nextStatus = std::chrono::steady_clock::now();

    while (!gShouldStop.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextSync) {
            node.broadcastBeacon();
            node.syncWithPeers();
            nextSync = now + config.syncInterval;
        }
        if (config.minerAddress && now >= nextMine) {
            daemonChain.minePendingTransactions(*config.minerAddress);
            nextMine = now + config.mineInterval;
        }
        if (now >= nextStatus) {
            printDaemonStatus(daemonChain, node);
            nextStatus = now + config.statusInterval;
        }
        std::this_thread::sleep_for(config.loopSleep);
    }

    std::cout << "novacoind daemon stopped\n";
    return 0;
}


enum class CommandOutcome {
    Ok,
    InvalidArgs,
    Unknown
};

CommandOutcome runCommand(const std::string& command,
                          const std::vector<std::string>& args,
                          Blockchain& daemonChain,
                          std::ostream& out,
                          std::string& error) {
    if (command == "status") {
        if (!args.empty()) {
            error = "Parametres invalides pour status";
            return CommandOutcome::InvalidArgs;
        }
        out << "novacoind started (simulation mode)\n"
            << "  height: " << daemonChain.getBlockCount() - 1 << "\n"
            << "  difficulty: " << daemonChain.getCurrentDifficulty() << "\n"
            << "  total_supply: " << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n"
            << "  pending_tx: " << daemonChain.getPendingTransactions().size() << "\n"
            << "  chain_valid: " << std::boolalpha << daemonChain.isValid() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "mine") {
        if (args.size() < 1 || args.size() > 2) {
            error = "Parametres invalides pour mine";
            return CommandOutcome::InvalidArgs;
        }
        const std::string miner = args[0];
        if (miner.empty()) {
            throw std::invalid_argument("Le miner ne peut pas etre vide.");
        }
        const std::size_t count = args.size() == 2 ? parseSize(args[1], "count") : 1;
        for (std::size_t i = 0; i < count; ++i) {
            daemonChain.minePendingTransactions(miner);
        }
        out << "mined " << count << " blocks\n"
            << "  height: " << daemonChain.getBlockCount() - 1 << "\n"
            << "  total_supply: " << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "submit") {
        if (args.size() < 3 || args.size() > 4) {
            error = "Parametres invalides pour submit";
            return CommandOutcome::InvalidArgs;
        }
        const Amount amount = Transaction::fromNOVA(parseDouble(args[2], "amount_nova"));
        const Amount fee =
            args.size() == 4 ? Transaction::fromNOVA(parseDouble(args[3], "fee_nova")) : Blockchain::kMinRelayFee;
        daemonChain.createTransaction(Transaction{args[0], args[1], amount, nowSeconds(), fee});
        out << "transaction accepted\n"
            << "  mempool_size=" << daemonChain.getPendingTransactions().size() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "mempool") {
        if (!args.empty()) {
            error = "Parametres invalides pour mempool";
            return CommandOutcome::InvalidArgs;
        }
        const auto stats = daemonChain.getMempoolStats();
        out << "mempool\n"
            << "  tx_count=" << stats.transactionCount << "\n"
            << "  total_fees=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
            << "  min_fee=" << Transaction::toNOVA(stats.minFee) << " NOVA\n"
            << "  max_fee=" << Transaction::toNOVA(stats.maxFee) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "mempool-stats") {
        if (!args.empty()) {
            error = "Parametres invalides pour mempool-stats";
            return CommandOutcome::InvalidArgs;
        }
        const auto stats = daemonChain.getMempoolStats();
        out << "mempool_stats\n"
            << "  tx_count=" << stats.transactionCount << "\n"
            << "  total_amount=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(stats.totalAmount) << " NOVA\n"
            << "  total_fees=" << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
            << "  min_fee=" << Transaction::toNOVA(stats.minFee) << " NOVA\n"
            << "  max_fee=" << Transaction::toNOVA(stats.maxFee) << " NOVA\n"
            << "  median_fee=" << Transaction::toNOVA(stats.medianFee) << " NOVA\n"
            << "  oldest_ts=" << stats.oldestTimestamp << "\n"
            << "  newest_ts=" << stats.newestTimestamp << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "mempool-ids") {
        if (!args.empty()) {
            error = "Parametres invalides pour mempool-ids";
            return CommandOutcome::InvalidArgs;
        }
        const auto blockTemplate = daemonChain.getPendingTransactionsForBlockTemplate();
        out << "mempool_ids=" << blockTemplate.size() << "\n";
        for (std::size_t i = 0; i < blockTemplate.size(); ++i) {
            const auto& tx = blockTemplate[i];
            out << "  #" << (i + 1) << " id=" << tx.id() << " fee=" << std::fixed << std::setprecision(8)
                << Transaction::toNOVA(tx.fee) << " NOVA"
                << " amount=" << Transaction::toNOVA(tx.amount) << " NOVA\n";
        }
        return CommandOutcome::Ok;
    }

    if (command == "mempool-summary") {
        if (!args.empty()) {
            error = "Parametres invalides pour mempool-summary";
            return CommandOutcome::InvalidArgs;
        }
        const auto stats = daemonChain.getMempoolStats();
        const Amount total = stats.totalAmount + stats.totalFees;
        out << "mempool_summary\n"
            << "  tx_count=" << stats.transactionCount << "\n"
            << "  total_amount=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(stats.totalAmount) << " NOVA\n"
            << "  total_fees=" << Transaction::toNOVA(stats.totalFees) << " NOVA\n"
            << "  total_including_fees=" << Transaction::toNOVA(total) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "mempool-age") {
        if (!args.empty()) {
            error = "Parametres invalides pour mempool-age";
            return CommandOutcome::InvalidArgs;
        }
        const auto stats = daemonChain.getMempoolStats();
        out << "mempool_age\n"
            << "  tx_count=" << stats.transactionCount << "\n"
            << "  oldest_ts=" << stats.oldestTimestamp << "\n"
            << "  newest_ts=" << stats.newestTimestamp << "\n"
            << "  min_age_s=" << stats.minAgeSeconds << "\n"
            << "  median_age_s=" << stats.medianAgeSeconds << "\n"
            << "  max_age_s=" << stats.maxAgeSeconds << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "network-stats") {
        if (!args.empty()) {
            error = "Parametres invalides pour network-stats";
            return CommandOutcome::InvalidArgs;
        }
        const auto stats = daemonChain.getNetworkStats();
        out << "network_stats\n"
            << "  block_count=" << stats.blockCount << "\n"
            << "  user_tx_count=" << stats.userTransactionCount << "\n"
            << "  coinbase_tx_count=" << stats.coinbaseTransactionCount << "\n"
            << "  pending_tx_count=" << stats.pendingTransactionCount << "\n"
            << "  total_transferred=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(stats.totalTransferred) << " NOVA\n"
            << "  total_fees_paid=" << Transaction::toNOVA(stats.totalFeesPaid) << " NOVA\n"
            << "  total_mined_rewards=" << Transaction::toNOVA(stats.totalMinedRewards) << " NOVA\n"
            << "  median_user_tx_amount=" << Transaction::toNOVA(stats.medianUserTransactionAmount) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "difficulty") {
        if (!args.empty()) {
            error = "Parametres invalides pour difficulty";
            return CommandOutcome::InvalidArgs;
        }
        out << "difficulty\n"
            << "  current=" << daemonChain.getCurrentDifficulty() << "\n"
            << "  next=" << daemonChain.estimateNextDifficulty() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "time") {
        if (!args.empty()) {
            error = "Parametres invalides pour time";
            return CommandOutcome::InvalidArgs;
        }
        out << "time\n"
            << "  median_time_past=" << daemonChain.getMedianTimePast() << "\n"
            << "  next_min_timestamp=" << daemonChain.estimateNextMinimumTimestamp() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "supply") {
        if (!args.empty()) {
            error = "Parametres invalides pour supply";
            return CommandOutcome::InvalidArgs;
        }
        out << "supply\n"
            << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
            << "  total=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(daemonChain.getTotalSupply()) << " NOVA\n"
            << "  max=" << Transaction::toNOVA(Blockchain::kMaxSupply) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "monetary") {
        if (args.size() > 1) {
            error = "Parametres invalides pour monetary";
            return CommandOutcome::InvalidArgs;
        }
        const std::size_t height = args.size() == 1
            ? parseSize(args[0], "height")
            : (daemonChain.getBlockCount() > 0 ? daemonChain.getBlockCount() - 1 : 0);
        const auto projection = daemonChain.getMonetaryProjection(height);
        out << "monetary\n"
            << "  height=" << projection.height << "\n"
            << "  subsidy_current=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(projection.currentSubsidy) << " NOVA\n"
            << "  projected_supply=" << Transaction::toNOVA(projection.projectedSupply) << " NOVA\n"
            << "  issuance_remaining=" << Transaction::toNOVA(projection.remainingIssuable) << " NOVA\n"
            << "  next_halving_height=" << projection.nextHalvingHeight << "\n"
            << "  next_subsidy=" << Transaction::toNOVA(projection.nextSubsidy) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "supply-audit") {
        if (args.size() != 2) {
            error = "Parametres invalides pour supply-audit";
            return CommandOutcome::InvalidArgs;
        }
        const std::size_t startHeight = parseSize(args[0], "start_height");
        const std::size_t maxCount = parseSize(args[1], "max_count");
        const auto audit = daemonChain.getSupplyAudit(startHeight, maxCount);
        out << "supply_audit=" << audit.size() << "\n";
        for (const auto& entry : audit) {
            out << "  h=" << entry.height
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
        return CommandOutcome::Ok;
    }

    if (command == "consensus") {
        if (!args.empty()) {
            error = "Parametres invalides pour consensus";
            return CommandOutcome::InvalidArgs;
        }
        out << "consensus\n"
            << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
            << "  current_difficulty=" << daemonChain.getCurrentDifficulty() << "\n"
            << "  next_difficulty=" << daemonChain.estimateNextDifficulty() << "\n"
            << "  cumulative_work=" << daemonChain.getCumulativeWork() << "\n"
            << "  median_time_past=" << daemonChain.getMedianTimePast() << "\n"
            << "  next_min_timestamp=" << daemonChain.estimateNextMinimumTimestamp() << "\n"
            << "  next_reward=" << std::fixed << std::setprecision(8)
            << Transaction::toNOVA(daemonChain.estimateNextMiningReward()) << " NOVA\n";
        return CommandOutcome::Ok;
    }

    if (command == "work") {
        if (!args.empty()) {
            error = "Parametres invalides pour work";
            return CommandOutcome::InvalidArgs;
        }
        out << "work\n"
            << "  cumulative_work=" << daemonChain.getCumulativeWork() << "\n"
            << "  height=" << daemonChain.getBlockCount() - 1 << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "reorgs") {
        if (!args.empty()) {
            error = "Parametres invalides pour reorgs";
            return CommandOutcome::InvalidArgs;
        }
        out << "reorgs\n"
            << "  reorg_count=" << daemonChain.getReorgCount() << "\n"
            << "  last_reorg_depth=" << daemonChain.getLastReorgDepth() << "\n"
            << "  last_fork_height=" << daemonChain.getLastForkHeight() << "\n"
            << "  last_fork_hash=" << daemonChain.getLastForkHash() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "chain-health") {
        if (!args.empty()) {
            error = "Parametres invalides pour chain-health";
            return CommandOutcome::InvalidArgs;
        }
        out << "chain_health\n"
            << "  height=" << daemonChain.getBlockCount() - 1 << "\n"
            << "  chain_valid=" << std::boolalpha << daemonChain.isValid() << "\n"
            << "  cumulative_work=" << daemonChain.getCumulativeWork() << "\n"
            << "  reorg_count=" << daemonChain.getReorgCount() << "\n"
            << "  last_reorg_depth=" << daemonChain.getLastReorgDepth() << "\n"
            << "  last_fork_height=" << daemonChain.getLastForkHeight() << "\n"
            << "  last_fork_hash=" << daemonChain.getLastForkHash() << "\n"
            << "  pending_tx=" << daemonChain.getPendingTransactions().size() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "height") {
        if (!args.empty()) {
            error = "Parametres invalides pour height";
            return CommandOutcome::InvalidArgs;
        }
        const auto& chainData = daemonChain.getChain();
        if (chainData.empty()) {
            out << "height=0\n";
            return CommandOutcome::Ok;
        }
        const auto& tip = chainData.back();
        out << "height=" << tip.getIndex() << "\n"
            << "hash=" << tip.getHash() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "tip") {
        if (!args.empty()) {
            error = "Parametres invalides pour tip";
            return CommandOutcome::InvalidArgs;
        }
        const auto& chainData = daemonChain.getChain();
        if (chainData.empty()) {
            out << "tip=none\n";
            return CommandOutcome::Ok;
        }
        const auto& tip = chainData.back();
        out << "tip\n"
            << "  height=" << tip.getIndex() << "\n"
            << "  hash=" << tip.getHash() << "\n"
            << "  prev_hash=" << tip.getPreviousHash() << "\n";
        return CommandOutcome::Ok;
    }

    if (command == "params") {
        if (!args.empty()) {
            error = "Parametres invalides pour params";
            return CommandOutcome::InvalidArgs;
        }
        out << "params\n"
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
        return CommandOutcome::Ok;
    }

    if (command == "version") {
        if (!args.empty()) {
            error = "Parametres invalides pour version";
            return CommandOutcome::InvalidArgs;
        }
        out << "novacoind version=0.1.0\n"
            << "  network=regtest\n";
        return CommandOutcome::Ok;
    }

    return CommandOutcome::Unknown;
}

rpc::RpcResponse handleRpcCommand(const std::string& command,
                                  const rpc::RpcRequest& request,
                                  Blockchain& daemonChain) {
    std::ostringstream responseStream;
    std::string error;
    const auto outcome = runCommand(command, request.params, daemonChain, responseStream, error);
    switch (outcome) {
    case CommandOutcome::Ok:
        return rpc::RpcResponse::success(request.id, responseStream.str());
    case CommandOutcome::InvalidArgs:
        if (error.empty()) {
            error = "Invalid RPC parameters";
        }
        return rpc::RpcResponse::failure(request.id, rpc::RpcErrorCode::InvalidRequest, error);
    case CommandOutcome::Unknown:
        return rpc::RpcResponse::failure(request.id, rpc::RpcErrorCode::MethodNotFound, "RPC method not found");
    }
    return rpc::RpcResponse::failure(request.id, rpc::RpcErrorCode::InternalError, "Unhandled RPC error");
}

void registerNodeHandlers(rpc::RpcDispatcher& dispatcher, Blockchain& daemonChain) {
    const std::vector<std::pair<std::string, std::string>> methods = {
        {"node.status", "status"},
        {"node.mine", "mine"},
        {"node.submit", "submit"},
        {"node.mempool", "mempool"},
        {"node.mempoolStats", "mempool-stats"},
        {"node.mempoolIds", "mempool-ids"},
        {"node.mempoolSummary", "mempool-summary"},
        {"node.mempoolAge", "mempool-age"},
        {"node.networkStats", "network-stats"},
        {"node.difficulty", "difficulty"},
        {"node.time", "time"},
        {"node.supply", "supply"},
        {"node.monetary", "monetary"},
        {"node.supplyAudit", "supply-audit"},
        {"node.consensus", "consensus"},
        {"node.work", "work"},
        {"node.reorgs", "reorgs"},
        {"node.chainHealth", "chain-health"},
        {"node.height", "height"},
        {"node.tip", "tip"},
        {"node.params", "params"},
        {"node.version", "version"}};

    for (const auto& entry : methods) {
        dispatcher.registerHandler(entry.first, [&daemonChain, command = entry.second](
                                                    const rpc::RpcRequest& request, const rpc::RpcContext&) {
            return handleRpcCommand(command, request, daemonChain);
        });
    }
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        Blockchain daemonChain{2, Transaction::fromNOVA(50.0), 20};
        const std::string command = argc > 1 ? argv[1] : "daemon";
        if (command == "daemon") {
            std::vector<std::string> args;
            args.reserve(argc > 2 ? static_cast<std::size_t>(argc - 2) : 0U);
            for (int i = 2; i < argc; ++i) {
                args.emplace_back(argv[i]);
            }
            return runDaemon(args, daemonChain);
        }
        if (command == "rpc") {
            if (argc < 3) {
                printUsage();
                return 1;
            }
            rpc::RpcDispatcher dispatcher;
            registerNodeHandlers(dispatcher, daemonChain);
            rpc::RpcServer server(rpc::buildDefaultContext(), dispatcher);
            rpc::RpcRequest request;
            request.id = 1;
            request.method = argv[2];
            for (int i = 3; i < argc; ++i) {
                request.params.emplace_back(argv[i]);
            }
            const auto response = server.handle(request);
            if (response.hasError()) {
                std::cout << "rpc_error code=" << rpc::toString(response.error->code)
                          << " message=" << response.error->message << "\n";
                return 1;
            }
            std::cout << response.result;
            if (!response.result.empty() && response.result.back() != '\n') {
                std::cout << "\n";
            }
            return 0;
        }

        std::vector<std::string> args;
        args.reserve(argc > 2 ? static_cast<std::size_t>(argc - 2) : 0U);
        for (int i = 2; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        std::string error;
        const auto outcome = runCommand(command, args, daemonChain, std::cout, error);
        if (outcome == CommandOutcome::Ok) {
            return 0;
        }
        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur novacoind: " << ex.what() << "\n";
        return 1;
    }
}
