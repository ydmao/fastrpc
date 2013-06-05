#ifndef APP_RPCC_HELPER
#define APP_RPCC_HELPER 1

namespace rpc {

template <typename T>
class make_reply_helper {
  public:
    make_reply_helper(T &x)
	: x_(x) {
    }
    template <typename U>
    void operator()(const U &, const T &x) {
	x_ = x;
    }
  private:
    T &x_;
};

template <typename T> make_reply_helper<T> make_reply(T &x) {
    x.set_eno(appns::UNKNOWN);
    return make_reply_helper<T>(x);
}

template <typename T, typename REQ, typename REPLY,
	  void (T::*method)(REPLY &)>
class make_unary_call_helper {
  public:
    make_unary_call_helper(T *obj) : obj_(obj) {
    }
    void operator()(REQ &, REPLY &x) {
	(obj_->*method)(x);
    }
  private:
    T *obj_;
};

template <typename T, typename REQ, typename REPLY,
	  void (T::*method)(REQ &, REPLY &)>
class make_binary_call_helper {
  public:
    make_binary_call_helper(T *obj) : obj_(obj) {
    }
    void operator()(REQ &x, REPLY &y) {
	(obj_->*method)(x, y);
    }
  private:
    T *obj_;
};

} // namespace rpc

#define APP_DECLARE_ONE_MAKE_CALL(proc, REQ, REPLY, SELF)			\
    template <void (SELF::*method)(appns::REPLY &)>				\
    inline make_unary_call_helper<SELF, appns::REQ, appns::REPLY, method> make_call() { \
	return rpc::make_unary_call_helper<SELF, appns::REQ, appns::REPLY, method>(this);	\
    }									\
    template <void (SELF::*method)(appns::REQ &, appns::REPLY &)>			\
    inline make_binary_call_helper<SELF, appns::REQ, appns::REPLY, method> make_call() { \
	return rpc::make_binary_call_helper<SELF, appns::REQ, appns::REPLY, method>(this);	\
    }
#define APP_DECLARE_MAKE_CALL(SELF)					\
    RPC_FOR_EACH_CLIENT_MESSAGE(APP_DECLARE_ONE_MAKE_CALL, SELF) \
    RPC_FOR_EACH_INTERCONNECT_MESSAGE(APP_DECLARE_ONE_MAKE_CALL, SELF)

#endif
