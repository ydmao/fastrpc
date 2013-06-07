#ifndef REQUEST_ANALYZER
#define REQUEST_ANALYZER 1

#include "proto/fastrpc_proto.hh"

namespace rpc {

template <typename REQ, typename REPLY, uint32_t PROC>
struct analyze_grequest_helper {
    typedef REQ request_type;
    typedef REPLY reply_type;
};

template <uint32_t PROC> struct analyze_grequest {};
// map requests to replies
#define RPC_DECLARE_ANALYZE_GREQUEST(service, PROC, REQ, REPLY)	\
    template <> struct analyze_grequest<app_param::ProcNumber::PROC> :	\
	public analyze_grequest_helper<appns::REQ, appns::REPLY, app_param::ProcNumber::PROC> {};
RPC_FOR_EACH_CLIENT_MESSAGE(RPC_DECLARE_ANALYZE_GREQUEST)
RPC_FOR_EACH_INTERCONNECT_MESSAGE(RPC_DECLARE_ANALYZE_GREQUEST)

}


#endif
