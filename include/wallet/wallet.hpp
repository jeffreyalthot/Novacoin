#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wallet {

enum class KeyMode : std::uint8_t {
    Seed = 0,
    Single = 1,
};

class WalletStore {
  public:
    static WalletStore createNew(bool encryptMasterKey, const std::string& passphrase = "");
    static WalletStore restoreFromWif(const std::string& wif,
                                      bool encryptMasterKey,
                                      const std::string& passphrase = "");
    static WalletStore load(const std::string& path, const std::string& passphrase = "");

    void save(const std::string& path) const;

    [[nodiscard]] std::string derivePrivateKeyHex(std::uint32_t index,
                                                  const std::string& passphrase = "") const;
    [[nodiscard]] std::string decryptMasterKeyHex(const std::string& passphrase = "") const;

    [[nodiscard]] std::string privateKeyHexToWif(const std::string& privateKeyHex) const;
    [[nodiscard]] std::string privateKeyHexFromWif(const std::string& wif) const;
    [[nodiscard]] std::string privateKeyHexToPublicKey(const std::string& privateKeyHex) const;
    [[nodiscard]] std::string publicKeyToPublicKeyScript(const std::string& publicKeyHex) const;
    [[nodiscard]] std::string publicKeyToAddress(const std::string& publicKeyHex) const;

  private:
    WalletStore(std::vector<std::uint8_t> masterKey,
                bool encrypted,
                std::vector<std::uint8_t> salt,
                KeyMode keyMode,
                std::uint32_t lastIndex);

    [[nodiscard]] std::vector<std::uint8_t> getMasterKeyBytes(const std::string& passphrase) const;

    std::vector<std::uint8_t> masterKey_;
    bool encrypted_ = false;
    std::vector<std::uint8_t> salt_;
    KeyMode keyMode_ = KeyMode::Seed;
    std::uint32_t lastIndex_ = 0;
};

} // namespace wallet
