#include "blockchain.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>


void runPlatformModulesTests();
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
    assertTrue(chain.getLastForkHash().empty(), "Le hash de fork doit rester vide sans reorg adoptee.");
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
    assertTrue(chain.getLastForkHash() == candidate[1].getHash(),
               "Le hash de fork doit correspondre au dernier bloc commun entre les branches.");
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

void testMempoolStatsExposePendingFeeDistribution() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    chain.createTransaction(
        Transaction{"miner", "bob", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.3)});
    chain.createTransaction(
        Transaction{"miner", "charlie", Transaction::fromNOVA(3.0), nowSeconds(), Transaction::fromNOVA(0.2)});

    const auto mempool = chain.getMempoolStats();
    assertTrue(mempool.transactionCount == 3, "Le nombre de transactions mempool doit etre exact.");
    assertAmountEq(mempool.totalAmount, Transaction::fromNOVA(6.0), "Le total montant mempool doit etre agrege.");
    assertAmountEq(mempool.totalFees, Transaction::fromNOVA(0.6), "Le total de frais mempool doit etre agrege.");
    assertAmountEq(mempool.minFee, Transaction::fromNOVA(0.1), "Le min fee mempool doit etre expose.");
    assertAmountEq(mempool.maxFee, Transaction::fromNOVA(0.3), "Le max fee mempool doit etre expose.");
    assertAmountEq(mempool.medianFee, Transaction::fromNOVA(0.2), "La mediane de fee mempool doit etre correcte.");
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



void testHeadersForLocatorWithStopHashBoundsResponse() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const std::string forkPointHash = chain.getChain()[1].getHash();
    const std::string stopHash = chain.getChain()[2].getHash();

    const auto headers = chain.getHeadersForLocatorWithStop({forkPointHash}, 10, stopHash);
    assertTrue(headers.size() == 1,
               "Le stop hash doit borner la reponse headers-sync au bloc cible inclus.");
    assertTrue(headers.front().index == 2,
               "La synchronisation avec stop doit commencer apres locator et s'arreter au stop hash.");
}

void testBlocksFromHeightAndLocatorHelpers() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const auto blocks = chain.getBlocksFromHeight(1, 2);
    assertTrue(blocks.size() == 2, "La recuperation blocks doit respecter start+maxCount.");
    assertTrue(blocks[0].index == 1 && blocks[1].index == 2,
               "Les blocks renvoyes doivent etre ordonnes par hauteur.");
    assertTrue(blocks[1].previousHash == blocks[0].hash,
               "Chaque block summary doit pointer vers le hash precedent.");

    const auto locator = chain.getBlockLocatorHashes();
    const auto none = chain.getBlocksForLocator(locator, 2);
    assertTrue(none.empty(), "Avec un locator au tip local, aucun block supplementaire ne doit etre renvoye.");

    const std::string forkPointHash = chain.getChain()[1].getHash();
    const auto fromForkPoint = chain.getBlocksForLocator({"unknown", forkPointHash}, 5);
    assertTrue(fromForkPoint.size() == 2,
               "La synchronisation blocks doit renvoyer les blocs apres le plus haut hash connu.");
    assertTrue(fromForkPoint[0].index == 2 && fromForkPoint[1].index == 3,
               "La reponse blocks-sync doit commencer juste apres le dernier bloc commun.");
}

void testBlocksForLocatorWithStopHashBoundsResponse() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const std::string forkPointHash = chain.getChain()[1].getHash();
    const std::string stopHash = chain.getChain()[2].getHash();

    const auto blocks = chain.getBlocksForLocatorWithStop({forkPointHash}, 10, stopHash);
    assertTrue(blocks.size() == 1,
               "Le stop hash doit borner la reponse blocks-sync au bloc cible inclus.");
    assertTrue(blocks.front().index == 2,
               "La synchronisation blocks avec stop doit commencer apres locator et s'arreter au stop hash.");
}

