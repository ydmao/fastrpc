#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <set>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>

namespace gp = google::protobuf;
namespace gpc = google::protobuf::compiler;

std::ofstream xx_;
std::ofstream xs_;
std::ofstream xc_;
std::ostringstream ap_;
std::map<std::string, int> proc_;
std::string dir_;

bool has_string(const google::protobuf::Descriptor* m) {
    for (int i = 0; i < m->field_count(); ++i) {
        if (m->field(i)->cpp_type() == gp::FieldDescriptor::CPPTYPE_STRING)
            return true;
        if (m->field(i)->cpp_type() == gp::FieldDescriptor::CPPTYPE_MESSAGE &&
            has_string(m->field(i)->message_type()))
            return true;
    }
    return false;
}

void write_default_value(std::ofstream& x, const google::protobuf::FieldDescriptor* f, bool nb) {
    switch (f->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
            const std::string& v = f->default_value_string();
            if (nb)
                x << "refcomp::str(\"" << v << "\", " << v.length() << ")";
            else
                x << "std::string(\"" << v << "\")";
        } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        assert(0 && "don't call me");
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        x << f->default_value_int32();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        x << f->default_value_int64();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        x << f->default_value_uint32();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        x << f->default_value_uint64();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        x << f->default_value_double();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        x << f->default_value_float();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        x << f->default_value_bool();
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        x << f->default_value_enum()->type()->name() << "::" << f->default_value_enum()->name();
        break;
    default:
        assert(0 && "Unknown type");
    };
}

std::string refcomp_type_name(const google::protobuf::FieldDescriptor* f, bool nb) {
    switch (f->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        return nb ? "refcomp::str" : "std::string";
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        return (nb ? "nb_" : "") + f->message_type()->name();
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        return "int32_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        return "int64_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        return "uint32_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        return "uint64_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        return "double";
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        return "float";
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        return "bool";
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        return f->enum_type()->name();
    default:
        assert(0 && "Unknown type");
    };
}

struct nbcg: public gpc::CodeGenerator {
    bool Generate(const gp::FileDescriptor* file, const std::string& parameter,
                  gpc::GeneratorContext*, std::string* error) const;
  private:
    void generateEnum(const gp::EnumDescriptor* d) const;
    void generateProcNumber(const gp::FileDescriptor* file) const;
    void generateRequestAnalyzer(const gp::FileDescriptor* file) const;
    void generateMessage(const gp::Descriptor* d) const;
    void generateMessage(const gp::Descriptor* d, bool nb) const;

    void generateXC(const gp::FileDescriptor* file) const;
    void generateXS(const gp::FileDescriptor* file) const;

};

