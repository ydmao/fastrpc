#ifndef RPC_SERVER_HH
#define RPC_SERVER_HH
#include <ev++.h>
#include "grequest.hh"
#include "libev_loop.hh"
#include "rpc_common/sock_helper.hh"

namespace rpc {

template <typename S>
struct async_rpc_server : public rpc_handler {
    typedef async_rpc_server<S> self;

    async_rpc_server(int port, S* s) : s_(s) {
        listener_ = rpc::common::sock_helper::listen(port, 100);
        rpc::common::sock_helper::make_nodelay(listener_);
        listener_ev_.set<self, &self::accept_one>(this);
        listener_ev_.start(listener_, ev::READ);
    }

    ~async_rpc_server() {
        if (listener_ >= 0)
	    close(listener_);
    }

    async_rpcc* register_rpcc(int fd) {
        async_rpcc *c = new async_rpcc(fd, nn_loop::get_loop(), this, &opcount_);
        mandatory_assert(c);
        return c;
    }

    void accept_one(ev::io &e, int flags) {
        int s1 = rpc::common::sock_helper::accept(listener_);
        assert(s1 >= 0);
        register_rpcc(s1);
    }

    proc_counters<app_param::nproc, true>& get_opcount() {
        return opcount_;
    }

    void handle_rpc(async_rpcc *c, parser& p) {
#define HANDLE_RPC(proc, REQ, REPLY) \
    case app_param::ProcNumber::proc: \
        handle_rpc(c, p, h, &S::proc); \
        break;
        rpc_header *h = p.header<rpc_header>();
        switch (h->proc_) {
        RPC_FOR_EACH_CLIENT_MESSAGE(HANDLE_RPC)
        RPC_FOR_EACH_INTERCONNECT_MESSAGE(HANDLE_RPC)
        default:
            assert(0);
        }
    }
  private:
    template <uint32_t PROC>
    void handle_rpc(async_rpcc *c, parser &p, rpc_header *h,
                    void (S::*f)(grequest<PROC> *q, async_rpcc* c, uint64_t now)) {
        grequest_remote<PROC> *q = new grequest_remote<PROC>(h->seq_, c);
        p.parse_message(q->req_);
        opcount_.add(PROC, count_recv_request, sizeof(*h) + h->len_, 0);
        (s_->*f)(q, c, tstamp());
    }

    proc_counters<app_param::nproc, true> opcount_;
    int listener_;
    ev::io listener_ev_;
    S* s_;
};

}

#endif
