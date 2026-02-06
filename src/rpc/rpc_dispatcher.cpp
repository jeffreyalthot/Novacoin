#include "rpc/rpc_dispatcher.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <string_view>

namespace rpc {
namespace {
constexpr std::array<std::string_view, 9> kBuiltinMethods = {
    "rpc.ping",
    "rpc.echo",
    "rpc.context",
    "rpc.health",
    "rpc.listMethods",
    "rpc.methodsCount",
    "rpc.time",
    "rpc.uptime",
    "rpc.version"};

constexpr const char* kRpcVersion = "0.1.0";

std::uint64_t nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

std::string joinParams(const std::vector<std::string>& params, const char* delimiter) {
    std::ostringstream out;
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            out << delimiter;
        }
        out << params[i];
    }
    return out.str();
}
} // namespace

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

    static const auto startTime = nowSeconds();

    if (request.method == "rpc.ping") {
        return RpcResponse::success(request.id, "pong");
    }

    if (request.method == "rpc.echo") {
        return RpcResponse::success(request.id, joinParams(request.params, " "));
    }

    if (request.method == "rpc.context") {
        std::ostringstream out;
        out << "node_name=" << context.nodeName << " network=" << context.network;
        return RpcResponse::success(request.id, out.str());
    }

    if (request.method == "rpc.health") {
        std::ostringstream out;
        out << "status=ok uptime_s=" << (nowSeconds() - startTime) << " now=" << nowSeconds();
        return RpcResponse::success(request.id, out.str());
    }

    if (request.method == "rpc.listMethods") {
        const auto methods = listMethods();
        std::ostringstream out;
        out << "methods=" << joinParams(methods, ", ");
        return RpcResponse::success(request.id, out.str());
    }

    if (request.method == "rpc.methodsCount") {
        const auto methods = listMethods();
        std::ostringstream out;
        out << "method_count=" << methods.size();
        return RpcResponse::success(request.id, out.str());
    }

    if (request.method == "rpc.time") {
        std::ostringstream out;
        out << "now=" << nowSeconds();
        return RpcResponse::success(request.id, out.str());
    }

    if (request.method == "rpc.uptime") {
        std::ostringstream out;
        out << "uptime_s=" << (nowSeconds() - startTime);
        return RpcResponse::success(request.id, out.str());
    }

    if (request.method == "rpc.version") {
        std::ostringstream out;
        out << "version=" << kRpcVersion << " node_name=" << context.nodeName << " network=" << context.network;
        return RpcResponse::success(request.id, out.str());
    }

    auto it = handlers_.find(request.method);
    if (it == handlers_.end()) {
        return RpcResponse::failure(request.id, RpcErrorCode::MethodNotFound, "RPC method not found");
    }

    return it->second(request, context);
}

std::vector<std::string> RpcDispatcher::listMethods() const {
    std::vector<std::string> methods;
    methods.reserve(handlers_.size() + kBuiltinMethods.size());
    for (const auto method : kBuiltinMethods) {
        methods.emplace_back(method);
    }
    for (const auto& entry : handlers_) {
        methods.push_back(entry.first);
    }
    std::sort(methods.begin(), methods.end());
    methods.erase(std::unique(methods.begin(), methods.end()), methods.end());
    return methods;
}

} // namespace rpc