void nbcg::generateXC(const gp::FileDescriptor* file) const {
    xc_.open(dir_ + "/fastrpc_proto_client.hh");
    xc_ << "#pragma once\n"
        << "#include \"rpc/async_rpcc_helper.hh\"\n\n"
        << "#include \"rpc/sync_rpc.hh\"\n\n"
        << "namespace " << file->package() << "{\n\n"
        << "template <typename SELF>\n"
        << "struct make_callable {\n";

    typedef std::pair<std::string, std::string> callback_type;
    std::set<callback_type> cb;
    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            callback_type unary("", m->output_type()->name());
            if (cb.find(unary) == cb.end()) {
                xc_ << "    template <void (SELF::*method)(" << m->output_type()->name() << "&)>\n"
                    << "    inline rpc::make_unary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name() 
                    << ", method> make_call() {\n"
                    << "        return rpc::make_unary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name()
                    << ", method>((SELF*)this);\n"
                    << "    }\n";
                cb.insert(unary);
            }

            callback_type binary(m->input_type()->name(), m->output_type()->name());
            if (cb.find(binary) == cb.end()) {
                xc_ << "    template <void (SELF::*method)(" << m->input_type()->name() << "&, " << m->output_type()->name() << "&)>\n"
                    << "    inline rpc::make_binary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name() 
                    << ", method> make_call() {\n"
                    << "        return rpc::make_binary_call_helper<SELF, " << m->input_type()->name() << ", " << m->output_type()->name()
                    << ", method>((SELF*)this);\n"
                    << "    }\n";
                cb.insert(binary);
            }
        }
    }
    xc_ << "};\n\n";

    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        xc_ << "template <typename BASE>\n"
            << "struct " << s->name() << "Client : BASE {\n"
            << "    explicit " << s->name() << "Client(const " << s->name() << "Client<BASE>&){assert(0 && \"disallowed\");}\n"
            << "    " << s->name() << "Client(): seq_(0) {}\n";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xc_ << "    bool " << m->name() << "(const " << m->input_type()->name() << "& req, " << m->output_type()->name() << "& reply) {\n"
                << "        if (!this->send_" << m->name() << "(req)) { this->disconnect(); return false; }\n"
                << "        if (!this->recv_" << m->name() << "(reply)) { this->disconnect(); return false; }\n"
                << "        return true;\n"
                << "    }\n";
            xc_ << "    bool send_" << m->name() << "(const " << m->input_type()->name() << "& req) {\n"
                << "        if (!this->connect()) { return false; }\n"
                << "        if (!rpc::send_request(this->out_, ProcNumber::" << m->name() << ", seq_++, 0, req)){ this->disconnect(); return false; }\n"
                << "        this->out_->flush(); \n"
                << "        return true;\n"
                << "    }\n";
            xc_ << "    bool recv_" << m->name() << "(" << m->output_type()->name() << "& reply) {\n"
                << "        if (!this->connected()) { return false; }\n"
                << "        rpc::rpc_header h;\n"
                << "        if (!rpc::read_reply(this->in_, reply, h)) { this->disconnect(); reply.Clear(); return false; }\n"
                << "        assert(h.seq_+1 == seq_);\n"
                << "        return true;\n"
                << "    }\n";
        }
        xc_ << "  private:\n"
            << "    uint32_t seq_;\n"
            << "};\n";
    }

    xc_ << "}; // namespace " << file->package() << "\n";
}

std::string up(const std::string& s) {
    std::string up = s;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    return up;
}

