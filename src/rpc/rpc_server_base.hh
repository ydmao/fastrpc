#pragma once

#include <vector>
#include <string>
#include "sync_rpc_transport.hh"

namespace rpc {

struct parser;
template <typename T>
struct async_rpcc;

template <typename T>
struct rpc_server_base {
    typedef buffered_sync_transport<T> buf_st_type;
    typedef sync_rpc_transport<buf_st_type> srt_type;

    virtual std::vector<int> proclist() const = 0;
    virtual void dispatch_sync(rpc_header&, std::string& body, srt_type*, uint64_t) = 0;
    virtual void dispatch(parser&, async_rpcc<T>*, uint64_t) = 0;
    virtual void client_failure(async_rpcc<T>*) = 0;
};

};

