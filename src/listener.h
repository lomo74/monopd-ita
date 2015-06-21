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

#ifndef NETWORK_LISTENER_H
#define NETWORK_LISTENER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string>
#include <vector>

class ListenPort;
class Socket;
class MonopdServer;

#define	LISTENQ	1024

class Listener
{
public:
	Listener(MonopdServer *server, const int port);
	virtual ~Listener();

	Socket *findSocket(int fd);

	void checkActivity();

	Socket *connectSocket(struct addrinfo *addrinfo);

protected:
	/*
	 * Add a listen port. Returns -1 on failure, 0 on success.
	 *
	 */
	int addListenPort(const int port);
	int addListenFd(const int fd);
	virtual void socketHandler( Socket *socket, const std::string &data = "" );

private:
	/*
	 * May return 0 in case the accept failed.
	 */
	Socket *acceptSocket(int fd);
	void delSocket(Socket *socket);

	fd_set m_readfdset;
	fd_set m_writefdset;

	std::vector<Socket *> m_sockets;
	std::vector<ListenPort *> m_listenPorts;

	MonopdServer *m_server;
};

#endif // NETWORK_LISTENER_H
