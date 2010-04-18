// Copyright (c) 2001-2002 Rob Kaper <cap@capsi.com>,
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

#ifndef MONOP_EVENT_H
#define MONOP_EVENT_H

#include <time.h>

#include "gameobject.h"

class Game;

class Event : public GameObject
{
public:
	enum EventType { Monopigator=0, TokenMovementTimeout, UserDecision, AuctionTimeout, PlayerTimeout, SocketTimeout };

	Event(int id, EventType type, Game *game = 0);

	void setLaunchTime(time_t _lt) { m_launchTime = _lt; }
	void setFrequency(time_t frequency) { m_frequency = frequency; }
	void setType(unsigned int _t) { m_type = _t; }
	void setObject(GameObject *object) { m_object = object; }

	time_t launchTime() { return m_launchTime; }
	time_t frequency() { return m_frequency; }
	unsigned int type() { return m_type; }
	GameObject *object() { return m_object; }

private:
	time_t m_launchTime;
	time_t m_frequency;
	unsigned int m_type;
	GameObject *m_object;
};

#endif // MONOP_EVENT_H
