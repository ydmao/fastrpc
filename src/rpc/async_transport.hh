#pragma once

#include "rpc_common/sock_helper.hh"
#include "rpc_parser.hh"
#include "proc_counters.hh"
#include "proto/fastrpc_proto.hh"
#include "tcp.hh"
#include <errno.h>
#include <string.h>
#include <ev++.h>
#include <malloc.h>

#include <boost/intrusive/slist.hpp>
namespace bi = boost::intrusive;

namespace rpc {
template <typename T>
struct async_buffered_transport;

template <typename T>
struct transport_handler {
    virtual void buffered_read(async_buffered_transport<T>*, uint8_t* buf, uint32_t len) = 0;
    virtual void handle_error(async_buffered_transport<T>*, int the_errno) = 0;
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

template <typename T>
struct async_buffered_transport {
    typedef typename T::async_transport transport;

    async_buffered_transport(transport* tp, transport_handler<T> *ioh);
    ~async_buffered_transport();
    bool error() const {
        return tp_->ev_flags() == 0;
    }

    // input
    inline void advance(uint8_t *head, uint32_t size);

    // output
    inline uint8_t *reserve(uint32_t size);

    int flush(int* the_errno);

    void shutdown() {
        tp_->shutdown();
    }

  private:
    outbuf *in_;

    // active output buffers. 
    // head is write/flush end, tail is buffering end
    bi::slist<outbuf, bi::constant_time_size<false>, bi::cache_last<true> > out_active_;
    bi::slist<outbuf, bi::constant_time_size<false>, bi::cache_last<false> > out_free_;

    transport* tp_;
    transport_handler<T> *ioh_;

    bool event_handler(transport*, int e);
    int fill(int* the_errno);

    void resize_inbuf(uint32_t size);
    void refill_outbuf(uint32_t size);
};

template <typename T>
bool async_buffered_transport<T>::event_handler(transport*, int e) {
    int ok = 1;
    int the_errno = 0;
    if (e & ev::READ)
	ok = fill(&the_errno);
    if (ok == 2 || (ok > 0 && (e & ev::WRITE)))
	ok = flush(&the_errno);
    if (ok <= 0) {
	tp_->eselect(0);
	ioh_->handle_error(this, the_errno); // NB may delete `this`
	return true;
    }
    return false;
}

template <typename T>
void async_buffered_transport<T>::advance(uint8_t *head, uint32_t need_space) {
    assert(head >= in_->buf + in_->head && head <= in_->buf + in_->tail);
    in_->head = head - in_->buf;
    if (in_->head + need_space > in_->capacity)
	resize_inbuf(need_space);
}

template <typename T>
uint8_t *async_buffered_transport<T>::reserve(uint32_t size) {
    refill_outbuf(size);
    outbuf& h = out_active_.back();
    uint8_t *x = h.buf + h.tail;
    h.tail += size;
    if (size)
	tp_->eselect(ev::READ | ev::WRITE);
    return x;
}

template <typename T>
async_buffered_transport<T>::async_buffered_transport(transport* tp, transport_handler<T>* ioh)
    : in_(outbuf::make(1)), ioh_(ioh) {
    tp_ = tp;
    using std::placeholders::_1;
    using std::placeholders::_2;
    tp_->register_callback(
	    std::bind(&async_buffered_transport<T>::event_handler, this, _1, _2),
	    ev::READ);
}

template <typename T>
async_buffered_transport<T>::~async_buffered_transport() {
    if (tp_) {
        tp_->eselect(0);
        delete tp_;
    }
    outbuf::free(in_);
    while (!out_active_.empty()) {
	outbuf* x = &(out_active_.front());
	out_active_.pop_front();
	outbuf::free(x);
    }
    while (!out_free_.empty()) {
	outbuf* x = &(out_free_.front());
	out_free_.pop_front();
	outbuf::free(x);
    }
}

/** Postcondition: in_ has at least size bytes of space from head_ */
template <typename T>
void async_buffered_transport<T>::resize_inbuf(uint32_t size) {
    uint32_t h = in_->head;
    if (h + size > in_->capacity && size < in_->capacity / 2) {
	in_->tail -= h;
	memmove(in_->buf, in_->buf + h, in_->tail);
	in_->head = 0;
    } else if (h + size > in_->capacity) {
	if (size < in_->capacity * 2)
	    size = in_->capacity * 2 + 16 + sizeof(outbuf);
	outbuf *x = outbuf::make(size);
	x->tail = in_->tail - h;
	memcpy(x->buf, in_->buf + h, x->tail);
	outbuf::free(in_);
	in_ = x;
    }
}

/** Postcondition: out_active_.front() has at least size bytes of space */
template <typename T>
void async_buffered_transport<T>::refill_outbuf(uint32_t size) {
    if (!out_active_.empty()) {
        outbuf& x = out_active_.back();
	if (x.tail + size <= x.capacity)
	    return;
    }
    auto prev = out_free_.begin();
    for (auto it = out_free_.begin(); it != out_free_.end(); prev = it, ++it) {
	if (it->tail + size <= it->capacity) {
	    outbuf* x = it.operator->();
	    if (it == out_free_.begin())
		out_free_.pop_front();
	    else
	        out_free_.erase_after(prev);
	    out_active_.push_back(*x);
	    return;
	}
    }
    outbuf* x = outbuf::make(size);
    mandatory_assert(x);
    out_active_.push_back(*x);
}

template <typename T>
int async_buffered_transport<T>::fill(int* the_errno) {
    if (in_->head == in_->tail)
	in_->head = in_->tail = 0;

    uint32_t old_tail = in_->tail;
    while (in_->tail != in_->capacity) {
	ssize_t r = tp_->read(in_->buf + in_->tail, in_->capacity - in_->tail);
	if (r != 0 && r != -1) {
	    in_->tail += r;
            break;
        } else if (r == -1 && errno == EINTR)
	    continue;
	else if ((r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		 || (r == 0 && in_->tail != old_tail))
	    break;
	else {
	    if (the_errno)
	        *the_errno = errno;
	    return 0;
	}
    }

    if (old_tail != in_->tail) {
	int old_flags = tp_->ev_flags();
	ioh_->buffered_read(this, in_->buf + in_->head, in_->tail - in_->head);
	// if flags have changed, we have something to write
	return tp_->ev_flags() != old_flags ? 2 : 1;
    } else
	return 1;
}

template <typename T>
int async_buffered_transport<T>::flush(int* the_errno) {
    while (1) {
	if (out_active_.empty()) {
	    tp_->eselect(ev::READ);
	    return 1;
	}
	outbuf* x = &(out_active_.front());
	mandatory_assert(x->tail != x->head && x->tail);

	ssize_t w = tp_->write(x->buf + x->head, x->tail - x->head);
	if (w != 0 && w != -1) {
	    x->head += w;
	    if (x->head == x->tail) {
		x->head = x->tail = 0;
	        out_active_.pop_front();
		out_free_.push_front(*x);
	    }
	} else if (w == -1 && errno == EINTR)
	    /* do nothing */;
	else if (w == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    tp_->eselect(ev::READ | ev::WRITE);
	    return 1;
	} else {
	    if (the_errno)
	        *the_errno = errno;
	    return 0;
	}
    }
}

} // namespace rpc
