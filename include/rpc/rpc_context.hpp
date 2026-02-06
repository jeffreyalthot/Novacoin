#pragma once

#include <string>

namespace rpc {
struct RpcContext {
    std::string nodeName;
    std::string network;
};

[[nodiscard]] RpcContext buildDefaultContext();
} // namespace rpc
