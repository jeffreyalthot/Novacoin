#pragma once

#include "consensus.hpp"

#include <cstdint>

namespace consensus {
struct ChainParams {
    std::uint64_t targetBlockTimeSeconds = kTargetBlockTimeSeconds;
    std::uint64_t maxFutureDriftSeconds = kMaxFutureDriftSeconds;
    std::size_t halvingInterval = kHalvingInterval;
};

[[nodiscard]] ChainParams defaultChainParams();
} // namespace consensus
