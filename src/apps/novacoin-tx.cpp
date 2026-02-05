#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoin-tx <from> <to> <amount_nova> [fee_nova]\n\n"
              << "Example:\n"
              << "  novacoin-tx alice bob 1.25 0.10\n";
}

double parseDouble(const std::string& raw, const std::string& field) {
    std::size_t consumed = 0;
    const double value = std::stod(raw, &consumed);
    if (consumed != raw.size()) {
        throw std::invalid_argument("Valeur invalide pour " + field + ": " + raw);
    }
    return value;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 4 || argc > 5) {
            printUsage();
            return 1;
        }

        const std::string from = argv[1];
        const std::string to = argv[2];
        const Amount amount = Transaction::fromNOVA(parseDouble(argv[3], "amount_nova"));
        const Amount fee = argc == 5 ? Transaction::fromNOVA(parseDouble(argv[4], "fee_nova")) : 0;

        if (from.empty() || to.empty()) {
            throw std::invalid_argument("Les adresses from/to ne peuvent pas etre vides.");
        }

        Transaction tx{from, to, amount, nowSeconds(), fee};

        std::cout << "Transaction construite\n"
                  << "  from: " << tx.from << "\n"
                  << "  to: " << tx.to << "\n"
                  << "  amount: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.amount) << " NOVA\n"
                  << "  fee: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.fee) << " NOVA\n"
                  << "  timestamp: " << tx.timestamp << "\n"
                  << "  id: " << tx.id() << "\n"
                  << "  serialized: " << tx.serialize() << "\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
