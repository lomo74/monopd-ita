// Copyright (c) 2002-2004 Rob Kaper <rob@unixcode.org>. All rights reserved.
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

#ifndef LIBCAPSI_NETWORK_SOCKET_H
#define LIBCAPSI_NETWORK_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string>

/*
 * Class to handle incoming sockets. Can/will be used to store fd
 * information, etc etc.
 */

class Socket
{
public:
	enum Status { Connect, ConnectFailed, New, Ok, Close, Closed };
	enum Type { Player, Metaserver };

	Socket(int fd);
	void setStatus(Status status) { m_status = status; }
	Status status() { return m_status; }
	void setType(Type type) { m_type = type; }
	Type type() { return m_type; }

	ssize_t ioWrite(const std::string data);
	bool hasReadLine();
	const std::string readLine();
	void fillBuffer(const std::string data);

	void setFd(int _fd) { m_fd = _fd; }
	int fd() const { return m_fd; }
	void setIpAddr(const std::string ipAddr) { m_ipAddr = ipAddr; }
	std::string ipAddr() const { return m_ipAddr; }
	void setAddrinfoNext(struct addrinfo *next) { m_addrinfoNext = next; }
	struct addrinfo *addrinfoNext() { return m_addrinfoNext; }

private:
	Status m_status;
	Type m_type;
	int m_fd;
	std::string m_ipAddr, m_ioBuf;
	struct addrinfo *m_addrinfoNext;
};

#endif // LIBCAPSI_NETWORK_SOCKET_H
