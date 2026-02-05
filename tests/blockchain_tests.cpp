#include "blockchain.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

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

void testRejectAlreadyConfirmedTransactionCreation() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    const Transaction tx{"miner", "alice", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.1)};
    chain.createTransaction(tx);
    chain.minePendingTransactions("miner");

    bool threw = false;
    try {
        chain.createTransaction(tx);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction deja confirmee ne doit pas etre reinjectable en mempool.");
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


void testAdoptChainWithMoreCumulativeWork() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    const auto before = chain.getChain();
    std::vector<Block> candidate = before;
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(adopted, "Une chaine valide avec plus de travail cumule doit etre adoptee.");
    assertTrue(chain.getBlockCount() == candidate.size(), "La chaine locale doit etre remplacee apres adoption.");
    assertTrue(chain.isValid(), "La chaine adoptee doit rester valide.");
}

void testRejectChainWithoutMoreWork() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    std::vector<Block> candidate = chain.getChain();
    candidate.pop_back();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(!adopted, "Une chaine avec moins de travail cumule ne doit pas etre adoptee.");
}


void testReorgReinjectsDetachedTransactionsIntoMempool() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const Transaction txA{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.3)};
    const Transaction txB{"miner", "bob", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)};

    chain.createTransaction(txA);
    chain.createTransaction(txB);
    const std::vector<Block> baseChainBeforeTxBlock = chain.getChain();
    chain.minePendingTransactions("miner");

    std::vector<Block> candidate = baseChainBeforeTxBlock;
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(adopted, "La reorg vers une chaine avec plus de travail doit etre acceptee.");
    assertTrue(chain.getPendingTransactions().size() == 2,
               "Les transactions detachees de l'ancienne branche doivent revenir en mempool.");
}

void testReorgMempoolRemovesTransactionsAlreadyOnNewChain() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const Transaction txA{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.3)};
    const Transaction txB{"miner", "bob", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)};

    chain.createTransaction(txA);
    chain.createTransaction(txB);

    std::vector<Block> candidate = chain.getChain();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               txA,
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(adopted, "Une chaine alternative valide doit etre adoptee.");
    assertTrue(chain.getPendingTransactions().size() == 1,
               "Une transaction deja incluse dans la nouvelle chaine doit quitter la mempool.");
    assertTrue(chain.getPendingTransactions().front().id() == txB.id(),
               "Seule la transaction non incluse sur la nouvelle branche doit rester en mempool.");
}

void testReorgMempoolDropsNowUnfundedTransactions() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    const std::vector<Block> forkBase = chain.getChain();
    chain.minePendingTransactions("miner");

    const Transaction spendA{"miner", "alice", Transaction::fromNOVA(10.0), nowSeconds(), Transaction::fromNOVA(0.1)};
    const Transaction spendB{"miner", "bob", Transaction::fromNOVA(10.0), nowSeconds(), Transaction::fromNOVA(0.1)};

    chain.createTransaction(spendA);
    chain.createTransaction(spendB);

    std::vector<Block> candidate = forkBase;
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(adopted, "La reorg vers une chaine plus lourde doit etre acceptee.");
    assertTrue(chain.getPendingTransactions().empty(),
               "Apres reorg, les transactions mempool non financables doivent etre purgees.");
}

void testNoReorgMetricsChangeWhenAdoptionRejected() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    std::vector<Block> weaker = chain.getChain();
    weaker.pop_back();

    const bool adopted = chain.tryAdoptChain(weaker);
    assertTrue(!adopted, "Une chaine rejetee ne doit pas etre adoptee.");
    assertTrue(chain.getReorgCount() == 0, "Le compteur de reorg ne doit pas augmenter en cas d'echec.");
    assertTrue(chain.getLastReorgDepth() == 0, "La profondeur de reorg doit rester a 0 sans adoption.");
}

void testReorgMetricsTrackDepthAndForkHeight() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    std::vector<Block> candidate = chain.getChain();
    candidate.pop_back();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(adopted, "Une chaine plus lourde doit etre adoptee.");
    assertTrue(chain.getReorgCount() == 1, "Le compteur de reorg doit augmenter apres adoption.");
    assertTrue(chain.getLastReorgDepth() == 1, "La profondeur doit correspondre au nombre de blocs detaches.");
    assertTrue(chain.getLastForkHeight() == 1, "La hauteur de fork doit pointer le dernier bloc commun.");
}


