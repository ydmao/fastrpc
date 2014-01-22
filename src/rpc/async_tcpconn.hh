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

struct tcpconn_handler {
    virtual void buffered_read(async_tcpconn* c, uint8_t* buf, uint32_t len) = 0;
    virtual void handle_error(async_tcpconn* c, int the_errno) = 0;
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
    async_tcpconn(int fd, tcpconn_handler *ioh);
    ~async_tcpconn();
    bool error() const {
        return ev_flags_ == 0;
    }

    // input
    inline void advance(uint8_t *head, uint32_t size);

    // output
    inline uint8_t *reserve(uint32_t size);

    int flush(int* the_errno);

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

} // namespace rpc
