#ifndef ASYNC_RPCC_HELPER
#define ASYNC_RPCC_HELPER 1

#include "rpc_common/compiler.hh"
#include "libev_loop.hh"
#include "async_rpcc.hh"
#include "proto/fastrpc_proto.hh"

namespace rpc {

class async_batched_rpcc {
  public:
    async_batched_rpcc(async_rpcc* cl, int w)
	: cl_(cl), loop_(nn_loop::get_tls_loop()), w_(w) {
    }
    bool drain() {
        mandatory_assert(loop_->enter() == 1,
                         "Don't call drain within a libev_loop!");
        bool work_done = cl_->winsize();
        while (cl_->winsize()) {
            mandatory_assert(!cl_->error());
            loop_->run_once();
        }
        loop_->leave();
        return work_done;
    }
    int noutstanding() const {
        return cl_->winsize();
    }

  protected:
    void winctrl() {
        if (w_ < 0)
            return;
        if (cl_->winsize() % (w_/2) == 0)
            cl_->connection().flush();
        if (loop_->enter() == 1) {
            while (cl_->winsize() >= w_) {
                mandatory_assert(!cl_->error());
                loop_->run_once();
            }
        }
        loop_->leave();
    }

    async_rpcc* cl_;
    nn_loop *loop_;
    int w_;
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

} // namespace rpc

#endif
