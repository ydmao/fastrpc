#pragma once

#include <errno.h>
#include <unistd.h>

template <typename T>
struct fdstream {
    fdstream(int fd) {
        tp_ = new transport(fd);
    }
    ~fdstream() {
	delete tp_;
    }
    bool read(char *buf, int n) const {
        int wanted = n;
        while(wanted) {
            int cc = tp_->read(buf, wanted);
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
            int cc = tp_->write(buf, rest);
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
    typedef typename T::sync_transport transport;
    transport* tp_;
};

