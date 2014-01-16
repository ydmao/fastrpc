#ifndef RPC_SERVER_HH
#define RPC_SERVER_HH
#include <ev++.h>
#include <thread>
#include <list>
#include "grequest.hh"
#include "libev_loop.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"
#include "rpc_common/fdstream.hh"
#include "rpc/rpc_server_base.hh"

namespace rpc {

struct async_rpc_server : public rpc_handler {
    typedef async_rpc_server self;

    async_rpc_server(int port, const std::string& h) : listener_ev_(nn_loop::get_tls_loop()->ev_loop()) {
        listener_ = rpc::common::sock_helper::listen(h, port, 100);
        rpc::common::sock_helper::make_nodelay(listener_);
        listener_ev_.set<self, &self::accept_one>(this);
        listener_ev_.start(listener_, ev::READ);
    }

    ~async_rpc_server() {
        if (listener_ >= 0)
	    close(listener_);
    }

    async_rpcc* register_rpcc(int fd) {
        async_rpcc *c = new async_rpcc(fd, nn_loop::get_loop(), this, random(), &opcount_);
        mandatory_assert(c);
        clients_.push_back(c);
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
            if (p >= (int)sp_.size())
                sp_.resize(p + 1);
            mandatory_assert(sp_[p] == NULL);
            sp_[p] = s;
        }
        unique_.push_back(s);
    }

    void handle_rpc(async_rpcc *c, parser& p) {
        rpc_header *h = p.header<rpc_header>();
        auto s = sp_[h->proc()];
        mandatory_assert(s);
        s->dispatch(p, c, rpc::common::tstamp());
    }

    void handle_client_failure(async_rpcc* c) {
        for (auto s: unique_)
            s->client_failure(c);
        for (auto it = clients_.begin(); it != clients_.end(); ++it)
            if (*it == c) {
                clients_.erase(it);
                return;
            }
        assert(0 && "connection not found? Impossible!");
    }
    std::list<async_rpcc*>& all_rpcc() {
        return clients_;
    }

  private:
    std::list<async_rpcc*> clients_;
    std::vector<rpc_server_base*> unique_; // service provider
    std::vector<rpc_server_base*> sp_; // service provider
    proc_counters<app_param::nproc, true> opcount_;
    int listener_;
    ev::io listener_ev_;
};

struct threaded_rpc_server {
    threaded_rpc_server(int port) {
        listener_ = rpc::common::sock_helper::listen(port, 100);
        rpc::common::sock_helper::make_nodelay(listener_);
    }

    ~threaded_rpc_server() {
        if (listener_ >= 0)
	    close(listener_);
    }

    void serve() {
        while (true) {
            int s1 = accept(listener_, NULL, NULL);
            rpc::common::sock_helper::make_nodelay(s1);
            auto t = new std::thread([&]{ process_client(s1); });
            t->detach();
        }
    }

    proc_counters<app_param::nproc, true>& get_opcount() {
        return opcount_;
    }

    void register_service(rpc_server_base* s) {
        auto pl = s->proclist();
        for (auto p : pl) {
            if (p >= (int)sp_.size())
                sp_.resize(p + 1);
            mandatory_assert(sp_[p] == NULL);
            sp_[p] = s;
        }
    }

    void process_client(int fd) {
        fdstream sm(fd);
        rpc_header h;
        std::string body;
        while (true) {
            if (!sm.read((char*)&h, sizeof(h)))
                return;
            body.resize(h.payload_length());
            if (!sm.read(&body[0], h.payload_length()))
                return;
            auto s = sp_[h.proc()];
            mandatory_assert(s);
            s->dispatch_sync(h, body, &sm, rpc::common::tstamp());
        }
    }

  private:
    std::vector<rpc_server_base*> sp_; // service provider
    proc_counters<app_param::nproc, true> opcount_;
    int listener_;
};


}

#endif
