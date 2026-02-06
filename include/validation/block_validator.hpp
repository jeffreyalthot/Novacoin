#pragma once

#include "block.hpp"

namespace validation {
struct BlockValidationResult {
    bool valid = false;
    const char* reason = "";
};

[[nodiscard]] BlockValidationResult validateBasicShape(const Block& block);
} // namespace validation
