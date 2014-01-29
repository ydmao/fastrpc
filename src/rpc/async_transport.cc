#include "async_transport.hh"
#include "libev_loop.hh"

namespace rpc {

async_buffered_transport::async_buffered_transport(int fd, transport_handler *ioh)
    : in_(outbuf::make(1)), ioh_(ioh) {
    tp_ = tcp_transport::make_async(fd);
    using std::placeholders::_1;
    using std::placeholders::_2;

    tp_->register_loop(
	    nn_loop::get_loop()->ev_loop(),
	    std::bind(&async_buffered_transport::event_handler, this, _1, _2),
	    ev::READ);
}

async_buffered_transport::~async_buffered_transport() {
    tp_->eselect(0);
    delete tp_;
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
void async_buffered_transport::resize_inbuf(uint32_t size) {
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
void async_buffered_transport::refill_outbuf(uint32_t size) {
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

int async_buffered_transport::fill(int* the_errno) {
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

int async_buffered_transport::flush(int* the_errno) {
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
