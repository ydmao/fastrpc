#include "async_rpcc.hh"
#include "async_tcpconn.hh"
#include "rpc_common/compiler.hh"
#include "rpc_common/sock_helper.hh"
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

using std::cout;
using std::endl;

namespace rpc {

uint32_t gcrequest_base::last_latency_ = 0;

async_rpcc::async_rpcc(const char *host, int port, rpc_handler* rh, int cid,
		       proc_counters<app_param::nproc, true> *counts)
    : async_rpcc(rpc::common::sock_helper::connect(host, port), rh, cid, counts) {
}

async_rpcc::async_rpcc(int fd, rpc_handler* rh, int cid,
		       proc_counters<app_param::nproc, true> *counts)
    : c_(fd, this, cid, counts), rh_(rh), 
      waiting_(new gcrequest_base *[1024]), waiting_capmask_(1023), 
      seq_(random() / 2) {
    bzero(waiting_, sizeof(gcrequest_base *) * 1024);
}

async_rpcc::~async_rpcc() {
    mandatory_assert(!c_.noutstanding_);
    delete[] waiting_;
}

void async_rpcc::buffered_read(async_tcpconn *, uint8_t *buf, uint32_t len) {
    parser p;
    while (p.parse<rpc_header>(buf, len, &c_)) {
	rpc_header *rhdr = p.header<rpc_header>();
        if (!rhdr->request()) {
            // Find the rpc request with sequence number @reply_hdr_.seq
	    gcrequest_base *q = waiting_[rhdr->seq_ & waiting_capmask_];
            mandatory_assert(q && q->seq_ == rhdr->seq_ && "RPC reply but no waiting call");
	    waiting_[rhdr->seq_ & waiting_capmask_] = 0;
	    --c_.noutstanding_;
	    gcrequest_base::last_latency_ = rhdr->latency();
	    q->process_reply(p, &c_);
        } else {
            ++c_.noutstanding_;
            mandatory_assert(rh_);
            rh_->handle_rpc(this, p);
        }
        p.reset();
    }
}

void async_rpcc::handle_error(async_tcpconn *, int the_errno) {
    if (rh_)
        rh_->handle_client_failure(this);
    if (c_.noutstanding_ != 0)
	fprintf(stderr, "error: %d rpcs outstanding (%s)\n",
		c_.noutstanding_, strerror(the_errno));
    unsigned ncap = waiting_capmask_ + 1;
    for (int i = 0; i < ncap; ++i) {
        gcrequest_base* q = waiting_[i];
        if (q)
            q->process_connection_error(&c_);
    }
    if (rh_)
    	rh_->handle_destroy(this);
}

void async_rpcc::expand_waiting() {
    do {
	unsigned ncapmask = waiting_capmask_ * 2 + 1;
	gcrequest_base **nw = new gcrequest_base *[ncapmask + 1];
	memset(nw, 0, sizeof(gcrequest_base *) * (ncapmask + 1));
	for (unsigned i = 0; i <= waiting_capmask_; ++i)
	    if (gcrequest_base *rr = waiting_[i])
		nw[rr->seq_ & ncapmask] = rr;
	delete[] waiting_;
	waiting_ = nw;
	waiting_capmask_ = ncapmask;
    } while (waiting_[seq_ & waiting_capmask_]);
}

} // namespace rpc
