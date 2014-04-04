#pragma once
#include <functional>
#include <ev++.h>
#include "rpc_common/sock_helper.hh"
#include "rpc_util/tcpfds.hh"

namespace rpc {

struct tcpnet;

struct socket_wrapper {
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
    friend class tcpnet;
    socket_wrapper(int fd) : fd_(fd) {
        rpc::common::sock_helper::make_nodelay(fd_);
	assert(fd >= 0);
    }

    int fd_;
};

// async_tcp
struct async_tcp : public socket_wrapper {
    typedef std::function<void(async_tcp*, int)> event_handler_type;

    void register_callback(event_handler_type cb, int flags) {
        ev_.set(nn_loop::get_tls_loop()->ev_loop());
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
  protected:
    friend class tcpnet;
    async_tcp(int fd) : socket_wrapper(fd), ev_flags_(0) {
        rpc::common::sock_helper::make_nonblock(fd_);
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

struct tcpnet {
    template <typename T>
    using select_provider = epoll_tcpfds<T>;

    typedef socket_wrapper sync_transport;
    typedef async_tcp async_transport;
    template <typename T>
    static T* make(int fd) {
	T* c = new T(fd);
	if (!c)
	    close(fd);
	return c;
    }
    static sync_transport* make_sync(int fd) {
	return make<sync_transport>(fd);
    }
    static async_transport* make_async(int fd) {
	return make<async_transport>(fd);
    }
    static void set_poll_interval(int) {
	// nothing to do. We don't use TCP in polling mode
    }
};

}
