#pragma once

#include "blockchain.hpp"

#include <string>
#include <vector>

struct ChainSnapshot {
    std::size_t height = 0;
    std::uint64_t cumulativeWork = 0;
    std::size_t pendingTransactionCount = 0;
    std::size_t reorgCount = 0;
    std::string tipHash;
};

class ChainSnapshotBuilder {
public:
    static ChainSnapshot fromBlockchain(const Blockchain& chain);
    static std::string toPrettyString(const ChainSnapshot& snapshot);
    static std::vector<std::string> toLines(const ChainSnapshot& snapshot);
};
