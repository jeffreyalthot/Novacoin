#pragma once

#include <cstdint>
#include <vector>

#include "transaction.hpp"
#include "wallet/wallet.hpp"

namespace wallet {

struct WalletDatPayload {
    std::vector<std::uint8_t> masterKey;
    bool encrypted{false};
    std::vector<std::uint8_t> salt;
    KeyMode keyMode{KeyMode::Seed};
    std::uint32_t lastIndex{0};
    std::vector<Transaction> incomingTransactions;
};

std::vector<std::uint8_t> encode_wallet(const WalletDatPayload& payload);
WalletDatPayload decode_wallet(const std::vector<std::uint8_t>& data);

} // namespace wallet
