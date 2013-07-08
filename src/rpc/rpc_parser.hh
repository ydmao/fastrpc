#ifndef RPC_PARSER_HH
#define RPC_PARSER_HH

#include "libev_loop.hh"
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <ev++.h>
#include <stack>
#include <iostream>

using namespace google::protobuf::io;
using namespace google::protobuf;


namespace rpc {

// request and reply at RPC layer
struct rpc_header {
    uint32_t len() {
        return len_ >> 1;
    }
    bool request() {
        return len_ & 1;
    }
    void set_length(uint32_t len, bool request) {
        len_ = (len << 1) | request;
    }
    uint32_t seq_;
    uint32_t proc_; // used by request only
  private:
    uint32_t len_;
};

struct parser {
    parser(): reqbody_() {
    }
    void reset() {
        reqbody_ = 0;
    }
    // must be called in a loop until it returns false
    template <typename H, typename T>
    bool parse(uint8_t *&buf, uint32_t &len, T *c) {
	assert(!reqbody_);
        check_unaligned_access();

	uint32_t need = sizeof(H);
	if (need <= len)
	    need += reinterpret_cast<H *>(buf)->len();
	if (need > len) {
	    c->advance(buf, need);
	    return false;
	}

	reqbody_ = buf + sizeof(H);
	reqlen_ = need - sizeof(H);

	buf += need;
	len -= need;
	return true;
    }

    template <typename H>
    inline H *header() const {
	assert(reqbody_);
	return reinterpret_cast<H *>(reqbody_ - sizeof(H));
    }

    template <typename T>
    inline void parse_message(T &m) {
        m.ParseFromArray(reqbody_, reqlen_);
    }

  private:
    uint8_t *reqbody_;
    uint32_t reqlen_;
};

} // namespace rpc
#endif
