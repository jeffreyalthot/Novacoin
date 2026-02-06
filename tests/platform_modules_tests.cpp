#include "network/peer_manager.hpp"
#include "storage/chain_snapshot.hpp"
#include "utils/logger.hpp"

#include <stdexcept>

namespace {
void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testLoggerRingBufferBehavior() {
    Logger logger{2};
    logger.info("node", "startup");
    logger.warning("net", "peer timeout");
    logger.error("consensus", "invalid block");

    const auto entries = logger.entries();
    assertTrue(entries.size() == 2, "Le logger doit respecter la taille maximale.");
    assertTrue(entries.front().component == "net", "Le logger doit evincer les plus anciennes entrees.");
}

void testPeerManagerCapacityAndValidation() {
    PeerManager peers{2};
    assertTrue(peers.addPeer("127.0.0.1:8333"), "Un peer valide doit etre ajoute.");
    assertTrue(!peers.addPeer("127.0.0.1:8333"), "Un peer en doublon ne doit pas etre ajoute.");
    assertTrue(!peers.addPeer("invalid-endpoint"), "Un endpoint sans port ne doit pas etre accepte.");
    assertTrue(peers.addPeer("10.0.0.2:8333"), "Le deuxieme peer doit etre accepte.");
    assertTrue(!peers.addPeer("10.0.0.3:8333"), "La capacite maximale doit etre appliquee.");
}

void testChainSnapshotBuilder() {
    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");

    const ChainSnapshot snapshot = ChainSnapshotBuilder::fromBlockchain(chain);
    assertTrue(snapshot.height >= 2, "Le snapshot doit reporter une hauteur non vide.");
    assertTrue(!snapshot.tipHash.empty(), "Le snapshot doit reporter le hash de tip.");

    const std::string pretty = ChainSnapshotBuilder::toPrettyString(snapshot);
    assertTrue(pretty.find("height=") != std::string::npos, "Le rendu texte du snapshot doit inclure la hauteur.");
}
}

void runPlatformModulesTests() {
    testLoggerRingBufferBehavior();
    testPeerManagerCapacityAndValidation();
    testChainSnapshotBuilder();
}
