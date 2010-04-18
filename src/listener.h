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

#ifndef LIBCAPSI_NETWORK_LISTENER_H
#define LIBCAPSI_NETWORK_LISTENER_H

#include <sys/types.h>
     
#include <string>
#include <vector>

class ListenPort;
class Socket;

#define	LISTENQ	1024

class Listener
{
public:
	Listener();
	virtual ~Listener();

	Socket *findSocket(unsigned int fd);

	void checkActivity();
		
protected:
	/*
	 * Add a listen port. Returns -1 on failure, 0 on success.
	 *
	 */
	int addListenPort(const int port);
	virtual void socketHandler( Socket *socket, const std::string &data = "" );

private:
	/*
	 * May return 0 in case the accept failed.
	 */
	Socket *newSocket(unsigned int fd);
	void delSocket(Socket *socket);

	fd_set m_fdset;

	std::vector<Socket *> m_sockets;
	std::vector<ListenPort *> m_listenPorts;
};

#endif
