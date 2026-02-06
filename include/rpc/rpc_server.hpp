#pragma once

#include "rpc/rpc_context.hpp"
#include "rpc/rpc_dispatcher.hpp"
#include "rpc/rpc_types.hpp"

namespace rpc {

class RpcServer {
public:
    RpcServer(RpcContext context, RpcDispatcher dispatcher);

    void setContext(RpcContext context);
    [[nodiscard]] const RpcContext& context() const;

    void setDispatcher(RpcDispatcher dispatcher);
    [[nodiscard]] RpcResponse handle(const RpcRequest& request) const;

private:
    RpcContext context_;
    RpcDispatcher dispatcher_;
};

} // namespace rpc
