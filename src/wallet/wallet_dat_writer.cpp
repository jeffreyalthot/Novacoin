#include "wallet/wallet_dat_writer.hpp"

#include <fstream>
#include <stdexcept>

namespace wallet {
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

void save_wallet_dat(const std::string& path, const WalletDatPayload& payload) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(out.is_open(), "Impossible d'ecrire wallet.dat.");
    auto data = encode_wallet(payload);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    require(out.good(), "Erreur d'ecriture wallet.dat.");
}

} // namespace wallet
