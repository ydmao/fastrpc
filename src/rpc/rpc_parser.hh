#ifndef RPC_PARSER_HH
#define RPC_PARSER_HH

#include "../../config.h"
#include "libev_loop.hh"
#include <google/protobuf/message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <ev++.h>
#include <stack>

using namespace google::protobuf::io;
using namespace google::protobuf;


namespace rpc {

// request and reply at RPC layer
struct rpc_header {
    uint32_t len_;
    bool     request_;
    uint32_t seq_;
    uint32_t proc_; // used by request only
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
	static_assert(HAVE_UNALIGNED_ACCESS, "requires unaligned access");

	uint32_t need = sizeof(H);
	if (need <= len)
	    need += reinterpret_cast<H *>(buf)->len_;
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
        ArrayInputStream in(reqbody_, reqlen_);
        m.ParseFromZeroCopyStream(&in);
        // @in must be exhausted and m must be fully initialized
        assert(m.IsInitialized() && in.ByteCount() == reqlen_);
    }

  private:
    uint8_t *reqbody_;
    uint32_t reqlen_;
};

} // namespace rpc
#endif
