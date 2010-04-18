// Copyright (c) 2001-2003 Rob Kaper <cap@capsi.com>,
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

#include <stdlib.h>

#include "cardgroup.h"
#include "card.h"

Card::Card(CardGroup *group, int id) : GameObject(id, GameObject::GCard)
{
	m_group = group;

	m_canBeOwned = m_outOfJail = false;
	m_owner = 0;

	m_rentMath = "";
	setPay(0);
	setPayHouse(0);
	setPayHotel(0);
	setPayEach(0);
	setAdvance(0);
	setAdvanceTo(-1); // Because 0 would be valid here
	setToJail(false);
	m_advanceToNextOf = 0;
}