void nbcg::generateXS(const gp::FileDescriptor* file) const {
    xs_.open(dir_ + "/fastrpc_proto_server.hh");
    xs_ << "#pragma once\n"
        << "#include \"fastrpc_proto.hh\"\n"
        << "#include \"rpc/grequest.hh\"\n"
        << "#include \"rpc/rpc_server_base.hh\"\n\n"
        << "namespace " << file->package() << "{\n\n";
    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        // templated_proc
        xs_ << "template <typename F>\n"
            << "inline bool " << s->name() << "_templated_call(ProcNumber proc, F& f) {\n"
            << "   switch (proc) {\n";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "    case ProcNumber::" << m->name() << ":{\n"
	        << "        f.template operator()<ProcNumber::" << m->name() << ">();\n"
                << "        break;}\n";
        };
        xs_ << "        default:\n"
            << "            return false;\n"
            << "    }\n"
            << "    return true;\n;"
            << "}\n\n";

        xs_ << "template <bool NB_DEFAULT = false";
        for (int j = 0; j < s->method_count(); ++j)
            xs_ << ", bool NB_" << up(s->method(j)->name()) << " = NB_DEFAULT";
        xs_ << ">\n";
        
        xs_ << "struct " << s->name() << "Interface : public rpc::rpc_server_base {\n"
            << "    template <bool NB, ProcNumber PROC>\n"
            << "    void report_failure() {\n"
            << "        if (NB){\n"
            << "            std::cerr << \"Please override non-blocking version of \" << ProcNumber_Name(PROC) << std::endl;\n"
            << "            assert(0);\n"
            << "        } else {\n"
            << "            std::cerr << \"Please override blocking version of \" << ProcNumber_Name(PROC) << std::endl;\n"
            << "            assert(0);\n"
            << "        }\n"
            << "    }\n";
        // stub
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "    virtual void " << m->name() << "(rpc::grequest<ProcNumber::" << m->name() << ">*, uint64_t) {\n"
                << "        report_failure<NB_" << up(m->name()) << ", ProcNumber::" << m->name() << ">();\n"
                << "    }\n";

            xs_ << "    virtual void " << m->name() << "(rpc::grequest<ProcNumber::" << m->name() << ", true>&, uint64_t) {\n"
                << "        report_failure<NB_" << up(m->name()) << ", ProcNumber::" << m->name() << ">();\n"
                << "    }\n";
        }

        // proclist
        xs_ << "    std::vector<int> proclist() const {\n"
            << "        ProcNumber pl[] = {";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "ProcNumber::" << m->name();
            if (j < s->method_count() - 1)
                xs_ << ", ";
        }
        xs_ << "};\n";

        xs_ << "        return std::vector<int>(pl, pl + " << s->method_count() << ");\n"
            << "    }\n";
        // dispatch
        xs_ << "    virtual void dispatch(rpc::parser& p, rpc::async_rpcc* c, uint64_t now) {\n"
            << "       rpc::rpc_header* h = p.header<rpc::rpc_header>();\n"
            << "       switch (h->proc()) {\n";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "        case ProcNumber::" << m->name() << ":\n"
                << "            if (NB_" << up(m->name()) << ") {\n"
                << "                rpc::grequest_remote<ProcNumber::" << m->name() << ", true> q(h->seq_, c);\n"
                << "                p.parse_message(q.req_);\n"
                << "                " << m->name() << "(q, now);\n"
                << "            } else {\n"
                << "                auto q = new rpc::grequest_remote<ProcNumber::" << m->name() << ", false>(h->seq_, c);\n"
                << "                p.parse_message(q->req_);\n"
                << "                " << m->name() << "(q, now);\n"
                << "            }break;\n";
        };
        xs_ << "        default:\n"
            << "            assert(0 && \"Unknown RPC\");\n"
            << "        }\n"
            << "    }\n";

        // dispatch_sync
        xs_ << "    void dispatch_sync(rpc::rpc_header& h, std::string& b, fdstream* c, uint64_t now) {\n"
            << "       switch (h.proc()) {\n";
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xs_ << "        case ProcNumber::" << m->name() << ":\n"
                << "            if (NB_" << up(m->name()) << ") {\n"
                << "                rpc::grequest_sync<ProcNumber::" << m->name() << ", true> q(h.seq_, c);\n"
                << "                q.req_.ParseFromArray(&b[0], h.payload_length());\n"
                << "                " << m->name() << "(q, now);\n"
                << "            } else {\n"
                << "                auto q = new rpc::grequest_sync<ProcNumber::" << m->name() << ", false>(h.seq_, c);\n"
                << "                q->req_.ParseFromArray(&b[0], h.payload_length());\n"
                << "                " << m->name() << "(q, now);\n"
                << "            }break;\n";
        };
        xs_ << "        default:\n"
            << "            assert(0 && \"Unknown RPC\");\n"
            << "        }\n"
            << "    }\n";

        xs_ << "}; // " << s->name() << "Interface\n";

    }
    

    xs_ << "}; // namespace " << file->package() << "\n";
}

bool nbcg::Generate(const gp::FileDescriptor* file, const std::string& parameter,
                    gpc::GeneratorContext*, std::string* error) const {
    dir_ = parameter;
    std::cerr << "shit " << file->package() << "\n";
    xx_.open(dir_ + "/fastrpc_proto.hh");
    xx_ << "#pragma once\n"
        << "#include <inttypes.h>\n"
        << "#include <iostream>\n"
        << "#include <assert.h>\n"
        << "#include <vector>\n"
        << "#include \"compiler/stream_parser.hh\"\n"
        << "\n";

    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            assert(proc_.find(m->name()) == proc_.end() && "method name must be globally unique");
            size_t index = proc_.size();
            proc_[m->name()] = index;
        }
    }
    for (int i = 0; i < file->enum_type_count(); ++i)
        generateEnum(file->enum_type(i));

    generateProcNumber(file);

    xx_ << "namespace " << file->package() << " {\n";
    for (int i = 0; i < file->message_type_count(); ++i)
        xx_ << "struct " << file->message_type(i)->name() << ";\n";
    xx_ << "}\n";
    for (int i = 0; i < file->message_type_count(); ++i)
        generateMessage(file->message_type(i));

    generateRequestAnalyzer(file);
    xx_ << ap_.str();

    generateXC(file);
    generateXS(file);
    return true;
}

