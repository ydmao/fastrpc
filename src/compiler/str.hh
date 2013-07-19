#pragma once

#include <string>
#include <string.h>
#include <assert.h>

namespace refcomp {

struct str {
    str(char* s, int len) {
        assign(s, len);
    }
    str(const char* s, int len) {
        assign(s, len);
    }
    str() {
        assign(0, 0);
    }
    str(const str& x) {
        assign(x.s_, x.len_);
    }
    explicit str(const std::string& s) {
        assign(s.data(), s.length());
    }

    void assign(const char* s, int len) {
        s_ = const_cast<char*>(s);
        len_ = len;
    }
    size_t length() const {
        return len_;
    }
    const char* data() const {
        return s_;
    }
    size_t size() const {
        return len_;
    }
    const char* c_str() const {
        assert(s_[len_] == 0);
        return s_;
    }
    bool operator==(const str& x) {
        return len_ == x.len_ && memcmp(s_, x.s_, len_) == 0;
    }
    bool operator!=(const str& x) {
        return !((*this) == x);
    }

    char* s_;
    int len_;
};

};
