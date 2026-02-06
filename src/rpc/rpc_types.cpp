#include "rpc/rpc_types.hpp"

namespace rpc {

bool RpcRequest::isValid() const {
    return id >= 0 && !method.empty();
}

bool RpcResponse::hasError() const {
    return error.has_value();
}

RpcResponse RpcResponse::success(int id, std::string result) {
    RpcResponse response;
    response.id = id;
    response.result = std::move(result);
    response.error.reset();
    return response;
}

RpcResponse RpcResponse::failure(int id, RpcErrorCode code, std::string message) {
    RpcResponse response;
    response.id = id;
    response.result.clear();
    response.error = RpcError{code, std::move(message)};
    return response;
}

std::string toString(RpcErrorCode code) {
    switch (code) {
    case RpcErrorCode::InvalidRequest:
        return "invalid_request";
    case RpcErrorCode::MethodNotFound:
        return "method_not_found";
    case RpcErrorCode::InternalError:
        return "internal_error";
    }

    return "unknown_error";
}

} // namespace rpc
