#include "storage/block_storage.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace {
constexpr unsigned char kCompressionMarker = 0x00;

void appendField(std::string& out, const std::string& field) {
    out += std::to_string(field.size());
    out += ':';
    out += field;
}

bool readField(std::string_view payload, std::size_t& offset, std::string& out) {
    const std::size_t separator = payload.find(':', offset);
    if (separator == std::string_view::npos) {
        return false;
    }
    if (separator == offset) {
        return false;
    }

    std::size_t length = 0;
    try {
        length = static_cast<std::size_t>(
            std::stoull(std::string(payload.substr(offset, separator - offset))));
    } catch (const std::exception&) {
        return false;
    }

    const std::size_t start = separator + 1;
    if (start + length > payload.size()) {
        return false;
    }

    out.assign(payload.substr(start, length));
    offset = start + length;
    return true;
}

std::string encodeNumber(std::uint64_t value) {
    return std::to_string(value);
}

std::optional<std::uint64_t> decodeNumber(const std::string& value) {
    try {
        return static_cast<std::uint64_t>(std::stoull(value));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string compressUltimateFast(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    std::size_t i = 0;
    while (i < input.size()) {
        const unsigned char value = static_cast<unsigned char>(input[i]);
        std::size_t run = 1;
        while (i + run < input.size()
               && static_cast<unsigned char>(input[i + run]) == value) {
            ++run;
        }

        if (value == kCompressionMarker || run >= 4) {
            std::size_t remaining = run;
            while (remaining > 0) {
                const auto chunk = static_cast<unsigned char>(std::min<std::size_t>(remaining, 255));
                out.push_back(static_cast<char>(kCompressionMarker));
                out.push_back(static_cast<char>(chunk));
                out.push_back(static_cast<char>(value));
                remaining -= chunk;
            }
        } else {
            out.append(input.substr(i, run));
        }

        i += run;
    }
    return out;
}

std::optional<std::string> decompressUltimateFast(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    std::size_t i = 0;
    while (i < input.size()) {
        const unsigned char value = static_cast<unsigned char>(input[i]);
        if (value != kCompressionMarker) {
            out.push_back(static_cast<char>(value));
            ++i;
            continue;
        }
        if (i + 2 >= input.size()) {
            return std::nullopt;
        }
        const unsigned char count = static_cast<unsigned char>(input[i + 1]);
        const unsigned char repeated = static_cast<unsigned char>(input[i + 2]);
        if (count == 0) {
            return std::nullopt;
        }
        out.append(count, static_cast<char>(repeated));
        i += 3;
    }
    return out;
}
} // namespace

StoredBlockHeader BlockStorageCodec::headerFromBlock(const Block& block) {
    StoredBlockHeader header;
    header.index = block.getIndex();
    header.hash = block.getHash();
    header.previousHash = block.getPreviousHash();
    header.timestamp = block.getTimestamp();
    header.nonce = block.getNonce();
    header.difficulty = block.getDifficulty();
    return header;
}

std::string BlockStorageCodec::encodeHeader(const StoredBlockHeader& header) {
    std::string out;
    appendField(out, encodeNumber(header.index));
    appendField(out, header.hash);
    appendField(out, header.previousHash);
    appendField(out, encodeNumber(header.timestamp));
    appendField(out, encodeNumber(header.nonce));
    appendField(out, encodeNumber(header.difficulty));
    return out;
}

std::optional<StoredBlockHeader> BlockStorageCodec::decodeHeader(const std::string& payload) {
    std::size_t offset = 0;
    std::string field;
    StoredBlockHeader header;

    if (!readField(payload, offset, field)) {
        return std::nullopt;
    }
    auto index = decodeNumber(field);
    if (!index) {
        return std::nullopt;
    }
    header.index = *index;

    if (!readField(payload, offset, header.hash)) {
        return std::nullopt;
    }
    if (!readField(payload, offset, header.previousHash)) {
        return std::nullopt;
    }
    if (!readField(payload, offset, field)) {
        return std::nullopt;
    }
    auto timestamp = decodeNumber(field);
    if (!timestamp) {
        return std::nullopt;
    }
    header.timestamp = *timestamp;

    if (!readField(payload, offset, field)) {
        return std::nullopt;
    }
    auto nonce = decodeNumber(field);
    if (!nonce) {
        return std::nullopt;
    }
    header.nonce = *nonce;

    if (!readField(payload, offset, field)) {
        return std::nullopt;
    }
    auto difficulty = decodeNumber(field);
    if (!difficulty) {
        return std::nullopt;
    }
    header.difficulty = static_cast<unsigned int>(*difficulty);

    if (offset != payload.size()) {
        return std::nullopt;
    }

    return header;
}

std::string BlockStorageCodec::compressHeader(const StoredBlockHeader& header) {
    return compressUltimateFast(encodeHeader(header));
}

std::optional<StoredBlockHeader> BlockStorageCodec::decompressHeader(const std::string& payload) {
    auto decompressed = decompressUltimateFast(payload);
    if (!decompressed) {
        return std::nullopt;
    }
    return decodeHeader(*decompressed);
}

std::string BlockStorageCodec::encodeBlock(const Block& block) {
    std::string out;
    appendField(out, encodeHeader(headerFromBlock(block)));

    appendField(out, encodeNumber(block.getTransactions().size()));
    for (const auto& tx : block.getTransactions()) {
        appendField(out, tx.serialize());
    }

    return out;
}

std::optional<Block> BlockStorageCodec::decodeBlock(const std::string& payload) {
    std::size_t offset = 0;
    std::string field;
    std::string_view view(payload);

    std::string headerPayload;
    if (!readField(view, offset, headerPayload)) {
        return std::nullopt;
    }
    auto headerOpt = decodeHeader(headerPayload);
    if (!headerOpt) {
        return std::nullopt;
    }

    if (!readField(view, offset, field)) {
        return std::nullopt;
    }
    auto txCount = decodeNumber(field);
    if (!txCount) {
        return std::nullopt;
    }

    std::vector<Transaction> transactions;
    transactions.reserve(*txCount);
    for (std::uint64_t i = 0; i < *txCount; ++i) {
        if (!readField(view, offset, field)) {
            return std::nullopt;
        }
        try {
            transactions.push_back(Transaction::deserialize(field));
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    if (offset != payload.size()) {
        return std::nullopt;
    }

    const auto& header = *headerOpt;
    return Block(header.index,
                 header.previousHash,
                 std::move(transactions),
                 header.difficulty,
                 header.timestamp,
                 header.nonce,
                 header.hash);
}

std::string BlockStorageCodec::compressBlock(const Block& block) {
    return compressUltimateFast(encodeBlock(block));
}

std::optional<Block> BlockStorageCodec::decompressBlock(const std::string& payload) {
    auto decompressed = decompressUltimateFast(payload);
    if (!decompressed) {
        return std::nullopt;
    }
    return decodeBlock(*decompressed);
}

std::string BlockStorageCodec::encodeHeaderBatch(const std::vector<StoredBlockHeader>& headers) {
    std::string out;
    appendField(out, encodeNumber(headers.size()));
    for (const auto& header : headers) {
        appendField(out, encodeHeader(header));
    }
    return out;
}

std::vector<StoredBlockHeader> BlockStorageCodec::decodeHeaderBatch(const std::string& payload) {
    std::size_t offset = 0;
    std::string field;
    std::string_view view(payload);

    if (!readField(view, offset, field)) {
        return {};
    }
    auto count = decodeNumber(field);
    if (!count) {
        return {};
    }

    std::vector<StoredBlockHeader> headers;
    headers.reserve(*count);
    for (std::uint64_t i = 0; i < *count; ++i) {
        if (!readField(view, offset, field)) {
            return {};
        }
        auto header = decodeHeader(field);
        if (!header) {
            return {};
        }
        headers.push_back(std::move(*header));
    }

    if (offset != payload.size()) {
        return {};
    }

    return headers;
}

std::string BlockStorageCodec::encodeBlockBatch(const std::vector<Block>& blocks) {
    std::string out;
    appendField(out, encodeNumber(blocks.size()));
    for (const auto& block : blocks) {
        appendField(out, encodeBlock(block));
    }
    return out;
}

std::vector<Block> BlockStorageCodec::decodeBlockBatch(const std::string& payload) {
    std::size_t offset = 0;
    std::string field;
    std::string_view view(payload);

    if (!readField(view, offset, field)) {
        return {};
    }
    auto count = decodeNumber(field);
    if (!count) {
        return {};
    }

    std::vector<Block> blocks;
    blocks.reserve(*count);
    for (std::uint64_t i = 0; i < *count; ++i) {
        if (!readField(view, offset, field)) {
            return {};
        }
        auto block = decodeBlock(field);
        if (!block) {
            return {};
        }
        blocks.push_back(std::move(*block));
    }

    if (offset != payload.size()) {
        return {};
    }

    return blocks;
}
