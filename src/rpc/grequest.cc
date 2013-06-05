#include "grequest.hh"

namespace rpc {

#define RPC_EXECUTE(PROC, REQ, REPLY)			\
    case app_param::ProcNumber::PROC: {					\
	grequest<app_param::ProcNumber::PROC> *self =			\
	    static_cast<grequest<app_param::ProcNumber::PROC> *>(this);	\
	self->reply_.set_eno(eno);				\
	self->execute();					\
	break; }

void grequest_base::execute(app_param::ErrorCode eno) {
    switch (proc_) {
	RPC_FOR_EACH_CLIENT_MESSAGE(RPC_EXECUTE)
	RPC_FOR_EACH_INTERCONNECT_MESSAGE(RPC_EXECUTE)
    }
}

} // namespace rpc
