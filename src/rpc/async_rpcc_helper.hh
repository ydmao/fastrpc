#pragma once

#include "rpc_common/compiler.hh"
#include "libev_loop.hh"
#include "async_rpcc.hh"
#include "proto/fastrpc_proto.hh"

namespace rpc {

/** async_batched_rpcc promises that:
 *   - the callback of every request will be called once, even on failure.
 *   - on failure, the connection will first be disconnected, then all
 *     the outstanding requests will be called to complete with its eno
 *     set to RPCERR.
 */
class async_batched_rpcc : public rpc_handler {
  public:
    async_batched_rpcc(const char* rmt, const char* local, int rmtport, 
		       int cid, int w, bool force_connected = true)
	: rmt_(rmt), local_(local), rmtport_(rmtport), 
	  cid_(cid), loop_(nn_loop::get_tls_loop()), w_(w), deadcl_(NULL) {
	if (force_connected)
	    mandatory_assert(connect());
    }
    ~async_batched_rpcc() {
	if (cl_) {
	    delete cl_;
	    cl_ = NULL;
	}
    }
    bool connect() {
	mandatory_assert(safely_disconnected());
	int fd = rpc::common::sock_helper::connect(rmt_.c_str(), rmtport_, local_.c_str(), 0);
	if (fd < 0)
	    return false;
	cl_ = new async_rpcc(fd, this, cid_);
	return true;
    }
    bool drain() {
        mandatory_assert(loop_->enter() == 1,
                         "Don't call drain within a libev_loop!");
        bool work_done = cl_ && cl_->noutstanding();
        while (cl_ && cl_->noutstanding()) {
            mandatory_assert(!cl_->error());
            loop_->run_once();
        }
        loop_->leave();
        return work_done;
    }
    int noutstanding() const {
        return cl_ ? cl_->noutstanding() : 0;
    }
    bool safely_disconnected() const {
	return cl_ == NULL && deadcl_ == NULL;
    }
    void handle_rpc(async_rpcc*, parser&) {
	assert(0 && "rpc client can't process rpc requests");
    }
    // called before outstanding requests are completed with error
    void handle_client_failure(async_rpcc* c) {
	assert(cl_);
	assert(c);
	assert(c == cl_);
	deadcl_ = cl_;
	cl_ = NULL;
    }
    // called after outstanding requests on c are completed with error
    void handle_destroy(async_rpcc* c) {
	assert(!cl_ && deadcl_ == c);
	deadcl_ = NULL;
	delete c;
    }
    bool connected() const {
	return cl_ != NULL;
    }
    void shutdown() {
	if (cl_)
	    cl_->shutdown();
    }
    void flush() {
	if (cl_)
	    cl_->flush();
    }

  protected:
    void winctrl() {
	assert(w_ > 0);
	if (!cl_)
	    return;
        if (w_ == 1 || cl_->noutstanding() % (w_/2) == 0)
            cl_->flush();
        if (loop_->enter() == 1) {
            while (cl_ && cl_->noutstanding() >= w_) {
                mandatory_assert(!cl_->error());
                loop_->run_once();
            }
        }
        loop_->leave();
    }
    template <typename T>
    void call(T* g) {
	if (cl_) {
	    cl_->call(g);
	    winctrl();
	} else
	    g->process_connection_error();
    }

  private:
    std::string rmt_;
    std::string local_;
    int rmtport_;
    int cid_;

    nn_loop *loop_;
    int w_;
    async_rpcc* cl_;
    async_rpcc* deadcl_;
};

template <typename T>
class make_reply_helper {
  public:
    make_reply_helper(T &x)
	: x_(x) {
    }
    template <typename U>
    void operator()(const U &, const T &x) {
	x_ = x;
    }
  private:
    T &x_;
};

template <typename T> make_reply_helper<T> make_reply(T &x) {
    x.set_eno(app_param::ErrorCode::UNKNOWN);
    return make_reply_helper<T>(x);
}

#if 0
template <typename T, typename REQ, typename REPLY,
	  void (T::*method)(REPLY &)>
class make_unary_call_helper {
  public:
    make_unary_call_helper(T *obj) : obj_(obj) {
    }
    void operator()(REQ &, REPLY &x) {
	(obj_->*method)(x);
    }
  private:
    T *obj_;
};

template <typename T, typename REQ, typename REPLY,
	  void (T::*method)(REQ &, REPLY &)>
class make_binary_call_helper {
  public:
    make_binary_call_helper(T *obj) : obj_(obj) {
    }
    void operator()(REQ &x, REPLY &y) {
	(obj_->*method)(x, y);
    }
  private:
    T *obj_;
};
#endif

} // namespace rpc

