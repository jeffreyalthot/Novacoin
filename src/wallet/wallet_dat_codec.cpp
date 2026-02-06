#include "wallet/wallet_dat_codec.hpp"

#include <array>
#include <limits>
#include <stdexcept>

namespace wallet {
namespace {
constexpr std::array<std::uint8_t, 4> kMagic{{'N', 'V', 'W', '1'}};
constexpr std::uint8_t kVersion = 3;
constexpr std::size_t kMasterKeySize = 32;
constexpr std::size_t kSaltSize = 16;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void appendBytes(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& data) {
    out.insert(out.end(), data.begin(), data.end());
}

void appendUint32LE(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

void appendUint64LE(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

void appendString(std::vector<std::uint8_t>& out, const std::string& value) {
    require(value.size() <= std::numeric_limits<std::uint32_t>::max(),
            "Champ string trop long pour wallet.dat.");
    appendUint32LE(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::uint8_t readByte(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    require(offset + 1 <= data.size(), "wallet.dat tronque.");
    return data[offset++];
}

std::vector<std::uint8_t> readBytes(const std::vector<std::uint8_t>& data,
                                    std::size_t& offset,
                                    std::size_t size) {
    require(offset + size <= data.size(), "wallet.dat tronque.");
    std::vector<std::uint8_t> out(data.begin() + static_cast<std::ptrdiff_t>(offset),
                                  data.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return out;
}

std::uint32_t readUint32LE(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    auto bytes = readBytes(data, offset, 4);
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes[i]) << (8 * i);
    }
    return value;
}

std::uint64_t readUint64LE(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    auto bytes = readBytes(data, offset, 8);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
    }
    return value;
}

std::string readString(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    auto size = readUint32LE(data, offset);
    require(offset + size <= data.size(), "wallet.dat tronque.");
    std::string value(data.begin() + static_cast<std::ptrdiff_t>(offset),
                      data.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return value;
}

void appendTransaction(std::vector<std::uint8_t>& out, const Transaction& tx) {
    appendString(out, tx.from);
    appendString(out, tx.to);
    appendUint64LE(out, static_cast<std::uint64_t>(tx.amount));
    appendUint64LE(out, tx.timestamp);
    appendUint64LE(out, static_cast<std::uint64_t>(tx.fee));
}

Transaction readTransaction(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    Transaction tx;
    tx.from = readString(data, offset);
    tx.to = readString(data, offset);
    tx.amount = static_cast<Amount>(readUint64LE(data, offset));
    tx.timestamp = readUint64LE(data, offset);
    tx.fee = static_cast<Amount>(readUint64LE(data, offset));
    return tx;
}
} // namespace

std::vector<std::uint8_t> encode_wallet(const WalletDatPayload& payload) {
    std::vector<std::uint8_t> out;
    appendBytes(out, std::vector<std::uint8_t>(kMagic.begin(), kMagic.end()));
    out.push_back(kVersion);
    std::uint8_t flags = 0;
    if (payload.encrypted) {
        flags |= 0x1;
    }
    if (payload.keyMode == KeyMode::Single) {
        flags |= 0x2;
    }
    out.push_back(flags);
    appendUint32LE(out, payload.lastIndex);
    std::vector<std::uint8_t> salt = payload.salt;
    if (salt.size() != kSaltSize) {
        salt.assign(kSaltSize, 0);
    }
    appendBytes(out, salt);
    std::vector<std::uint8_t> master = payload.masterKey;
    master.resize(kMasterKeySize, 0);
    appendBytes(out, master);
    appendUint32LE(out, static_cast<std::uint32_t>(payload.incomingTransactions.size()));
    for (const auto& tx : payload.incomingTransactions) {
        appendTransaction(out, tx);
    }
    appendUint32LE(out, static_cast<std::uint32_t>(payload.ckey.size()));
    appendBytes(out, payload.ckey);
    appendUint64LE(out, payload.ckeyTimestamp);
    return out;
}

WalletDatPayload decode_wallet(const std::vector<std::uint8_t>& data) {
    std::size_t offset = 0;
    auto magic = readBytes(data, offset, kMagic.size());
    require(std::equal(magic.begin(), magic.end(), kMagic.begin()), "wallet.dat invalide.");
    auto version = readByte(data, offset);
    require(version == 1 || version == 2 || version == 3, "Version wallet inconnue.");
    auto flags = readByte(data, offset);
    bool encrypted = (flags & 0x1) != 0;
    bool singleKey = (flags & 0x2) != 0;
    auto lastIndex = readUint32LE(data, offset);
    auto salt = readBytes(data, offset, kSaltSize);
    auto masterKey = readBytes(data, offset, kMasterKeySize);
    WalletDatPayload payload;
    payload.masterKey = std::move(masterKey);
    payload.encrypted = encrypted;
    payload.salt = std::move(salt);
    payload.keyMode = singleKey ? KeyMode::Single : KeyMode::Seed;
    payload.lastIndex = lastIndex;
    if (version >= 2) {
        auto txCount = readUint32LE(data, offset);
        payload.incomingTransactions.reserve(txCount);
        for (std::uint32_t i = 0; i < txCount; ++i) {
            payload.incomingTransactions.push_back(readTransaction(data, offset));
        }
    }
    if (version >= 3) {
        auto ckeySize = readUint32LE(data, offset);
        payload.ckey = readBytes(data, offset, ckeySize);
        payload.ckeyTimestamp = readUint64LE(data, offset);
    }
    require(offset == data.size(), "wallet.dat contient des donnees en trop.");
    return payload;
}

} // namespace wallet
