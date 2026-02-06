#include "rpc/rpc_dispatcher.hpp"

#include <algorithm>

namespace rpc {

bool RpcDispatcher::registerHandler(const std::string& method, RpcHandler handler) {
    if (method.empty() || !handler) {
        return false;
    }

    return handlers_.emplace(method, std::move(handler)).second;
}

bool RpcDispatcher::unregisterHandler(const std::string& method) {
    return handlers_.erase(method) > 0;
}

bool RpcDispatcher::hasHandler(const std::string& method) const {
    return handlers_.find(method) != handlers_.end();
}

RpcResponse RpcDispatcher::dispatch(const RpcRequest& request, const RpcContext& context) const {
    if (!request.isValid()) {
        return RpcResponse::failure(request.id, RpcErrorCode::InvalidRequest, "Invalid RPC request");
    }

    auto it = handlers_.find(request.method);
    if (it == handlers_.end()) {
        return RpcResponse::failure(request.id, RpcErrorCode::MethodNotFound, "RPC method not found");
    }

    return it->second(request, context);
}

std::vector<std::string> RpcDispatcher::listMethods() const {
    std::vector<std::string> methods;
    methods.reserve(handlers_.size());
    for (const auto& entry : handlers_) {
        methods.push_back(entry.first);
    }
    std::sort(methods.begin(), methods.end());
    return methods;
}

} // namespace rpc
