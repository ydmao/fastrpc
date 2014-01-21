#pragma once

#include "rpc_common/compiler.hh"
#include "rpc_common/spinlock.hh"
#include <ev++.h>
#include <pthread.h>

namespace rpc {

/** @brief Non-Nested Loop. The nn_loop abstraction ensures that
     there is a one-to-one mapping between an ev::loop_ref and a pthread.  This
    is achieved by two design. First, each nn_loop has a unique loop_ref object,
    which is ensured by creating a new ev_loop object per nn_loop.  Second, a
    nn_loop can be used by the creating thread only.
 */
struct nn_loop {
#if (__clang__ && __APPLE__)
    static nn_loop *get_tls_loop() {
      if(tls_loop_key_ == 0){
        static rpc::common::spinlock lock_;
        acquire(&lock_);
        if(tls_loop_key_ == 0)
          pthread_key_create(&tls_loop_key_, NULL);
        release(&lock_);
        mandatory_assert(tls_loop_key_);
      }
      nn_loop *tl = (nn_loop *) pthread_getspecific(tls_loop_key_);
      if(tl == 0){
            static rpc::common::spinlock lock_;
            static bool def_loop_used_ = false;
            acquire(&lock_);
            if (!def_loop_used_) {
                tl = new nn_loop(ev::get_default_loop());
                def_loop_used_ = true;
            } else
                tl = new nn_loop(ev::loop_ref(ev_loop_new(ev::AUTO)));
            release(&lock_);
            pthread_setspecific(tls_loop_key_, tl);
            mandatory_assert(pthread_getspecific(tls_loop_key_));
      }
      return tl;
    }
#else
    static nn_loop *get_tls_loop() {
        if (__builtin_expect(!tls_loop_, 0)) {
            static rpc::common::spinlock lock_;
            static bool def_loop_used_ = false;
            acquire(&lock_);
            if (!def_loop_used_) {
                tls_loop_ = new nn_loop(ev::get_default_loop());
                def_loop_used_ = true;
            } else
                tls_loop_ = new nn_loop(ev::loop_ref(ev_loop_new(ev::AUTO)));
            release(&lock_);
        }
        return tls_loop_;
    }
#endif
    static nn_loop *get_loop(nn_loop *loop = 0) {
	return loop ? loop : get_tls_loop();
    }
    int enter() {
        return ++ nest_;
    }
    int leave() {
        return -- nest_;
    }
    void run_once() {
        mandatory_assert(nest_ == 1 && pthread_self() == tid_);
        loop_.run(ev::ONCE);
    }
    void run() {
        mandatory_assert(nest_ == 1 && pthread_self() == tid_);
        loop_.run();
    }
    ev::loop_ref ev_loop() const {
	return loop_;
    }
    ev::async *new_ev_async() {
        return new ev::async(loop_);
    }
    void destroy_ev_async(ev::async *e) {
        delete e;
    }
    void break_loop() {
        loop_.break_loop();
    }
    void post_fork() {
        loop_.post_fork();
    }
  private:
#if (__clang__ && __APPLE__)
    static pthread_key_t tls_loop_key_;
#else
    static __thread nn_loop *tls_loop_;
#endif
    nn_loop(const ev::loop_ref &loop) : nest_(0), loop_(loop) {
        tid_ = pthread_self();
    }
    pthread_t tid_;
    int nest_;
    ev::loop_ref loop_;
};

}
