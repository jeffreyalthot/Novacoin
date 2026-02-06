#include "transaction.hpp"

#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

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

Transaction Transaction::deserialize(const std::string& payload) {
    std::istringstream in(payload);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(in, part, '|')) {
        parts.push_back(part);
    }

    if (parts.size() != 5) {
        throw std::invalid_argument("Transaction invalide: format incorrect.");
    }

    Transaction tx;
    tx.from = parts[0];
    tx.to = parts[1];
    try {
        tx.amount = static_cast<Amount>(std::stoll(parts[2]));
        tx.timestamp = static_cast<std::uint64_t>(std::stoull(parts[3]));
        tx.fee = static_cast<Amount>(std::stoll(parts[4]));
    } catch (const std::exception&) {
        throw std::invalid_argument("Transaction invalide: valeurs numeriques incorrectes.");
    }

    if (tx.from.empty() || tx.to.empty()) {
        throw std::invalid_argument("Transaction invalide: adresses manquantes.");
    }
    if (tx.amount < 0 || tx.fee < 0) {
        throw std::invalid_argument("Transaction invalide: montants negatifs.");
    }

    return tx;
}

std::string Transaction::id() const {
    return toHex(std::hash<std::string>{}(serialize()));
}
