#ifndef RPC_PARSER_HH
#define RPC_PARSER_HH

#include "libev_loop.hh"
#include <ev++.h>
#include <stack>
#include <iostream>

namespace rpc {

// request and reply at RPC layer
struct rpc_header {
    uint32_t payload_length() const {
        return (len_ & 0x7fffffff) - sizeof(rpc_header);
    }
    bool request() const {
        return len_ & 0x80000000;
    }
    void set_payload_length(uint32_t payload_length, bool request) {
        len_ = (request << 31) | (payload_length + sizeof(rpc_header));
    }
  private:
    uint32_t len_;
  public:
    uint32_t seq_;
    uint32_t cid_; // client id
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
        check_unaligned_access();

	uint32_t need = sizeof(H);
	if (need <= len)
	    need += reinterpret_cast<H *>(buf)->payload_length();
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
    uint8_t *reqbody_;
    uint32_t reqlen_;
};

} // namespace rpc
#endif
