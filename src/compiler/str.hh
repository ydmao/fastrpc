#ifndef STR_HH
#define STR_HH 1

struct str {
    str(char* s, int len) {
        assign(s, len);
    }
    str() {
        assign(0, 0);
    }
    str(const str& x) {
        assign(x.s_, x.len_);
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
    char* s_;
    int len_;
};

#endif