void nbcg::generateRequestAnalyzer(const gp::FileDescriptor* file) const {
    xx_ << "namespace rpc {\n"
        << "template <uint32_t PROC, bool NB> struct analyze_grequest{};\n\n";

    for (int i = 0; i < file->service_count(); ++i) {
        auto s = file->service(i);
        for (int j = 0; j < s->method_count(); ++j) {
            auto m = s->method(j);
            xx_ << "template<> struct analyze_grequest<" << file->package() 
                << "::ProcNumber::" << m->name() << ", true> {\n"
                << "    typedef " << file->package() << "::nb_" << m->input_type()->name() << " request_type;\n"
                << "    typedef " << file->package() << "::nb_" << m->output_type()->name() << " reply_type;\n"
                << "};\n\n";

            xx_ << "template<> struct analyze_grequest<" << file->package() 
                << "::ProcNumber::" << m->name() << ", false> {\n"
                << "    typedef " << file->package() << "::" << m->input_type()->name() << " request_type;\n"
                << "    typedef " << file->package() << "::" << m->output_type()->name() << " reply_type;\n"
                << "};\n\n";
        }
    }

    xx_ << "struct app_param {\n"
        << "    typedef " << file->package() << "::ErrorCode ErrorCode;\n"
        << "    static constexpr uint32_t nproc = " << file->package() << "::ProcNumber::nproc;\n"
        << "};\n";

    xx_ << "}; // namespace rpc\n";
}

void nbcg::generateProcNumber(const gp::FileDescriptor* file) const {
    xx_ << "namespace " << file->package() << "{\n"
        << "enum ProcNumber {\n";
    for (auto p : proc_)
        xx_ << "    " << p.first << " = " << p.second << ",\n";
    xx_ << "    nproc = " << proc_.size() << ",\n";
    xx_ << "}; // enum \n";

    xx_ << "inline const char* ProcNumber_Name(ProcNumber n) {\n"
        << "    switch(n) {\n";
    for (auto p : proc_)
        xx_ << "    case ProcNumber::" << p.first << ":\n"
            << "        return \"" << p.first << "\";\n";
    xx_ << "    default:\n"
        << "        assert(0 && \"Bad value\");\n"
        << "    }\n"
        << "}\n";

    xx_ << "}; // namespace\n";

}

void nbcg::generateEnum(const gp::EnumDescriptor* d) const {
    xx_ << "namespace " << d->file()->package() << "{\n"
        << "enum " << d->name() << "{\n";
    for (int i = 0; i < d->value_count(); ++i) {
        auto vd = d->value(i);
        xx_ << "    " << vd->name() << " = " << vd->number() << ",\n";
    }
    xx_ << "}; // enum \n";

    xx_ << "inline const char* " << d->name() << "_name(" << d->name() << " n) {\n"
        << "    switch(n) {\n";
    for (int i = 0; i < d->value_count(); ++i) {
        auto vd = d->value(i);
        xx_ << "    case " << d->name() << "::"<< vd->name() << ":\n"
            << "        return \"" << vd->name() << "\";\n";
    }
    xx_ << "    default:\n"
        << "        assert(0 && \"Bad value\");\n"
        << "    }\n"
        << "}\n";

    xx_ << "}; // namespace\n";
}

void nbcg::generateMessage(const gp::Descriptor* d) const {
    generateMessage(d, true);
    generateMessage(d, false);
}

