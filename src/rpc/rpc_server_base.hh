#pragma once

#include <vector>
#include <string>
#include "rpc_common/fdstream.hh"

namespace rpc {

struct parser;
template <typename T>
struct async_rpcc;

template <typename T>
struct rpc_server_base {
    virtual std::vector<int> proclist() const = 0;
    virtual void dispatch_sync(rpc_header&, std::string& body, fdstream<T>*, uint64_t) = 0;
    virtual void dispatch(parser&, async_rpcc<T>*, uint64_t) = 0;
    virtual void client_failure(async_rpcc<T>*) = 0;
};

};

