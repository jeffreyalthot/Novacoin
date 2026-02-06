#include "network/p2p_protocol.hpp"

#include <limits>

namespace {
void appendUint32LE(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

void appendUint64LE(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

std::uint32_t readUint32LE(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    if (offset + 4 > data.size()) {
        return 0;
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(data[offset++]) << (8 * i);
    }
    return value;
}

std::uint64_t readUint64LE(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    if (offset + 8 > data.size()) {
        return 0;
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[offset++]) << (8 * i);
    }
    return value;
}
} // namespace

std::vector<std::uint8_t> P2PProtocol::encodeBeacon(const std::string& networkId,
                                                    const std::string& nodeId,
                                                    std::uint64_t timestamp) {
    std::vector<std::uint8_t> out;
    out.insert(out.end(), kBeaconMagic.begin(), kBeaconMagic.end());
    const auto networkSize = static_cast<std::uint32_t>(
        std::min<std::size_t>(networkId.size(), std::numeric_limits<std::uint32_t>::max()));
    const auto nodeSize = static_cast<std::uint32_t>(
        std::min<std::size_t>(nodeId.size(), std::numeric_limits<std::uint32_t>::max()));
    appendUint32LE(out, networkSize);
    out.insert(out.end(), networkId.begin(), networkId.begin() + static_cast<std::ptrdiff_t>(networkSize));
    appendUint32LE(out, nodeSize);
    out.insert(out.end(), nodeId.begin(), nodeId.begin() + static_cast<std::ptrdiff_t>(nodeSize));
    appendUint64LE(out, timestamp);
    return out;
}

bool P2PProtocol::decodeBeacon(const std::vector<std::uint8_t>& data, BeaconSignal& out) {
    if (data.size() < kBeaconMagic.size() + 4 + 4 + 8) {
        return false;
    }
    std::size_t offset = 0;
    if (!std::equal(kBeaconMagic.begin(), kBeaconMagic.end(), data.begin())) {
        return false;
    }
    offset += kBeaconMagic.size();
    const auto networkSize = readUint32LE(data, offset);
    if (offset + networkSize > data.size()) {
        return false;
    }
    out.networkId.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                         data.begin() + static_cast<std::ptrdiff_t>(offset + networkSize));
    offset += networkSize;
    const auto nodeSize = readUint32LE(data, offset);
    if (offset + nodeSize > data.size()) {
        return false;
    }
    out.nodeId.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                      data.begin() + static_cast<std::ptrdiff_t>(offset + nodeSize));
    offset += nodeSize;
    if (offset + 8 > data.size()) {
        return false;
    }
    out.timestamp = readUint64LE(data, offset);
    return offset == data.size();
}
