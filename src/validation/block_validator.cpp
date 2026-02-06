#include "validation/block_validator.hpp"

namespace validation {
BlockValidationResult validateBasicShape(const Block& block) {
    if (block.getHash().empty()) {
        return {false, "empty hash"};
    }

    if (block.getTransactions().empty()) {
        return {false, "empty transaction set"};
    }

    return {true, "ok"};
}
} // namespace validation
