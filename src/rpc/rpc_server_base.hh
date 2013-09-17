#ifndef RPC_SERVER_BASE
#define RPC_SERVER_BASE
#include <vector>
#include "rpc_common/fdstream.hh"

namespace rpc {

struct parser;
class async_rpcc;

struct rpc_server_base {
    virtual std::vector<int> proclist() const = 0;
    virtual void dispatch_sync(rpc_header&, std::string& body, fdstream*, uint64_t) = 0;
    virtual void dispatch(parser&, async_rpcc*, uint64_t) = 0;
    virtual void client_failure(async_rpcc*) = 0;
};

};

#endif
