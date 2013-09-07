#ifndef GREQUEST_HH
#define GREQUEST_HH 1
#include "async_rpcc.hh"

namespace rpc {

struct grequest_base {
    inline grequest_base(int proc) : proc_(proc) {
    }
    inline int proc() const {
        return proc_;
    }
    virtual void execute() = 0;
  private:
    int proc_;
};

template <uint32_t PROC, bool NB = false>
struct grequest : public grequest_base {
    typedef typename analyze_grequest<PROC, NB>::request_type request_type;
    typedef typename analyze_grequest<PROC, NB>::reply_type reply_type;
    virtual ~grequest() {
    }
    inline grequest() : grequest_base(PROC) {
    }
    using grequest_base::execute;
    void execute(app_param::ErrorCode eno) {
        reply_.set_eno(eno);
        this->execute();
    }
    virtual async_rpcc* rpcc() = 0;

    request_type req_;
    reply_type reply_;
};

template <uint32_t PROC, bool NB = false>
struct grequest_remote : public grequest<PROC, NB> {
    inline grequest_remote(uint32_t seq, async_rpcc* c) 
        : grequest<PROC, NB>(), c_(c), seq_(seq) {
    }
    inline uint32_t seq() const {
        return seq_;
    }
    virtual ~grequest_remote() {
    }
    using typename grequest<PROC, NB>::execute;
    inline void execute() {
        c_->connection().write_reply(PROC, this->seq_, this->reply_);
        if (!NB)
            delete this;
    }
    async_rpcc* rpcc() {
        return c_;
    }
  private:
    async_rpcc* c_;
    uint32_t seq_;
};

template <uint32_t PROC, typename F, bool NB = false>
struct grequest_local : public grequest<PROC, NB>, public F {
    inline grequest_local(F callback) : F(callback) {
    }
    using typename grequest<PROC, NB>::execute;
    inline void execute() {
        (static_cast<F &>(*this))(this->req_, this->reply_);
        if (!NB)
            delete this;
    }
    async_rpcc* rpcc() {
        return NULL;
    }
};

template <uint32_t PROC>
struct req_maker {
    template <typename F>
    static grequest_local<PROC, F>* make_local(const F& f) {
        return new grequest_local<PROC, F>(f);
    }
};

} // namespace rpc
#endif
