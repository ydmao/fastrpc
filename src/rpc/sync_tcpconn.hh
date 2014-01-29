#pragma once
#include <string>
#include <mutex>
#include <sys/socket.h>
#include <atomic>
#include "rpc_util/kvio.h"

namespace rpc {

struct spinlock_object {
    spinlock_object() : lock_(ATOMIC_FLAG_INIT) {
    }
    void lock() {
        while (lock_.test_and_set(std::memory_order_acquire))
            ;
    }
    void unlock() {
        lock_.clear(std::memory_order_release);
    }
  private:
    std::atomic_flag lock_;
};

template <typename T>
struct buffered_tcpconn : public spinlock_object, public std::enable_shared_from_this<buffered_tcpconn<T> > {
    buffered_tcpconn(int fd) 
	: fd_(fd), in_(fd, 65536), out_(fd, 65536) {
	assert(fd_ >= 0);
    }
    ~buffered_tcpconn() {
	close(fd_);
    }
    bool safe_flush() {
	lock();
	bool ok = flush();
	unlock();
	return ok;
    }
    bool flush() {
	return out_.flush();
    }
    void shutdown() {
	flush();
	::shutdown(fd_, SHUT_RDWR);
    }
    kvin& in() {
	return in_;
    }
    template <typename M>
    bool read_reply(rpc::rpc_header& h, M& m) {
	return rpc::read_reply(&in_, m, h);
    }
    template <typename M>
    bool send_request(uint32_t proc, uint32_t seq, uint32_t cid, const M& m) {
	return rpc::send_request(&out_, proc, seq, cid, m);
    }
    template <typename REPLY>
    void safe_send_reply(const REPLY& reply, const rpc::rpc_header& h, bool doflush) {
	lock();
	rpc::send_reply(&out_, h.mproc(), h.seq_, h.cid_, reply);
	if (doflush)
	    flush();
	unlock();
    }
    T context_;

  private:
    int fd_;
    kvin in_;
    kvout out_;
};

struct buffered_tcpconn_client : public spinlock_object {
    buffered_tcpconn_client() : buffered_tcpconn_client("", 0) {
    }
    buffered_tcpconn_client(const std::string& h, int port) 
        : h_(h), port_(port), conn_(NULL), cid_(0) {
    }
    ~buffered_tcpconn_client() {
        disconnect();
    }
    explicit buffered_tcpconn_client(const buffered_tcpconn_client&) = delete;
    void operator=(const buffered_tcpconn_client&) = delete;
    void init(const std::string& h, int port, const std::string& localhost, int localport) {
        h_ = h;
        port_ = port;
        localhost_ = localhost;
	localport_ = localport;
    }
    bool connect() {
        if (conn_ == NULL) {
            int fd = rpc::common::sock_helper::connect(h_.c_str(), port_, localhost_.c_str(), localport_);
            if (fd < 0)
                return false;
            rpc::common::sock_helper::make_nodelay(fd);
	    conn_ = new buffered_tcpconn<int>(fd);
        }
        return true;
    }
    bool connected() const {
        return conn_;
    }
    void disconnect() {
        if (conn_ != NULL) {
	    delete conn_;
	    conn_ = NULL;
	}
    }
    void shutdown() {
	if (conn_ != NULL)
	    conn_->shutdown();
    }
    template <typename M>
    bool read_reply(rpc::rpc_header& h, M& m) {
	assert(connected());
	return conn_->read_reply(h, m);
    }
    template <typename M>
    bool send_request(uint32_t proc, uint32_t seq, const M& m) {
	assert(connected());
	return conn_->send_request(proc, seq, cid_, m);
    }
    bool flush() {
	assert(connected());
	return conn_->flush();
    }
    void set_rpc_clientid(int cid) {
	cid_ = cid;
    }
  protected:
    std::string h_;
    int port_;
    std::string localhost_;
    int localport_;
    buffered_tcpconn<int>* conn_;
    int cid_;
};
}
