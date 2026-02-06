#include "storage/chain_snapshot.hpp"

#include <sstream>

ChainSnapshot ChainSnapshotBuilder::fromBlockchain(const Blockchain& chain) {
    ChainSnapshot snapshot;
    snapshot.height = chain.getBlockCount();
    snapshot.cumulativeWork = chain.getCumulativeWork();
    snapshot.pendingTransactionCount = chain.getPendingTransactions().size();
    snapshot.reorgCount = chain.getReorgCount();

    const auto blocks = chain.getChain();
    if (!blocks.empty()) {
        snapshot.tipHash = blocks.back().getHash();
    }

    return snapshot;
}

std::string ChainSnapshotBuilder::toPrettyString(const ChainSnapshot& snapshot) {
    std::ostringstream out;
    const auto lines = toLines(snapshot);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 < lines.size()) {
            out << '\n';
        }
    }

    return out.str();
}

std::vector<std::string> ChainSnapshotBuilder::toLines(const ChainSnapshot& snapshot) {
    return {
        "height=" + std::to_string(snapshot.height),
        "cumulative_work=" + std::to_string(snapshot.cumulativeWork),
        "pending_txs=" + std::to_string(snapshot.pendingTransactionCount),
        "reorg_count=" + std::to_string(snapshot.reorgCount),
        "tip_hash=" + (snapshot.tipHash.empty() ? std::string{"<none>"} : snapshot.tipHash),
    };
}
