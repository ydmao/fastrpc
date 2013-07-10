#ifndef RPC_SERVER_HH
#define RPC_SERVER_HH
#include <ev++.h>
#include "grequest.hh"
#include "libev_loop.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"
#include "rpc/rpc_server_base.hh"

namespace rpc {

struct async_rpc_server : public rpc_handler {
    typedef async_rpc_server self;

    async_rpc_server(int port) : listener_ev_(nn_loop::get_tls_loop()->ev_loop()) {
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

    void register_service(rpc_server_base* s) {
        auto pl = s->proclist();
        for (auto p : pl) {
            if (p >= sp_.size())
                sp_.resize(p + 1);
            mandatory_assert(sp_[p] == NULL);
            sp_[p] = s;
        }
    }

    void handle_rpc(async_rpcc *c, parser& p) {
        rpc_header *h = p.header<rpc_header>();
        auto s = sp_[h->proc_];
        mandatory_assert(s);
        s->dispatch(p, c, rpc::common::tstamp());
    }

  private:
    std::vector<rpc_server_base*> sp_; // service provider
    proc_counters<app_param::nproc, true> opcount_;
    int listener_;
    ev::io listener_ev_;
};

}

#endif
