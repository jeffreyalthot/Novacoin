#pragma once

#include <string>

namespace wallet {
struct WalletProfile {
    std::string label;
    bool watchOnly = false;
};

[[nodiscard]] WalletProfile defaultProfile();
} // namespace wallet
