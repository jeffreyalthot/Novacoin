#pragma once

#include <string>

#include "wallet/wallet_dat_codec.hpp"

namespace wallet {

void save_wallet_dat(const std::string& path, const WalletDatPayload& payload);

} // namespace wallet
