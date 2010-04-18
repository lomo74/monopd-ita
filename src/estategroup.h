// Copyright (c) 2002-2003 Rob Kaper <cap@capsi.com>
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

#ifndef MONOP_ESTATEGROUP_H
#define MONOP_ESTATEGROUP_H

#include <string>

#include "gameobject.h"

class Game;
class Player;

class EstateGroup : public GameObject
{
public:
	EstateGroup(int id, Game *game, std::string name);

	void setPrice(unsigned int price);
	unsigned int price();
	void setHousePrice(unsigned int housePrice);
	unsigned int housePrice();
	void setRentMath(const std::string rentMath);
	void setRentVar(const std::string rentVar);
	int rent(Player *pLand, const std::string &rentMath);

private:
	std::string m_rentMath, m_rentVar;
	unsigned int m_price, m_housePrice;
};

#endif // MONOP_GROUP_H
