#pragma once

#include "rpc_parser.hh"
#include "proto/fastrpc_proto.hh"

namespace rpc {

template <typename T, typename M>
inline bool send_reply(T* out, uint32_t mproc, uint32_t seq, const M& m) {
    uint32_t bodysz = m.ByteSize();
    rpc_header h;
    h.set_payload_length(bodysz, false);
    h.seq_ = seq;
    h.set_mproc(mproc);
    out->write((const char*)&h, sizeof(h));
    m.SerializeToStream(*out);
    return true;
}

template <typename T, typename M>
inline bool send_request(T* out, int32_t cmd, uint32_t seq, const M& m) {
    uint32_t bodysz = m.ByteSize();
    rpc_header h;
    h.set_payload_length(bodysz, true);
    h.seq_ = seq;
    h.set_mproc(rpc_header::make_mproc(cmd, 0));
    out->write((const char*)&h, sizeof(h));
    m.SerializeToStream(*out);
    return true;
}

template <typename T>
inline bool recv_header(T* in, rpc_header& h) {
    return in->read((char*)&h, sizeof(h));
}

template <typename T, typename M>
inline bool recv_body(T* in, M& m) {
    static_assert(!M::NB, "Synchronous reply can't be non blocking");
    return m.ParseFromStream(*in);
}

template <typename T, typename M>
inline bool read_reply(T* in, M& m, rpc_header& h) {
    return recv_header(in, h) && recv_body(in, m);
}

template <typename OUT, typename IN, typename REQ, typename REPLY>
inline bool sync_call(OUT* out, IN* in,
                      uint32_t seq, int cmd, const REQ& req, REPLY& reply) {
    send_request(out, cmd, seq, req);
    out->flush();
    rpc_header h;
    if (!read_reply(in, reply, h))
        return false;
    assert(h.seq_ == seq);
    return true;
}

}
