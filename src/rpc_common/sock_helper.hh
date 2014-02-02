#pragma once

#include "rpc_common/compiler.hh"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <string>

namespace rpc {

namespace common {

class sock_helper {
  public:
    static int connect(const char *host, int port, const char* localhost = "0.0.0.0", int localport = 0) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);
	struct sockaddr_in localaddr;
        make_sockaddr(localhost, localport, localaddr);
	int r = ::bind(fd, (struct sockaddr *) &localaddr, sizeof(localaddr));
	mandatory_assert(!r);

        struct sockaddr_in sin;
        make_sockaddr(host, port, sin);
        if (::connect(fd, (sockaddr *)&sin, sizeof(sin)) != 0) {
            close(fd);
            return -1;
        }
        return fd;
    }
    static int listen(const std::string& h, int port, int backlog = 0) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd >= 0);
	int yes = 1;
	int r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	mandatory_assert(r == 0);
	struct sockaddr_in sin;
        make_sockaddr(h.c_str(), port, sin);
	r = ::bind(fd, (struct sockaddr *) &sin, sizeof(sin));
	mandatory_assert(r == 0);
	r = ::listen(fd, backlog ? backlog : 100);
	mandatory_assert(r == 0);
	return fd;
    }
    static int listen(int port, int backlog = 0) {
        return listen("0.0.0.0", port, backlog);
    }
    static int accept(int fd) {
	struct sockaddr_in sin;
	socklen_t sinlen;
	return accept(fd, sin, sinlen);
    }
    static int accept(int fd, struct sockaddr_in &sin, socklen_t &sinlen) {
	sinlen = sizeof(sin);
	int rfd = ::accept(fd, (struct sockaddr *) &sin, &sinlen);
	if (rfd < 0)
	    perror("accept");
	return rfd;
    }
    static void make_nodelay(int fd) {
        int yes = 1;
        mandatory_assert(fd >= 0);
	int r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
	mandatory_assert(r == 0);
    }
    static void make_nonblock(int fd) {
	int r = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	mandatory_assert(r == 0);
    }
    static uint64_t get_uid(const char *host, int port) {
        sockaddr_in sin;
        make_sockaddr(host, port, sin);
        return((uint64_t)(sin.sin_addr.s_addr) << 32) | port;
    }
    static void peerinfo(int s, std::string& addr, int& port) {
        sockaddr_in sin;
        socklen_t len = sizeof(sin);
        int r = getpeername(s, (sockaddr*)&sin, &len);
        mandatory_assert(r == 0);
        port = ntohs(sin.sin_port);
        char buf[128];
        mandatory_assert(inet_ntop(sin.sin_family, &sin.sin_addr, buf, sizeof(buf)));
        addr.assign(buf, strlen(buf));
    }
  private:
    static void make_sockaddr(const char *host, int port, struct sockaddr_in &sin) {
        bzero(&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        in_addr_t a = inet_addr(host);
        if (a != INADDR_NONE)
            sin.sin_addr.s_addr = a;
        else {
            struct hostent *hp = gethostbyname(host);
            if (hp == NULL || hp->h_length != 4 || hp->h_addrtype != AF_INET) {
                fprintf(stderr, "Cannot get an IPV4 address for host %s\n", host);
                exit(-1);
            }
            sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
        }
        sin.sin_port = htons(port);
    }
};

} // namespace common
} // namespace rpc
