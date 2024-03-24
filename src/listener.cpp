// Copyright © 2002-2004 Rob Kaper <rob@unixcode.org>.
//             2010-2015 Sylvain Rochet <gradator@gradator.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// version 2 as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING. If not, see
// <http://www.gnu.org/licenses/>.

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>

#if USE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif /* USE_SYSTEMD_DAEMON */

#include "listener.h"
#include "listenport.h"
#include "socket.h"
#include "server.h"

#define	MAXLINE	1024

Listener::Listener(MonopdServer *server, const int port)
{
	m_server = server;

	signal(SIGPIPE, SIG_IGN);

#if USE_SYSTEMD_DAEMON
	int socket_count = sd_listen_fds(0);
	if (socket_count > 0) {
		for (int fd = SD_LISTEN_FDS_START; socket_count--; fd++) {
			addListenFd(fd);
		}
		syslog(LOG_NOTICE, "listener: systemd");
	} else
#endif /* USE_SYSTEMD_DAEMON */

	if (addListenPort(port)) {
		syslog(LOG_ERR, "could not bind port %d, exiting", port);
#if USE_SYSTEMD_DAEMON
		sd_notifyf(1, "STATUS=Failed to start: could not bind port %d\nERRNO=%d", port, -2);
		usleep(100000);
#endif /* USE_SYSTEMD_DAEMON */
		exit(-2);
	}
	else
		syslog(LOG_NOTICE, "listener: port=[%d]", port);
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
	if ( listenPort->error() ) {
		delete listenPort;
	} else {
		m_listenPorts.push_back(listenPort);
	}

	listenPort = new ListenPort(AF_INET, "0.0.0.0", port);
	if ( listenPort->error() ) {
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
	for (std::vector<Socket *>::iterator it = m_sockets.begin() ; it != m_sockets.end() && (*it);) {
		if ((*it)->status() == Socket::Close || (*it)->status() == Socket::ConnectFailed) {
			socketHandler(*it);
			delSocket(*it);
			// damn vectors
			it = m_sockets.begin();
			continue;
		}

		++it;
	}

	// Construct file descriptor set for new events
	FD_ZERO(&m_readfdset);
	FD_ZERO(&m_writefdset);
	int highestFd = 0;

	ListenPort *listenPort = 0;
	for(std::vector<ListenPort *>::iterator it = m_listenPorts.begin() ; it != m_listenPorts.end() && (listenPort = *it) ; ++it)
	{
		if ( !listenPort->error() )
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
			if ((*it)->sendBufNotEmpty())
				FD_SET( (*it)->fd(), &m_writefdset );
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

	struct timeval tv, *tvp = NULL;
	int timeout = m_server->timeleftEvent();
	if (timeout >= 0) {
		// timeout is in ms
		tv.tv_sec = timeout/1000;
		tv.tv_usec = (timeout%1000)*1000;
		tvp = &tv;
	}

	// Check filedescriptors for input.
	if ( (select(highestFd+1, &m_readfdset, &m_writefdset, NULL, tvp)) <= 0 )
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
				(*it)->setStatus(Socket::Close);
				delete[] readBuf;
				continue;
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
			if ( (*it)->status() == Socket::Ok ) {
				(*it)->sendMore();
			}
			else if ( (*it)->status() == Socket::Connect ) {
				int err;
				int sockerr;
				socklen_t len = sizeof(sockerr);
				err = getsockopt((*it)->fd(), SOL_SOCKET, SO_ERROR, &sockerr, &len);
				if (err == 0 && sockerr == 0) {
					(*it)->setStatus(Socket::New);
					socketHandler( (*it) );
					(*it)->setStatus(Socket::Ok);
				} else {
					int socketFd;
					char ip_str[INET6_ADDRSTRLEN];
					syslog( LOG_INFO, "connect() failed: ip=[%s], error=[%s]", (*it)->ipAddr().c_str(), strerror(sockerr) );

					/* Try next */
					struct addrinfo *addrinfo = (*it)->addrinfoNext();
					for (; addrinfo != NULL; addrinfo = addrinfo->ai_next) {
						socketFd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
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

#ifdef TCP_CORK
						flags = 1;
						setsockopt(socketFd, IPPROTO_TCP, TCP_CORK, &flags, sizeof(int));
#endif /* TCP_CORK */

						ip_str[0] = '\0';
						if (addrinfo->ai_family == AF_INET) {
							inet_ntop(addrinfo->ai_family, &(((struct sockaddr_in *)addrinfo->ai_addr)->sin_addr), ip_str, INET6_ADDRSTRLEN);
						} else if(addrinfo->ai_family == AF_INET6) {
							inet_ntop(addrinfo->ai_family, &(((struct sockaddr_in6 *)addrinfo->ai_addr)->sin6_addr), ip_str, INET6_ADDRSTRLEN);
						}

						err = connect(socketFd, addrinfo->ai_addr, addrinfo->ai_addrlen);
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
					if (addrinfo == NULL) {
						(*it)->setStatus(Socket::ConnectFailed);
						continue;
					}

					close((*it)->fd());
					(*it)->setFd(socketFd);
					(*it)->setAddrinfoNext(addrinfo->ai_next);
					(*it)->setIpAddr(ip_str);
				}
			}
		}
	}
}

Socket *Listener::acceptSocket(int fd)
{
	struct sockaddr_storage clientaddr;
	char ip_str[INET6_ADDRSTRLEN];
	int flags;

	int len = sizeof(clientaddr);
	int socketFd = accept(fd, (struct sockaddr *) &clientaddr, (socklen_t *) &len);
	if (socketFd == -1) {
		if (errno == EINVAL) {
			syslog(LOG_INFO, "accept() failed: %s, probably because systemd socket activation [Socket] configuration use Accept=yes, that must be set to no", strerror(errno));
			exit(-2);
		}
		return 0;
	}

	Socket *socket = new Socket(socketFd);
	if(clientaddr.ss_family == AF_INET) {
		inet_ntop(clientaddr.ss_family, &(((struct sockaddr_in *)&clientaddr)->sin_addr), ip_str, INET6_ADDRSTRLEN);
		socket->setIpAddr(ip_str);
	} else if(clientaddr.ss_family == AF_INET6) {
		inet_ntop(clientaddr.ss_family, &(((struct sockaddr_in6 *)&clientaddr)->sin6_addr), ip_str, INET6_ADDRSTRLEN);
		socket->setIpAddr(ip_str);
	}

	// set socket to non-blocking
	flags = fcntl(socketFd, F_GETFL);
	if (flags >= 0) {
		flags |= O_NDELAY;
		fcntl(socketFd, F_SETFL, flags);
	}

#ifdef TCP_CORK
	flags = 1;
	setsockopt(socketFd, IPPROTO_TCP, TCP_CORK, &flags, sizeof(int));
#endif /* TCP_CORK */

	socket->setType( Socket::Player );
	m_sockets.push_back(socket);

	socketHandler( socket );
	socket->setStatus( Socket::Ok );

	return socket;
}

Socket *Listener::connectSocket(struct addrinfo *addrinfo) {
	int socketFd;
	int err;
	Socket *sock;
	int flags;
	char ip_str[INET6_ADDRSTRLEN];

	for (; addrinfo != NULL; addrinfo = addrinfo->ai_next) {
		socketFd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
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

#ifdef TCP_CORK
		flags = 1;
		setsockopt(socketFd, IPPROTO_TCP, TCP_CORK, &flags, sizeof(int));
#endif /* TCP_CORK */

		ip_str[0] = '\0';
		if (addrinfo->ai_family == AF_INET) {
			inet_ntop(addrinfo->ai_family, &(((struct sockaddr_in *)addrinfo->ai_addr)->sin_addr), ip_str, INET6_ADDRSTRLEN);
		} else if(addrinfo->ai_family == AF_INET6) {
			inet_ntop(addrinfo->ai_family, &(((struct sockaddr_in6 *)addrinfo->ai_addr)->sin6_addr), ip_str, INET6_ADDRSTRLEN);
		}

		err = connect(socketFd, addrinfo->ai_addr, addrinfo->ai_addrlen);
		if (err < 0 && errno != EINPROGRESS) {
			syslog(LOG_INFO, "connect() failed: ip=[%s], error=[%s]", ip_str, strerror(errno));
			goto connect_failed;
		}
		break;

connect_failed:
non_blocking_failed:
		close(socketFd);
	}

	if (addrinfo == NULL) {
		return NULL;
	}

	sock = new Socket(socketFd);
	sock->setType( Socket::Metaserver );
	sock->setStatus( Socket::Connect );
	sock->setAddrinfoNext(addrinfo->ai_next);
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
	switch (socket->type()) {

	case Socket::Player:
		switch (socket->status()) {

		case Socket::New:
			syslog( LOG_INFO, "connection: fd=[%d], ip=[%s]", socket->fd(), socket->ipAddr().c_str() );
			m_server->welcomeNew( socket );
			break;

		case Socket::Ok:
			m_server->processInput( socket, data );
			break;

		case Socket::Close:
			syslog( LOG_INFO, "disconnect: fd=[%d], ip=[%s]", socket->fd(), socket->ipAddr().c_str() );
			m_server->closedSocket( socket );
			break;

		case Socket::Connect:
		case Socket::ConnectFailed:
			break;
		}
		break;

	case Socket::Metaserver:
		switch (socket->status()) {

		case Socket::New:
			m_server->welcomeMetaserver( socket );
			break;

		case Socket::Close:
		case Socket::ConnectFailed:
			m_server->closedMetaserver(socket);
			break;

		case Socket::Connect:
		case Socket::Ok:
			break;
		}
		break;
	}
}
