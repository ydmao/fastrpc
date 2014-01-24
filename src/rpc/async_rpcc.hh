#pragma once

#include "proto/fastrpc_proto.hh"
#include "async_tcpconn.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"
#include "gcrequest.hh"

namespace rpc {

struct async_rpcc;

struct rpc_handler {
    virtual void handle_rpc(async_rpcc *c, parser& p) = 0;
    virtual void handle_client_failure(async_rpcc *c) = 0;
    virtual void handle_destroy(async_rpcc* c) = 0;
};

class async_rpcc : public tcpconn_handler {
  public:
    async_rpcc(int fd, rpc_handler* rh, int cid,
	       proc_counters<app_param::nproc, true> *counts = 0);
    virtual ~async_rpcc();
    inline bool error() const {
	return c_.error();
    }
    inline int noutstanding() const {
	return noutstanding_;
    }
    inline void flush() {
	c_.flush(NULL);
    }
    inline void shutdown() {
	c_.shutdown();
    }

    void buffered_read(async_tcpconn *c, uint8_t *buf, uint32_t len);
    void handle_error(async_tcpconn *c, int the_errno);

    template <uint32_t PROC, typename CB>
    inline void call(gcrequest_iface<PROC, CB> *q);

    template <typename R>
    void request_received(uint32_t proc, const R& r);

    // complete an outstanding RPC.
    // on network failure, don't write anything;
    // otherwise, write reply to the underlying connection
    template <typename R>
    void execute(uint32_t proc, uint32_t seq, R& r, uint64_t latency);

    // write request. Connection must have no error
    template <typename M>
    inline void write_request(uint32_t proc, uint32_t seq, M& message);
    // write reply. Connection must have no error
    template <typename M>
    void write_reply(uint32_t proc, uint32_t seq, M& message);

    void* caller_arg_;

  private:
    async_tcpconn c_;
    gcrequest_base **waiting_;
    unsigned waiting_capmask_;
    uint32_t seq_;
    rpc_handler* rh_;
    int noutstanding_;
    proc_counters<app_param::nproc, true> *counts_;
    int cid_;

    void expand_waiting();
};

template <uint32_t PROC, typename CB>
inline void async_rpcc::call(gcrequest_iface<PROC, CB> *q) {
    ++seq_;
    q->seq_ = seq_;
    write_request(PROC, seq_, q->req());
    if (waiting_[seq_ & waiting_capmask_])
	expand_waiting();
    waiting_[seq_ & waiting_capmask_] = q;
}

template <typename R>
inline void async_rpcc::request_received(uint32_t proc, const R& r) {
    if (counts_)
	counts_->add(proc, count_recv_request, r.ByteSize());
}

template <typename R>
inline void async_rpcc::execute(uint32_t proc, uint32_t seq, R& r, uint64_t latency) {
    if (!error()) {
	if (counts_) {
	    counts_->add(proc, count_sent_reply, r.ByteSize());
	    counts_->add_latency(proc, latency);
	}
        write_reply(proc, seq, r);
    } else
        --noutstanding_;
}

template <typename M>
inline void async_rpcc::write_request(uint32_t proc, uint32_t seq, M& message) {
    check_unaligned_access();
    mandatory_assert(!error());
    uint32_t req_sz = message.ByteSize();
    uint8_t *x = c_.reserve(sizeof(rpc_header) + req_sz);
    rpc_header *h = reinterpret_cast<rpc_header *>(x);
    h->set_payload_length(req_sz, true);
    h->seq_ = seq;
    h->set_mproc(rpc_header::make_mproc(proc, 0));
    h->cid_ = cid_;
    message.SerializeToArray(x + sizeof(*h), req_sz);
    ++noutstanding_;
    if (counts_)
	counts_->add(proc, count_sent_request, sizeof(rpc_header) + req_sz);
}

template <typename M>
inline void async_rpcc::write_reply(uint32_t proc, uint32_t seq, M& message) {
    check_unaligned_access();
    --noutstanding_;
    mandatory_assert(!c_.error());
    uint32_t reply_sz = message.ByteSize();
    uint8_t *x = c_.reserve(sizeof(rpc_header) + reply_sz);
    rpc_header *h = reinterpret_cast<rpc_header *>(x);
    h->set_mproc(rpc_header::make_mproc(proc, 0)); // not needed, by better to shut valgrind up
    h->set_payload_length(reply_sz, false);
    h->seq_ = seq;
    h->cid_ = cid_;
    message.SerializeToArray(x + sizeof(*h), reply_sz);
    if (counts_)
	counts_->add(proc, count_sent_reply, sizeof(rpc_header) + reply_sz);
}

} // namespace rpc
