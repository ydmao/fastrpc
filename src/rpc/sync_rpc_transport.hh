#pragma once
#include <string>
#include <mutex>
#include <sys/socket.h>
#include <atomic>
#include <memory>
#include "rpc_util/kvio.h"
#include "rpc_common/sock_helper.hh"
#include "rpc_util/buffered_rpc_stream.hh"
#include "rpc_parser.hh"
#include "sync_rpc.hh"
#include "tcp_provider.hh"

namespace rpc {

struct spinlock {
    spinlock() : lock_(ATOMIC_FLAG_INIT) {
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
struct direct_sync_transport : public rpc_istream_base, public rpc_ostream_base {
    typedef direct_sync_transport<T> sync_transport;
    typedef typename T::sync_transport child_transport;

    direct_sync_transport(child_transport* tp) : tp_(tp) {
	assert(tp_ != NULL);
    }
    ~direct_sync_transport() {
	delete tp_;
    }
    bool flush() {
	return true;
    }
    void shutdown() {
	flush();
	tp_->shutdown();
    }
    rpc_istream_base* in() {
	return this;
    }
    rpc_ostream_base* out() {
	return this;
    }
    bool read(void* buffer, size_t len) {
	ssize_t off = 0;
	while (off < len) {
	    int cc = tp_->read((char*)buffer + off, len - off);
	    if (cc <= 0) {
	     	if (errno == EINTR)
		    continue;
		else   
		    return false;
	    }
	    off += cc;
	}
	return true;
    }
    bool write(const void* buffer, size_t len) {
	ssize_t off = 0;
	while (off < len) {
	    int cc = tp_->write((char*)buffer + off, len - off);
	    if (cc <= 0) {
	     	if (errno == EINTR)
		    continue;
		else
		    return false;
	    }
	    off += cc;
	}
	return true;
    }
    static sync_transport* make_sync(int fd) {
	child_transport* tp = T::make_sync(fd);
	sync_transport* st = NULL;
	if (tp && (st = new sync_transport(tp)))
	    return st;
	if (tp)
	    delete tp;
	return NULL;
    }

  private:
    child_transport* tp_;
};


template <typename T>
struct buffered_sync_transport {
    typedef buffered_sync_transport<T> sync_transport;
    typedef typename T::sync_transport child_transport;

    buffered_sync_transport(child_transport* tp) 
	: tp_(tp), in_(tp_, 1<<20), out_(tp_, 1<<20) {
	assert(tp_ != NULL);
    }
    ~buffered_sync_transport() {
	delete tp_;
    }
    bool flush() {
	return out_.flush();
    }
    void shutdown() {
	flush();
	tp_->shutdown();
    }
    rpc_istream_base* in() {
	return &in_;
    }
    rpc_ostream_base* out() {
	return &out_;
    }

    static sync_transport* make_sync(int fd) {
	child_transport* tp = T::make_sync(fd);
	sync_transport* st = NULL;
	if (tp && (st = new sync_transport(tp)))
	    return st;
	if (tp)
	    delete tp;
	return NULL;
    }

  private:
    child_transport* tp_;
    buffered_rpc_istream<child_transport> in_;
    buffered_rpc_ostream<child_transport> out_;
};

template <typename T>
struct sync_rpc_transport : public spinlock {
    sync_rpc_transport() : p_(NULL), conn_(NULL) {
    }
    sync_rpc_transport(const std::string& h, int port) : conn_(NULL) {
	p_ = NULL;
	set_address(h, port, "0.0.0.0");
    }
    sync_rpc_transport(int fd) : conn_(NULL) {
	p_ = new onetime_tcpp(fd);
	connect();
    }
    ~sync_rpc_transport() {
	if (p_)
	    delete p_;
        disconnect();
    }
    explicit sync_rpc_transport(const sync_rpc_transport&) = delete;
    void operator=(const sync_rpc_transport&) = delete;
    void set_address(const std::string& h, int port, const std::string& local) {
	if (p_)
	    delete p_;
	p_ = new multi_tcpp(h.c_str(), local.c_str(), port);
    }
    bool connect() {
        if (conn_ == NULL) {
	    int fd = p_->connect();
            if (fd < 0)
                return false;
            rpc::common::sock_helper::make_nodelay(fd);
	    conn_ = T::make_sync(fd);
        }
        return conn_ != NULL;
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
    bool hard_read(void* buffer, size_t len) {
	if (!connected())
	    return false;
	return conn_->in()->read((char*)buffer, len);
    }
    template <typename M>
    bool read_message(M& m) {
	if (!connected())
	    return false;
	return m.ParseFromStream(*conn_->in());
    }
    bool safe_flush() {
	this->lock();
	bool ok = flush();
	this->unlock();
	return ok;
    }

    template <typename REPLY>
    bool safe_send_reply(const REPLY& r, const rpc::rpc_header& h, bool doflush) {
	this->lock();
	if (!connected()) {
	    this->unlock();
	    return false;
	}
	bool ok = rpc::send_reply(conn_->out(), h.mproc(), h.seq_,  r);
	if (ok && doflush)
	    ok = flush();
	this->unlock();
	return ok;
    }
    bool recv_header(rpc::rpc_header& h) {
	if (!connected())
   	    return false;
	return rpc::recv_header(conn_->in(), h);
    }
    template <typename M>
    bool recv_body(M& m) {
	if (!connected())
	    return false;
	return rpc::recv_body(conn_->in(), m);
    }
   
    template <typename M>
    bool read_reply(rpc::rpc_header& h, M& m) {
	if (!connected())
	    return false;
	return rpc::read_reply(conn_->in(), m, h);
    }
    template <typename PROC, typename M>
    bool send_request(PROC proc, uint32_t seq, const M& m, bool doflush) {
	if (!connected())
	    return false;
	bool ok = rpc::send_request(conn_->out(), proc, seq, m);
	if (ok && doflush)
	    conn_->flush();
	return ok;
    }
    template <typename PROC, typename REPLY>
    bool write_reply(PROC mproc, int seq, const REPLY& r, uint64_t /*latency*/) {
	if (!connected())
	    return false;
	bool ok = rpc::send_reply(conn_->out(), mproc, seq, r);
	if (ok && /*doflush*/false)
	    flush();
	return ok;
    }
    template <typename PROC, typename M, typename REPLY>
    bool sync_call(uint32_t seq, PROC proc, const M& req, REPLY& r) {
	if (!connected())
	    return false;
	return rpc::sync_call(conn_->out(), conn_->in(), seq, proc, req, r);
    }
    bool flush() {
	if (!connected())
	    return false;
	return conn_->flush();
    }
    // using cid or not depends on the subclass.
    void set_cid(int cid) {
	cid_ = cid;
    }
    int cid() {
	return cid_;
    }
  protected:
    tcp_provider* p_;
    typedef typename T::sync_transport child_transport;
    child_transport* conn_;
    int cid_;
};
}