void testSyncStatusProvidesDeterministicWindow() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const std::string locatorHash = chain.getChain()[1].getHash();
    const std::string stopHash = chain.getChain()[2].getHash();

    const auto status = chain.getSyncStatus({"unknown", locatorHash}, 10, stopHash);
    assertTrue(status.localHeight == 3, "Le status de sync doit exposer la hauteur locale courante.");
    assertTrue(status.locatorHeight.has_value() && status.locatorHeight.value() == 1,
               "Le status de sync doit exposer la hauteur du locator resolu.");
    assertTrue(status.nextHeight == 2,
               "La prochaine hauteur a synchroniser doit suivre le locator commun le plus haut.");
    assertTrue(status.remainingBlocks == 2,
               "Le nombre de blocs restants doit compter du point de reprise au tip.");
    assertTrue(status.stopHeight.has_value() && status.stopHeight.value() == 2,
               "Le stop hash doit etre resolu en hauteur lorsqu'il est connu.");
    assertTrue(status.responseBlockCount == 1,
               "Le nombre de blocs a renvoyer doit etre borne par le stop hash inclus.");
    assertTrue(!status.isAtTip,
               "Quand des blocs restent a envoyer, le status ne doit pas signaler un tip atteint.");
    assertTrue(status.isStopHashLimiting,
               "Le status doit indiquer que la fenetre est limitee par le stop hash.");
}

void testSyncStatusHandlesUnknownLocatorAndStop() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const auto status = chain.getSyncStatus({"deadbeef"}, 1, "unknown-stop");
    assertTrue(!status.locatorHeight.has_value(),
               "Un locator inconnu ne doit pas exposer de hauteur de reprise connue.");
    assertTrue(!status.stopHeight.has_value(), "Un stop hash inconnu ne doit pas exposer de hauteur stop.");
    assertTrue(status.nextHeight == 0, "Sans locator resolu, la reprise doit commencer au genesis.");
    assertTrue(status.responseBlockCount == 1,
               "Quand le stop est inconnu, la reponse doit rester bornee par max_count.");
    assertTrue(!status.isAtTip,
               "Avec une fenetre non vide, le noeud ne doit pas etre considere au tip du pair.");
    assertTrue(!status.isStopHashLimiting,
               "Sans stop hash resolu, la fenetre ne doit pas etre marquee comme stop-bornee.");
}


void testSyncStatusAtTipSignalsTerminalWindow() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const auto locator = chain.getBlockLocatorHashes();
    const auto status = chain.getSyncStatus(locator, 10, "");

    assertTrue(status.responseBlockCount == 0,
               "Quand le locator pointe sur le tip local, aucun bloc ne doit etre renvoye.");
    assertTrue(status.isAtTip,
               "Le status doit expliciter que le pair est deja aligne sur le tip local.");
    assertTrue(!status.isStopHashLimiting,
               "Sans stop hash, la fenetre ne doit pas etre marquee comme stop-limitee.");
}

void testHeadersForLocatorWithUnknownStopHashFallsBackToMaxCount() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const std::string knownLocator = chain.getChain()[0].getHash();
    const auto headers = chain.getHeadersForLocatorWithStop({knownLocator}, 2, "unknown-stop");
    assertTrue(headers.size() == 2,
               "Un stop hash inconnu doit conserver le comportement limite par max_count.");
    assertTrue(headers[0].index == 1 && headers[1].index == 2,
               "Sans stop hash resolvable, les headers doivent etre renvoyes sequentiellement.");
}

void testExpiredMempoolTransactionsArePruned() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const std::uint64_t expiredTs = nowSeconds() - Blockchain::kMempoolExpirySeconds - 10;
    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), expiredTs, Transaction::fromNOVA(0.1)});

    assertTrue(chain.getPendingTransactions().size() == 1,
               "La transaction expiree doit etre presente avant la purge.");

    chain.minePendingTransactions("miner");

    assertTrue(chain.getPendingTransactions().empty(),
               "Les transactions expirees doivent etre purgees de la mempool avant minage.");
    assertAmountEq(chain.getBalance("alice"), 0,
                   "Une transaction expiree ne doit pas etre incluse dans un bloc.");
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