void testAddressStatsAndTopBalances() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");

    const Transaction tx1{"miner", "alice", Transaction::fromNOVA(3.0), nowSeconds(), Transaction::fromNOVA(0.2)};
    const Transaction tx2{"miner", "bob", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.1)};
    chain.createTransaction(tx1);
    chain.createTransaction(tx2);

    const auto minerBefore = chain.getAddressStats("miner");
    assertAmountEq(minerBefore.pendingOutgoing,
                   Transaction::fromNOVA(5.3),
                   "Le pending outgoing du mineur doit inclure montants + frais en mempool.");

    chain.minePendingTransactions("miner");

    const auto minerStats = chain.getAddressStats("miner");
    assertTrue(minerStats.minedBlockCount >= 2, "Le mineur doit avoir au moins 2 blocs coinbase.");
    assertAmountEq(minerStats.totalSent,
                   Transaction::fromNOVA(5.0),
                   "Le total envoye par le mineur doit correspondre aux transactions confirmees.");
    assertAmountEq(minerStats.feesPaid,
                   Transaction::fromNOVA(0.3),
                   "Les frais payes du mineur doivent etre agreges.");

    const auto top = chain.getTopBalances(2);
    assertTrue(top.size() == 2, "Le top balances doit respecter la limite demandee.");
    assertTrue(top[0].second >= top[1].second, "Le classement des soldes doit etre decroissant.");
}

void testNetworkStatsExposeChainActivity() {
    Blockchain chain{1, Transaction::fromNOVA(10.0), 4};
    chain.minePendingTransactions("miner");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.5), nowSeconds(), Transaction::fromNOVA(0.2)});
    chain.createTransaction(
        Transaction{"miner", "bob", Transaction::fromNOVA(2.5), nowSeconds(), Transaction::fromNOVA(0.3)});
    chain.minePendingTransactions("miner");

    const auto network = chain.getNetworkStats();
    assertTrue(network.blockCount == chain.getBlockCount(), "Le blockCount expose doit suivre la chaine.");
    assertTrue(network.userTransactionCount >= 2, "Le compteur de transactions utilisateur doit etre incremente.");
    assertTrue(network.coinbaseTransactionCount >= 2, "Le compteur coinbase doit compter les blocs mines.");
    assertAmountEq(network.totalTransferred,
                   Transaction::fromNOVA(4.0),
                   "Le total transfere doit sommer les montants de transactions utilisateur.");
    assertAmountEq(network.totalFeesPaid,
                   Transaction::fromNOVA(0.5),
                   "Le total de frais reseau doit etre la somme des frais confirmes.");
}


void testHeadersFromHeightAndLocatorHelpers() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const auto headers = chain.getHeadersFromHeight(1, 2);
    assertTrue(headers.size() == 2, "La recuperation headers doit respecter start+maxCount.");
    assertTrue(headers[0].index == 1 && headers[1].index == 2,
               "Les headers renvoyes doivent etre ordonnes par hauteur.");
    assertTrue(headers[1].previousHash == headers[0].hash,
               "Chaque header doit pointer vers le hash precedent.");

    const auto locator = chain.getBlockLocatorHashes();
    assertTrue(!locator.empty(), "Le locator ne doit pas etre vide pour une chaine non vide.");
    assertTrue(locator.front() == chain.getChain().back().getHash(),
               "Le locator doit commencer par le tip de chaine.");

    const auto match = chain.findHighestLocatorMatch(locator);
    assertTrue(match.has_value(), "Un locator local doit matcher la chaine locale.");
    assertTrue(match.value() == chain.getBlockCount() - 1,
               "Le plus haut match d'un locator local doit etre le tip.");

    const auto noMatch = chain.findHighestLocatorMatch({"deadbeef", "badcafe"});
    assertTrue(!noMatch.has_value(), "Un locator sans hash connu ne doit pas retourner de match.");
}


void testHeadersForLocatorReturnsNextSegment() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const auto locator = chain.getBlockLocatorHashes();
    const auto headers = chain.getHeadersForLocator(locator, 2);
    assertTrue(headers.empty(), "Avec un locator au tip local, aucun header supplementaire ne doit etre renvoye.");

    const std::string heightOneHash = chain.getChain()[1].getHash();
    const auto fromForkPoint = chain.getHeadersForLocator({"unknown", heightOneHash}, 5);
    assertTrue(fromForkPoint.size() == 2,
               "Le serveur doit renvoyer les headers apres le plus haut hash connu du locator.");
    assertTrue(fromForkPoint[0].index == 2 && fromForkPoint[1].index == 3,
               "La reponse headers-sync doit commencer juste apres le dernier bloc commun.");

    const auto fromGenesis = chain.getHeadersForLocator({}, 2);
    assertTrue(fromGenesis.size() == 2, "Sans locator, la synchronisation doit commencer depuis le genesis.");
    assertTrue(fromGenesis[0].index == 0 && fromGenesis[1].index == 1,
               "Sans locator, les premiers headers doivent etre retournes dans l'ordre.");
}

