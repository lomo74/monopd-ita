// Copyright © 2002-2003 Rob Kaper <cap@capsi.com>
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

#include "config.h"

#include <stdlib.h>
#include <iostream>

#include <muParser.h>

#include "estate.h"
#include "estategroup.h"
#include "game.h"
#include "io.h"
#include "player.h"

EstateGroup::EstateGroup(int id, Game *game, std::string name) : GameObject(id, GameObject::Unknown, game)
{
	setProperty("name", name, game);
	m_rentMath = m_rentVar = "";
	m_price = m_housePrice = 0;
}

void EstateGroup::setPrice(unsigned int price)
{
	if (m_price != price)
		m_price = price;
}

unsigned int EstateGroup::price()
{
	return m_price;
}

void EstateGroup::setHousePrice(unsigned int housePrice)
{
	if (m_housePrice != housePrice)
		m_housePrice = housePrice;
}

unsigned int EstateGroup::housePrice()
{
	return m_housePrice;
}

void EstateGroup::setRentMath(const std::string rentMath)
{
	if (m_rentMath != rentMath)
		m_rentMath = rentMath;
}

void EstateGroup::setRentVar(const std::string rentVar)
{
	if (m_rentVar != rentVar)
		m_rentVar = rentVar;
}

int EstateGroup::rent(Player *pLand, const std::string &rentMath)
{
	std::string mathStr = rentMath.size() ? rentMath : m_rentMath;

	if (!mathStr.size()) {
		return 0;
	}

	Estate *estate = pLand->estate();
	stringReplace(mathStr, "${DICE}", std::string(itoa((m_game->dice[0] + m_game->dice[1]))));
	stringReplace(mathStr, "${GROUPOWNED}", std::string(itoa(estate->groupSize(estate->owner()))));
	stringReplace(mathStr, "${HOUSES}", std::string(itoa(estate->getIntProperty("houses"))));

	try {
		mu::Parser p;
		p.SetExpr(mathStr);
		return (int)round(p.Eval());
	} catch (mu::Parser::exception_type &e) {
		return 0;
	} catch (...) {
		return 0;
	}

	return 0;
}
