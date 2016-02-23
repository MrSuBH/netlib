/*
 * Acceptor.cpp
 *
 *  Created on: Dec 16,2015
 *      Author: zhangyalei
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include "Accept.h"
#include "Common_Func.h"
#include "Lib_Log.h"
#include "Server.h"

Accept::Accept(void):
	server_(0),
	port_(0),
  listenfd_(0)
{ }

Accept::~Accept(void) { }

void Accept::set(Server *server, int port) {
	server_ = server;
	port_ = port;
}

int Accept::init() {
	return 0;
}

int Accept::fini(void) {
	LOG_DEBUG("close listenfd = %d", listenfd_);
	::close(listenfd_);
	return 0;
}

void Accept::server_listen(void) {
	struct sockaddr_in serveraddr;

	if ((listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		LOG_SYS("socket");
		LOG_ABORT();
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port_);

	int flag = 1;
	if (::setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
		LOG_SYS("setsockopt SO_REUSEADDR");
		LOG_ABORT();
	}

	int peer_buf_len = 2 * 1024 * 1024;
	if (::setsockopt(listenfd_, SOL_SOCKET, SO_SNDBUF, &peer_buf_len, sizeof(peer_buf_len)) == -1) {
		LOG_SYS("setsockopt SO_REUSEADDR");
		LOG_ABORT();
	}
	if (::setsockopt(listenfd_, SOL_SOCKET, SO_RCVBUF, &peer_buf_len, sizeof(peer_buf_len)) == -1) {
		LOG_SYS("setsockopt SO_REUSEADDR");
		LOG_ABORT();
	}

	//socklen_t len= sizeof(int);
	//int i = ::getsockopt(listenfd_, SOL_SOCKET, SO_SNDTIMEO, (char*)&so_send_timeo, &len);
	//int so_send_timeo = 100;

	struct timeval timeout = {1, 0};//0.1s
	if (::setsockopt(listenfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
		LOG_SYS("setsockopt SO_REUSEADDR");
		LOG_ABORT();
	}/*
	if (::setsockopt(listenfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
		LOG_SYS("setsockopt SO_REUSEADDR");
		LOG_ABORT();
	}
	*/

	if (::bind(listenfd_, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1) {
		LOG_SYS("bind");
		LOG_ABORT();
	}
	if (::listen(listenfd_, 1024) == -1) {
		LOG_SYS("listen");
		LOG_ABORT();
	}

	LOG_DEBUG("listen at %d", port_);
}

void Accept::server_accept(void) {
	int connfd = 0;
	struct sockaddr_in client_addr;
	socklen_t addr_len = 0;
	char client_ip[128];

	while (1) {
		memset(&client_addr, 0, sizeof(client_addr));
		addr_len = sizeof(client_addr);
		if ((connfd = ::accept4(listenfd_, (struct sockaddr *) &client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC)) == -1) {
			LOG_SYS("accept");
			continue;
		}
		struct linger so_linger;
		so_linger.l_onoff = 1;
		so_linger.l_linger = 0;
		if (::setsockopt(connfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0) {
			LOG_SYS("setsockopt linger");
			continue;
		}

/** turn on/off TCP Nagel algorithm
		int nodelay_on=1;
		if (::setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay_on, sizeof(nodelay_on)) < 0) {
			LOG_SYS("setsockopt nodelay");
		}
*/

		memset(client_ip, 0, sizeof(client_ip));
		if (::inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL) {
			LOG_SYS("inet_ntop");
			continue;
		}

		LOG_DEBUG("client ip = [%s], port = %d", client_ip, ntohs(client_addr.sin_port));

		accept_svc(connfd);
	}
}

int Accept::accept_svc(int connfd) {
	LOG_USER_TRACE("SHOULD NOT HERE");
	return 0;
}

void Accept::run_handler(void) {
	LOG_DEBUG("start acceptor");
	server_listen();
	server_accept();
}

void Accept::exit_handler(void) {
	fini();
}

int Accept::get_port(void) {
	return port_;
}
