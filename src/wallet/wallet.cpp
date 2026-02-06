#include "wallet/wallet.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wallet {
namespace {
constexpr std::array<std::uint8_t, 4> kMagic{{'N', 'V', 'W', '1'}};
constexpr std::size_t kMasterKeySize = 32;
constexpr std::size_t kSaltSize = 16;
constexpr std::uint8_t kAddressVersion = 0x35;
constexpr std::uint8_t kWifVersion = 0xB2;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<std::uint8_t> randomBytes(std::size_t size) {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<std::uint8_t> out(size);
    for (auto& value : out) {
        value = static_cast<std::uint8_t>(dist(rd));
    }
    return out;
}

std::vector<std::uint8_t> hexToBytes(const std::string& hex) {
    require(hex.size() % 2 == 0, "Hex invalide.");
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        std::uint8_t high = static_cast<std::uint8_t>(std::stoi(hex.substr(i, 1), nullptr, 16));
        std::uint8_t low = static_cast<std::uint8_t>(std::stoi(hex.substr(i + 1, 1), nullptr, 16));
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return out;
}

std::string bytesToHex(const std::vector<std::uint8_t>& bytes) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto value : bytes) {
        out.push_back(kHexDigits[(value >> 4) & 0x0F]);
        out.push_back(kHexDigits[value & 0x0F]);
    }
    return out;
}

std::vector<std::uint8_t> hash256(const std::vector<std::uint8_t>& input) {
    std::hash<std::string> hasher;
    std::string data(reinterpret_cast<const char*>(input.data()), input.size());
    const std::size_t h1 = hasher(data);
    const std::size_t h2 = hasher(data + "a");
    const std::size_t h3 = hasher(data + "b");
    const std::size_t h4 = hasher(data + "c");
    std::vector<std::uint8_t> out(32);
    auto fill = [&](std::size_t value, std::size_t offset) {
        for (std::size_t i = 0; i < 8; ++i) {
            out[offset + i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
        }
    };
    fill(h1, 0);
    fill(h2, 8);
    fill(h3, 16);
    fill(h4, 24);
    return out;
}

std::vector<std::uint8_t> hmacLike(const std::vector<std::uint8_t>& key, const std::string& label) {
    std::vector<std::uint8_t> data = key;
    data.insert(data.end(), label.begin(), label.end());
    return hash256(data);
}

std::vector<std::uint8_t> xorWithKey(const std::vector<std::uint8_t>& data,
                                     const std::vector<std::uint8_t>& key) {
    require(!key.empty(), "Cle d'encryption invalide.");
    std::vector<std::uint8_t> out(data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(data[i] ^ key[i % key.size()]);
    }
    return out;
}

std::vector<std::uint8_t> deriveEncryptionKey(const std::string& passphrase,
                                              const std::vector<std::uint8_t>& salt) {
    std::vector<std::uint8_t> data(passphrase.begin(), passphrase.end());
    data.insert(data.end(), salt.begin(), salt.end());
    return hash256(data);
}

std::string base58Encode(const std::vector<std::uint8_t>& input) {
    static constexpr char kAlphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    std::vector<std::uint8_t> digits(1, 0);
    for (auto byte : input) {
        int carry = byte;
        for (auto& digit : digits) {
            int value = digit * 256 + carry;
            digit = static_cast<std::uint8_t>(value % 58);
            carry = value / 58;
        }
        while (carry > 0) {
            digits.push_back(static_cast<std::uint8_t>(carry % 58));
            carry /= 58;
        }
    }
    std::string out;
    for (auto byte : input) {
        if (byte == 0) {
            out.push_back('1');
        } else {
            break;
        }
    }
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        out.push_back(kAlphabet[*it]);
    }
    return out;
}

std::vector<std::uint8_t> base58Decode(const std::string& input) {
    static constexpr char kAlphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    std::array<int, 256> map{};
    map.fill(-1);
    for (int i = 0; kAlphabet[i] != 0; ++i) {
        map[static_cast<unsigned char>(kAlphabet[i])] = i;
    }
    std::vector<std::uint8_t> bytes(1, 0);
    for (char ch : input) {
        int value = map[static_cast<unsigned char>(ch)];
        require(value >= 0, "Base58 invalide.");
        int carry = value;
        for (auto& byte : bytes) {
            int total = byte * 58 + carry;
            byte = static_cast<std::uint8_t>(total & 0xFF);
            carry = total >> 8;
        }
        while (carry > 0) {
            bytes.push_back(static_cast<std::uint8_t>(carry & 0xFF));
            carry >>= 8;
        }
    }
    std::size_t leading = 0;
    while (leading < input.size() && input[leading] == '1') {
        ++leading;
    }
    for (std::size_t i = 0; i < leading; ++i) {
        bytes.push_back(0);
    }
    std::reverse(bytes.begin(), bytes.end());
    return bytes;
}

