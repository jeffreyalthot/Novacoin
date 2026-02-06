#include "block.hpp"

#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>

namespace {
std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

std::string toHex(std::size_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

std::string digest64(const std::string& input) {
    const std::size_t h1 = std::hash<std::string>{}(input);
    const std::size_t h2 = std::hash<std::string>{}(input + "_a");
    const std::size_t h3 = std::hash<std::string>{}(input + "_b");
    const std::size_t h4 = std::hash<std::string>{}(input + "_c");

    return toHex(h1) + toHex(h2) + toHex(h3) + toHex(h4);
}

std::string difficultyPrefix(unsigned int difficulty) {
    return std::string(difficulty, '0');
}
} // namespace

Block::Block(std::uint64_t index,
             std::string previousHash,
             std::vector<Transaction> transactions,
             unsigned int difficulty,
             std::uint64_t timestamp)
    : index_(index),
      previousHash_(std::move(previousHash)),
      transactions_(std::move(transactions)),
      timestamp_(timestamp == 0 ? nowSeconds() : timestamp),
      nonce_(0),
      difficulty_(difficulty),
      hash_() {
    hash_ = computeHash();
}

Block::Block(std::uint64_t index,
             std::string previousHash,
             std::vector<Transaction> transactions,
             unsigned int difficulty,
             std::uint64_t timestamp,
             std::uint64_t nonce,
             std::string hash)
    : index_(index),
      previousHash_(std::move(previousHash)),
      transactions_(std::move(transactions)),
      timestamp_(timestamp),
      nonce_(nonce),
      difficulty_(difficulty),
      hash_(std::move(hash)) {}

void Block::mine() {
    const std::size_t hashLength = hash_.size();
    if (difficulty_ == 0 || difficulty_ > hashLength) {
        return;
    }
    const std::string targetPrefix = difficultyPrefix(difficulty_);

    do {
        ++nonce_;
        hash_ = computeHash();
    } while (hash_.compare(0, targetPrefix.size(), targetPrefix) != 0);
}

bool Block::hasValidHash() const {
    if (difficulty_ > hash_.size()) {
        return false;
    }
    const std::string targetPrefix = difficultyPrefix(difficulty_);
    return hash_ == computeHash() && hash_.compare(0, targetPrefix.size(), targetPrefix) == 0;
}

std::uint64_t Block::getIndex() const { return index_; }
const std::string& Block::getPreviousHash() const { return previousHash_; }
const std::string& Block::getHash() const { return hash_; }
std::uint64_t Block::getNonce() const { return nonce_; }
std::uint64_t Block::getTimestamp() const { return timestamp_; }
unsigned int Block::getDifficulty() const { return difficulty_; }
const std::vector<Transaction>& Block::getTransactions() const { return transactions_; }

std::string Block::computeHash() const {
    std::ostringstream payload;
    payload << index_ << previousHash_ << timestamp_ << nonce_ << difficulty_;
    for (const auto& tx : transactions_) {
        payload << tx.serialize();
    }
    return digest64(payload.str());
}
