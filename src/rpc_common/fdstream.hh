#pragma once

#include <errno.h>
#include <unistd.h>

struct fdstream {
    fdstream(int fd) : fd_(fd) {
    }
    bool read(char *buf, int n) const {
        int wanted = n;
        while(wanted) {
            int cc = ::read(fd_, buf, wanted);
            if(cc < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                else
                    return false;
            } else {
                wanted -= cc;
                buf += cc;
            }
        }
        return true;
    }
    bool write(const char* buf, int n) const {
        int rest = n;
        while(rest) {
            int cc = ::write(fd_, buf, rest);
            if(cc < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                else
                    return false;
            } else {
                rest -= cc;
                buf += cc;
            }
        }
        return true;
    }
  private:
    int fd_;
};

