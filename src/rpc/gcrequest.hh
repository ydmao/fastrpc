#pragma once

#include "rpc_common/util.hh"
#include "rpc_parser.hh"

namespace rpc {

struct gcrequest_base {
    virtual void process_reply(parser& p) = 0;
    virtual void process_connection_error() = 0;
    virtual uint32_t proc() const = 0;
    virtual uint64_t start_at() const = 0;
    virtual ~gcrequest_base() {
    }
    uint32_t seq_;
    static uint32_t last_latency_;
};

template <typename T>
struct has_eno {
    template <typename C>
    static uint8_t test(typename C::set_eno*);
    template <typename>
    static uint32_t test(...);
    static const bool value = (sizeof(test<T>(0)) == 1);
};

template <bool>
struct default_eno {
    template <typename T>
    static void set(T*) {
    }
};

template <>
struct default_eno<true> {
    template <typename T>
    static void set(T* r) {
        r->set_eno(app_param::ErrorCode::RPCERR);
    }
};

template <uint32_t PROC, typename F>
struct gcrequest : public gcrequest_base {
    typedef typename analyze_grequest<PROC, false>::request_type request_type;
    typedef typename analyze_grequest<PROC, false>::reply_type reply_type;

    gcrequest(F callback): cb_(callback), tstart_(rpc::common::tstamp()) {
    }
    //@lat: latency in 100microseconds
    void process_reply(parser& p) {
	p.parse_message(reply_);
	cb_.operator()(req_, reply_);
        delete this;
    }
    void process_connection_error() {
        //reply_.set_eno(app_param::ErrorCode::RPCERR);
        default_eno<has_eno<reply_type>::value>::set(&reply_);
	cb_.operator()(req_, reply_);
        delete this;
    }
    uint32_t proc() const {
	return PROC;
    }
    uint64_t start_at() const {
	return tstart_;
    }
    request_type req_;
    reply_type reply_;
    F cb_;
    uint64_t tstart_;
};

};

