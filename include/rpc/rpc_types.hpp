#pragma once

#include <optional>
#include <string>
#include <vector>

namespace rpc {

enum class RpcErrorCode {
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InternalError = -32603
};

struct RpcError {
    RpcErrorCode code;
    std::string message;
};

struct RpcRequest {
    int id = 0;
    std::string method;
    std::vector<std::string> params;

    [[nodiscard]] bool isValid() const;
};

struct RpcResponse {
    int id = 0;
    std::string result;
    std::optional<RpcError> error;

    [[nodiscard]] bool hasError() const;

    static RpcResponse success(int id, std::string result);
    static RpcResponse failure(int id, RpcErrorCode code, std::string message);
};

[[nodiscard]] std::string toString(RpcErrorCode code);

} // namespace rpc
