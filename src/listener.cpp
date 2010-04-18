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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <unistd.h>

#include "listener.h"
#include "listenport.h"
#include "socket.h"

#define	MAXLINE	1024

extern int errno;

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
	ListenPort *listenPort = new ListenPort("0.0.0.0", port);
	m_listenPorts.push_back(listenPort);
	if ( !listenPort->isBound() )
		return -1;
	return 0;
}

void Listener::checkActivity()
{
	// Notify socket close events and delete them.
	for ( std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; )
		if ( (*it)->status() == Socket::Close || (*it)->status() == Socket::Closed )
		{
			socketHandler( (*it) );
			delSocket( (*it) );
			it = m_sockets.begin();
			continue;
		}
		else
			++it;

	// Construct file descriptor set for new events
	FD_ZERO(&m_fdset);
	unsigned int highestFd = 0;

	ListenPort *listenPort = 0;
	for(std::vector<ListenPort *>::iterator it = m_listenPorts.begin() ; it != m_listenPorts.end() && (listenPort = *it) ; ++it)
	{
		if ( listenPort->isBound() )
		{
			FD_SET(listenPort->fd(), &m_fdset);
			if (listenPort->fd() > highestFd)
				highestFd = listenPort->fd();
		}
	}

	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; ++it)
	{
		FD_SET( (*it)->fd(), &m_fdset );
		if ( (*it)->fd() > highestFd )
			highestFd = (*it)->fd();
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
	if ( (select(highestFd+1, &m_fdset, NULL, NULL, &tv)) <= 0 )
		return;

	// Check for new connections
	for(std::vector<ListenPort *>::iterator it = m_listenPorts.begin() ; it != m_listenPorts.end() && (listenPort = *it) ; ++it)
		if (FD_ISSET(listenPort->fd(), &m_fdset))
		{
			newSocket( listenPort->fd() );
		}

	// Check socket data.
	for ( std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; ++it )
	{
		if ( (*it)->status() == Socket::Ok && FD_ISSET( (*it)->fd(), &m_fdset ) )
		{
			char *readBuf = new char[MAXLINE];
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
	}
}

Socket *Listener::newSocket(unsigned int fd)
{
// TODO: reenable softbooting code
//	if (!isNew)
//	{
//		FD_SET(fd, &m_fdset);
//
//		Socket *socket = new Socket(fd);
//		socket->setStatus(Socket::Ok);
//		m_sockets.push_back(socket);
//
//		return socket;
//	}

	struct sockaddr_in clientaddr;
	struct hostent *host;

	unsigned int len = sizeof(clientaddr);
	unsigned int socketFd = accept(fd, (struct sockaddr *) &clientaddr, (socklen_t *) &len);
	if (socketFd == -1)
		return 0;

	Socket *socket = new Socket(socketFd);
	socket->setIpAddr((char *) inet_ntoa(clientaddr.sin_addr));
	if( (host = gethostbyaddr((char *)&clientaddr.sin_addr, sizeof(clientaddr.sin_addr), AF_INET)) != NULL)
		socket->setFqdn(host->h_name);
	m_sockets.push_back(socket);

	socketHandler( socket );
	socket->setStatus( Socket::Ok );

	return socket;
}

void Listener::delSocket(Socket *socket)
{
	FD_CLR(socket->fd(), &m_fdset);
	close(socket->fd());
		
	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it) ; ++it)
		if (*it == socket)
		{
			delete *it;
			m_sockets.erase(it);
			return;
		}
}

Socket *Listener::findSocket(unsigned int fd)
{
	Socket *socket = 0;
	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (socket = *it) ; ++it)
		if (socket->fd() == fd)
			return socket;

	return 0;
}

void Listener::socketHandler( Socket *socket, const std::string &data )
{
	switch( socket->status() )
	{
		case Socket::New:
			socket->ioWrite( std::string( "you are a new connection. welcome.\n" ) );
			break;
			
		case Socket::Close:
		case Socket::Closed:
			break;
			
		case Socket::Ok:
			socket->ioWrite( std::string( "your input (" + data + ") is appreciated.\n" ) );
			break;
	}
}
