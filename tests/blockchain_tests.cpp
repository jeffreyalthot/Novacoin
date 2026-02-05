#include "blockchain.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void assertAmountEq(Amount actual, Amount expected, const std::string& message) {
    if (actual != expected) {
        throw std::runtime_error(message + " (attendu=" + std::to_string(expected) + ", obtenu=" +
                                 std::to_string(actual) + ")");
    }
}

void testHardCapRespectedByMining() {
    Blockchain chain{1, Transaction::fromNOVA(1'000'000.0), 2};
    for (int i = 0; i < 1000; ++i) {
        chain.minePendingTransactions("miner");
    }

    assertTrue(chain.getTotalSupply() <= Blockchain::kMaxSupply, "Le hard cap ne doit jamais etre depasse.");
}

void testRejectNetworkTransactionCreation() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    bool threw = false;
    try {
        chain.createTransaction(Transaction{"network", "alice", Transaction::fromNOVA(1.0), nowSeconds(), 0});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assertTrue(threw, "Les transactions network ne doivent pas etre injectables via mempool.");
}

void testRewardIncludesFeesBoundedByCap() {
    Blockchain chain{1, Transaction::fromNOVA(5.0), 3};
    chain.minePendingTransactions("miner");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.25)});

    const Amount estimatedReward = chain.estimateNextMiningReward();
    assertTrue(estimatedReward >= Transaction::fromNOVA(5.25), "La recompense estimee doit inclure les frais.");

    chain.minePendingTransactions("miner");
    assertTrue(chain.isValid(), "La chaine doit rester valide apres minage avec frais.");
}

void testRejectDuplicatePendingTransaction() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    const Transaction tx{"miner", "alice", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.1)};
    chain.createTransaction(tx);

    bool threw = false;
    try {
        chain.createTransaction(tx);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction en double doit etre rejetee dans la mempool.");
}

void testBlockTemplateRespectsCapacity() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    chain.createTransaction(
        Transaction{"miner", "bob", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    chain.createTransaction(
        Transaction{"miner", "charlie", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)});

    const auto templateTxs = chain.getPendingTransactionsForBlockTemplate();
    assertTrue(templateTxs.size() == 2, "Le template de bloc doit respecter maxTransactionsPerBlock-1.");
}

void testRejectTooLowFeeTransaction() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    bool threw = false;
    try {
        chain.createTransaction(Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), 0});
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction avec frais sous le minimum doit etre rejetee.");
}

void testRejectFutureTimestampTransaction() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    const std::uint64_t futureTs = nowSeconds() + Blockchain::kMaxFutureDriftSeconds + 5;
    bool threw = false;
    try {
        chain.createTransaction(
            Transaction{"miner", "alice", Transaction::fromNOVA(1.0), futureTs, Transaction::fromNOVA(0.1)});
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction trop future doit etre rejetee.");
}

void testTemplatePrioritizesHigherFees() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    chain.createTransaction(
        Transaction{"miner", "bob", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.9)});
    chain.createTransaction(
        Transaction{"miner", "charlie", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.5)});

    const auto templateTxs = chain.getPendingTransactionsForBlockTemplate();
    assertTrue(templateTxs.size() == 2, "Le template doit rester limite a la capacite disponible.");
    assertTrue(templateTxs[0].to == "bob", "La transaction au fee le plus eleve doit etre priorisee.");
    assertTrue(templateTxs[1].to == "charlie", "La deuxieme transaction doit etre la suivante en fee.");
}

void testAmountConversionRoundTrip() {
    const Amount sats = Transaction::fromNOVA(12.3456789);
    assertAmountEq(sats, 1'234'567'890, "La conversion NOVA -> satoshis doit etre exacte a 8 decimales.");
    assertTrue(std::fabs(Transaction::toNOVA(sats) - 12.3456789) < 1e-12,
               "La conversion satoshis -> NOVA doit etre stable.");
}

} // namespace

int main() {
    try {
        testHardCapRespectedByMining();
        testRejectNetworkTransactionCreation();
        testRewardIncludesFeesBoundedByCap();
        testRejectDuplicatePendingTransaction();
        testBlockTemplateRespectsCapacity();
        testRejectTooLowFeeTransaction();
        testRejectFutureTimestampTransaction();
        testTemplatePrioritizesHigherFees();
        testAmountConversionRoundTrip();
        std::cout << "Tous les tests Novacoin sont passes.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Echec test: " << ex.what() << '\n';
        return 1;
    }
}
