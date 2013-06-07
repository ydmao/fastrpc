#ifndef RPC_SERVER_HH
#define RPC_SERVER_HH
#include <ev++.h>
#include "grequest.hh"
#include "libev_loop.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"

namespace rpc {

template <typename S, int SERVICE = 0>
struct async_rpc_server : public rpc_handler {
    typedef async_rpc_server<S, SERVICE> self;

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
#define HANDLE_RPC(service, proc, REQ, REPLY) \
    case app_param::ProcNumber::proc: \
        handle_rpc(c, p, h, method_getter_##proc<service == SERVICE || SERVICE == 0, S>::get()); \
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
#define METHOD_GETTER(service, proc, REQ, REPLY) \
    template <bool, typename T> \
    struct method_getter_##proc { \
        static constexpr void* get() { return 0; } \
    }; \
    template <typename T> \
    struct method_getter_##proc<true, T> { \
        typedef void (T::*method_type)(grequest<app_param::ProcNumber::proc>*, async_rpcc*, uint64_t); \
        static constexpr method_type get() { return &T::proc; } \
    };

    RPC_FOR_EACH_CLIENT_MESSAGE(METHOD_GETTER)
    RPC_FOR_EACH_INTERCONNECT_MESSAGE(METHOD_GETTER)

    template <uint32_t PROC>
    void handle_rpc(async_rpcc *c, parser &p, rpc_header *h,
                    void (S::*f)(grequest<PROC> *q, async_rpcc* c, uint64_t now)) {
        grequest_remote<PROC> *q = new grequest_remote<PROC>(h->seq_, c);
        p.parse_message(q->req_);
        opcount_.add(PROC, count_recv_request, sizeof(*h) + h->len_, 0);
        (s_->*f)(q, c, rpc::common::tstamp());
    }
    void handle_rpc(async_rpcc *c, parser &p, rpc_header *h, void*) {
        mandatory_assert(0 && "should never reach here");
    }

    proc_counters<app_param::nproc, true> opcount_;
    int listener_;
    ev::io listener_ev_;
    S* s_;
};

}

#endif
