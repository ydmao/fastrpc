#pragma once

#include <stdint.h>
#include <algorithm>
#include "proto/fastrpc_proto.hh"
namespace rpc {

enum proc_counter_type {
    count_sent_request,
    count_sent_reply,
    count_recv_request,
    count_recv_reply
};

template <unsigned NPROC, bool BYTES> struct proc_counters {};

template <unsigned NPROC> struct proc_counters<NPROC, true> {
    inline proc_counters() {
	bzero(c_, sizeof(c_));
    }
    inline void add(unsigned proc, proc_counter_type t, unsigned nbytes) {
	if (proc < NPROC) {
	    c_[proc].count[t] += 1;
	    c_[proc].bytes[t] += nbytes;
	}
    }
    inline void add_latency(unsigned proc, uint64_t time) {
	if (proc < NPROC)
	    c_[proc].time += time;
    }
    inline uint64_t count(unsigned proc, proc_counter_type t) const {
	return proc < NPROC ? c_[proc].count[t] : 0;
    }
    inline uint64_t bytes(unsigned proc, proc_counter_type t) const {
	return proc < NPROC ? c_[proc].bytes[t] : 0;
    }
    inline void clear() {
	bzero(c_, sizeof(c_));
    }
    inline void print(FILE* fp) {
	fprintf(fp, "%20s %10s %10s\n",
	        "proc", "request", "time/req");
	for (int32_t i = 0; i < NPROC; ++i) {
	    if (c_[i].count[count_sent_reply] == 0 &&
		c_[i].count[count_recv_request] == 0)
	        continue;
	    fprintf(fp, "%20s %10ld %10ld\n", 
		    app_param::proc_name(i),
		    c_[i].count[count_sent_reply],
		    c_[i].time / std::max(uint64_t(1), c_[i].count[count_sent_reply]));
        }
    }
  private:
    struct counter {
	uint64_t count[4];
	uint64_t bytes[4];
        uint64_t time;
    };
    counter c_[NPROC];
};

template <unsigned NPROC> struct proc_counters<NPROC, false> {
    inline proc_counters() {
    }
    inline void add(unsigned proc, proc_counter_type t, unsigned) {
    }
    inline void add_latency(unsigned, uint64_t) {
    }
    inline uint64_t count(unsigned proc, proc_counter_type t) const {
	return 0;
    }
    inline uint64_t bytes(unsigned, proc_counter_type) const {
	return 0;
    }
    inline void clear() {
    }
    inline void print(FILE* fp) {
    }
};

} // namespace rpc