void nbcg::generateMessage(const gp::Descriptor* d, bool nb) const {
    std::string className = (nb ? "nb_" : "") + d->name();
    if (nb && !has_string(d)) {
        xx_ << "namespace " << d->file()->package() << "{\n"
            << "typedef " << d->name() << " " << className << ";\n"
            << "}\n";
        return;
    }
    xx_ << "namespace " << d->file()->package() << "{\n"
        << "class " << className << "{\n"
        << "  public:\n";
    
    // constructor
    xx_ << "    " << className << "() {\n";
    xx_ << "        Clear();\n"
        << "    }\n";

    // SerializeToArray
    xx_ << "    bool SerializeToArray(uint8_t* s, size_t) const {\n"
        << "        refcomp::simple_ostream so(s);\n"
        << "        return SerializeToStream(so);\n"
        << "    }\n";

    // SerializeToStream
    xx_ << "    template <typename S>\n"
        << "    bool SerializeToStream(S& s) const {\n"
        << "        refcomp::stream_unparser<S> su(s);\n";

    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (f->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            if (!f->is_repeated())
                xx_ << "        " << f->name() << "_.SerializeToStream(s);\n";
            else {
                xx_ << "        su.unparse(" << f->name() << "_, [&](const " << refcomp_type_name(f, nb) << "& t){t.SerializeToStream(s);});\n";
            }
        } else
            xx_ << "        su.unparse(" << f->name() << "_);\n";
    }
    xx_ << "        return true;\n";
    xx_ << "    }\n";
    
    // ParseFromArray
    xx_ << "    bool ParseFromArray(const void* data, size_t size) {\n"
        << "        refcomp::simple_istream si(data, size);\n"
        << "        return ParseFromStream(si);\n"
        << "    }\n";

    // ParseFromStream
     xx_ << "    template <typename S>\n"
         << "    bool ParseFromStream(S& s) {\n"
         << "        refcomp::stream_parser<S> sp(s);\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (f->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            if (!f->is_repeated())
                xx_ << "        if (!" << f->name() << "_.ParseFromStream(s)) return false;\n";
            else
                xx_ << "        if (!sp.parse(" << f->name() << "_, [&](" << refcomp_type_name(f, nb) << "& t){return t.ParseFromStream(s);})) return false;\n";
        } else
            xx_ << "        if (!sp.parse(" << f->name() << "_)) return false;\n";
    }
    xx_ << "        return true;\n";
    xx_ << "    }\n";

    // ByteSize
    xx_ << "    size_t ByteSize() const {\n"
        << "        size_t size = 0;\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (f->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            if (!f->is_repeated())
                xx_ << "        size += " << f->name() << "_.ByteSize();\n";
            else
                xx_ << "        size += refcomp::stream_unparser<refcomp::simple_ostream>::bytecount(" 
                    << f->name() << "_, [&](const " << refcomp_type_name(f, nb) << "& t){ return t.ByteSize();});\n";
        } else
            xx_ << "        size += refcomp::stream_unparser<refcomp::simple_ostream>::bytecount(" << f->name() << "_);\n";
    }
    xx_ << "        return size;\n"
        << "    }\n";

    // Clear
    xx_ << "    void Clear() {\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (f->is_repeated())
            xx_ << "        " << f->name() << "_.clear();\n";
        else if (f->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            xx_ << "        " << f->name() << "_ = ";
            write_default_value(xx_, f, nb);
            xx_ << ";\n";
        } else {
            xx_ << "        " << f->name() << "_.Clear();\n";
        }
    }
    xx_ << "    }\n";

    // operator<
    xx_ << "    bool operator<(const " << className << "& x) const;\n";

    // assign_nb_toggled
    xx_ << "    void assign_nb_toggled(const " << (nb ? "" : "nb_") << d->name() << "* x);\n";

    ap_ << "namespace " << d->file()->package() << "{\n"
        << "inline void " << className <<"::assign_nb_toggled(const " << (nb ? "" : "nb_") << d->name() << "* x) {\n"
        << "    Clear();\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (f->cpp_type() == gp::FieldDescriptor::CPPTYPE_MESSAGE) {
            if (has_string(f->message_type())) {
                if (f->is_repeated()) {
                    ap_ << "    " << f->name() << "_.resize(x->" << f->name() << "_size());\n"
                        << "    for (size_t i = 0; i < x->" << f->name() << "_size(); ++i)\n"
                        << "        " << f->name() << "_[i].assign_nb_toggled(&x->"<< f->name()<< "(i));\n";
                } else
                    ap_ << "    " << f->name() << "_.assign_nb_toggled(&x->"<< f->name()<< "());\n";
            } else
                    ap_ << "    " << f->name() << "_ = x->"<< f->name()<< "();\n";
        } else if (f->cpp_type() == gp::FieldDescriptor::CPPTYPE_STRING) {
                if (f->is_repeated()) {
                    ap_ << "    " << f->name() << "_.resize(x->" << f->name() << "_size());\n"
                        << "    for (size_t i = 0; i < x->" << f->name() << "_size(); ++i)\n"
                        << "        " << f->name() << "_[i].assign(x->"<< f->name()<< "(i).data(), x->" << f->name() << "(i).length());\n";
                } else
                    ap_ << "    " << f->name() << "_.assign(x->"<< f->name()<< "().data(), x->" << f->name() << "().length());\n";
        } else {
                ap_ << "    " << f->name() << "_ = x->"<< f->name()<< "();\n";
        }
    }
    ap_ << "}\n"
        << "}\n";
        

    // Getter/Setter
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (!f->is_repeated()) {
            xx_ << "    const " << refcomp_type_name(f, nb) << "& " << f->name() << "() const {\n"
                << "        return " << f->name() << "_;\n"
                << "    }\n";

            xx_ << "    " << className <<"& set_" << f->name() << "(const " << refcomp_type_name(f, nb) << "& v) {\n"
                << "        " << f->name() << "_ = v;\n"
	        << "        return *this;\n"
                << "    }\n";

            xx_ << "    " << refcomp_type_name(f, nb) << "* mutable_" << f->name() << "() {\n"
                << "        return &" << f->name() << "_;\n"
                << "    }\n";
        } else {
            xx_ << "    const std::vector<" << refcomp_type_name(f, nb) << ">& " << f->name() << "() const {\n"
                << "        return " << f->name() << "_;\n"
                << "    }\n";

            xx_ << "    const " << refcomp_type_name(f, nb) << "& " << f->name() << "(int index) const {\n"
                << "        return " << f->name() << "_[index];\n"
                << "    }\n";

            xx_ << "    size_t " << f->name() << "_size() const {\n"
                << "        return " << f->name() << "_.size();\n"
                << "    }\n";

            xx_ << "    " << className << "& set_" << f->name() << "(int index, const " << refcomp_type_name(f, nb) << "& v) {\n"
                << "        " << f->name() << "_[index] = v;\n"
		<< "        return *this;\n"
                << "    }\n";

            xx_ << "    " << refcomp_type_name(f, nb) << "* mutable_" << f->name() << "(int index) {\n"
                << "        return &" << f->name() << "_[index];\n"
                << "    }\n";

            xx_ << "    std::vector<" << refcomp_type_name(f, nb) << ">* mutable_" << f->name() << "() {\n"
                << "        return &" << f->name() << "_;\n"
                << "    }\n";

            xx_ << "    " << refcomp_type_name(f, nb) << "* add_" << f->name() << "() {\n"
                << "        " << f->name() << "_.resize(" << f->name() << "_" << ".size() + 1);\n"
                << "        return &" << f->name() << "_.back();\n"
                << "    }\n";

            xx_ << "    void add_" << f->name() << "(const " << refcomp_type_name(f, nb) << "& v) {\n"
                << "        " << f->name() << "_.push_back(v);\n"
                << "    }\n";
        }
    }
    // nb
    xx_ << "    static constexpr bool NB = " << (nb?"true":"false") << ";\n";
    // Storage
    xx_ << "  private:\n";
    for (int i = 0; i < d->field_count(); ++i) {
        auto f = d->field(i);
        if (!f->is_repeated())
            xx_ << "    " << refcomp_type_name(f, nb) << " " << f->name() << "_;\n";
        else
            xx_ << "    std::vector<" << refcomp_type_name(f, nb) << "> " << f->name() << "_;\n";
    }

    xx_ << "};\n"
        << "}; //namespace " << d->file()->package() << "\n";
}

int main(int argc, char* argv[]) {
    nbcg generator;
    return gpc::PluginMain(argc, argv, &generator);
}
