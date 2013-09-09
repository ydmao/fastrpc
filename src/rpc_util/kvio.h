#pragma once

#include <string.h>
#include <string>
#include <vector>

class kvin {
private:
  int fd;
  char *buf;
  int len; // underlying size of buf[]
  // buf[i0..i1-1] are valid
  int i0;
  int i1;
  std::string s;

  int reallyread();

public:
  kvin(int xfd);
  kvin(int xfd, int xlen);
  kvin(const char *xbuf, int xlen);
  kvin(std::string xs);
  ~kvin();
  void setlen(int len);
  bool read(char *xbuf, int n);
  int check(int tryhard);
  long long offset();
  void seek(long long off, int whence);
  int get_fd();

  template<class T>
  bool r(T &x) {
    static_assert(std::is_pod<T>::value, "Must be POD");
    if(i1 - i0 >= (int) sizeof(x)){
      x = *(T*)(buf+i0);
      i0 += sizeof(x);
      return true;
    } else {
      return read((char*)&x, sizeof(x));
    }
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
};

class kvout {
private:
  int fd;
  int len;
  char *buf;
  int n;
public:
  kvout(int xfd);
  kvout(int fd, int max);
  kvout(char *xbuf, int max);
  ~kvout();

  const char* get_buf() const;
  int get_n() const;

  void reset();
  bool write(const void *xbuf, int n);
  bool flush();
  int get_fd();
  long long offset();
  void seek(long long off, int whence);

  template<class T>
  void w(const T x) {
    static_assert(std::is_pod<T>::value, "Must be POD");
    if(len - n >= (int) sizeof(x)){
      *(T*)(buf+n) = x;
      n += sizeof(x);
    } else {
      write(&x, sizeof(x));
    }
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
};

#define KVW(kvout, x)  { (kvout)->w(x); }

#define KVR(kvin, x) ( (kvin)->r(x) )
