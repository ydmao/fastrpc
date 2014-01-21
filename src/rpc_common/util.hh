#pragma once

#include <sstream>
#include <iostream>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <memory>
#include "rpc_common/compiler.hh"

namespace rpc {
namespace common {

inline double now() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

inline uint64_t tous(double seconds) {
    mandatory_assert(seconds >= 0);
    return (uint64_t) (seconds * 1000000);
}

inline double fromus(uint64_t t) {
    return (double) t / 1000000.;
}

inline uint64_t tv2us(struct timeval &tv) {
    return (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

inline uint64_t tstamp() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return rpc::common::tv2us(tv);
}

inline double to_real(timeval tv) {
    return tv.tv_sec + tv.tv_usec / (double) 1e6;
}

}} // namespace rpc::common
