// Copyright (c) 2002 Rob Kaper <rob@unixcode.org>. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "listenport.h"

#define	MAXLINE	1024

#ifdef USE_INET_ATON
#define inet_pton(a, b, c) inet_aton(b, c)
#endif

extern int errno;

ListenPort::ListenPort(sa_family_t family, const std::string ip, const int port)
{
	m_ipAddr = ip;
	m_port = port;
	m_isBound = false;
	m_fd = socket(family, SOCK_STREAM, 0);
	if(m_fd < 0) {
		return;
	}

	struct sockaddr_storage servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.ss_family = family;
	if(family == AF_INET) {
		struct sockaddr_in *servaddr_in = (struct sockaddr_in *)&servaddr;
		inet_pton(AF_INET, m_ipAddr.c_str(), &(servaddr_in->sin_addr));
		servaddr_in->sin_port = htons(m_port);
	} else if (family == AF_INET6) {
		struct sockaddr_in6 *servaddr_in6 = (struct sockaddr_in6 *)&servaddr;
		inet_pton(AF_INET6, m_ipAddr.c_str(), &(servaddr_in6->sin6_addr));
		servaddr_in6->sin6_port = htons(m_port);

		// bind on IPv6 only if possible
		int v6only = 1;
		setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
	} else {
		close(m_fd);
		return;
	}

	// release the socket after program crash, avoid TIME_WAIT
	int reuse = 1;
	if(setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
	{
		close(m_fd);
		return;
	}

	if( (bind(m_fd, (struct sockaddr *) &servaddr, sizeof(servaddr))) == -1)
	{
		close(m_fd);
		return;
	}
	m_isBound = true;

	if(listen(m_fd, LISTENQ) == -1)
	{
		close(m_fd);
		return;
	}

	// get current socket flags
	int flags;
	if ((flags=fcntl(m_fd, F_GETFL)) == -1)
		return;

	// set socket to non-blocking
	flags |= O_NDELAY;
	if (fcntl(m_fd, F_SETFL, flags) == -1)
		return;
}

ListenPort::ListenPort(int fd) {
	m_ipAddr = "";
	m_port = 0;
	m_isBound = true;
	m_fd = fd;

	// get current socket flags
	int flags;
	if ((flags=fcntl(m_fd, F_GETFL)) == -1)
		return;

	// set socket to non-blocking
	flags |= O_NDELAY;
	if (fcntl(m_fd, F_SETFL, flags) == -1)
		return;
}

bool ListenPort::isBound() const
{
	return m_isBound;
}
