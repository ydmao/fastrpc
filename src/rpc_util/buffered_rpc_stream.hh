#pragma once

#include <string.h>
#include <string>
#include <vector>
#include <errno.h>
#include <assert.h>
#include "rpc_stream_base.hh"

namespace rpc {

template <typename ISM>
struct buffered_rpc_istream : public rpc_istream_base {
    buffered_rpc_istream(ISM* ism, int xlen) : ism_(ism) {
	assert(xlen > 0);
	buf_ = (char*)malloc(xlen);
	assert(buf_);
	len_ = xlen;
	i0_ = i1_ = 0;
    }
    ~buffered_rpc_istream() {
	delete buf_;
    }
    // read exactly n bytes.
    bool read(void* xbuf, size_t n) {
        int i = 0;
        while(i < int(n)){
            if(i1_ > i0_){
                int cc = i1_ - i0_;
                if(cc > int(n - i))
                    cc = n - i;
                bcopy(buf_ + i0_, (char*)xbuf + i, cc);
                i += cc;
                i0_ += cc;
            } else if(reallyread() < 1)
                return false;
        }
        return true;
    }
  private:
    // do a read().
    // return the number of bytes now
    // available in the buffer.
    int reallyread() {
        if(i0_ == i1_)
            i0_ = i1_ = 0;
        int wanted = len_ - i1_;
        assert(wanted > 0 && i1_ + wanted <= len_);
        int cc;
        while(true) {
            cc = ism_->read(buf_ + i1_, wanted);
            if(cc < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                else
                    return -1;
            } else
                break;
        }
        i1_ += cc;
        return i1_ - i0_;
    }
    ISM* ism_;
    char* buf_;
    int len_; // underlying size of buf[]
    // buf[i0..i1-1] are valid
    int i0_;
    int i1_;
};

template <typename OSM>
struct buffered_rpc_ostream : public rpc_ostream_base {
    OSM* osm_;
    int len_;
    char* buf_;
    int n_;
  public:
    buffered_rpc_ostream(OSM* osm, int max) {
	osm_ = osm;
	len_ = max;
	buf_ = (char*)malloc(max);
	assert(buf_);
	n_ = 0;
    }
    ~buffered_rpc_ostream() {
	delete buf_;
    }

    void reset() {
	n_ = 0;
    }
    bool write(const void *xbuf, size_t xn) {
        if(n_ + xn > size_t(len_))
            if(flush() == false)
                return false;
        assert(n_ + xn <= size_t(len_));
        bcopy(xbuf, buf_ + n_, xn);
        n_ += xn;
        return true;
    }
    bool flush() {
	if (n_ == 0)
	    return true;
        int cc;
        while((cc = osm_->write(buf_, n_)) == -1 &&
              (errno == EWOULDBLOCK || errno == EAGAIN))
            ;
        if(cc != n_){
            fprintf(stderr, "kvout::flush: write failed\n");
            return false;
        }
        n_ = 0;
        return true;
    }
};

}
