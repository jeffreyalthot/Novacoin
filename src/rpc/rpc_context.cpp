#include "rpc/rpc_context.hpp"

namespace rpc {
RpcContext buildDefaultContext() {
    return {"novacoind", "regtest"};
}
} // namespace rpc
