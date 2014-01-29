#pragma once

#include "proto/fastrpc_proto.hh"
#include "async_transport.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"
#include "gcrequest.hh"

namespace rpc {

struct async_rpcc;

struct rpc_handler {
    virtual void handle_rpc(async_rpcc *c, parser& p) = 0;
    virtual void handle_client_failure(async_rpcc *c) = 0;
    virtual void handle_post_failure(async_rpcc *c) = 0;
};

struct tcp_provider {
    virtual int connect() = 0;
    virtual ~tcp_provider() {}
};

struct multi_tcpp : public tcp_provider {
    multi_tcpp(const char* remote, const char* local, int remote_port)
	: rmt_(remote), local_(local), rmtport_(remote_port) {
    }
    int connect() {
	return rpc::common::sock_helper::connect(rmt_.c_str(), rmtport_, local_.c_str(), 0);
    }
  private:
    std::string rmt_;
    std::string local_;
    int rmtport_;
};

struct onetime_tcpp : public tcp_provider {
    onetime_tcpp(int fd) : fd_(fd) {}
    int connect() {
	int x = fd_;
	fd_ = -1;
	return x;
    }
    int fd_;
};

class async_rpcc : public transport_handler {
  public:
    async_rpcc(tcp_provider* tcpp,
	       rpc_handler* rh, int cid, bool force_connected,
	       proc_counters<app_param::nproc, true> *counts = 0);

    virtual ~async_rpcc();
    inline bool connect() {
	mandatory_assert(c_ == NULL);
	int fd = tcpp_->connect();
        if (fd < 0)
            return false;
        c_ = new async_transport(fd, this);
        return true;
    }
    inline bool connected() const {
	return c_ != NULL && !c_->error();
    }
    inline int noutstanding() const {
	return noutstanding_;
    }
    inline void flush() {
	c_->flush(NULL);
    }
    inline void shutdown() {
	c_->shutdown();
    }

    void buffered_read(async_transport *c, uint8_t *buf, uint32_t len);
    void handle_error(async_transport *c, int the_errno);

    // write reply. Connection may have error
    template <typename M>
    void write_reply(uint32_t proc, uint32_t seq, M& message, uint64_t latency);

    void* caller_arg_;

  protected:
    template <uint32_t PROC>
    inline void buffered_call(gcrequest_iface<PROC> *q);

  private:
    tcp_provider* tcpp_;
    async_transport* c_;
    gcrequest_base **waiting_;
    unsigned waiting_capmask_;
    uint32_t seq_;
    rpc_handler* rh_;
    int noutstanding_;
    proc_counters<app_param::nproc, true> *counts_;
    int cid_;

    void expand_waiting();

    // write request. Connection must have no error
    template <typename M>
    inline void write_request(uint32_t proc, uint32_t seq, M& message);
};

template <uint32_t PROC>
inline void async_rpcc::buffered_call(gcrequest_iface<PROC> *q) {
    if (!connected()) {
	q->process_connection_error();
	return;
    }
    ++seq_;
    q->seq_ = seq_;
    write_request(PROC, seq_, q->req());
    if (waiting_[seq_ & waiting_capmask_])
	expand_waiting();
    waiting_[seq_ & waiting_capmask_] = q;
}

template <typename M>
inline void async_rpcc::write_request(uint32_t proc, uint32_t seq, M& message) {
    check_unaligned_access();
    // write_request doesn't need to handle connection failure
    // the call method won't call write_request if not connected
    mandatory_assert(connected());
    uint32_t req_sz = message.ByteSize();
    uint8_t *x = c_->reserve(sizeof(rpc_header) + req_sz);
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
inline void async_rpcc::write_reply(uint32_t proc, uint32_t seq, M& message, uint64_t latency) {
    check_unaligned_access();
    --noutstanding_;
    // write_reply need to handle connection failure because the caller
    // doesn't know
    if (!connected())
	return;
    uint32_t reply_sz = message.ByteSize();
    uint8_t *x = c_->reserve(sizeof(rpc_header) + reply_sz);
    rpc_header *h = reinterpret_cast<rpc_header *>(x);
    h->set_mproc(rpc_header::make_mproc(proc, 0)); // not needed, by better to shut valgrind up
    h->set_payload_length(reply_sz, false);
    h->seq_ = seq;
    h->cid_ = cid_;
    message.SerializeToArray(x + sizeof(*h), reply_sz);
    if (counts_) {
	counts_->add(proc, count_sent_reply, sizeof(rpc_header) + reply_sz);
	counts_->add_latency(proc, latency);
    }
}

} // namespace rpc
