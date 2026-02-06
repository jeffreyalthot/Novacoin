#pragma once

#include "block.hpp"

#include <optional>
#include <string>
#include <vector>

struct StoredBlockHeader {
    std::uint64_t index = 0;
    std::string hash;
    std::string previousHash;
    std::uint64_t timestamp = 0;
    std::uint64_t nonce = 0;
    unsigned int difficulty = 0;
};

class BlockStorageCodec {
public:
    static StoredBlockHeader headerFromBlock(const Block& block);

    static std::string encodeHeader(const StoredBlockHeader& header);
    static std::optional<StoredBlockHeader> decodeHeader(const std::string& payload);

    static std::string encodeBlock(const Block& block);
    static std::optional<Block> decodeBlock(const std::string& payload);

    static std::string encodeHeaderBatch(const std::vector<StoredBlockHeader>& headers);
    static std::vector<StoredBlockHeader> decodeHeaderBatch(const std::string& payload);

    static std::string encodeBlockBatch(const std::vector<Block>& blocks);
    static std::vector<Block> decodeBlockBatch(const std::string& payload);
};
