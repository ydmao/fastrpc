#pragma once

#include <vector>
#include <string>
#include "rpc_common/fdstream.hh"
#include "sync_rpc_transport.hh"

namespace rpc {

struct parser;
template <typename T>
struct async_rpcc;

template <typename T>
struct rpc_server_base {
    typedef typename T::sync_transport base_st_type;
    typedef buffered_sync_transport<nop_lock, base_st_type> buf_st_type;
    typedef sync_rpc_transport<buf_st_type> srt_type;

    virtual std::vector<int> proclist() const = 0;
    virtual void dispatch_sync(rpc_header&, std::string& body, srt_type*, uint64_t) = 0;
    virtual void dispatch(parser&, async_rpcc<T>*, uint64_t) = 0;
    virtual void client_failure(async_rpcc<T>*) = 0;
};

};

