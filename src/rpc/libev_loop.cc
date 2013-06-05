#include "libev_loop.hh"

namespace rpc {

#if (__clang__ && __APPLE__)
pthread_key_t nn_loop::tls_loop_key_;
#else
__thread nn_loop *nn_loop::tls_loop_ = NULL;
#endif

}

