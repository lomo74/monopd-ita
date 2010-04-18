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

ListenPort::ListenPort(const std::string ip, const unsigned int port)
{
	m_ipAddr = ip;
	m_port = port;
	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	m_isBound = false;

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, m_ipAddr.c_str(), &servaddr.sin_addr);
//	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any host may connect
	servaddr.sin_port = htons(m_port);

	struct hostent *host;
	if((host = gethostbyaddr((char *)&servaddr.sin_addr, sizeof(servaddr.sin_addr), servaddr.sin_family)) != NULL)
		m_fqdn = host->h_name;
	else
		m_fqdn = m_ipAddr;

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

bool ListenPort::isBound() const
{
	return m_isBound;
}
