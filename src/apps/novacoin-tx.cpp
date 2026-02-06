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
              << "  novacoin-tx create <from> <to> <amount_nova> [fee_nova]\n"
              << "  novacoin-tx decode <serialized_tx>\n"
              << "  novacoin-tx id <serialized_tx>\n"
              << "  novacoin-tx size <serialized_tx>\n"
              << "  novacoin-tx summary <serialized_tx>\n"
              << "  novacoin-tx addresses <serialized_tx>\n"
              << "  novacoin-tx from <serialized_tx>\n"
              << "  novacoin-tx to <serialized_tx>\n"
              << "  novacoin-tx amount <serialized_tx>\n"
              << "  novacoin-tx total-nova <serialized_tx>\n"
              << "  novacoin-tx amounts <serialized_tx>\n"
              << "  novacoin-tx fee <serialized_tx>\n"
              << "  novacoin-tx fee-rate <serialized_tx>\n"
              << "  novacoin-tx total <serialized_tx>\n"
              << "  novacoin-tx timestamp <serialized_tx>\n"
              << "  novacoin-tx serialize <from> <to> <amount_nova> [fee_nova]\n"
              << "  novacoin-tx <from> <to> <amount_nova> [fee_nova]\n\n"
              << "Example:\n"
              << "  novacoin-tx create alice bob 1.25 0.10\n";
}

double parseDouble(const std::string& raw, const std::string& field) {
    std::size_t consumed = 0;
    const double value = std::stod(raw, &consumed);
    if (consumed != raw.size()) {
        throw std::invalid_argument("Valeur invalide pour " + field + ": " + raw);
    }
    return value;
}

Transaction buildTransaction(const std::string& from,
                             const std::string& to,
                             const std::string& amountRaw,
                             const std::string& feeRaw) {
    if (from.empty() || to.empty()) {
        throw std::invalid_argument("Les adresses from/to ne peuvent pas etre vides.");
    }

    const Amount amount = Transaction::fromNOVA(parseDouble(amountRaw, "amount_nova"));
    const Amount fee = feeRaw.empty() ? 0 : Transaction::fromNOVA(parseDouble(feeRaw, "fee_nova"));
    return Transaction{from, to, amount, nowSeconds(), fee};
}

void printTransactionDetails(const Transaction& tx) {
    std::cout << "Transaction construite\n"
              << "  from: " << tx.from << "\n"
              << "  to: " << tx.to << "\n"
              << "  amount: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.amount) << " NOVA\n"
              << "  fee: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.fee) << " NOVA\n"
              << "  timestamp: " << tx.timestamp << "\n"
              << "  id: " << tx.id() << "\n"
              << "  serialized: " << tx.serialize() << "\n";
}

void printTransactionSummary(const Transaction& tx) {
    const Amount total = tx.amount + tx.fee;
    std::cout << "Summary\n"
              << "  id: " << tx.id() << "\n"
              << "  from: " << tx.from << "\n"
              << "  to: " << tx.to << "\n"
              << "  amount: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.amount) << " NOVA\n"
              << "  fee: " << Transaction::toNOVA(tx.fee) << " NOVA\n"
              << "  total: " << Transaction::toNOVA(total) << " NOVA\n";
}

void printTransactionAddresses(const Transaction& tx) {
    std::cout << "Addresses\n"
              << "  from: " << tx.from << "\n"
              << "  to: " << tx.to << "\n";
}

void printTransactionFrom(const Transaction& tx) {
    std::cout << tx.from << "\n";
}

void printTransactionTo(const Transaction& tx) {
    std::cout << tx.to << "\n";
}

void printTransactionAmount(const Transaction& tx) {
    std::cout << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.amount) << "\n";
}

void printTransactionTotalNova(const Transaction& tx) {
    const Amount total = tx.amount + tx.fee;
    std::cout << std::fixed << std::setprecision(8) << Transaction::toNOVA(total) << "\n";
}

void printTransactionAmounts(const Transaction& tx) {
    const Amount total = tx.amount + tx.fee;
    std::cout << "Amounts\n"
              << "  amount_atoms=" << tx.amount << "\n"
              << "  fee_atoms=" << tx.fee << "\n"
              << "  total_atoms=" << total << "\n";
}

void printTransactionFee(const Transaction& tx) {
    std::cout << "Fee\n"
              << "  fee_atoms=" << tx.fee << "\n"
              << "  fee_nova=" << std::fixed << std::setprecision(8) << Transaction::toNOVA(tx.fee) << "\n";
}

void printTransactionFeeRate(const Transaction& tx) {
    const std::size_t size = tx.serialize().size();
    const double rate = size == 0 ? 0.0 : static_cast<double>(tx.fee) / static_cast<double>(size);
    std::cout << "Fee rate\n"
              << "  size_bytes=" << size << "\n"
              << "  fee_atoms=" << tx.fee << "\n"
              << "  fee_per_byte_atoms=" << std::fixed << std::setprecision(8) << rate << "\n";
}

void printTransactionTotal(const Transaction& tx) {
    const Amount total = tx.amount + tx.fee;
    std::cout << "Total\n"
              << "  total_atoms=" << total << "\n"
              << "  total_nova=" << std::fixed << std::setprecision(8) << Transaction::toNOVA(total) << "\n";
}

void printTransactionTimestamp(const Transaction& tx) {
    std::cout << "Timestamp\n"
              << "  ts=" << tx.timestamp << "\n";
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        const std::string command = argv[1];
        if (command == "create") {
            if (argc < 5 || argc > 6) {
                printUsage();
                return 1;
            }

            const std::string feeRaw = argc == 6 ? argv[5] : "";
            const Transaction tx = buildTransaction(argv[2], argv[3], argv[4], feeRaw);
            printTransactionDetails(tx);
            return 0;
        }

        if (command == "decode") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionDetails(tx);
            return 0;
        }

        if (command == "id") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            std::cout << tx.id() << "\n";
            return 0;
        }

        if (command == "size") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            std::cout << "serialized_size=" << tx.serialize().size() << "\n";
            return 0;
        }

        if (command == "summary") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionSummary(tx);
            return 0;
        }

        if (command == "addresses") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionAddresses(tx);
            return 0;
        }

        if (command == "from") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionFrom(tx);
            return 0;
        }

        if (command == "to") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionTo(tx);
            return 0;
        }

        if (command == "amount") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionAmount(tx);
            return 0;
        }

        if (command == "total-nova") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionTotalNova(tx);
            return 0;
        }

        if (command == "amounts") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionAmounts(tx);
            return 0;
        }

        if (command == "fee") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionFee(tx);
            return 0;
        }

        if (command == "fee-rate") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionFeeRate(tx);
            return 0;
        }

        if (command == "total") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionTotal(tx);
            return 0;
        }

        if (command == "timestamp") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const Transaction tx = Transaction::deserialize(argv[2]);
            printTransactionTimestamp(tx);
            return 0;
        }

        if (command == "serialize") {
            if (argc < 5 || argc > 6) {
                printUsage();
                return 1;
            }
            const std::string feeRaw = argc == 6 ? argv[5] : "";
            const Transaction tx = buildTransaction(argv[2], argv[3], argv[4], feeRaw);
            std::cout << tx.serialize() << "\n";
            return 0;
        }

        if (argc < 4 || argc > 5) {
            printUsage();
            return 1;
        }

        const std::string feeRaw = argc == 5 ? argv[4] : "";
        const Transaction tx = buildTransaction(argv[1], argv[2], argv[3], feeRaw);
        printTransactionDetails(tx);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
