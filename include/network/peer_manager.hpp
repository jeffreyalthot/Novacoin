#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

class PeerManager {
public:
    explicit PeerManager(std::size_t maxPeers = 128);

    [[nodiscard]] bool addPeer(const std::string& endpoint);
    [[nodiscard]] bool removePeer(const std::string& endpoint);
    [[nodiscard]] bool hasPeer(const std::string& endpoint) const;

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t maxPeers() const;
    [[nodiscard]] bool isFull() const;

    [[nodiscard]] std::vector<std::string> listPeers() const;

private:
    std::size_t maxPeers_ = 128;
    std::unordered_set<std::string> peers_;

    static bool isEndpointValid(const std::string& endpoint);
};
