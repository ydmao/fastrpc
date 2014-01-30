#pragma once
#include <string>
#include <vector>
#include "compiler/str.hh"

namespace rpc {

struct rpc_istream_base {
    virtual ~rpc_istream_base() {}

    template<class T>
    bool r(T &x) {
        static_assert(std::is_pod<T>::value, "Must be POD");
        return read((void*)&x, sizeof(x));
    }
    bool r(std::string &s, long long max) {
        int len;
        if(!r(len)) return false;
        if(max >= 0 && len > max) return false;
        s.resize(len);
        if(!read(&s[0], len)) return false;
        return true;
    }
    bool r(std::string& v) {
	return r(v, -1);
    }
    bool r(std::vector<std::string> &v, long long total) {
        v.clear();
        int n;
        if(!r(n)) return false;
        long long sum = 0;
        for(int i = 0; i < n; i++){
            std::string s;
            if(!r(s, total - sum)) return false;
            v.push_back(s);
            sum += s.size();
        }
        return true;
    }
    template <typename T>
    bool r(std::vector<T>& v) {
        int len;
        if (!r(len))
            return false;
        v.resize(len);
        for (int i = 0; i < len; ++i)
            if (!r(v[i]))
                return false;
        return true;
    }
    bool r(refcomp::str& v) {
	int len;
	if (!r(len))
	    return false;
	v.len_ = len;
        return read_inline((const char**)&v.s_, len);
    }
    virtual bool read(void* buffer, size_t len) = 0;
    virtual bool read_inline(const char** d, int n) {
	assert(0 && "read_inline is only supported by string_rpc_istream");
    }
};

struct rpc_ostream_base {
    virtual ~rpc_ostream_base() {}
    template <typename T>
    static size_t bytecount(const T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        return sizeof(T);
    }
    static size_t bytecount(const std::string& v) {
        return sizeof(int) + v.length();
    }
    static size_t bytecount(const refcomp::str& v) {
        return sizeof(int) + v.length();
    }
    template <typename T>
    static size_t bytecount(const std::vector<T>& v) {
        size_t size = sizeof(int);
        for (size_t i = 0; i < v.size(); ++i)
            size += bytecount(v[i]);
        return size;
    }

    template <typename T>
    bool w(const T& v) {
        static_assert(std::is_pod<T>::value, "T must be POD type");
        return write(&v, sizeof(v));
    }
    bool w(const std::string& v) {
        return w(refcomp::str(v.data(), v.length()));
    }
    bool w(const refcomp::str& v) {
        const int len = v.length();
        if (!w(len))
	    return false;
        return write(v.s_, v.len_);
    }
    template <typename T>
    bool w(const std::vector<T>& v) {
        const int len = v.size();
        if (!w(len))
	    return false;
        for (int i = 0; i < int(v.size()); ++i)
            if (!w(v[i]))
		return false;
	return true;
    }

    virtual bool write(const void* buffer, size_t) = 0;
    virtual bool flush() = 0;
};

}
