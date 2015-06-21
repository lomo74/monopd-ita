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

#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

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
	enum Status { Connect, ConnectFailed, New, Ok, Close };
	enum Type { Player, Metaserver };

	Socket(int fd);
	void setStatus(Status status) { m_status = status; }
	Status status() { return m_status; }
	void setType(Type type) { m_type = type; }
	Type type() { return m_type; }

	void ioWrite(const std::string data);
	void sendMore();
	bool hasReadLine();
	const std::string readLine();
	void fillBuffer(const std::string data);
	bool sendBufNotEmpty() { return (m_sendBuf.size() > 0); };

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
	std::string m_ipAddr, m_recvBuf, m_sendBuf;
	struct addrinfo *m_addrinfoNext;
};

#endif // NETWORK_SOCKET_H
