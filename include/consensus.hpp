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
} // namespace consensus
