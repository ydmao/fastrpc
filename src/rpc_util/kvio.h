#pragma once

#include <string.h>
#include <string>
#include <vector>
#include "rpc_stream_base.hh"

class kvin : public rpc::rpc_istream_base {
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
  bool read(void *xbuf, size_t n);
  int check(int tryhard);
  long long offset();
  void seek(long long off, int whence);
  int get_fd();
};

class kvout : public rpc::rpc_ostream_base {
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
  bool write(const void *xbuf, size_t n);
  bool flush();
  int get_fd();
  long long offset();
  void seek(long long off, int whence);
};

#define KVW(kvout, x)  { (kvout)->w(x); }

#define KVR(kvin, x) ( (kvin)->r(x) )