std::vector<std::uint8_t> withChecksum(const std::vector<std::uint8_t>& payload) {
    auto checksum = hash256(hash256(payload));
    std::vector<std::uint8_t> out = payload;
    out.insert(out.end(), checksum.begin(), checksum.begin() + 4);
    return out;
}

void verifyChecksum(const std::vector<std::uint8_t>& data) {
    require(data.size() >= 4, "Payload trop court.");
    std::vector<std::uint8_t> payload(data.begin(), data.end() - 4);
    std::vector<std::uint8_t> expected = withChecksum(payload);
    require(std::equal(data.end() - 4, data.end(), expected.end() - 4), "Checksum invalide.");
}

std::string encodeBase58Check(const std::vector<std::uint8_t>& payload) {
    return base58Encode(withChecksum(payload));
}

std::vector<std::uint8_t> decodeBase58Check(const std::string& encoded) {
    auto data = base58Decode(encoded);
    verifyChecksum(data);
    data.resize(data.size() - 4);
    return data;
}

std::vector<std::uint8_t> scalarToPublic(const std::vector<std::uint8_t>& privateKey) {
    std::vector<std::uint8_t> hashed = hash256(privateKey);
    std::vector<std::uint8_t> publicKey(33);
    publicKey[0] = 0x02;
    std::copy_n(hashed.begin(), 32, publicKey.begin() + 1);
    return publicKey;
}

void writeBytes(std::ofstream& out, const std::vector<std::uint8_t>& data) {
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::vector<std::uint8_t> readBytes(std::ifstream& in, std::size_t size) {
    std::vector<std::uint8_t> data(size);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    require(in.gcount() == static_cast<std::streamsize>(size), "Fichier wallet incomplet.");
    return data;
}

} // namespace

WalletStore::WalletStore(std::vector<std::uint8_t> masterKey,
                         bool encrypted,
                         std::vector<std::uint8_t> salt,
                         KeyMode keyMode,
                         std::uint32_t lastIndex)
    : masterKey_(std::move(masterKey)),
      encrypted_(encrypted),
      salt_(std::move(salt)),
      keyMode_(keyMode),
      lastIndex_(lastIndex) {}

WalletStore WalletStore::createNew(bool encryptMasterKey, const std::string& passphrase) {
    auto masterKey = randomBytes(kMasterKeySize);
    std::vector<std::uint8_t> salt;
    bool encrypted = false;
    if (encryptMasterKey) {
        require(!passphrase.empty(), "Passphrase requise pour l'encryption.");
        salt = randomBytes(kSaltSize);
        auto key = deriveEncryptionKey(passphrase, salt);
        masterKey = xorWithKey(masterKey, key);
        encrypted = true;
    }
    return WalletStore{std::move(masterKey), encrypted, std::move(salt), KeyMode::Seed, 0};
}

WalletStore WalletStore::restoreFromWif(const std::string& wif,
                                        bool encryptMasterKey,
                                        const std::string& passphrase) {
    auto payload = decodeBase58Check(wif);
    require(!payload.empty(), "WIF invalide.");
    require(payload[0] == kWifVersion, "Version WIF invalide.");
    require(payload.size() == 33, "Longueur WIF invalide.");
    std::vector<std::uint8_t> privateKey(payload.begin() + 1, payload.end());
    std::vector<std::uint8_t> salt;
    bool encrypted = false;
    if (encryptMasterKey) {
        require(!passphrase.empty(), "Passphrase requise pour l'encryption.");
        salt = randomBytes(kSaltSize);
        auto key = deriveEncryptionKey(passphrase, salt);
        privateKey = xorWithKey(privateKey, key);
        encrypted = true;
    }
    return WalletStore{std::move(privateKey), encrypted, std::move(salt), KeyMode::Single, 0};
}

WalletStore WalletStore::load(const std::string& path, const std::string& passphrase) {
    std::ifstream in(path, std::ios::binary);
    require(in.is_open(), "Impossible d'ouvrir le wallet.dat.");
    auto magic = readBytes(in, kMagic.size());
    require(std::equal(magic.begin(), magic.end(), kMagic.begin()), "wallet.dat invalide.");
    const auto version = readBytes(in, 1);
    require(version[0] == 1, "Version wallet inconnue.");
    const auto flags = readBytes(in, 1);
    bool encrypted = (flags[0] & 0x1) != 0;
    bool singleKey = (flags[0] & 0x2) != 0;
    std::uint32_t lastIndex = 0;
    auto indexBytes = readBytes(in, 4);
    for (std::size_t i = 0; i < 4; ++i) {
        lastIndex |= static_cast<std::uint32_t>(indexBytes[i]) << (8 * i);
    }
    auto salt = readBytes(in, kSaltSize);
    auto masterKey = readBytes(in, kMasterKeySize);
    WalletStore wallet{std::move(masterKey), encrypted, std::move(salt),
                       singleKey ? KeyMode::Single : KeyMode::Seed, lastIndex};
    if (encrypted) {
        require(!passphrase.empty(), "Passphrase requise pour le wallet chiffré.");
        wallet.getMasterKeyBytes(passphrase);
    }
    return wallet;
}

