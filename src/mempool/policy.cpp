#include "mempool/policy.hpp"

#include "consensus.hpp"

namespace mempool {
bool accepts(const Transaction& tx, const Policy& policy) {
    if (tx.from.empty() || tx.to.empty()) {
        return false;
    }

    if (tx.from == "network") {
        return false;
    }

    if (tx.amount <= 0 || tx.fee < 0) {
        return false;
    }

    return tx.fee >= policy.minRelayFee;
}
} // namespace mempool
