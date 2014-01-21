#pragma once

#include "proto/fastrpc_proto.hh"
#include "async_tcpconn.hh"
#include "gcrequest.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"

namespace rpc {

struct async_rpcc;
struct rpc_handler {
    virtual void handle_rpc(async_rpcc *c, parser& p) = 0;
    virtual void handle_client_failure(async_rpcc *c) = 0;
    virtual void handle_destroy(async_rpcc* c) = 0;
};

class async_rpcc : public tcpconn_handler {
  public:
    async_rpcc(const char *host, int port, nn_loop *loop, rpc_handler* rh, int cid,
	       proc_counters<app_param::nproc, true> *counts = 0);
    async_rpcc(int fd, nn_loop *loop, rpc_handler* rh, int cid,
	       proc_counters<app_param::nproc, true> *counts = 0);
    virtual ~async_rpcc();
    inline bool error() const {
	return c_.error();
    }
    inline int winsize() const {
	return c_.noutstanding_;
    }
    inline async_tcpconn& connection() {
        return c_;
    }

    template <uint32_t PROC, typename CB>
    inline void call(gcrequest<PROC, CB> *q);
    void buffered_read(async_tcpconn *c, uint8_t *buf, uint32_t len);
    void handle_error(async_tcpconn *c, int the_errno);

    template <typename R>
    void execute(uint32_t proc, uint32_t seq, R& r) {
        if (!error())
            c_.write_reply(proc, seq, r);
        else
            c_.complete_onerror();
    }

  private:
    async_tcpconn c_;
    gcrequest_base **waiting_;
    unsigned waiting_capmask_;
    uint32_t seq_;
    rpc_handler* rh_;

    void expand_waiting();
};

template <uint32_t PROC, typename CB>
inline void async_rpcc::call(gcrequest<PROC, CB> *q) {
    ++seq_;
    q->seq_ = seq_;
    c_.write_request(PROC, seq_, q->req_);
    if (waiting_[seq_ & waiting_capmask_])
	expand_waiting();
    waiting_[seq_ & waiting_capmask_] = q;
}

} // namespace rpc
