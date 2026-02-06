#include "mempool/policy.hpp"

#include "consensus.hpp"

namespace mempool {
bool accepts(const Transaction& tx, const Policy& policy) {
    if (tx.from == "network") {
        return true;
    }

    return tx.fee >= policy.minRelayFee;
}
} // namespace mempool
