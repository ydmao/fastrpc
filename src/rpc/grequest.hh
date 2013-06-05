#ifndef GREQUEST_HH
#define GREQUEST_HH 1
#include "rpc_parser.hh"
#include "async_rpcc.hh"
#include "rpc_impl/request_analyzer.hh"

namespace rpc {
struct grequest_base {
    inline grequest_base(int proc) : proc_(proc) {
    }
    inline int proc() const {
        return proc_;
    }
    void execute(app_param::ErrorCode eno);
  private:
    int proc_;
};

template <uint32_t PROC>
struct grequest : public grequest_base {
    typedef typename analyze_grequest<PROC>::request_type request_type;
    typedef typename analyze_grequest<PROC>::reply_type reply_type;

    inline grequest() : grequest_base(PROC) {
    }
    using grequest_base::execute;
    virtual void execute() = 0;

    request_type req_;
    reply_type reply_;
};

template <uint32_t PROC>
struct grequest_remote : public grequest<PROC> {
    inline grequest_remote(uint32_t seq, async_rpcc* c) 
        : grequest<PROC>(), c_(c), seq_(seq) {
    }
    inline uint32_t seq() const {
        return seq_;
    }
    using typename grequest<PROC>::execute;
    inline void execute() {
        c_->connection().write_reply(PROC, this->seq_, this->reply_);
        delete this;
    }
  private:
    async_rpcc* c_;
    uint32_t seq_;
};

template <uint32_t PROC, typename F>
struct grequest_local : public grequest<PROC>, public F {
    inline grequest_local(F callback) : F(callback) {
    }
    using typename grequest<PROC>::execute;
    inline void execute() {
        (static_cast<F &>(*this))(this->req_, this->reply_);
        delete this;
    }
};

template <uint32_t PROC>
struct grequest_maker {
    typedef typename analyze_grequest<PROC>::request_type request_type;

    template <typename F>
    static grequest_local<PROC, F>* make(const request_type& req, F callback) {
        auto* g = new grequest_local<PROC, F>(callback);
        g->req_ = req;
        return g;
    }

    template <typename F>
    static grequest_local<PROC, F>* make_swap(request_type* req, F callback) {
        auto* g = new grequest_local<PROC, F>(callback);
        g->req_.Swap(req);
        return g;
    }

};

} // namespace rpc
#endif
