#include "network/peer_manager.hpp"

#include <algorithm>

PeerManager::PeerManager(std::size_t maxPeers) : maxPeers_(std::max<std::size_t>(1, maxPeers)) {}

bool PeerManager::addPeer(const std::string& endpoint) {
    if (!isEndpointValid(endpoint) || isFull()) {
        return false;
    }

    return peers_.insert(endpoint).second;
}

bool PeerManager::removePeer(const std::string& endpoint) {
    return peers_.erase(endpoint) > 0;
}

bool PeerManager::hasPeer(const std::string& endpoint) const {
    return peers_.find(endpoint) != peers_.end();
}

std::size_t PeerManager::size() const {
    return peers_.size();
}

std::size_t PeerManager::maxPeers() const {
    return maxPeers_;
}

bool PeerManager::isFull() const {
    return peers_.size() >= maxPeers_;
}

std::vector<std::string> PeerManager::listPeers() const {
    std::vector<std::string> values;
    values.reserve(peers_.size());
    for (const auto& peer : peers_) {
        values.push_back(peer);
    }
    std::sort(values.begin(), values.end());
    return values;
}

bool PeerManager::isEndpointValid(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    return endpoint.find(':') != std::string::npos;
}
