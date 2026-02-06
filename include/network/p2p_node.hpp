#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "blockchain.hpp"
#include "network/peer_manager.hpp"
#include "network/p2p_protocol.hpp"
#include "transaction.hpp"
#include "wallet/wallet.hpp"

class P2PNode {
public:
    P2PNode(std::string nodeId,
            std::string endpoint,
            std::string networkId,
            Blockchain& blockchain,
            wallet::WalletStore* wallet = nullptr);

    [[nodiscard]] bool connectPeer(P2PNode& peer, const std::string& endpoint);
    [[nodiscard]] bool connectBidirectional(P2PNode& peer,
                                            const std::string& localEndpoint,
                                            const std::string& peerEndpoint);

    void broadcastBeacon();
    [[nodiscard]] bool receiveBeacon(const std::vector<std::uint8_t>& data, const std::string& endpoint);

    void syncWithPeers();
    void announceTransaction(const Transaction& tx);

    void watchWalletAddress(const std::string& address);
    [[nodiscard]] const std::vector<std::string>& watchedWalletAddresses() const;

    [[nodiscard]] const Blockchain& blockchain() const;
    [[nodiscard]] std::size_t peerCount() const;
    [[nodiscard]] std::optional<std::size_t> reportedPeerHeight(const std::string& endpoint) const;

private:
    struct PeerLink {
        P2PNode* node = nullptr;
        std::string endpoint;
    };

    void syncHeadersFromPeer(P2PNode& peer, const std::string& endpoint);
    void syncBlocksFromPeer(P2PNode& peer);
    void syncMempoolFromPeer(P2PNode& peer);
    void syncWalletFromPeer(P2PNode& peer);
    void pushMempoolToPeer(P2PNode& peer);
    void pushHeadersToPeer(P2PNode& peer);
    [[nodiscard]] bool receiveHeaders(const std::vector<BlockHeaderInfo>& headers,
                                      const std::string& endpoint);
    [[nodiscard]] bool receiveTransactionFromPeer(const Transaction& tx, const std::string& endpoint);
    [[nodiscard]] bool verifyHeaderSequence(const std::vector<BlockHeaderInfo>& headers) const;
    [[nodiscard]] bool maybeAcceptTransaction(const Transaction& tx);

    std::string nodeId_;
    std::string endpoint_;
    std::string networkId_;
    Blockchain& blockchain_;
    wallet::WalletStore* wallet_ = nullptr;
    PeerManager peerManager_;
    std::unordered_map<std::string, PeerLink> peers_;
    std::unordered_map<std::string, std::size_t> peerHeights_;
    std::unordered_map<std::string, std::uint64_t> peerBeaconTimestamps_;
    std::unordered_set<std::string> seenMempoolTxIds_;
    std::unordered_set<std::string> seenWalletTxIds_;
    std::vector<std::string> watchedWalletAddresses_;
};