void testBlockSummaryLookupByHeightAndHash() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");
    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)});
    chain.minePendingTransactions("miner");

    const auto byHeight = chain.getBlockSummaryByHeight(2);
    assertTrue(byHeight.has_value(), "Le resume de bloc doit etre resolvable par hauteur.");
    assertTrue(byHeight->index == 2, "Le resume de bloc doit exposer la bonne hauteur.");
    assertTrue(byHeight->transactionCount == 2, "Le bloc mine doit contenir tx utilisateur + coinbase.");
    assertTrue(byHeight->userTransactionCount == 1, "Le nombre de transactions utilisateur doit etre exact.");
    assertAmountEq(byHeight->totalFees, Transaction::fromNOVA(0.2), "Les frais du bloc doivent etre agregees.");

    const auto byHash = chain.getBlockSummaryByHash(byHeight->hash);
    assertTrue(byHash.has_value(), "Le resume de bloc doit etre resolvable par hash.");
    assertTrue(byHash->hash == byHeight->hash && byHash->index == byHeight->index,
               "Les resolutions par hash et hauteur doivent pointer le meme bloc.");

    assertTrue(!chain.getBlockSummaryByHeight(42).has_value(), "Une hauteur inconnue ne doit pas retourner de bloc.");
    assertTrue(!chain.getBlockSummaryByHash("unknown-hash").has_value(), "Un hash inconnu ne doit pas matcher.");
}

void testRecentBlockSummariesAreOrderedFromTip() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const auto summaries = chain.getRecentBlockSummaries(3);
    assertTrue(summaries.size() == 3, "Le nombre de resumes doit respecter la limite demandee.");
    assertTrue(summaries[0].index == chain.getBlockCount() - 1,
               "Le premier resume doit correspondre au tip de chaine.");
    assertTrue(summaries[1].index + 1 == summaries[0].index && summaries[2].index + 1 == summaries[1].index,
               "Les resumes doivent etre ordonnes du plus recent au plus ancien.");

    const auto allSummaries = chain.getRecentBlockSummaries(99);
    assertTrue(allSummaries.size() == chain.getBlockCount(),
               "Demander plus que la chaine doit retourner toute la chaine.");
    assertTrue(chain.getRecentBlockSummaries(0).empty(), "Une limite nulle doit retourner une liste vide.");
}


void testFindTransactionByIdForConfirmedTransaction() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");

    const Transaction tx{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)};
    chain.createTransaction(tx);
    chain.minePendingTransactions("miner");

    const auto lookup = chain.findTransactionById(tx.id());
    assertTrue(lookup.has_value(), "La recherche txid doit retrouver une transaction confirmee.");
    assertTrue(lookup->isConfirmed, "La transaction minee doit etre marquee confirmee.");
    assertTrue(lookup->blockHeight.has_value() && lookup->blockHeight.value() == 2,
               "La hauteur du bloc contenant la transaction doit etre exposee.");
    assertTrue(lookup->confirmations == 1, "Une tx dans le tip doit avoir une confirmation.");
    assertTrue(lookup->tx.id() == tx.id(), "Le contenu de la transaction retournee doit matcher le txid.");
}

void testFindTransactionByIdForPendingAndUnknownTransaction() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");

    const Transaction tx{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)};
    chain.createTransaction(tx);

    const auto pendingLookup = chain.findTransactionById(tx.id());
    assertTrue(pendingLookup.has_value(), "La recherche txid doit retrouver une transaction mempool.");
    assertTrue(!pendingLookup->isConfirmed, "La transaction en mempool ne doit pas etre marquee confirmee.");
    assertTrue(!pendingLookup->blockHeight.has_value(), "La tx mempool ne doit pas avoir de hauteur de bloc.");
    assertTrue(pendingLookup->confirmations == 0, "La tx mempool doit avoir 0 confirmation.");

    assertTrue(!chain.findTransactionById("unknown-txid").has_value(),
               "Un txid inconnu ne doit retourner aucun resultat.");
}



