#include "consensus/chain_params.hpp"
#include "mempool/policy.hpp"
#include "network/peer_manager.hpp"
#include "rpc/rpc_context.hpp"
#include "storage/chain_snapshot.hpp"
#include "utils/logger.hpp"
#include "validation/block_validator.hpp"
#include "wallet/wallet_profile.hpp"

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


void testExtendedModuleScaffolding() {
    const auto params = consensus::defaultChainParams();
    assertTrue(params.targetBlockTimeSeconds == consensus::kTargetBlockTimeSeconds,
               "Les chain params doivent exposer les constantes consensus.");

    const Transaction lowFee{"alice", "bob", 10, 1, 1};
    const Transaction highFee{"alice", "bob", 10, 1, consensus::kMinRelayFee};
    assertTrue(!mempool::accepts(lowFee), "Une transaction fee faible doit etre rejetee par la policy.");
    assertTrue(mempool::accepts(highFee), "Une transaction fee suffisante doit etre acceptee par la policy.");

    Blockchain chain{1, Transaction::fromNOVA(25.0), 3};
    chain.minePendingTransactions("miner");
    const auto result = validation::validateBasicShape(chain.getChain().back());
    assertTrue(result.valid, "Le validateur basique doit accepter un bloc mine valide.");

    const auto ctx = rpc::buildDefaultContext();
    assertTrue(ctx.nodeName == "novacoind", "Le contexte RPC par defaut doit etre coherent.");

    const auto profile = wallet::defaultProfile();
    assertTrue(profile.label == "default", "Le wallet profile par defaut doit etre initialise.");
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
    testExtendedModuleScaffolding();
    testChainSnapshotBuilder();
}
