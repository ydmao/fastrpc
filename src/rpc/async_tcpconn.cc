#include "async_tcpconn.hh"
#include "libev_loop.hh"

namespace rpc {

async_tcpconn::async_tcpconn(int fd, tcpconn_handler *ioh, int cid,
			     proc_counters<app_param::nproc, true> *counts)
    : in_(outbuf::make(1)),
      ev_(nn_loop::get_loop()->ev_loop()), ev_flags_(0), fd_(fd), cid_(cid), ioh_(ioh),
      counts_(counts), noutstanding_(0), caller_arg_(0) {
    assert(fd_ >= 0);
    rpc::common::sock_helper::make_nodelay(fd_);
    rpc::common::sock_helper::make_nonblock(fd_);
    ev_.set<async_tcpconn, &async_tcpconn::event_handler>(this);
    eselect(ev::READ);
}

async_tcpconn::~async_tcpconn() {
    eselect(0);
    close(fd_);
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

void async_tcpconn::hard_eselect(int flags) {
    if (ev_flags_)
	ev_.stop();
    if ((ev_flags_ = flags))
	ev_.start(fd_, ev_flags_);
}

/** Postcondition: in_ has at least size bytes of space from head_ */
void async_tcpconn::resize_inbuf(uint32_t size) {
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
void async_tcpconn::refill_outbuf(uint32_t size) {
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

int async_tcpconn::fill(int* the_errno) {
    if (in_->head == in_->tail)
	in_->head = in_->tail = 0;

    uint32_t old_tail = in_->tail;
    while (in_->tail != in_->capacity) {
	ssize_t r = read(fd_, in_->buf + in_->tail, in_->capacity - in_->tail);
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
	int old_flags = ev_flags_;
	ioh_->buffered_read(this, in_->buf + in_->head, in_->tail - in_->head);
	// if flags have changed, we have something to write
	return ev_flags_ != old_flags ? 2 : 1;
    } else
	return 1;
}

int async_tcpconn::flush(int* the_errno) {
    while (1) {
	if (out_active_.empty()) {
	    eselect(ev::READ);
	    return 1;
	}
	outbuf* x = &(out_active_.front());
	mandatory_assert(x->tail != x->head && x->tail);

	ssize_t w = write(fd_, x->buf + x->head, x->tail - x->head);
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
	    eselect(ev::READ | ev::WRITE);
	    return 1;
	} else {
	    if (the_errno)
	        *the_errno = errno;
	    return 0;
	}
    }
}

} // namespace rpc
