#pragma once

#include "rpc/rpc_context.hpp"
#include "rpc/rpc_types.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rpc {

using RpcHandler = std::function<RpcResponse(const RpcRequest&, const RpcContext&)>;

class RpcDispatcher {
public:
    [[nodiscard]] bool registerHandler(const std::string& method, RpcHandler handler);
    [[nodiscard]] bool unregisterHandler(const std::string& method);
    [[nodiscard]] bool hasHandler(const std::string& method) const;

    [[nodiscard]] RpcResponse dispatch(const RpcRequest& request, const RpcContext& context) const;
    [[nodiscard]] std::vector<std::string> listMethods() const;

private:
    std::unordered_map<std::string, RpcHandler> handlers_;
};

} // namespace rpc
