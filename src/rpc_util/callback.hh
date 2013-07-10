#ifndef CALLBACK_HH
#define CALLBACK_HH

#include "proto/fastrpc_proto.hh"

namespace rpc {

class check_eno {
  public:
    check_eno() : expected_(app_param::ErrorCode::OK) {}
    check_eno(int eno) : expected_(eno) {}

    template <typename T>
    void operator()(const T &x) {
	mandatory_assert(x.eno() == expected_);
    }
    template <typename T>
    void operator()(T &x) {
	mandatory_assert(x.eno() == expected_);
    }
    template <typename T, typename U>
    void operator()(const T &, const U &x) {
	mandatory_assert(x.eno() == expected_);
    }
    template <typename T, typename U>
    void operator()(T &, U &x) {
	mandatory_assert(x.eno() == expected_);
    }
  private:
    int expected_;
};

template <typename CB_TYPE>
struct shared_cb {
    shared_cb(CB_TYPE *cb) : cb_(cb) {
    }
    template <typename REQ, typename REPLY>
    void operator()(REQ &req, REPLY &reply) {
        cb_->done_and_try_delete(req, reply);
    }
  private:
    CB_TYPE *cb_;
};

template <typename CB_TYPE>
inline shared_cb<CB_TYPE> use_shared_cb(CB_TYPE *cb) {
    return shared_cb<CB_TYPE>(cb);
}

struct check_eno_barrier {
    check_eno_barrier() : n_(0) {}
    template <typename REQ, typename REPLY>
    void done_and_try_delete(REQ &, REPLY &reply) {
        --n_;
        if (n_ == 0)
            delete this;
    }
    void inc() {
        ++n_;
    }
  private:
    int n_;
};

}
#endif
