#include "wallet/wallet_dat_loader.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace wallet {
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

WalletDatPayload load_wallet_dat(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    require(in.is_open(), "Impossible d'ouvrir le wallet.dat.");
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(size);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    require(in.gcount() == static_cast<std::streamsize>(size), "wallet.dat tronque.");
    return decode_wallet(data);
}

} // namespace wallet
