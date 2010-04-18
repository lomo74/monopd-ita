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

#ifndef MONOP_CARD_H
#define MONOP_CARD_H

#include <string>

#include "gameobject.h"

class CardGroup;
class EstateGroup;
class Player;

class Card : public GameObject
{
public:
	Card(CardGroup *cardGroup, int id);

	void setOwner(Player *owner) { m_owner = owner; }
	Player *owner() { return m_owner; }

	void setCanBeOwned (bool canBeOwned) { m_canBeOwned = canBeOwned; }
	bool canBeOwned() { return m_canBeOwned; }
	void setPay(int _pay) { m_pay = _pay; }
	void setPayHouse(int _pay) { m_payHouse = _pay; }
	void setPayHotel(int _pay) { m_payHotel = _pay; }
	void setPayEach(int _pay) { m_payEach = _pay; }
	void setAdvance(int _a) { m_advance = _a; }
	void setAdvanceTo(int _a) { m_advanceTo = _a; }
	void setAdvanceToNextOf(EstateGroup *group) { m_advanceToNextOf = group; } 
	void setToJail(bool _j) { m_toJail = _j; }
	void setOutOfJail(bool outOfJail) { m_outOfJail = outOfJail; }
	void setRentMath(std::string rentMath) { m_rentMath = rentMath; }
	std::string rentMath() { return m_rentMath; }

	int pay() { return m_pay; }
	int payHouse() { return m_payHouse; }
	int payHotel() { return m_payHotel; }
	int payEach() { return m_payEach; }
	int advance() { return m_advance; }
	int advanceTo() { return m_advanceTo; }
	EstateGroup *advanceToNextOf() { return m_advanceToNextOf; }
	bool toJail() { return m_toJail; }
	bool outOfJail() { return m_outOfJail; }
	CardGroup *group() { return m_group; }

private:
	Player *m_owner;
	CardGroup *m_group;
	EstateGroup *m_advanceToNextOf;
	int m_pay, m_payHouse, m_payHotel, m_payEach, m_advance, m_advanceTo;
	std::string m_rentMath;
	bool m_canBeOwned, m_toJail, m_outOfJail;
};

#endif // MONOP_CARD_H
