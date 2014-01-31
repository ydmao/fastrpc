#pragma once
#include <string>
#include <vector>
#include "compiler/str.hh"

namespace rpc {

struct rpc_istream_base {
    virtual ~rpc_istream_base() {}

    template <typename T>
    bool r(T& v);
    bool r(std::string &s, long long max);
    bool r(std::vector<std::string> &v, long long total);

    virtual bool read(void* buffer, size_t len) = 0;
    virtual bool read_inline(const char** d, int n) {
	assert(0 && "read_inline is only supported by string_rpc_istream");
    }
};

struct rpc_ostream_base {
    virtual ~rpc_ostream_base() {}
    template <typename T>
    static size_t bytecount(const T& v);
    template <typename T>
    bool w(const T& v);
    virtual bool write(const void* buffer, size_t) = 0;
    virtual bool flush() = 0;
};

template <typename T>
struct stream_helper;

template <typename T, bool POD>
struct stream_helper_impl;

template <typename T>
struct stream_helper_impl<T, true> {
    static size_t bytecount(const T& v) {
        return sizeof(T);
    }
    static bool r(T &x, rpc_istream_base* s) {
        return s->read((void*)&x, sizeof(x));
    }
    static bool w(const T& v, rpc_ostream_base* s) {
        return s->write((const void*)&v, sizeof(v));
    }
};

template <typename T>
struct stream_helper_impl<T, false> {
    static size_t bytecount(const T& v) {
	return v.ByteSize();
    }
    static bool r(T &x, rpc_istream_base* s) {
	return x.ParseFromStream(*s);
    }
    static bool w(const T& v, rpc_ostream_base* s) {
	return v.SerializeToStream(*s);
    }
};

template <typename T>
struct stream_helper {
    static size_t bytecount(const T& v) {
	return stream_helper_impl<T, std::is_pod<T>::value>::bytecount(v);
    }
    static bool r(T &v, rpc_istream_base* s) {
	return stream_helper_impl<T, std::is_pod<T>::value>::r(v, s);
    }
    static bool w(const T& v, rpc_ostream_base* s) {
	return stream_helper_impl<T, std::is_pod<T>::value>::w(v, s);
    }
};

template <>
struct stream_helper<std::string> {
    static size_t bytecount(const std::string& v) {
        return sizeof(int) + v.length();
    }
    static bool r(std::string& v, rpc_istream_base* s) {
	return r(v, -1, s);
    }
    static bool r(std::string& v, long long max, rpc_istream_base* s) {
        int len;
        if(!s->r(len)) return false;
        if(max >= 0 && len > max) return false;
        v.resize(len);
        if(!s->read(&v[0], len)) return false;
        return true;
    }
    static bool w(const std::string& v, rpc_ostream_base* s) {
        return s->w(refcomp::str(v.data(), v.length()));
    }
};

template <>
struct stream_helper<refcomp::str> {
    static size_t bytecount(const refcomp::str& v) {
        return sizeof(int) + v.length();
    }
    static bool r(refcomp::str& v, rpc_istream_base* s) {
	int len;
	if (!s->r(len))
	    return false;
	v.len_ = len;
        return s->read_inline((const char**)&v.s_, len);
    }
    static bool w(const refcomp::str& v, rpc_ostream_base* s) {
        const int len = v.length();
        if (!s->w(len))
	    return false;
        return s->write(v.s_, v.len_);
    }
};

template <typename T>
struct stream_helper<std::vector<T> > {
    static size_t bytecount(const std::vector<T>& v) {
        size_t size = sizeof(int);
        for (size_t i = 0; i < v.size(); ++i)
            size += stream_helper<T>::bytecount(v[i]);
        return size;
    }
    static bool r(std::vector<T>& v, rpc_istream_base* s) {
        int len;
        if (!s->r(len))
            return false;
        v.resize(len);
        for (int i = 0; i < len; ++i)
            if (!s->r(v[i]))
                return false;
        return true;
    }
    static bool w(const std::vector<T>& v, rpc_ostream_base* s) {
        const int len = v.size();
        if (!s->w(len))
	    return false;
        for (int i = 0; i < int(v.size()); ++i)
            if (!s->w(v[i]))
		return false;
	return true;
    }
};

template <typename T>
inline bool rpc_istream_base::r(T& v) {
    return stream_helper<T>::r(v, this);
}

inline bool rpc_istream_base::r(std::string &s, long long max) {
    return stream_helper<std::string>::r(s, max, this);
}

inline bool rpc_istream_base::r(std::vector<std::string> &v, long long total) {
    v.clear();
    int n;
    if(!r(n)) return false;
    long long sum = 0;
    for(int i = 0; i < n; i++){
        std::string s;
        if(!r(s, total - sum)) return false;
        sum += s.size();
        v.push_back(std::move(s));
    }
    return true;
}

template <typename T>
inline bool rpc_ostream_base::w(const T& v) {
    return stream_helper<T>::w(v, this);
}

template <typename T>
inline size_t rpc_ostream_base::bytecount(const T& v) {
    return stream_helper<T>::bytecount(v);
}

}
