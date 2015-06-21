// Copyright © 2001-2003 Rob Kaper <cap@capsi.com>,
//             2001 Erik Bourget <ebourg@cs.mcgill.ca>
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

#include <stdlib.h>
#include <sys/time.h>

#include "event.h"

Event::Event(int id, EventType type, Game *game) : GameObject(id, GameObject::Unknown, game)
{
	m_type = type;
	m_launchTime.tv_sec = 0;
	m_launchTime.tv_usec = 0;
	m_frequency = 0;
	m_object = NULL;
}

void Event::setLaunchTime(int ms) {
	struct timeval timenow, timeout = { .tv_sec = ms/1000, .tv_usec = (ms%1000)*1000 };

	gettimeofday(&timenow, NULL);
	timeradd(&timenow, &timeout, &m_launchTime);
}
