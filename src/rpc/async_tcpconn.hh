#pragma once

#include "rpc_common/sock_helper.hh"
#include "rpc_parser.hh"
#include "proc_counters.hh"
#include "proto/fastrpc_proto.hh"
#include <errno.h>
#include <string.h>
#include <ev++.h>
#include <malloc.h>

#include <boost/intrusive/slist.hpp>
namespace bi = boost::intrusive;

namespace rpc {
struct async_tcpconn;
struct nn_loop;

struct tcpconn_handler {
    virtual void buffered_read(async_tcpconn *c, uint8_t *buf, uint32_t len) = 0;
    virtual void handle_error(async_tcpconn *c, int the_errno) = 0;
};

struct outbuf : public bi::slist_base_hook<> {
    static outbuf* make(uint32_t size) {
        if (size < 65520 - sizeof(outbuf))
	    size = 65520 - sizeof(outbuf);
        outbuf *x = new (malloc(size + sizeof(outbuf))) outbuf;
        x->capacity = size;
        x->head = x->tail = 0;
        return x;
    }
    static void free(outbuf* x) {
	delete x;
    }
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint8_t buf[0];
  private:
    outbuf() {}
};

struct async_tcpconn {
    async_tcpconn(int fd, tcpconn_handler *ioh, int cid,
                  proc_counters<app_param::nproc, true> *counts);
    ~async_tcpconn();
    bool error() const {
        return ev_flags_ == 0;
    }

    // input
    inline void advance(uint8_t *head, uint32_t size);

    // output
    inline uint8_t *reserve(uint32_t size);

    // helpers
    template <typename M>
    inline void write_request(uint32_t proc, uint32_t seq, M &message);
    template <typename M>
    inline void write_reply(uint32_t proc, uint32_t seq, M &message);

    int flush(int* the_errno);

    void complete_onerror() {
        --noutstanding_;
    }
    
    void* caller_arg_;
    void shutdown() {
        ::shutdown(SHUT_RDWR, fd_);
    }

  private:
    outbuf *in_;

    // active output buffers. 
    // head is write/flush end, tail is buffering end
    bi::slist<outbuf, bi::constant_time_size<false>, bi::cache_last<true> > out_active_;
    bi::slist<outbuf, bi::constant_time_size<false>, bi::cache_last<false> > out_free_;

    ev::io ev_;
    int ev_flags_;
    int fd_;
    tcpconn_handler *ioh_;
    int cid_;
  public:
    // XXX should be in a different abstraction
    proc_counters<app_param::nproc, true> *counts_;
    int noutstanding_;

  private:
    inline void eselect(int flags);
    void hard_eselect(int flags);

    inline void event_handler(ev::io &w, int e);
    int fill(int* the_errno);

    void resize_inbuf(uint32_t size);
    void refill_outbuf(uint32_t size);
};

inline void async_tcpconn::eselect(int flags) {
    if (ev_flags_ != flags)
	hard_eselect(flags);
}

inline void async_tcpconn::event_handler(ev::io &, int e) {
    int ok = 1;
    int the_errno = 0;
    if (e & ev::READ)
	ok = fill(&the_errno);
    if (ok == 2 || (ok > 0 && (e & ev::WRITE)))
	ok = flush(&the_errno);
    if (ok <= 0) {
	eselect(0);
	ioh_->handle_error(this, the_errno); // NB may delete `this`
    }
}

inline void async_tcpconn::advance(uint8_t *head, uint32_t need_space) {
    assert(head >= in_->buf + in_->head && head <= in_->buf + in_->tail);
    in_->head = head - in_->buf;
    if (in_->head + need_space > in_->capacity)
	resize_inbuf(need_space);
}

inline uint8_t *async_tcpconn::reserve(uint32_t size) {
    refill_outbuf(size);
    outbuf& h = out_active_.back();
    uint8_t *x = h.buf + h.tail;
    h.tail += size;
    if (size)
	eselect(ev::READ | ev::WRITE);
    return x;
}

template <typename M>
inline void async_tcpconn::write_request(uint32_t proc, uint32_t seq, M &message) {
    check_unaligned_access();
    mandatory_assert(!error());
    uint32_t req_sz = message.ByteSize();
    uint8_t *x = reserve(sizeof(rpc_header) + req_sz);
    rpc_header *h = reinterpret_cast<rpc_header *>(x);
    h->set_payload_length(req_sz, true);
    h->seq_ = seq;
    h->set_mproc(rpc_header::make_mproc(proc, 0));
    h->cid_ = cid_;
    message.SerializeToArray(x + sizeof(*h), req_sz);
    ++noutstanding_;
    if (counts_)
	counts_->add(proc, count_sent_request, sizeof(rpc_header) + req_sz, 0);
}

template <typename M>
inline void async_tcpconn::write_reply(uint32_t proc, uint32_t seq, M &message) {
    check_unaligned_access();
    --noutstanding_;
    mandatory_assert(!error());
    uint32_t reply_sz = message.ByteSize();
    uint8_t *x = reserve(sizeof(rpc_header) + reply_sz);
    rpc_header *h = reinterpret_cast<rpc_header *>(x);
    h->set_mproc(rpc_header::make_mproc(proc, 0)); // not needed, by better to shut valgrind up
    h->set_payload_length(reply_sz, false);
    h->seq_ = seq;
    h->cid_ = cid_;
    message.SerializeToArray(x + sizeof(*h), reply_sz);
    if (counts_)
	counts_->add(proc, count_sent_reply, sizeof(rpc_header) + reply_sz, 0);
}

} // namespace rpc
