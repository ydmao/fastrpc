#pragma once
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include "rpc_common/compiler.hh"
// stolen from Masstree

template <typename T>
struct epoll_tcpfds {
    int epollfd;
    epoll_tcpfds(int pipefd) {
        epollfd = epoll_create(10);
        if (epollfd < 0) {
            perror("epoll_create");
            exit(EXIT_FAILURE);
        }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = (void *) 1;
        int r = epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd, &ev);
        mandatory_assert(r == 0);
    }

    enum { max_events = 100 };
    typedef struct epoll_event eventset[max_events];
    int wait(eventset &es) {
        return epoll_wait(epollfd, es, max_events, -1);
    }

    T *event_conn(eventset &es, int i) const {
        return (T *) es[i].data.ptr;
    }

    void add(int fd, T *c) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = c;
        int r = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
        mandatory_assert(r == 0);
    }

    void remove(int fd) {
        int r = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        mandatory_assert(r == 0);
    }
};

template <typename T>
struct select_tcpfds {
    int pipefd;
    int nfds;
    fd_set rfds;
    std::vector<T *> conns;

    select_tcpfds(int pipefd_) {
        mandatory_assert(pipefd < FD_SETSIZE);
        FD_ZERO(&rfds);
        FD_SET(pipefd_, &rfds);
        pipefd = pipefd_;
        nfds = pipefd + 1;
        conns.resize(nfds, 0);
        conns[pipefd] = (T *) 1;
    }

    typedef fd_set eventset;
    int wait(eventset &es) {
        es = rfds;
        return select(nfds, &es, 0, 0, 0);
    }

    T *event_conn(eventset &es, int i) const {
        return FD_ISSET(i, &es) ? conns[i] : 0;
    }

    void add(int fd, T *c) {
        mandatory_assert(fd < FD_SETSIZE);
        FD_SET(fd, &rfds);
        if (fd >= nfds) {
            nfds = fd + 1;
            conns.resize(nfds, 0);
        }
        conns[fd] = c;
    }

    void remove(int fd) {
        mandatory_assert(fd < FD_SETSIZE);
        FD_CLR(fd, &rfds);
        if (fd == nfds - 1) {
            while (nfds > 0 && !FD_ISSET(nfds - 1, &rfds))
                --nfds;
        }
    }
};
