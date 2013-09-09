// buffered read and write for kvc/kvd.
// stdio is good but not quite what I want.
// need to be able to check if any input
// available, and do non-blocking check.
// also, fwrite just isn't very fast, at
// least on the Mac.

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <assert.h>
#include "kvio.h"

kvin::kvin(int xfd) : kvin(xfd, 4096) {
}

kvin::kvin(int xfd, int xlen)
{
  fd = xfd;
  len = xlen;
  buf = (char *) malloc(len);
  assert(buf);
  i0 = 0;
  i1 = 0;
}

kvin::kvin(const char *xbuf, int xlen)
{
  fd = -1;
  buf = (char *) xbuf;
  len = 0;
  i0 = 0;
  i1 = 0;
  setlen(xlen);
}

kvin::kvin(std::string xs)
{
  fd = -1;
  s = xs;
  buf = (char *) s.data();
  len = 0;
  i0 = 0;
  i1 = 0;
  setlen(s.size());
}

kvin::~kvin()
{
  if(buf && fd != -1)
    free(buf);
  buf = 0;
  fd = -1;
}

void
kvin::setlen(int xlen)
{
  assert(fd == -1);
  len = xlen;
  i0 = 0;
  i1 = xlen;
}

// do a read().
// return the number of bytes now
// available in the buffer.
int
kvin::reallyread()
{
  if(i0 == i1)
    i0 = i1 = 0;
  int wanted = len - i1;
  assert(fd >= 0);
  assert(wanted > 0 && i1 + wanted <= len);
  int cc;
  while(true) {
    cc = ::read(fd, buf + i1, wanted);
    if(cc < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      else
        return -1;
    } else
      break;
  }
  i1 += cc;
  return i1 - i0;
}

// read exactly n bytes.
bool
kvin::read(char *xbuf, int n)
{
  int i = 0;
  while(i < n){
    if(i1 > i0){
      int cc = i1 - i0;
      if(cc > n - i)
        cc = n - i;
      bcopy(buf + i0, xbuf + i, cc);
      i += cc;
      i0 += cc;
    } else {
      if(reallyread() < 1)
        return false;
    }
  }
  return true;
}

// see how much data is waiting.
// tryhard=0 -> just check buffer.
// tryhard=1 -> do non-blocking read if buf is empty.
// tryhard=2 -> block if buf is empty.
int
kvin::check(int tryhard)
{
  if(i1 == i0){
    assert(fd >= 0);
    if(tryhard == 2){
      // blocking read
      assert(reallyread() > 0);
    } else if(tryhard == 1){
      // non-blocking read
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(fd, &rfds);
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 0;
      select(fd + 1, &rfds, NULL, NULL, &tv);
      if(FD_ISSET(fd, &rfds))
        reallyread();
    }
  }
  return i1 - i0;
}

//
// at what file offset is caller about to read?
//
long long
kvin::offset()
{
  long long off = lseek(fd, (off_t)0, SEEK_CUR);
  off -= (i1 - i0);
  return off;
}

int
kvin::get_fd() {
  return fd;
}

//
// cause next read to come from this offset.
//
void
kvin::seek(long long off, int whence)
{
  lseek(fd, off, whence);
  i0 = i1 = 0;
}

kvout::kvout(int xfd, int max)
{
  fd = xfd;
  len = max;
  buf = (char *) malloc(max);
  assert(buf);
  n = 0;
}

kvout::kvout(int xfd) : kvout(xfd, 4096) {
}

kvout::kvout(char *xbuf, int max)
{
  fd = -1;
  len = max;
  buf = xbuf;
  n = 0;
}

// does not close() the fd.
kvout::~kvout()
{
  if(buf)
    free(buf);
  buf = 0;
  fd = -1;
}

void
kvout::reset()
{
  assert(fd < 0);
  n = 0;
}

bool
kvout::flush()
{
  assert(fd >= 0);
  if(n > 0){
    int cc;
    while((cc = ::write(fd, buf, n)) == -1 &&
          (errno == EWOULDBLOCK || errno == EAGAIN))
        ;
    if(cc != n){
      fprintf(stderr, "kvout::flush: write failed\n");
      return false;
    }
  }
  n = 0;
  return true;
}

int
kvout::get_fd() {
  return fd;
}

bool
kvout::write(const void *xbuf, int xn)
{
  if(n + xn > len)
    if(flush() == false)
      return false;
  assert(n + xn <= len);
  bcopy(xbuf, buf + n, xn);
  n += xn;
  return true;
}

//
// at what file offset will next write go?
//
long long
kvout::offset()
{
  flush();
  off_t off = lseek(fd, (off_t)0, SEEK_CUR);
  return off;
}

void
kvout::seek(long long off, int whence)
{
  flush();
  lseek(fd, off, whence);
}

void
kvio_test()
{
  char buf[8192];
  bool b;
  int fds[2];

  assert(pipe(fds) == 0);

  kvin *in = new kvin(fds[0]);
  kvout *out = new kvout(fds[1]);

  assert(in->check(0) == 0);
  assert(in->check(1) == 0);

  b = out->write((void*) "z", 1);
  assert(b);
  assert(in->check(0) == 0);
  assert(in->check(1) == 0);

  out->flush();
  assert(in->check(0) == 0);

  b = in->read(buf, 1);
  assert(b && buf[0] == 'z');
  assert(in->check(0) == 0);
  assert(in->check(1) == 0);

  b = out->write((void*) "abc", 3);
  assert(b);
  out->flush();
  assert(in->check(0) == 0);
  assert(in->check(2) == 3);

  b = in->read(buf, 1);
  assert(b && buf[0] == 'a');
  assert(in->check(0) == 2);
  assert(in->check(1) == 2);
  b = in->read(buf, 2);
  assert(b && buf[0] == 'b' && buf[1] == 'c');

  long long xx = 1234, yy = 0;
  KVW(out, xx);
  out->flush();
  b = in->read((char*)&yy, (int) sizeof(yy));
  assert(b && yy == xx);

  close(fds[0]);
  close(fds[1]);
  delete in;
  delete out;
}

const char* kvout::get_buf() const {
    return buf;
}

int kvout::get_n() const {
    return n;
}

#if 0
main()
{
  kvio_test();
  exit(0);
}
#endif
