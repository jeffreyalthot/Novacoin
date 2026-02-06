#include "network/p2p_node.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

bool isEndpointValid(const std::string& endpoint) {
    return !endpoint.empty() && endpoint.find(':') != std::string::npos;
}
} // namespace

P2PNode::P2PNode(std::string nodeId,
                 std::string endpoint,
                 std::string networkId,
                 Blockchain& blockchain,
                 wallet::WalletStore* wallet)
    : nodeId_(std::move(nodeId)),
      endpoint_(std::move(endpoint)),
      networkId_(std::move(networkId)),
      blockchain_(blockchain),
      wallet_(wallet),
      peerManager_(128) {
    if (!isEndpointValid(endpoint_)) {
        throw std::invalid_argument("Endpoint P2P invalide.");
    }
    if (nodeId_.empty()) {
        throw std::invalid_argument("nodeId invalide.");
    }
    if (networkId_.empty()) {
        throw std::invalid_argument("networkId invalide.");
    }
}

bool P2PNode::connectPeer(P2PNode& peer, const std::string& endpoint) {
    if (!isEndpointValid(endpoint)) {
        return false;
    }
    if (!peerManager_.addPeer(endpoint)) {
        return false;
    }
    peers_[endpoint] = PeerLink{&peer, endpoint};
    return true;
}

bool P2PNode::connectBidirectional(P2PNode& peer,
                                   const std::string& localEndpoint,
                                   const std::string& peerEndpoint) {
    const bool localOk = connectPeer(peer, peerEndpoint);
    const bool peerOk = peer.connectPeer(*this, localEndpoint);
    return localOk && peerOk;
}

void P2PNode::broadcastBeacon() {
    const auto signal = P2PProtocol::encodeBeacon(networkId_, nodeId_, nowSeconds());
    for (auto& [endpoint, link] : peers_) {
        if (link.node) {
            link.node->receiveBeacon(signal, endpoint_);
        }
    }
}

bool P2PNode::receiveBeacon(const std::vector<std::uint8_t>& data, const std::string& endpoint) {
    BeaconSignal signal;
    if (!P2PProtocol::decodeBeacon(data, signal)) {
        return false;
    }
    if (signal.networkId != networkId_) {
        return false;
    }
    if (!peerManager_.addPeer(endpoint) && !peerManager_.hasPeer(endpoint)) {
        return false;
    }
    peerBeaconTimestamps_[endpoint] = signal.timestamp;
    return true;
}

void P2PNode::syncWithPeers() {
    for (auto& [endpoint, link] : peers_) {
        if (!link.node) {
            continue;
        }
        syncHeadersFromPeer(*link.node, endpoint);
        syncBlocksFromPeer(*link.node);
        syncMempoolFromPeer(*link.node);
        syncWalletFromPeer(*link.node);
        pushHeadersToPeer(*link.node);
        pushMempoolToPeer(*link.node);
    }
}

void P2PNode::announceTransaction(const Transaction& tx) {
    if (!maybeAcceptTransaction(tx)) {
        return;
    }
    for (auto& [endpoint, link] : peers_) {
        if (link.node) {
            link.node->receiveTransactionFromPeer(tx, endpoint_);
        }
    }
}

void P2PNode::watchWalletAddress(const std::string& address) {
    if (address.empty()) {
        return;
    }
    if (std::find(watchedWalletAddresses_.begin(), watchedWalletAddresses_.end(), address) ==
        watchedWalletAddresses_.end()) {
        watchedWalletAddresses_.push_back(address);
    }
}

const std::vector<std::string>& P2PNode::watchedWalletAddresses() const {
    return watchedWalletAddresses_;
}

const Blockchain& P2PNode::blockchain() const {
    return blockchain_;
}

std::size_t P2PNode::peerCount() const {
    return peerManager_.size();
}

