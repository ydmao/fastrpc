#pragma once

#include "libev_loop.hh"
#include <ev++.h>

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
    static uint32_t make_mproc(uint32_t p, uint32_t latency) {
	assert(p < 256 && latency <(1<<24));
	return (p<<24) | latency;
    }
    void set_mproc(uint32_t p) {
	proc_ = p;
    }
    //mproc: multiplexed proc
    uint32_t mproc() const {
	return proc_;
    }
    uint32_t proc() const {
	return proc_ >> 24;
    }
    uint32_t latency() const {
	return proc_ & 0xffffff;
    }
  private:
    uint32_t len_;
  public:
    uint32_t seq_;
    uint32_t cid_; // client id
  private:
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
