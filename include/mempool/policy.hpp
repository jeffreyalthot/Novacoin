#pragma once

#include "consensus.hpp"
#include "transaction.hpp"

namespace mempool {
struct Policy {
    Amount minRelayFee = consensus::kMinRelayFee;
    std::size_t maxTransactions = consensus::kMaxMempoolTransactions;
};

[[nodiscard]] bool accepts(const Transaction& tx, const Policy& policy = Policy{});
} // namespace mempool
