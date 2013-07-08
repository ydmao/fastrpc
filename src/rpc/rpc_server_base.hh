#ifndef RPC_SERVER_BASE
#define RPC_SERVER_BASE
#include <vector>
struct parser;
struct async_rpcc;

namespace rpc {
struct rpc_server_base {
    virtual std::vector<int> proclist() const = 0;
    virtual void dispatch(parser&, async_rpcc*, uint64_t) = 0;
};
};

#endif
