#include "wallet/wallet_profile.hpp"

namespace wallet {
WalletProfile defaultProfile() {
    return {"default", false};
}

WalletProfile supplementarySystemProfile() {
    return {"system-supplementaire", true};
}
} // namespace wallet
