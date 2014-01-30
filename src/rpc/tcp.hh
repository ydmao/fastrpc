#pragma once
#include <functional>
#include <ev++.h>
#include "rpc_common/sock_helper.hh"

namespace rpc {

struct socket_wrapper {
    socket_wrapper(int fd) : fd_(fd) {
        rpc::common::sock_helper::make_nodelay(fd_);
	assert(fd >= 0);
    }
    ~socket_wrapper() {
	::close(fd_);
    }
    ssize_t read(void* buffer, size_t len) {
	return ::read(fd_, buffer, len);
    }
    ssize_t write(const void* buffer, size_t len) {
	return ::write(fd_, buffer, len);
    }
    void shutdown() {
	::shutdown(fd_, SHUT_RDWR);
    }
  protected:
    int fd_;
};

// async_tcp
struct async_tcp : public socket_wrapper {
    async_tcp(int fd) : socket_wrapper(fd), ev_flags_(0) {
        rpc::common::sock_helper::make_nonblock(fd_);
    }
    typedef std::function<void(async_tcp*, int)> event_handler_type;

    void register_loop(ev::loop_ref loop, event_handler_type cb, int flags) {
        ev_.set(loop);
        ev_.set<async_tcp, &async_tcp::event_handler>(this);
	cb_ = cb;
	assert(flags);
        eselect(flags);
    }
    void eselect(int flags) {
	if (flags != ev_flags_)
	    hard_eselect(flags);
    }
    int ev_flags() const {
	return ev_flags_;
    }
  private:
    void event_handler(ev::io&, int e) {
        int flags = e & ev_flags_;
        if (flags)
	    cb_(this, flags);
    }
    void hard_eselect(int flags) {
        if (ev_flags_)
	    ev_.stop();
        if ((ev_flags_ = flags))
   	    ev_.start(fd_, ev_flags_);
    }

    event_handler_type cb_;
    ev::io ev_;
    int ev_flags_;
};

struct tcp_transport {
    typedef socket_wrapper sync_transport;
    typedef async_tcp async_transport;
};

}
