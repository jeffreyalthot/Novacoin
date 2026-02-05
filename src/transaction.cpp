#include "transaction.hpp"

#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
std::string toHex(std::size_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}
} // namespace

Amount Transaction::fromNOVA(double value) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument("Montant NOVA invalide.");
    }

    const long double scaled = static_cast<long double>(value) * static_cast<long double>(kCoin);
    if (scaled > static_cast<long double>(std::numeric_limits<Amount>::max())) {
        throw std::overflow_error("Montant NOVA hors limites.");
    }

    return static_cast<Amount>(std::llround(scaled));
}

double Transaction::toNOVA(Amount value) {
    return static_cast<double>(value) / static_cast<double>(kCoin);
}

std::string Transaction::serialize() const {
    std::ostringstream out;
    out << from << '|' << to << '|' << amount << '|' << timestamp << '|' << fee;
    return out.str();
}

std::string Transaction::id() const {
    return toHex(std::hash<std::string>{}(serialize()));
}
