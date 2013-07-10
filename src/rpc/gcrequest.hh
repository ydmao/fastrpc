#ifndef GCREQUEST_HH
#define GCREQUEST_HH

#include "rpc_common/util.hh"
#include "rpc_parser.hh"

namespace rpc {

struct gcrequest_base {
    virtual void process_reply(parser& p, async_tcpconn* c) = 0;
    virtual void process_connection_error(async_tcpconn* c) = 0;
    uint32_t seq_;
};

template <uint32_t PROC, typename F>
struct gcrequest : public gcrequest_base, public F {
    typedef typename analyze_grequest<PROC>::request_type request_type;
    typedef typename analyze_grequest<PROC>::reply_type reply_type;

    gcrequest(F callback): F(callback), tstart_(rpc::common::tstamp()) {
    }
    void process_reply(parser& p, async_tcpconn *c) {
	p.parse_message(reply_);
	if (c->counts_)
	    c->counts_->add(PROC, count_recv_reply, sizeof(rpc_header) + p.header<rpc_header>()->len_,
                            rpc::common::tstamp() - tstart_);
	(static_cast<F &>(*this))(req_, reply_);
        delete this;
    }
    void process_connection_error(async_tcpconn* c) {
        reply_.set_eno(appns::RPCERR);
	(static_cast<F &>(*this))(req_, reply_);
        delete this;
    }

    request_type req_;
    reply_type reply_;
    uint64_t tstart_;
};

};

#endif
