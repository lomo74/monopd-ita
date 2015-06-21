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

#ifndef MONOP_ESTATE_H
#define MONOP_ESTATE_H

#include <string>
#include <vector>

#include "gameobject.h"

class CardGroup;
class Estate;
class EstateGroup;
class Game;
class Player;

class Estate : public GameObject
{
public:
	Estate(int id, std::vector<Estate *> *);

	void setEstateGroup(EstateGroup *group) { m_group = group; }
	void setOwner(Player *_p) { m_owner = _p; }
	void setIcon(const std::string &icon);
	std::string icon() const;
	void setRent(int _houses, int _m) { m_rent[_houses] = _m; }
	void addMoney(int money);
	void setPayAmount(int amount);
	int payAmount();
	void setPayTarget(Estate *estate) { m_payTarget = estate; }

	void setTakeCardGroup(CardGroup *cardGroup);
	CardGroup *takeCardGroup();

	EstateGroup *group() { return m_group; }
	Player *owner() { return m_owner; }
	const std::string color();
	const std::string bgColor();
	int price();
	int housePrice();
	int rentByHouses(int _h) { return m_rent[_h]; }
	int rent(Player *player, const std::string &rentMath);
	Estate *payTarget() { return m_payTarget; }

	bool canBeOwned();
	bool canToggleMortgage(Player *owner);
	bool canBuyHouses(Player *);
	bool canSellHouses(Player *);

	int groupSize(Player *);
	int minHouses();
	int maxHouses();
	bool groupHasBuildings();
	bool groupHasMortgages();
	bool estateGroupIsMonopoly();

private:
	Estate *m_payTarget;
	EstateGroup *m_group;
	Player *m_owner;
	CardGroup *m_takeCardGroup;
	std::vector<Estate *> *m_estates;
	int m_rent[6];
};

#endif // MONOP_ESTATE_H
