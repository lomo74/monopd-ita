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

#ifndef __MONOPD_MAIN_H__
#define	__MONOPD_MAIN_H__

class MonopdServer;
class Socket;

#include <string>
#include <libcapsinetwork/listener.h>

class MonopdListener : public Listener
{
public:
	MonopdListener( MonopdServer *server );
	void socketHandler( Socket *socket, const std::string &data = "" );

private:
	Listener *m_listener;
	MonopdServer *m_server;
};

int main(int, char **);

#endif
