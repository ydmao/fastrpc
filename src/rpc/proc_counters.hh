#ifndef PROC_COUNTERS_HH
#define PROC_COUNTERS_HH 1
#include <stdint.h>
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
    inline bool has(unsigned proc) const {
	return proc < NPROC;
    }
    inline bool has_bytes(unsigned proc) const {
	return proc < NPROC;
    }
    inline void add(unsigned proc, proc_counter_type t, unsigned nbytes, unsigned ts) {
	if (proc < NPROC) {
	    c_[proc].count[t] += 1;
	    c_[proc].bytes[t] += nbytes;
            c_[proc].time += ts;
	}
    }
    inline uint64_t count(unsigned proc, proc_counter_type t) const {
	return proc < NPROC ? c_[proc].count[t] : 0;
    }
    inline uint64_t bytes(unsigned proc, proc_counter_type t) const {
	return proc < NPROC ? c_[proc].bytes[t] : 0;
    }
    inline uint64_t time(unsigned proc) const {
	return proc < NPROC ? c_[proc].time : 0;
    }
    inline void clear() {
	bzero(c_, sizeof(c_));
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
	memset(c_, 0, sizeof(c_));
    }
    inline bool has(unsigned proc) const {
	return proc < NPROC;
    }
    inline bool has_bytes(unsigned proc) const {
        (void)proc;
        return false;
    }
    inline void add(unsigned proc, proc_counter_type t, unsigned) {
	if (proc < NPROC)
	    c_[proc].count[t] += 1;
    }
    inline uint64_t count(unsigned proc, proc_counter_type t) const {
	return proc < NPROC ? c_[proc].count[t] : 0;
    }
    inline uint64_t bytes(unsigned, proc_counter_type) const {
	return 0;
    }
    inline void clear() {
	memset(c_, 0, sizeof(c_));
    }
  private:
    struct counter {
	uint64_t count[4];
    };
    counter c_[NPROC];
};

} // namespace rpc
#endif
