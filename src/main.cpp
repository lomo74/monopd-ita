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

#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>
#include <string>

#include "config.h"
#include "listener.h"
#include "socket.h"
#include "server.h"

int main(int argc, char **argv)
{
	srand( (unsigned) time(0) );
	signal( SIGPIPE, SIG_IGN );

	openlog( "monopd", LOG_PID, LOG_DAEMON );
	syslog( LOG_NOTICE, "monopd %s started", VERSION );

	MonopdServer *server = new MonopdServer(argc, argv);

	Listener *listener = new Listener( server );
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
			if (delSocket)
				delSocket->setStatus(Socket::Close);
		}
	}

	// Clean up memory
	delete server;
	delete listener;

	return 0;
}
