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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "socket.h"

Socket::Socket( int fd )
 :	m_status( Socket::New ),
	m_type( Socket::Player ),
	m_fd( fd ),
	m_addrinfoNext( NULL )
{
}

void Socket::ioWrite(const std::string data)
{
	if (!(m_status == New || m_status == Ok)) {
		return;
	}

	if (m_sendBuf.size() == 0) {
		ssize_t len = strlen(data.c_str());
		ssize_t written = write(m_fd, data.c_str(), len);
		if (written == len) {
			if (data.at(len -1) == '\n') {
				int flag = 1;
				setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
				flag = 0;
				setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
			}
			return;
		}

		if (written < 0) {
			switch(errno)  {
			case EAGAIN:
			case EINTR:
				m_sendBuf.append(data);
				break;
			default:
				setStatus(Socket::Close);
			}
			return;
		}

		m_sendBuf.append(data.substr(written, len - written));
		return;
	}

	m_sendBuf.append(data);
}

void Socket::sendMore()
{
	ssize_t len = strlen(m_sendBuf.c_str());
	if (len <= 0) {
		m_sendBuf.erase();
		return;
	}

	ssize_t written = write(m_fd, m_sendBuf.c_str(), len);
	if (written == len) {
		if (m_sendBuf.at(len -1) == '\n') {
			int flag = 1;
			setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
			flag = 0;
			setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
		}
		m_sendBuf.erase();
		return;
	}

	if (written < 0) {
		switch(errno)  {
		case EAGAIN:
		case EINTR:
			return;
		default:
			setStatus(Socket::Close);
		}
		return;
	}

	m_sendBuf.erase(0, written);
}

bool Socket::hasReadLine()
{
	static std::string newLine = "\r\n";
	std::string::size_type pos = m_recvBuf.find_first_of(newLine);

	return (!(pos == std::string::npos));
}

const std::string Socket::readLine()
{
	static std::string newLine = "\r\n";
	std::string::size_type pos = m_recvBuf.find_first_of(newLine);

	if (pos != std::string::npos)
	{
		// Grab first part for the listener
		std::string data = m_recvBuf.substr(0, pos);

		// Remove grabbed part from buffer
		m_recvBuf.erase(0, pos);

		// Remove all subsequent newlines
		m_recvBuf.erase(0, m_recvBuf.find_first_not_of(newLine));

		return data;
	}
	return std::string();
}

void Socket::fillBuffer(const std::string data)
{
	if (m_recvBuf.size())
		m_recvBuf.append(data);
	else
	{
		m_recvBuf.erase();
		m_recvBuf.append(data);
	}
}