void testAmountConversionRoundTrip() {
    const Amount sats = Transaction::fromNOVA(12.3456789);
    assertAmountEq(sats, 1'234'567'890, "La conversion NOVA -> satoshis doit etre exacte a 8 decimales.");
    assertTrue(std::fabs(Transaction::toNOVA(sats) - 12.3456789) < 1e-12,
               "La conversion satoshis -> NOVA doit etre stable.");
}

void testDifficultyRetargetIncreasesWhenBlocksTooFast() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};

    for (std::size_t i = 0; i < Blockchain::kDifficultyAdjustmentInterval; ++i) {
        chain.minePendingTransactions("miner");
    }

    assertTrue(chain.getCurrentDifficulty() >= 2,
               "La difficulte courante doit augmenter apres une fenetre de blocs trop rapide.");
    assertTrue(chain.estimateNextDifficulty() == chain.getCurrentDifficulty(),
               "Hors point de retarget, la prochaine difficulte doit rester stable.");
    assertTrue(chain.isValid(), "La chaine doit rester valide apres retarget de difficulte.");
}


void testRejectChainWithCoinbaseNotInLastPosition() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const Transaction spend{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)};

    std::vector<Block> candidate = chain.getChain();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0},
                               spend},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(!adopted, "Une chaine doit rejeter un bloc avec coinbase hors derniere position.");
}

void testRejectChainWithMultipleCoinbaseTransactions() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    std::vector<Block> candidate = chain.getChain();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(12.5), nowSeconds(), 0},
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(12.5), nowSeconds(), 0}},
                           1);
    candidate.back().mine();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(!adopted, "Une chaine doit rejeter un bloc avec plusieurs coinbase.");
}
void testRejectChainContainingDuplicateUserTransactionIds() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const Transaction duplicated{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(),
                                 Transaction::fromNOVA(0.2)};

    std::vector<Block> candidate = chain.getChain();
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               duplicated,
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               duplicated,
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(!adopted, "Une chaine contenant deux fois le meme txid utilisateur doit etre rejetee.");
}

void testRejectChainFromDifferentGenesis() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    std::this_thread::sleep_for(std::chrono::seconds(1));
    Blockchain other{1, Transaction::fromNOVA(25.0), 4};
    other.minePendingTransactions("alt-miner");
    other.minePendingTransactions("alt-miner");

    const bool adopted = chain.tryAdoptChain(other.getChain());
    assertTrue(!adopted, "Une chaine avec genesis different ne doit jamais etre adoptee.");
}

} // namespace

int main() {
    try {
        testHardCapRespectedByMining();
        testRejectNetworkTransactionCreation();
        testRewardIncludesFeesBoundedByCap();
        testRejectDuplicatePendingTransaction();
        testRejectAlreadyConfirmedTransactionCreation();
        testBlockTemplateRespectsCapacity();
        testRejectTooLowFeeTransaction();
        testRejectFutureTimestampTransaction();
        testTemplatePrioritizesHigherFees();
        testAdoptChainWithMoreCumulativeWork();
        testRejectChainWithoutMoreWork();
        testReorgReinjectsDetachedTransactionsIntoMempool();
        testReorgMempoolRemovesTransactionsAlreadyOnNewChain();
        testReorgMempoolDropsNowUnfundedTransactions();
        testNoReorgMetricsChangeWhenAdoptionRejected();
        testReorgMetricsTrackDepthAndForkHeight();
        testAddressStatsAndTopBalances();
        testNetworkStatsExposeChainActivity();
        testHeadersFromHeightAndLocatorHelpers();
        testHeadersForLocatorReturnsNextSegment();
        testAmountConversionRoundTrip();
        testDifficultyRetargetIncreasesWhenBlocksTooFast();
        testRejectChainWithCoinbaseNotInLastPosition();
        testRejectChainWithMultipleCoinbaseTransactions();
        testRejectChainContainingDuplicateUserTransactionIds();
        testRejectChainFromDifferentGenesis();
        std::cout << "Tous les tests Novacoin sont passes.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Echec test: " << ex.what() << '\n';
        return 1;
    }
}
