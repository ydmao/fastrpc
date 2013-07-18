#ifndef GCREQUEST_HH
#define GCREQUEST_HH

#include "rpc_common/util.hh"
#include "rpc_parser.hh"

namespace rpc {

struct gcrequest_base {
    virtual void process_reply(parser& p, async_tcpconn* c) = 0;
    virtual void process_connection_error(async_tcpconn* c) = 0;
    virtual ~gcrequest_base() {
    }
    uint32_t seq_;
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
struct gcrequest : public gcrequest_base, public F {
    typedef typename analyze_grequest<PROC, false>::request_type request_type;
    typedef typename analyze_grequest<PROC, false>::reply_type reply_type;

    gcrequest(F callback): F(callback), tstart_(rpc::common::tstamp()) {
    }
    void process_reply(parser& p, async_tcpconn *c) {
	p.parse_message(reply_);
	if (c->counts_)
	    c->counts_->add(PROC, count_recv_reply, sizeof(rpc_header) + p.header<rpc_header>()->len(),
                            rpc::common::tstamp() - tstart_);
	(static_cast<F &>(*this))(req_, reply_);
        delete this;
    }
    void process_connection_error(async_tcpconn* c) {
        //reply_.set_eno(app_param::ErrorCode::RPCERR);
        default_eno<has_eno<reply_type>::value>::set(&reply_);
	(static_cast<F &>(*this))(req_, reply_);
        delete this;
    }

    request_type req_;
    reply_type reply_;
    uint64_t tstart_;
};

};

#endif
