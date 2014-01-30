#pragma once
#include <string>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <vector>
#include "compiler/str.hh"
#include "rpc_stream_base.hh"

namespace rpc {

struct string_rpc_istream : public rpc_istream_base {
    string_rpc_istream(const void* s, size_t size): 
        s_(reinterpret_cast<const uint8_t*>(s)), e_(s_ + size) {
    }
    bool read(void* b, size_t n) {
        if (s_ + n > e_)
            return false;
        memcpy(b, s_, n);
        s_ += n;
        return true;
    }
    bool read_inline(const char** d, int n) {
        if (s_ + n > e_)
            return false;
        *d = (const char*)s_;
        s_ += n;
        return true;
    }
    
  private:
    const uint8_t* s_;
    const uint8_t* e_;
};

struct string_rpc_ostream : public rpc::rpc_ostream_base {
    string_rpc_ostream(void* s, size_t len) : s_(reinterpret_cast<uint8_t*>(s)), len_(len) {
	e_ = s_ + len_;
    }
    bool write(const void* b, size_t n) {
	if (s_ + n > e_)
	    return false;
        memcpy(s_, b, n);
        s_ += n;
	return true;
    }
    bool flush() {
	return true;
    }
  private:
    uint8_t* s_;
    size_t len_;
    uint8_t* e_;
};

}
