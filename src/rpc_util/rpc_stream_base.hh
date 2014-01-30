#pragma once
#include <string>
#include <vector>

// TODO: simplify compiler/stream_parser.hh by reusing stream_base.hh
struct rpc_istream_base {
    virtual ~rpc_istream_base() {}

    template<class T>
    bool r(T &x) {
        static_assert(std::is_pod<T>::value, "Must be POD");
        return read((void*)&x, sizeof(x));
    }
    bool r(std::string &s, long long max) {
        int len;
        if(r(len) == false) return false;
        if(len > max) return false;
        s.resize(len);
        if(read(&s[0], len) == false) return false;
        return true;
    }
    bool r(std::vector<std::string> &v, long long total) {
        v.clear();
        int n;
        if(r(n) == false) return false;
        long long sum = 0;
        for(int i = 0; i < n; i++){
            std::string s;
            if(r(s, total - sum) == false) return false;
            v.push_back(s);
            sum += s.size();
        }
        return true;
    }
    virtual bool read(void* buffer, size_t len) = 0;
};

struct rpc_ostream_base {
    virtual ~rpc_ostream_base() {}
    template<class T>
    void w(const T x) {
        static_assert(std::is_pod<T>::value, "Must be POD");
        write(&x, sizeof(x));
    }
    void w(const std::string &x) {
        int sz = (int) x.size();
        w(sz);
        write(x.data(), sz);
    }
    void w(const std::vector<std::string> &v) {
        int n = (int) v.size();
        w(n);
        for(int i = 0; i < n; i++)
            w(v[i]);
    }
    virtual bool write(const void* buffer, size_t) = 0;
    virtual bool flush() = 0;
};


