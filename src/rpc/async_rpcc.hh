#pragma once

#include "proto/fastrpc_proto.hh"
#include "async_transport.hh"
#include "rpc_common/sock_helper.hh"
#include "rpc_common/util.hh"
#include "rpc_common/compiler.hh"
#include "gcrequest.hh"
#include "tcp_provider.hh"

namespace rpc {

template <typename T>
struct async_rpcc;

template <typename T>
struct rpc_handler {
    virtual void handle_rpc(async_rpcc<T> *c, parser& p) = 0;
    virtual void handle_client_failure(async_rpcc<T> *c) = 0;
    virtual void handle_post_failure(async_rpcc<T> *c) = 0;
};

template <typename T>
class async_rpcc : public transport_handler<T> {
  public:
    async_rpcc(tcp_provider* tcpp,
	       rpc_handler<T>* rh, bool force_connected,
	       proc_counters<app_param::nproc, true> *counts = 0);

    virtual ~async_rpcc();
    inline bool connect() {
	mandatory_assert(c_ == NULL);
	int fd = tcpp_->connect();
        if (fd < 0) {
	    fprintf(stderr, "async_rpcc: failed to connect\n");
            return false;
	}
	typedef typename T::async_transport transport;
	transport* tp = T::template make<transport>(fd);
	if (tp)
            c_ = new async_buffered_transport<T>(tp, this);
        return c_ != NULL;
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

    void buffered_read(async_buffered_transport<T> *c, uint8_t *buf, uint32_t len);
    void handle_error(async_buffered_transport<T> *c, int the_errno);

    // write reply. Connection may have error
    template <typename M>
    void write_reply(uint32_t proc, uint32_t seq, M& message, uint64_t latency);

    void* caller_arg_;

  protected:
    template <uint32_t PROC>
    inline void buffered_call(gcrequest_iface<PROC> *q);

  private:
    tcp_provider* tcpp_;
    async_buffered_transport<T>* c_;
    gcrequest_base **waiting_;
    unsigned waiting_capmask_;
    uint32_t seq_;
    rpc_handler<T>* rh_;
    int noutstanding_;
    proc_counters<app_param::nproc, true> *counts_;

    void expand_waiting();

    // write request. Connection must have no error
    template <typename M>
    inline void write_request(uint32_t proc, uint32_t seq, M& message);
};

template <typename T>
template <uint32_t PROC>
void async_rpcc<T>::buffered_call(gcrequest_iface<PROC> *q) {
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

template <typename T>
template <typename M>
inline void async_rpcc<T>::write_request(uint32_t proc, uint32_t seq, M& message) {
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
    message.SerializeToArray(x + sizeof(*h), req_sz);
    ++noutstanding_;
    if (counts_)
	counts_->add(proc, count_sent_request, sizeof(rpc_header) + req_sz);
}

template <typename T>
template <typename M>
void async_rpcc<T>::write_reply(uint32_t proc, uint32_t seq, M& message, uint64_t latency) {
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
    message.SerializeToArray(x + sizeof(*h), reply_sz);
    if (counts_) {
	counts_->add(proc, count_sent_reply, sizeof(rpc_header) + reply_sz);
	counts_->add_latency(proc, latency);
    }
}

template <typename T>
async_rpcc<T>::async_rpcc(tcp_provider* tcpp, 
		       rpc_handler<T>* rh, bool force_connected,
		       proc_counters<app_param::nproc, true> *counts)
    : caller_arg_(), tcpp_(tcpp), c_(NULL),
      waiting_(new gcrequest_base *[1024]), waiting_capmask_(1023), 
      seq_(random() / 2), rh_(rh), noutstanding_(0), counts_(counts) {
    bzero(waiting_, sizeof(gcrequest_base *) * 1024);
    if (force_connected)
	mandatory_assert(connect());
}

template <typename T>
async_rpcc<T>::~async_rpcc() {
    mandatory_assert(!noutstanding_);
    delete[] waiting_;
    delete tcpp_;
    if (c_)
        delete c_;
}

template <typename T>
void async_rpcc<T>::buffered_read(async_buffered_transport<T> *, uint8_t *buf, uint32_t len) {
    parser p;
    while (p.parse<rpc_header>(buf, len, c_)) {
	rpc_header *rhdr = p.header<rpc_header>();
        if (!rhdr->request()) {
            // Find the rpc request with sequence number @reply_hdr_.seq
	    gcrequest_base *q = waiting_[rhdr->seq_ & waiting_capmask_];
            mandatory_assert(q && q->seq_ == rhdr->seq_ && "RPC reply but no waiting call");
	    waiting_[rhdr->seq_ & waiting_capmask_] = 0;
	    --noutstanding_;
	    gcrequest_base::last_server_latency_ = rhdr->latency();
	    // update counts_ before process_reply, which will delete itself
	    if (counts_) {
	        counts_->add(q->proc(), count_recv_reply,
                             sizeof(rpc_header) + p.header<rpc_header>()->payload_length());
		counts_->add_latency(q->proc(), rpc::common::tstamp() - q->start_at());
	    }
	    q->process_reply(p);
        } else {
            ++noutstanding_;
            mandatory_assert(rh_);
            rh_->handle_rpc(this, p);
        }
        p.reset();
    }
}

template <typename T>
void async_rpcc<T>::handle_error(async_buffered_transport<T> *c, int the_errno) {
    mandatory_assert(c == c_);
    c_ = NULL;
    if (rh_)
        rh_->handle_client_failure(this);
    if (noutstanding_ != 0)
	fprintf(stderr, "error: %d rpcs outstanding (%s)\n",
		noutstanding_, strerror(the_errno));
    unsigned ncap = waiting_capmask_ + 1;
    for (int i = 0; i < ncap; ++i) {
        gcrequest_base* q = waiting_[i];
        if (q) {
	    --noutstanding_;
            q->process_connection_error();
	}
    }
    delete c;
    if (rh_)
	rh_->handle_post_failure(this);
}

template <typename T>
void async_rpcc<T>::expand_waiting() {
    do {
	unsigned ncapmask = waiting_capmask_ * 2 + 1;
	gcrequest_base **nw = new gcrequest_base *[ncapmask + 1];
	memset(nw, 0, sizeof(gcrequest_base *) * (ncapmask + 1));
	for (unsigned i = 0; i <= waiting_capmask_; ++i)
	    if (gcrequest_base *rr = waiting_[i])
		nw[rr->seq_ & ncapmask] = rr;
	delete[] waiting_;
	waiting_ = nw;
	waiting_capmask_ = ncapmask;
    } while (waiting_[seq_ & waiting_capmask_]);
}


} // namespace rpc