std::optional<std::size_t> P2PNode::reportedPeerHeight(const std::string& endpoint) const {
    const auto it = peerHeights_.find(endpoint);
    if (it == peerHeights_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void P2PNode::syncHeadersFromPeer(P2PNode& peer, const std::string& endpoint) {
    constexpr std::size_t kMaxHeadersPerSync = 2000;
    const auto locator = blockchain_.getBlockLocatorHashes();
    const auto headers = peer.blockchain().getHeadersForLocator(locator, kMaxHeadersPerSync);
    if (!verifyHeaderSequence(headers)) {
        return;
    }
    if (!headers.empty()) {
        peerHeights_[endpoint] = headers.back().index;
        return;
    }
    if (peer.blockchain().getBlockCount() > 0) {
        peerHeights_[endpoint] = peer.blockchain().getBlockCount() - 1;
    }
}

void P2PNode::syncBlocksFromPeer(P2PNode& peer) {
    if (peer.blockchain().getCumulativeWork() <= blockchain_.getCumulativeWork()) {
        return;
    }
    blockchain_.tryAdoptChain(peer.blockchain().getChain());
}

void P2PNode::syncMempoolFromPeer(P2PNode& peer) {
    const auto& txs = peer.blockchain().getPendingTransactions();
    for (const auto& tx : txs) {
        receiveTransactionFromPeer(tx, peer.endpoint_);
    }
}

void P2PNode::syncWalletFromPeer(P2PNode& peer) {
    if (!wallet_ || watchedWalletAddresses_.empty()) {
        return;
    }
    for (const auto& address : watchedWalletAddresses_) {
        const auto history = peer.blockchain().getTransactionHistoryDetailed(address, 0, true);
        for (const auto& entry : history) {
            const auto txId = entry.tx.id();
            if (entry.tx.to != address) {
                continue;
            }
            if (seenWalletTxIds_.find(txId) != seenWalletTxIds_.end()) {
                continue;
            }
            wallet_->addIncomingTransaction(entry.tx, address);
            seenWalletTxIds_.insert(txId);
        }
    }
}

void P2PNode::pushMempoolToPeer(P2PNode& peer) {
    const auto& txs = blockchain_.getPendingTransactions();
    for (const auto& tx : txs) {
        peer.receiveTransactionFromPeer(tx, endpoint_);
    }
}

void P2PNode::pushHeadersToPeer(P2PNode& peer) {
    const auto height = blockchain_.getBlockCount();
    if (height == 0) {
        return;
    }
    const auto headers = blockchain_.getHeadersFromHeight(height - 1, 1);
    peer.receiveHeaders(headers, endpoint_);
}

bool P2PNode::receiveHeaders(const std::vector<BlockHeaderInfo>& headers, const std::string& endpoint) {
    if (!verifyHeaderSequence(headers)) {
        return false;
    }
    if (!headers.empty()) {
        peerHeights_[endpoint] = headers.back().index;
    }
    return true;
}

bool P2PNode::receiveTransactionFromPeer(const Transaction& tx, const std::string& endpoint) {
    (void)endpoint;
    return maybeAcceptTransaction(tx);
}

bool P2PNode::verifyHeaderSequence(const std::vector<BlockHeaderInfo>& headers) const {
    if (headers.empty()) {
        return true;
    }
    for (std::size_t i = 1; i < headers.size(); ++i) {
        if (headers[i].index != headers[i - 1].index + 1) {
            return false;
        }
        if (headers[i].previousHash != headers[i - 1].hash) {
            return false;
        }
    }
    if (headers.front().index > 0) {
        auto localPrev = blockchain_.getBlockSummaryByHeight(headers.front().index - 1);
        if (localPrev && localPrev->hash != headers.front().previousHash) {
            return false;
        }
    }
    return true;
}

bool P2PNode::maybeAcceptTransaction(const Transaction& tx) {
    const auto txId = tx.id();
    if (seenMempoolTxIds_.find(txId) != seenMempoolTxIds_.end()) {
        return false;
    }
    try {
        blockchain_.createTransaction(tx);
    } catch (const std::exception&) {
        return false;
    }
    seenMempoolTxIds_.insert(txId);
    return true;
}
