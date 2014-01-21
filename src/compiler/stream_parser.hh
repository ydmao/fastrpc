#pragma once

#include <string>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <vector>
#include "compiler/str.hh"

namespace refcomp {

struct simple_istream {
    simple_istream(const void* s, size_t size): 
        s_(reinterpret_cast<const uint8_t*>(s)), e_(s_ + size) {
    }
    bool read(char* b, size_t n) {
        if (s_ + n > e_)
            return false;
        memcpy(b, s_, n);
        s_ += n;
        return true;
    }
    bool skip(const char** d, int n) {
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

struct simple_ostream {
    simple_ostream(void* s) : s_(reinterpret_cast<uint8_t*>(s)), len_(0) {
    }
    void write(const char* b, size_t n) {
        memcpy(s_, b, n);
        s_ += n;
    }
  private:
    uint8_t* s_;
    size_t len_;
};

template <typename T>
bool read_inline(T& s, const char** d, int n);

template <>
inline bool read_inline<simple_istream>(simple_istream& s, const char** d, int n) {
    return s.skip(d, n);
}

template <typename T>
inline bool read_inline(T& s, const char** d, int n) {
    assert(0 && "read_inline is only supported by simple_istream");
}

template <typename S>
struct stream_parser {
    stream_parser(S& s) : s_(s) {
    }
    S& s_;
    template <typename T>
    bool parse(T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        return s_.read((char*)&v, sizeof(v));
    }
    bool parse(std::string& v) {
        int len;
        if (!parse(len))
            return false;
        v.resize(len);
        return s_.read(&v[0], len);
    }
    bool parse(refcomp::str& v) {
        int len;
        if (!parse(len))
            return false;
        v.len_ = len;
        return read_inline<S>(s_, (const char**)&v.s_, len);
    }
    template <typename T>
    bool parse(std::vector<T>& v) {
        return parse(v, [&](T& t){ return this->parse(t); });
    }
    template <typename T, typename F>
    bool parse(std::vector<T>& v, F f) {
        int len;
        if (!parse(len))
            return false;
        v.resize(len);
        for (int i = 0; i < len; ++i)
            if (!f(v[i]))
                return false;
        return true;
    }
};

template <typename S>
struct stream_unparser {
    stream_unparser(S& s): s_(s) {
    }
    S& s_;
    template <typename T>
    static size_t bytecount(const T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        return sizeof(T);
    }
    static size_t bytecount(const std::string& v) {
        return sizeof(int) + v.length();
    }
    static size_t bytecount(const str& v) {
        return sizeof(int) + v.length();
    }
    template <typename T>
    static size_t bytecount(const std::vector<T>& v) {
        return bytecount(v, [&](const T& t){return bytecount(t);});
    }
    template <typename T, typename F>
    static size_t bytecount(const std::vector<T>& v, F f) {
        size_t size = sizeof(int);
        for (size_t i = 0; i < v.size(); ++i)
            size += f(v[i]);
        return size;
    }



    template <typename T>
    void unparse(const T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        s_.write((const char*)&v, sizeof(v));
    }
    void unparse(const std::string& v) {
        unparse(refcomp::str(v.data(), v.length()));
    }
    void unparse(const refcomp::str& v) {
        const int len = v.length();
        unparse(len);
        s_.write(v.s_, v.len_);
    }
    template <typename T>
    void unparse(const std::vector<T>& v) {
        unparse(v, [&](const T& t) { this->unparse(t); });
    }

    template <typename T, typename F>
    void unparse(const std::vector<T>& v, F f) {
        const int len = v.size();
        unparse(len);
        for (int i = 0; i < int(v.size()); ++i)
            f(v[i]);
    }
};

};

