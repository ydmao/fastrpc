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
    str(const char* s) {
        assign(s, strlen(s));
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
    bool operator==(const str& x) const {
        return compare(x) == 0;
    }
    bool operator!=(const str& x) const {
        return compare(x);
    }
    bool operator>(const str& x) const {
        return compare(x) > 0;
    }
    bool operator>=(const str& x) const {
        return compare(x) >= 0;
    }
    bool operator<(const str& x) const {
        return compare(x) < 0;
    }
    bool operator<=(const str& x) const {
        return compare(x) <= 0;
    }
    int compare(const str& x) const {
        int lencmp = len_ - x.len_;
        int c = memcmp(s_, x.s_, lencmp > 0 ? x.len_ : len_);
        if (c)
            return c;
        return lencmp;
    }

    char* s_;
    int len_;
};

};
