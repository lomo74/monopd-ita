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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>

#include "listener.h"
#include "listenport.h"
#include "socket.h"

#define	MAXLINE	1024

Listener::Listener()
{
}

Listener::~Listener()
{
	while (!m_listenPorts.empty()) { delete *m_listenPorts.begin(); m_listenPorts.erase(m_listenPorts.begin()); }
	while (!m_sockets.empty()) { delete *m_sockets.begin(); m_sockets.erase(m_sockets.begin()); }
}

int Listener::addListenPort(const int port)
{
	ListenPort *listenPort;

	listenPort = new ListenPort(AF_INET6, "::", port);
	if ( !listenPort->isBound() ) {
		delete listenPort;
	} else {
		m_listenPorts.push_back(listenPort);
	}

	listenPort = new ListenPort(AF_INET, "0.0.0.0", port);
	if ( !listenPort->isBound() ) {
		delete listenPort;
		return -1;
	}
	m_listenPorts.push_back(listenPort);
	return 0;
}

int Listener::addListenFd(const int fd) {
	ListenPort *listenPort = new ListenPort(fd);
	m_listenPorts.push_back(listenPort);
	return 0;
}

void Listener::checkActivity()
{
	// Notify socket close events and delete them.
	for ( std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; )
		if ( (*it)->status() == Socket::Close || (*it)->status() == Socket::Closed
			|| (*it)->status() == Socket::ConnectFailed )
		{
			socketHandler( (*it) );
			delSocket( (*it) );
			it = m_sockets.begin();
			continue;
		}
		else
			++it;

	// Construct file descriptor set for new events
	FD_ZERO(&m_readfdset);
	FD_ZERO(&m_writefdset);
	int highestFd = 0;

	ListenPort *listenPort = 0;
	for(std::vector<ListenPort *>::iterator it = m_listenPorts.begin() ; it != m_listenPorts.end() && (listenPort = *it) ; ++it)
	{
		if ( listenPort->isBound() )
		{
			FD_SET(listenPort->fd(), &m_readfdset);
			if (listenPort->fd() > highestFd)
				highestFd = listenPort->fd();
		}
	}

	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; ++it)
	{
		if ( (*it)->status() == Socket::Ok ) {
			FD_SET( (*it)->fd(), &m_readfdset );
			if ( (*it)->fd() > highestFd )
				highestFd = (*it)->fd();
		}
		/* connection in progress */
		else if ( (*it)->status() == Socket::Connect ) {
			FD_SET( (*it)->fd(), &m_writefdset );
			if ( (*it)->fd() > highestFd )
				highestFd = (*it)->fd();
		}
	}

	// No file descriptors are opened, exit.
	if ( !highestFd )
	{
		// FIXME: try to (re)bind unbound listenports
		sleep(1);
		exit(1);
		return;
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000; // perhaps decrease with increasing amount of sockets, or make this configurable?

	// Check filedescriptors for input.
	if ( (select(highestFd+1, &m_readfdset, &m_writefdset, NULL, &tv)) <= 0 )
		return;

	// Check for new connections
	for(std::vector<ListenPort *>::iterator it = m_listenPorts.begin() ; it != m_listenPorts.end() && (listenPort = *it) ; ++it)
		if (FD_ISSET(listenPort->fd(), &m_readfdset))
		{
			acceptSocket( listenPort->fd() );
		}

	// Check socket data.
	for ( std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; ++it )
	{
		if ( FD_ISSET( (*it)->fd(), &m_readfdset ) )
		{
			char *readBuf = new char[MAXLINE+1]; // MAXLINE + '\0'
			int n = read((*it)->fd(), readBuf, MAXLINE);
			if (n <= 0) // socket was closed
			{
				(*it)->setStatus(Socket::Closed);
				delete[] readBuf;
				return; // notification is (still) in earlier iteration
			}
			readBuf[n] = 0;
			(*it)->fillBuffer(readBuf);
			delete[] readBuf;

			while ( (*it)->hasReadLine() )
			{
				std::string data = (*it)->readLine();

				// Return activity if we have a line
				if ( data.size() > 0 )
				{
					socketHandler( (*it), data );
					continue;
				}
			}
		}

		if ( FD_ISSET( (*it)->fd(), &m_writefdset ) ) {
			if ( (*it)->status() == Socket::Connect ) {
				int err;
				int sockerr;
				socklen_t len = sizeof(sockerr);
				err = getsockopt((*it)->fd(), SOL_SOCKET, SO_ERROR, &sockerr, &len);
				if (err == 0 && sockerr == 0) {
					(*it)->clearAddrinfo(); // no longer needed
					(*it)->setStatus(Socket::New);
					socketHandler( (*it) );
					(*it)->setStatus(Socket::Ok);
				} else {
					int socketFd;
					char ip_str[INET6_ADDRSTRLEN];
					syslog( LOG_INFO, "connect() failed: ip=[%s], error=[%s]", (*it)->ipAddr().c_str(), strerror(sockerr) );

					/* Try next */
					struct addrinfo *rp = (*it)->addrinfoCursor()->ai_next;
					for (; rp != NULL; rp = rp->ai_next) {
						socketFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
						if (socketFd < 0)
							continue;

						// get current socket flags
						int flags = fcntl(socketFd, F_GETFL);
						if (flags < 0)
							goto non_blocking_failed;

						// set socket to non-blocking
						flags |= O_NDELAY;
						if (fcntl(socketFd, F_SETFL, flags) < 0)
							goto non_blocking_failed;

						if (rp->ai_family == AF_INET) {
							inet_ntop(rp->ai_family, &(((struct sockaddr_in *)rp->ai_addr)->sin_addr), ip_str, INET6_ADDRSTRLEN);
						} else if(rp->ai_family == AF_INET6) {
							inet_ntop(rp->ai_family, &(((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr), ip_str, INET6_ADDRSTRLEN);
						} else {
							ip_str[0] = '\0';
						}

						err = connect(socketFd, rp->ai_addr, rp->ai_addrlen);
						if (err < 0 && errno != EINPROGRESS) {
							syslog(LOG_INFO, "connect() failed: ip=[%s], error=[%s]", ip_str, strerror(errno));
							goto connect_failed;
						}
						break;

connect_failed:
non_blocking_failed:
						close(socketFd);
					}

					// Connect failed
					if (rp == NULL) {
						(*it)->setStatus(Socket::ConnectFailed);
						continue;
					}

					close((*it)->fd());
					(*it)->setFd(socketFd);
					(*it)->setAddrinfoCursor(rp);
					(*it)->setIpAddr(ip_str);
				}
			}
		}
	}
}

Socket *Listener::acceptSocket(int fd)
{
// TODO: reenable softbooting code
//	if (!isNew)
//	{
//		FD_SET(fd, &m_readfdset);
//
//		Socket *socket = new Socket(fd);
//		socket->setStatus(Socket::Ok);
//		m_sockets.push_back(socket);
//
//		return socket;
//	}

	struct sockaddr_storage clientaddr;
//	struct hostent *host;
	char ip_str[INET6_ADDRSTRLEN];
	int flags;

	int len = sizeof(clientaddr);
	int socketFd = accept(fd, (struct sockaddr *) &clientaddr, (socklen_t *) &len);
	if (socketFd == -1)
		return 0;

	Socket *socket = new Socket(socketFd);
	if(clientaddr.ss_family == AF_INET) {
		inet_ntop(clientaddr.ss_family, &(((struct sockaddr_in *)&clientaddr)->sin_addr), ip_str, INET6_ADDRSTRLEN);
		socket->setIpAddr(ip_str);
	} else if(clientaddr.ss_family == AF_INET6) {
		inet_ntop(clientaddr.ss_family, &(((struct sockaddr_in6 *)&clientaddr)->sin6_addr), ip_str, INET6_ADDRSTRLEN);
		socket->setIpAddr(ip_str);
	}
//	TODO: redo that asynchronously (and using getnameinfo() instead)
//	if( (host = gethostbyaddr((char *)&clientaddr.sin_addr, sizeof(clientaddr.sin_addr), AF_INET)) != NULL)
//		socket->setFqdn(host->h_name);

	// set socket to non-blocking
	flags = fcntl(socketFd, F_GETFL);
	if (flags >= 0) {
		flags |= O_NDELAY;
		fcntl(socketFd, F_SETFL, flags);
	}

	socket->setType( Socket::Player );
	m_sockets.push_back(socket);

	socketHandler( socket );
	socket->setStatus( Socket::Ok );

	return socket;
}

Socket *Listener::connectSocket(const std::string &host, int port) {
	int socketFd;
	int err;
	Socket *sock;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char port_str[6];
	int r;
	int flags;
	char ip_str[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(port_str, sizeof(port_str), "%d", port);
	r = getaddrinfo(host.c_str(), port_str, &hints, &result);
	if (r != 0) {
		syslog(LOG_INFO, "getaddrinfo() failed: error=[%s]", gai_strerror(r));
		return NULL;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		socketFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (socketFd < 0)
			continue;

		// get current socket flags
		flags = fcntl(socketFd, F_GETFL);
		if (flags < 0)
			goto non_blocking_failed;

		// set socket to non-blocking
		flags |= O_NDELAY;
		if (fcntl(socketFd, F_SETFL, flags) < 0)
			goto non_blocking_failed;

		if (rp->ai_family == AF_INET) {
			inet_ntop(rp->ai_family, &(((struct sockaddr_in *)rp->ai_addr)->sin_addr), ip_str, INET6_ADDRSTRLEN);
		} else if(rp->ai_family == AF_INET6) {
			inet_ntop(rp->ai_family, &(((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr), ip_str, INET6_ADDRSTRLEN);
		} else {
			ip_str[0] = '\0';
		}

		err = connect(socketFd, rp->ai_addr, rp->ai_addrlen);
		if (err < 0 && errno != EINPROGRESS) {
			syslog(LOG_INFO, "connect() failed: ip=[%s], error=[%s]", ip_str, strerror(errno));
			goto connect_failed;
		}
		break;

connect_failed:
non_blocking_failed:
		close(socketFd);
	}

	if (rp == NULL) {
		freeaddrinfo(result);
		return NULL;
	}

	sock = new Socket(socketFd);
	sock->setType( Socket::Metaserver );
	sock->setStatus( Socket::Connect );
	sock->setAddrinfoResult(result);
	sock->setAddrinfoCursor(rp);
	sock->setIpAddr(ip_str);
	m_sockets.push_back(sock);
	socketHandler( sock );
	return sock;
}

void Listener::delSocket(Socket *socket)
{
	FD_CLR(socket->fd(), &m_readfdset);
	FD_CLR(socket->fd(), &m_writefdset);
	shutdown(socket->fd(), SHUT_RDWR);
	close(socket->fd());
	socket->clearAddrinfo();

	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; ++it)
		if (*it == socket)
		{
			delete *it;
			m_sockets.erase(it);
			return;
		}
}

Socket *Listener::findSocket(int fd)
{
	Socket *socket = 0;
	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (socket = *it) ; ++it)
		if (socket->fd() == fd)
			return socket;

	return 0;
}

void Listener::socketHandler( Socket *socket, const std::string &data )
{
	(void)socket;
	(void)data;
}
