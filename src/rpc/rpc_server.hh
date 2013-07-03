#ifndef RPC_SERVER_HH
#define RPC_SERVER_HH
#include <ev++.h>
#include "grequest.hh"
#include "libev_loop.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"

namespace rpc {

#define CALL_IF_HAS_HANDLER(__SERVICE, __PROC, REQ, REPLY) \
template <bool, bool> \
struct call_if_has_##__PROC { \
    template <typename... ARGS> \
    static void call(ARGS...) { \
        mandatory_assert(0 && "RPC " #__PROC " doesn't belong to service " #__SERVICE); \
    } \
}; \
template <> \
struct call_if_has_##__PROC<true, false> { \
    static constexpr uint32_t PROC = app_param::ProcNumber::__PROC; \
    template <typename RPCS> \
    static void call(async_rpcc* c, parser& p, rpc_header* h, RPCS* rpcs) { \
        grequest_remote<PROC, false> *q = new grequest_remote<PROC, false>(h->seq_, c); \
        p.parse_message(q->req_); \
        rpcs->get_opcount().add(PROC, count_recv_request, sizeof(*h) + h->len(), 0); \
        rpcs->server()->__PROC(q, c, rpc::common::tstamp()); \
    } \
}; \
template <> \
struct call_if_has_##__PROC<true, true> { \
    static constexpr uint32_t PROC = app_param::ProcNumber::__PROC; \
    template <typename RPCS> \
    static void call(async_rpcc* c, parser& p, rpc_header* h, RPCS* rpcs) { \
        grequest_remote<PROC, true> q(h->seq_, c); \
        p.parse_message(q.req_); \
        rpcs->get_opcount().add(PROC, count_recv_request, sizeof(*h) + h->len(), 0); \
        rpcs->server()->nb_##__PROC(q, c, rpc::common::tstamp()); \
    } \
};


RPC_FOR_EACH_CLIENT_MESSAGE(CALL_IF_HAS_HANDLER)
RPC_FOR_EACH_INTERCONNECT_MESSAGE(CALL_IF_HAS_HANDLER)

template <typename S, int SERVICE = 0, bool NON_BLOCKING = false>
struct async_rpc_server : public rpc_handler {
    typedef async_rpc_server<S, SERVICE, NON_BLOCKING> self;

    async_rpc_server(int port, S* s) : s_(s), listener_ev_(nn_loop::get_tls_loop()->ev_loop()) {
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
#define HANDLE_RPC(__SERVICE, __PROC, REQ, REPLY) \
    case app_param::ProcNumber::__PROC: \
        call_if_has_##__PROC<__SERVICE == SERVICE || SERVICE == 0, NON_BLOCKING>::call(c, p, h, this); \
        break;

        rpc_header *h = p.header<rpc_header>();
        switch (h->proc_) {
        RPC_FOR_EACH_CLIENT_MESSAGE(HANDLE_RPC)
        RPC_FOR_EACH_INTERCONNECT_MESSAGE(HANDLE_RPC)
        default:
            assert(0);
        }
    }
    S* server() {
        return s_;
    }

  private:
    proc_counters<app_param::nproc, true> opcount_;
    int listener_;
    ev::io listener_ev_;
    S* s_;
};

}

#endif
