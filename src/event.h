// Copyright © 2001-2002 Rob Kaper <cap@capsi.com>,
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
#ifndef MONOP_EVENT_H
#define MONOP_EVENT_H

#include <time.h>
#include <sys/time.h>

#include "gameobject.h"

class Game;

class Event : public GameObject
{
public:
	enum EventType { Metaserver=0, TokenMovementTimeout, UserDecision, AuctionTimeout, PlayerTimeout, SocketTimeout };

	Event(int id, EventType type, Game *game = 0);

	void setLaunchTime(int ms);
	void setFrequency(int ms) { m_frequency = ms; }
	void setType(unsigned int type) { m_type = type; }
	void setObject(GameObject *object) { m_object = object; }

	struct timeval *launchTime() { return &m_launchTime; }
	int frequency() { return m_frequency; }
	unsigned int type() { return m_type; }
	GameObject *object() { return m_object; }

private:
	struct timeval m_launchTime;
	int m_frequency;
	unsigned int m_type;
	GameObject *m_object;
};

#endif // MONOP_EVENT_H
