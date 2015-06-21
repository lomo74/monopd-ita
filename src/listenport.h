// Copyright © 2002 Rob Kaper <rob@unixcode.org>.
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

#ifndef NETWORK_LISTENPORT_H
#define NETWORK_LISTENPORT_H

#include <string>

#define	LISTENQ	1024

class ListenPort
{
public:
	ListenPort(sa_family_t family, const std::string host, const int port); // for a traditional bind socket
	ListenPort(int fd); // for an already bound socket (e.g. using systemd socket activation)
	const std::string ipAddr() { return m_ipAddr; }
	int port() { return m_port; }
	int fd() { return m_fd; }
	bool isBound() const;

private:
	std::string m_ipAddr;
	int m_fd, m_port;
	bool m_isBound;
};

#endif // NETWORK_LISTENPORT_H