void testTransactionHistoryDetailedIncludesConfirmedAndPending() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 5};
    chain.minePendingTransactions("miner");

    const Transaction confirmed{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.2)};
    chain.createTransaction(confirmed);
    chain.minePendingTransactions("miner");

    const Transaction pending{"alice", "bob", Transaction::fromNOVA(0.4), nowSeconds(), Transaction::fromNOVA(0.1)};
    chain.createTransaction(pending);

    const auto history = chain.getTransactionHistoryDetailed("alice");
    assertTrue(history.size() == 2,
               "L'historique detaille doit inclure tx confirmees et mempool pour une adresse.");
    const auto confirmedIt = std::find_if(history.begin(), history.end(), [&](const TransactionHistoryEntry& entry) {
        return entry.tx.id() == confirmed.id();
    });
    const auto pendingIt = std::find_if(history.begin(), history.end(), [&](const TransactionHistoryEntry& entry) {
        return entry.tx.id() == pending.id();
    });

    assertTrue(confirmedIt != history.end() && confirmedIt->isConfirmed,
               "La transaction confirmee doit exposer son statut confirme.");
    assertTrue(confirmedIt->blockHeight.has_value() && confirmedIt->confirmations >= 1,
               "Une tx confirmee doit exposer hauteur et confirmations.");
    assertTrue(pendingIt != history.end() && !pendingIt->isConfirmed,
               "La transaction mempool doit apparaitre avec status non confirme.");
}

void testTransactionHistoryDetailedSupportsLimitAndConfirmedOnly() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 6};
    chain.minePendingTransactions("miner");

    const Transaction tx1{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)};
    const Transaction tx2{"miner", "alice", Transaction::fromNOVA(2.0), nowSeconds(), Transaction::fromNOVA(0.1)};
    chain.createTransaction(tx1);
    chain.createTransaction(tx2);
    chain.minePendingTransactions("miner");

    const Transaction pending{"alice", "carol", Transaction::fromNOVA(0.5), nowSeconds(), Transaction::fromNOVA(0.1)};
    chain.createTransaction(pending);

    const auto limited = chain.getTransactionHistoryDetailed("alice", 1, true);
    assertTrue(limited.size() == 1,
               "Le parametre limit doit borner strictement le nombre d'entrees detaillees.");

    const auto confirmedOnly = chain.getTransactionHistoryDetailed("alice", 0, false);
    assertTrue(std::all_of(confirmedOnly.begin(), confirmedOnly.end(), [](const TransactionHistoryEntry& e) {
                   return e.isConfirmed;
               }),
               "Le mode confirmed-only ne doit renvoyer aucune transaction mempool.");
}

void testMempoolCapacityEvictsLowestFeeTransaction() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 2};
    chain.minePendingTransactions("miner");

    const std::uint64_t ts = nowSeconds();
    for (std::size_t i = 0; i < Blockchain::kMaxMempoolTransactions; ++i) {
        chain.createTransaction(Transaction{"miner",
                                            "user" + std::to_string(i),
                                            Transaction::fromNOVA(0.01),
                                            ts + i,
                                            Transaction::fromNOVA(0.0001)});
    }

    chain.createTransaction(Transaction{"miner", "vip", Transaction::fromNOVA(0.01), ts + 2000, Transaction::fromNOVA(0.001)});

    assertTrue(chain.getPendingTransactions().size() == Blockchain::kMaxMempoolTransactions,
               "La mempool doit rester bornee apres eviction.");

    const bool lowFeeStillPresent = std::any_of(chain.getPendingTransactions().begin(),
                                                chain.getPendingTransactions().end(),
                                                [](const Transaction& tx) { return tx.to == "user0"; });
    assertTrue(!lowFeeStillPresent, "La transaction la moins prioritaire doit etre evincee.");
}

void testMempoolCapacityRejectsTooLowFeeWhenFull() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 2};
    chain.minePendingTransactions("miner");

    const std::uint64_t ts = nowSeconds();
    for (std::size_t i = 0; i < Blockchain::kMaxMempoolTransactions; ++i) {
        chain.createTransaction(Transaction{"miner",
                                            "addr" + std::to_string(i),
                                            Transaction::fromNOVA(0.01),
                                            ts + i,
                                            Transaction::fromNOVA(0.0002)});
    }

    bool threw = false;
    try {
        chain.createTransaction(Transaction{"miner", "late", Transaction::fromNOVA(0.01), ts + 3000, Transaction::fromNOVA(0.0001)});
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une transaction moins fee que le minimum mempool doit etre rejetee quand la mempool est pleine.");
}


