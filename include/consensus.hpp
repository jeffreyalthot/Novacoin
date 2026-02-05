#pragma once

#include "transaction.hpp"

#include <cstddef>
#include <cstdint>

namespace consensus {
inline constexpr Amount kMaxSupply = 29'000'000LL * Transaction::kCoin;
inline constexpr std::size_t kHalvingInterval = 100;
inline constexpr Amount kInitialBlockReward = 12'5000'0000;
inline constexpr Amount kMinRelayFee = 10'000;
inline constexpr std::uint64_t kMaxFutureDriftSeconds = 2 * 60 * 60;
inline constexpr std::uint64_t kTargetBlockTimeSeconds = 60;
inline constexpr std::uint64_t kMempoolExpirySeconds = 24 * 60 * 60;
inline constexpr std::size_t kMaxMempoolTransactions = 1'000;
inline constexpr std::size_t kDifficultyAdjustmentInterval = 10;
inline constexpr unsigned int kMinDifficulty = 1;
inline constexpr unsigned int kMaxDifficulty = 2;
} // namespace consensus
