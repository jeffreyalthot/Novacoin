#include "rpc/rpc_server.hpp"

namespace rpc {

RpcServer::RpcServer(RpcContext context, RpcDispatcher dispatcher)
    : context_(std::move(context)), dispatcher_(std::move(dispatcher)) {}

void RpcServer::setContext(RpcContext context) {
    context_ = std::move(context);
}

const RpcContext& RpcServer::context() const {
    return context_;
}

void RpcServer::setDispatcher(RpcDispatcher dispatcher) {
    dispatcher_ = std::move(dispatcher);
}

RpcResponse RpcServer::handle(const RpcRequest& request) const {
    return dispatcher_.dispatch(request, context_);
}

} // namespace rpc