void testFeeEstimateReturnsRelayFeeWhenMempoolEmptyOrTargetLarge() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    assertAmountEq(chain.estimateRequiredFeeForInclusion(1), Blockchain::kMinRelayFee,
                   "Sans transactions en mempool, le fee estime doit retomber au minimum relay.");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.5)});
    assertAmountEq(chain.estimateRequiredFeeForInclusion(10), Blockchain::kMinRelayFee,
                   "Si la capacite cible depasse la mempool, le fee estime doit etre le minimum relay.");
}

void testFeeEstimateMatchesMempoolCutoff() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    chain.createTransaction(
        Transaction{"miner", "alice", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.1)});
    chain.createTransaction(
        Transaction{"miner", "bob", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.8)});
    chain.createTransaction(
        Transaction{"miner", "charlie", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.3)});
    chain.createTransaction(
        Transaction{"miner", "dave", Transaction::fromNOVA(1.0), nowSeconds(), Transaction::fromNOVA(0.5)});

    // maxTransactionsPerBlock=3 => 2 tx utilisateur par bloc.
    assertAmountEq(chain.estimateRequiredFeeForInclusion(1), Transaction::fromNOVA(0.5),
                   "Pour 1 bloc, le cutoff doit correspondre au 2e fee le plus eleve.");
    assertAmountEq(chain.estimateRequiredFeeForInclusion(2), Transaction::fromNOVA(0.1),
                   "Pour 2 blocs, le cutoff doit correspondre au 4e fee le plus eleve.");
}

void testFeeEstimateRejectsZeroTarget() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    bool threw = false;
    try {
        static_cast<void>(chain.estimateRequiredFeeForInclusion(0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assertTrue(threw, "Une estimation de frais avec targetBlocks=0 doit etre rejetee.");
}


void testRejectChainWithTimestampBelowMedianTimePast() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    std::vector<Block> candidate = chain.getChain();
    const std::uint64_t medianTimePast = chain.getMedianTimePast();

    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1,
                           medianTimePast > 0 ? medianTimePast - 1 : 0);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    assertTrue(!adopted, "Une chaine dont le nouveau bloc est sous le median-time-past doit etre rejetee.");
}

void testMedianTimePastAndNextMinimumTimestampExposure() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");
    chain.minePendingTransactions("miner");

    const std::uint64_t mtp = chain.getMedianTimePast();
    const std::uint64_t nextMinTimestamp = chain.estimateNextMinimumTimestamp();

    assertTrue(mtp > 0, "Le median-time-past doit etre expose et strictement positif.");
    assertTrue(nextMinTimestamp == mtp,
               "La contrainte de timestamp minimum du prochain bloc doit correspondre au median-time-past actuel.");
}

void testEqualWorkChainUsesDeterministicTieBreak() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const std::vector<Block> forkBase = chain.getChain();

    chain.minePendingTransactions("miner");
    const std::vector<Block> local = chain.getChain();

    std::vector<Block> candidate = forkBase;
    candidate.emplace_back(static_cast<std::uint64_t>(candidate.size()),
                           candidate.back().getHash(),
                           std::vector<Transaction>{
                               Transaction{"network", "alt-miner", Transaction::fromNOVA(25.0), nowSeconds(), 0}},
                           1);
    candidate.back().mine();

    const bool adopted = chain.tryAdoptChain(candidate);
    const bool shouldAdopt = candidate.back().getHash() < local.back().getHash();
    assertTrue(adopted == shouldAdopt,
               "En cas de travail cumule egal, la chaine au tip hash lexicographiquement plus petit doit gagner.");
}

void testIdenticalChainIsNotCountedAsReorg() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};
    chain.minePendingTransactions("miner");

    const std::vector<Block> sameChain = chain.getChain();
    const bool adopted = chain.tryAdoptChain(sameChain);
    assertTrue(!adopted, "Reproposer la meme chaine ne doit pas etre considere comme une adoption.");
    assertTrue(chain.getReorgCount() == 0, "Une chaine identique ne doit pas incrementer les metriques de reorg.");
}


