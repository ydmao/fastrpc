#ifndef STREAM_PARSER_HH
#define STREAM_PARSER_HH

#include <string>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <vector>
#include "compiler/str.hh"

namespace refcomp {

struct stream_parser {
    stream_parser(const void* s, size_t size): s_(reinterpret_cast<const uint8_t*>(s)), e_(s_ + size) {
    }
    template <typename T>
    void parse(T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        const uint8_t* ns = s_ + sizeof(T);
        assert(ns <= e_);
        v = *reinterpret_cast<const T*>(s_);
        s_ = ns;
    }
    void parse(std::string& v) {
        size_t len;
        parse(len);
        auto ns = s_ + len;
        assert(ns <= e_);
        v.assign(reinterpret_cast<const char*>(s_), len);
        s_ = ns;
    }
    void parse(refcomp::str& v) {
        size_t len;
        parse(len);
        auto ns = s_ + len;
        assert(ns <= e_);
        v.assign(reinterpret_cast<const char*>(s_), len);
        s_ = ns;
    }
    template <typename T>
    void parse(std::vector<T>& v) {
        size_t len;
        parse(len);
        v.resize(len);
        for (int i = 0; i < len; ++i)
            parse(v[i]);
    }
  private:
    const uint8_t* s_;
    const uint8_t* e_;
};

struct stream_unparser {
    stream_unparser(void* s) : s_(reinterpret_cast<uint8_t*>(s)), len_(0) {
    }
    size_t length() const {
        return len_;
    }
    template <typename T>
    static size_t bytecount(const T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        return sizeof(T);
    }
    static size_t bytecount(const std::string& v) {
        return sizeof(size_t) + v.length();
    }
    static size_t bytecount(const str& v) {
        return sizeof(size_t) + v.length();
    }
    template <typename T>
    static size_t bytecount(const std::vector<T>& v) {
        size_t size = sizeof(size_t);
        for (size_t i = 0; i < v.size(); ++i)
            size += bytecount(v[i]);
        return size;
    }

    template <typename T>
    void unparse(const T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        *reinterpret_cast<T*>(s_) = v;
        len_ += sizeof(T);
        s_ += sizeof(T);
    }
    void unparse(const std::string& v) {
        const size_t len = v.length();
        unparse(len);
        memcpy(s_, v.data(), len);
        s_ += len;
    }
    void unparse(const refcomp::str& v) {
        const size_t len = v.length();
        unparse(len);
        memcpy(s_, v.data(), len);
        s_ += len;
    }
    template <typename T>
    void unparse(const std::vector<T>& v) {
        const size_t len = v.size();
        unparse(len);
        for (int i = 0; i < v.size(); ++i)
            unparse(v[i]);
    }
  private:
    uint8_t* s_;
    size_t len_;
};

};

#endif
