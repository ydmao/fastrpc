#pragma once
#include <string>
#include "rpc_common/sock_helper.hh"

namespace rpc {

struct tcp_provider {
    virtual int connect() = 0;
    virtual ~tcp_provider() {}
};

struct multi_tcpp : public tcp_provider {
    multi_tcpp(const char* remote, const char* local, int remote_port)
	: rmt_(remote), local_(local), rmtport_(remote_port) {
    }
    int connect() {
	return rpc::common::sock_helper::connect(rmt_.c_str(), rmtport_, local_.c_str(), 0);
    }
  private:
    std::string rmt_;
    std::string local_;
    int rmtport_;
};

struct onetime_tcpp : public tcp_provider {
    onetime_tcpp(int fd) : fd_(fd) {}
    int connect() {
	int x = fd_;
	fd_ = -1;
	return x;
    }
    int fd_;
};

}
