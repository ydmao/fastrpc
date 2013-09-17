#include "async_tcpconn.hh"
#include "libev_loop.hh"

namespace rpc {

inline async_tcpconn::outbuf *async_tcpconn::make_outbuf(uint32_t size) {
    if (size < 65520 - sizeof(outbuf))
	size = 65520 - sizeof(outbuf);
    outbuf *x = reinterpret_cast<outbuf *>(malloc(size + sizeof(outbuf)));
    x->capacity = size;
    x->head = x->tail = 0;
    x->next = 0;
    return x;
}

inline void async_tcpconn::free_outbuf(outbuf *x) {
    free((void *) x);
}


async_tcpconn::async_tcpconn(nn_loop *loop, int fd, tcpconn_handler *ioh, int cid,
			     proc_counters<app_param::nproc, true> *counts)
    : in_(make_outbuf(1)), out_writehead_(), out_bufhead_(), out_tail_(),
      ev_(nn_loop::get_loop(loop)->ev_loop()), ev_flags_(0), fd_(fd), cid_(cid), ioh_(ioh),
      counts_(counts), noutstanding_(0) {
    rpc::common::sock_helper::make_nodelay(fd_);
    rpc::common::sock_helper::make_nonblock(fd_);
    refill_outbuf(1);
    ev_.set<async_tcpconn, &async_tcpconn::event_handler>(this);
    eselect(ev::READ);
}

async_tcpconn::~async_tcpconn() {
    eselect(0);
    close(fd_);
    free_outbuf(in_);
    while (outbuf *x = out_writehead_) {
	out_writehead_ = x->next;
	free_outbuf(x);
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
	outbuf *x = make_outbuf(size);
	x->tail = in_->tail - h;
	memcpy(x->buf, in_->buf + h, x->tail);
	free_outbuf(in_);
	in_ = x;
    }
}

/** Postcondition: out_bufhead_ has at least size bytes of space */
void async_tcpconn::refill_outbuf(uint32_t size) {
    outbuf **pprev = &out_bufhead_;
    while (*pprev && (*pprev)->tail + size > (*pprev)->capacity) {
	outbuf *x = *pprev;
	if (x->head == 0 && x != out_bufhead_) {
	    *pprev = x->next;
	    free_outbuf(x);
	} else
	    pprev = &x->next;
    }
    if (*pprev)
	out_bufhead_ = *pprev;
    else {
	*pprev = out_bufhead_ = out_tail_ = make_outbuf(size);
	if (!out_writehead_)
	    out_writehead_ = out_tail_;
    }
}

int async_tcpconn::fill() {
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
	else
	    return 0;
    }

    if (old_tail != in_->tail) {
	int old_flags = ev_flags_;
	ioh_->buffered_read(this, in_->buf + in_->head, in_->tail - in_->head);
	// if flags have changed, we have something to write
	return ev_flags_ != old_flags ? 2 : 1;
    } else
	return 1;
}

int async_tcpconn::flush() {
    while (1) {
	outbuf *x = out_writehead_;

	// if out_writehead_ is complete (has no more data to write),
	// either exit or move to next buffer
	if (x->head == x->tail)
	    x->head = x->tail = 0;
	if (x->tail == 0 && x == out_bufhead_) {
	    eselect(ev::READ);
	    return 1;
	} else if (x->tail == 0) {
	    out_tail_->next = x;
	    out_writehead_ = x->next;
	    x->next = 0;
	    out_tail_ = x;
	    continue;
	}

	ssize_t w = write(fd_, x->buf + x->head, x->tail - x->head);
	if (w != 0 && w != -1)
	    x->head += w;
	else if (w == -1 && errno == EINTR)
	    /* do nothing */;
	else if (w == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    eselect(ev::READ | ev::WRITE);
	    return 1;
	} else
	    return 0;
    }
}

} // namespace rpc