void WalletStore::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(out.is_open(), "Impossible d'ecrire wallet.dat.");
    writeBytes(out, std::vector<std::uint8_t>(kMagic.begin(), kMagic.end()));
    std::uint8_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), 1);
    std::uint8_t flags = 0;
    if (encrypted_) {
        flags |= 0x1;
    }
    if (keyMode_ == KeyMode::Single) {
        flags |= 0x2;
    }
    out.write(reinterpret_cast<const char*>(&flags), 1);
    std::array<std::uint8_t, 4> indexBytes{};
    for (std::size_t i = 0; i < 4; ++i) {
        indexBytes[i] = static_cast<std::uint8_t>((lastIndex_ >> (8 * i)) & 0xFF);
    }
    writeBytes(out, std::vector<std::uint8_t>(indexBytes.begin(), indexBytes.end()));
    std::vector<std::uint8_t> salt = salt_;
    if (salt.size() != kSaltSize) {
        salt.assign(kSaltSize, 0);
    }
    writeBytes(out, salt);
    std::vector<std::uint8_t> master = masterKey_;
    master.resize(kMasterKeySize, 0);
    writeBytes(out, master);
}

std::vector<std::uint8_t> WalletStore::getMasterKeyBytes(const std::string& passphrase) const {
    if (!encrypted_) {
        return masterKey_;
    }
    require(!passphrase.empty(), "Passphrase requise pour decrypter.");
    auto key = deriveEncryptionKey(passphrase, salt_);
    return xorWithKey(masterKey_, key);
}

std::string WalletStore::decryptMasterKeyHex(const std::string& passphrase) const {
    auto key = getMasterKeyBytes(passphrase);
    return bytesToHex(key);
}

std::string WalletStore::derivePrivateKeyHex(std::uint32_t index, const std::string& passphrase) const {
    std::vector<std::uint8_t> master = getMasterKeyBytes(passphrase);
    std::vector<std::uint8_t> derived = master;
    if (keyMode_ == KeyMode::Seed) {
        std::string label = "derive:" + std::to_string(index);
        derived = hmacLike(master, label);
    }
    derived.resize(kMasterKeySize, 0);
    return bytesToHex(derived);
}

std::string WalletStore::privateKeyHexToWif(const std::string& privateKeyHex) const {
    auto keyBytes = hexToBytes(privateKeyHex);
    require(keyBytes.size() == kMasterKeySize, "Longueur de cle privee invalide.");
    std::vector<std::uint8_t> payload;
    payload.reserve(1 + keyBytes.size());
    payload.push_back(kWifVersion);
    payload.insert(payload.end(), keyBytes.begin(), keyBytes.end());
    return encodeBase58Check(payload);
}

std::string WalletStore::privateKeyHexFromWif(const std::string& wif) const {
    auto payload = decodeBase58Check(wif);
    require(payload.size() == 33, "WIF invalide.");
    require(payload[0] == kWifVersion, "Version WIF invalide.");
    std::vector<std::uint8_t> key(payload.begin() + 1, payload.end());
    return bytesToHex(key);
}

std::string WalletStore::privateKeyHexToPublicKey(const std::string& privateKeyHex) const {
    auto keyBytes = hexToBytes(privateKeyHex);
    require(keyBytes.size() == kMasterKeySize, "Cle privee invalide.");
    auto pubKey = scalarToPublic(keyBytes);
    return bytesToHex(pubKey);
}

std::string WalletStore::publicKeyToPublicKeyScript(const std::string& publicKeyHex) const {
    auto pubKeyBytes = hexToBytes(publicKeyHex);
    require(pubKeyBytes.size() == 33, "Public key invalide.");
    auto hashed = hash256(pubKeyBytes);
    std::vector<std::uint8_t> hash160(hashed.begin(), hashed.begin() + 20);
    std::ostringstream script;
    script << "76a914" << bytesToHex(hash160) << "88ac";
    return script.str();
}

std::string WalletStore::publicKeyToAddress(const std::string& publicKeyHex) const {
    auto pubKeyBytes = hexToBytes(publicKeyHex);
    auto hashed = hash256(pubKeyBytes);
    std::vector<std::uint8_t> hash160(hashed.begin(), hashed.begin() + 20);
    std::vector<std::uint8_t> payload;
    payload.reserve(1 + hash160.size());
    payload.push_back(kAddressVersion);
    payload.insert(payload.end(), hash160.begin(), hash160.end());
    return encodeBase58Check(payload);
}

} // namespace wallet
