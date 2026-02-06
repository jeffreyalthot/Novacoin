#pragma once

#include <string>

#include "wallet/wallet_dat_codec.hpp"

namespace wallet {

WalletDatPayload load_wallet_dat(const std::string& path);

} // namespace wallet