void testEstimateSupplyAtHeightMonotonicAndCapped() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};

    const Amount h1 = chain.estimateSupplyAtHeight(1);
    const Amount hHalving = chain.estimateSupplyAtHeight(Blockchain::kHalvingInterval);
    const Amount hFar = chain.estimateSupplyAtHeight(1'000'000);

    assertTrue(h1 > 0, "La projection monetaire au bloc 1 doit etre strictement positive.");
    assertTrue(hHalving >= h1, "La supply projetee doit etre monotone croissante avec la hauteur.");
    assertTrue(hFar <= Blockchain::kMaxSupply, "La supply projetee ne doit jamais depasser le hard cap.");
}

void testMonetaryProjectionExposesConsistentFields() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 4};

    const std::size_t targetHeight = Blockchain::kHalvingInterval + 1;
    const auto projection = chain.getMonetaryProjection(targetHeight);

    assertTrue(projection.height == targetHeight, "La projection doit exposer la hauteur demandee.");
    assertAmountEq(projection.projectedSupply,
                   chain.estimateSupplyAtHeight(targetHeight),
                   "La supply projetee du snapshot doit correspondre a estimateSupplyAtHeight.");
    assertAmountEq(projection.remainingIssuable,
                   Blockchain::kMaxSupply - projection.projectedSupply,
                   "Le restant emissible doit etre coherent avec le hard cap.");
    assertTrue(projection.nextHalvingHeight == 2 * Blockchain::kHalvingInterval,
               "La prochaine hauteur de halving doit etre alignee sur l'intervalle consensus.");
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
        runPlatformModulesTests();
        testReorgMempoolDropsNowUnfundedTransactions();
        testNoReorgMetricsChangeWhenAdoptionRejected();
        testReorgMetricsTrackDepthAndForkHeight();
        testAddressStatsAndTopBalances();
        testNetworkStatsExposeChainActivity();
        testMempoolStatsExposePendingFeeDistribution();
        testHeadersFromHeightAndLocatorHelpers();
        testHeadersForLocatorReturnsNextSegment();
        testHeadersForLocatorWithStopHashBoundsResponse();
        testHeadersForLocatorWithUnknownStopHashFallsBackToMaxCount();
        testSyncStatusProvidesDeterministicWindow();
        testSyncStatusHandlesUnknownLocatorAndStop();
        testSyncStatusAtTipSignalsTerminalWindow();
        testBlocksFromHeightAndLocatorHelpers();
        testBlocksForLocatorWithStopHashBoundsResponse();
        testBlockSummaryLookupByHeightAndHash();
        testRecentBlockSummariesAreOrderedFromTip();
        testFindTransactionByIdForConfirmedTransaction();
        testFindTransactionByIdForPendingAndUnknownTransaction();
        testTransactionHistoryDetailedIncludesConfirmedAndPending();
        testTransactionHistoryDetailedSupportsLimitAndConfirmedOnly();
        testMempoolCapacityEvictsLowestFeeTransaction();
        testMempoolCapacityRejectsTooLowFeeWhenFull();
        testFeeEstimateReturnsRelayFeeWhenMempoolEmptyOrTargetLarge();
        testFeeEstimateMatchesMempoolCutoff();
        testFeeEstimateRejectsZeroTarget();
        testRejectChainWithTimestampBelowMedianTimePast();
        testMedianTimePastAndNextMinimumTimestampExposure();
        testEqualWorkChainUsesDeterministicTieBreak();
        testIdenticalChainIsNotCountedAsReorg();
        testExpiredMempoolTransactionsArePruned();
        testAmountConversionRoundTrip();
        testDifficultyRetargetIncreasesWhenBlocksTooFast();
        testRejectChainWithCoinbaseNotInLastPosition();
        testRejectChainWithMultipleCoinbaseTransactions();
        testRejectChainContainingDuplicateUserTransactionIds();
        testEstimateSupplyAtHeightMonotonicAndCapped();
        testMonetaryProjectionExposesConsistentFields();
        testRejectChainFromDifferentGenesis();
        std::cout << "Tous les tests Novacoin sont passes.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Echec test: " << ex.what() << '\n';
        return 1;
    }
}
