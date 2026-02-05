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

void assertTrue(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testHardCapRespectedByMining() {
    Blockchain chain{1, 10'000'000.0, 3};

    for (int i = 0; i < 8; ++i) {
        chain.minePendingTransactions("miner");
    }

    assertTrue(chain.getTotalSupply() <= Blockchain::kMaxSupply + 1e-9, "Le hard cap doit etre respecte en minage.");
    assertTrue(chain.isValid(), "La chaine doit rester valide apres minage vers le plafond.");
}

void testRejectNetworkTransactionCreation() {
    Blockchain chain{1, 25.0, 3};
    bool threw = false;

    try {
        chain.createTransaction(Transaction{"network", "attacker", 10.0, nowSeconds(), 0.0});
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction network externe doit etre rejetee.");
}

void testRewardIncludesFeesBoundedByCap() {
    Blockchain chain{1, 50.0, 4};
    chain.minePendingTransactions("miner");

    chain.createTransaction(Transaction{"miner", "bob", 10.0, nowSeconds(), 1.5});
    const double estimated = chain.estimateNextMiningReward();
    assertTrue(std::abs(estimated - 51.5) < 1e-9, "La recompense estimee doit inclure les frais du bloc courant.");

    chain.minePendingTransactions("miner");
    assertTrue(std::abs(chain.getBalance("miner") - 90.0) < 1e-9, "Le solde mineur doit refleter depense, frais et nouvelle coinbase.");
    assertTrue(chain.isValid(), "La chaine doit rester valide avec frais minés.");
}

void testRejectDuplicatePendingTransaction() {
    Blockchain chain{1, 25.0, 3};
    chain.minePendingTransactions("miner");

    const Transaction tx{"miner", "alice", 3.0, nowSeconds(), 0.1};
    chain.createTransaction(tx);

    bool threw = false;
    try {
        chain.createTransaction(tx);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction dupliquee doit etre rejetee de la mempool.");
}

void testBlockTemplateRespectsCapacity() {
    Blockchain chain{1, 25.0, 3};
    chain.minePendingTransactions("miner");

    chain.createTransaction(Transaction{"miner", "alice", 1.0, nowSeconds(), 0.1});
    chain.createTransaction(Transaction{"miner", "bob", 1.0, nowSeconds(), 0.1});
    chain.createTransaction(Transaction{"miner", "charlie", 1.0, nowSeconds(), 0.1});

    const auto templateTxs = chain.getPendingTransactionsForBlockTemplate();
    assertTrue(templateTxs.size() == 2, "Le template de bloc doit respecter maxTransactionsPerBlock-1.");
}


void testRejectTooLowFeeTransaction() {
    Blockchain chain{1, 25.0, 3};
    chain.minePendingTransactions("miner");

    bool threw = false;
    try {
        chain.createTransaction(Transaction{"miner", "alice", 1.0, nowSeconds(), 0.0});
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction avec frais sous le minimum doit etre rejetee.");
}

void testRejectFutureTimestampTransaction() {
    Blockchain chain{1, 25.0, 3};
    chain.minePendingTransactions("miner");

    const std::uint64_t futureTs = nowSeconds() + Blockchain::kMaxFutureDriftSeconds + 5;
    bool threw = false;
    try {
        chain.createTransaction(Transaction{"miner", "alice", 1.0, futureTs, 0.1});
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction trop future doit etre rejetee.");
}

void testTemplatePrioritizesHigherFees() {
    Blockchain chain{1, 25.0, 3};
    chain.minePendingTransactions("miner");

    chain.createTransaction(Transaction{"miner", "alice", 1.0, nowSeconds(), 0.1});
    chain.createTransaction(Transaction{"miner", "bob", 1.0, nowSeconds(), 0.9});
    chain.createTransaction(Transaction{"miner", "charlie", 1.0, nowSeconds(), 0.5});

    const auto templateTxs = chain.getPendingTransactionsForBlockTemplate();
    assertTrue(templateTxs.size() == 2, "Le template doit rester limite a la capacite disponible.");
    assertTrue(templateTxs[0].to == "bob", "La transaction au fee le plus eleve doit etre priorisee.");
    assertTrue(templateTxs[1].to == "charlie", "La deuxieme transaction doit etre la suivante en fee.");
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
        std::cout << "Tous les tests Novacoin sont passes.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Echec test: " << ex.what() << '\n';
        return 1;
    }
}
