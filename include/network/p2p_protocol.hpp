#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct BeaconSignal {
    std::string networkId;
    std::string nodeId;
    std::uint64_t timestamp = 0;
};

class P2PProtocol {
public:
    static constexpr std::array<std::uint8_t, 4> kBeaconMagic{{'N', 'O', 'V', 'A'}};

    static std::vector<std::uint8_t> encodeBeacon(const std::string& networkId,
                                                  const std::string& nodeId,
                                                  std::uint64_t timestamp);
    static bool decodeBeacon(const std::vector<std::uint8_t>& data, BeaconSignal& out);
};
