// Copyright (c) 2001-2004 Rob Kaper <cap@capsi.com>,
//               2001 Erik Bourget <ebourg@cs.mcgill.ca>
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
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>
#include <string>

#include "config.h"
#include "listener.h"
#include "socket.h"
#include "main.h"
#include "server.h"

#if USE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif /* USE_SYSTEMD_DAEMON */

MonopdListener::MonopdListener( MonopdServer *server )
 :	Listener(),
 	m_server( server )
{
#if USE_SYSTEMD_DAEMON
	int socket_count = sd_listen_fds(0);
	if (socket_count > 0) {
		for (int fd = SD_LISTEN_FDS_START; socket_count--; fd++) {
			addListenFd(fd);
		}
		syslog( LOG_NOTICE, "listener: systemd");
	} else
#endif /* USE_SYSTEMD_DAEMON */

	if ( addListenPort( server->port() ) == -1 )
	{
		syslog( LOG_ERR, "could not bind port %d, exiting", server->port() );
		exit(1);
	}
	else
		syslog( LOG_NOTICE, "listener: port=[%d]", server->port() );
}

void MonopdListener::socketHandler( Socket *socket, const std::string &data )
{
	switch( socket->status() )
	{
		case Socket::New:
			syslog( LOG_INFO, "connection: fd=[%d], ip=[%s]", socket->fd(), socket->ipAddr().c_str() );
			m_server->welcomeNew( socket );
			m_server->initSocketTimeoutEvent( socket->fd() );
			break;

		case Socket::Close:
		case Socket::Closed:
			syslog( LOG_INFO, "disconnect: fd=[%d], ip=[%s]", socket->fd(), socket->ipAddr().c_str() );
			m_server->closedSocket( socket );
			break;

		case Socket::Ok:
			switch (socket->type()) {
			case Socket::Player:
				m_server->processInput( socket, data );
				break;
			default:
				m_server->processInput( socket, data );
				break;
			}
	}
}

int main(int argc, char **argv)
{
	srand( (unsigned) time(0) );
	signal( SIGPIPE, SIG_IGN );

	openlog( "monopd", LOG_PID, LOG_DAEMON );
	syslog( LOG_NOTICE, "monopd %s started", VERSION );

	MonopdServer *server = new MonopdServer();
	if ( argc > 1 )
		server->setPort( atoi(argv[1]) );

	MonopdListener *listener = new MonopdListener( server );
	server->setListener(listener);

	// close stdin, stdout, stderr
	// close(0); close(1); close(2);

	server->initMetaserverEvent();

	/* Indicate to systemd that we are ready to answer requests. */
	server->updateSystemdStatus();

	for(;;)
	{
		// Check for network events
		listener->checkActivity();

		// Check for scheduled events in the timer
		int fd = server->processEvents();
		if(fd != -1)
		{
			Socket *delSocket = listener->findSocket(fd);
			if (delSocket && !server->findPlayer(delSocket))
				delSocket->setStatus(Socket::Close);
		}
	}

	// Clean up memory
	delete server;
	delete listener;

	return 0;
}
