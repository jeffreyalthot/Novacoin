#include "blockchain.hpp"
#include "transaction.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
} // namespace

int main() {
    try {
        Blockchain regtest{1, Transaction::fromNOVA(100.0), 6};

        regtest.minePendingTransactions("minerA");
        regtest.createTransaction(
            Transaction{"minerA", "alice", Transaction::fromNOVA(12.0), nowSeconds(), Transaction::fromNOVA(0.2)});
        regtest.createTransaction(
            Transaction{"minerA", "bob", Transaction::fromNOVA(8.0), nowSeconds(), Transaction::fromNOVA(0.2)});
        regtest.minePendingTransactions("minerA");

        regtest.createTransaction(
            Transaction{"alice", "carol", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.1)});
        regtest.minePendingTransactions("minerB");

        std::cout << "regtest summary\n"
                  << "  blocks: " << regtest.getBlockCount() << "\n"
                  << "  supply: " << std::fixed << std::setprecision(8)
                  << Transaction::toNOVA(regtest.getTotalSupply()) << " NOVA\n"
                  << "  minerA: " << Transaction::toNOVA(regtest.getBalance("minerA")) << " NOVA\n"
                  << "  minerB: " << Transaction::toNOVA(regtest.getBalance("minerB")) << " NOVA\n"
                  << "  alice: " << Transaction::toNOVA(regtest.getBalance("alice")) << " NOVA\n"
                  << "  bob: " << Transaction::toNOVA(regtest.getBalance("bob")) << " NOVA\n"
                  << "  carol: " << Transaction::toNOVA(regtest.getBalance("carol")) << " NOVA\n"
                  << "  valid: " << std::boolalpha << regtest.isValid() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur regtest: " << ex.what() << "\n";
        return 1;
    }
}
