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

#include "estate.h"
#include "estategroup.h"

Estate::Estate(int id, std::vector<Estate *> *ev) : GameObject(id, GameObject::GEstate)
{
	m_estates = ev;
	m_group = 0;
	m_payTarget = 0;
	m_owner = 0;
	m_takeCardGroup = 0;
	m_rent[0] = m_rent[1] = m_rent[2] = m_rent[3] = m_rent[4] = m_rent[5] = 0;
}

int Estate::rent(Player *pLand, const std::string &rentMath)
{
	if (getBoolProperty("mortgaged"))
		return 0;

	// Don't use m_housePrice, housePrice() also checks within estate group
	if (housePrice() > 0)
	{
		// Rent for estates that can have houses
		int houses = getIntProperty("houses");
		if (!houses && estateGroupIsMonopoly())
			return 2 * m_rent[houses];
		else
			return m_rent[houses];
	}

	// No custom rent rules, check group rules
	if (m_group)
		return m_group->rent(pLand, rentMath);
	return 0;
}

int Estate::groupSize(Player *owner)
{
	int grouped = 0;

	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates->begin(); it != m_estates->end() && (eTmp = *it) ; ++it)
		if (m_group == eTmp->group() && m_owner == eTmp->owner())
			grouped++;

	return grouped;
}

int Estate::minHouses()
{
	int m_minHouses = 0;

	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates->begin(); it != m_estates->end() && (eTmp = *it) ; ++it)
	{
		if ((eTmp->group() == m_group) && (eTmp->getIntProperty("houses") > m_minHouses))
			m_minHouses = eTmp->getIntProperty("houses") - 1;
	}
	return m_minHouses;
}

int Estate::maxHouses()
{
	int m_maxHouses = 5;

	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates->begin(); it != m_estates->end() && (eTmp = *it) ; ++it)
	{
		if ((eTmp->group() == m_group) && (eTmp->getIntProperty("houses") < m_maxHouses))
			m_maxHouses = eTmp->getIntProperty("houses") + 1;
	}
	return m_maxHouses;
}

bool Estate::canBuyHouses(Player *p)
{
	// Don't use m_housePrice, housePrice() also checks within estate group
	if ( (p == m_owner) && housePrice() && estateGroupIsMonopoly() && !groupHasMortgages() && (getIntProperty("houses") < maxHouses()) )
		return true;
	else
		return false;
}

bool Estate::canSellHouses(Player *p)
{
	if ( (p == m_owner) && estateGroupIsMonopoly() && (getIntProperty("houses") > minHouses()) )
		return true;
	else
		return false;
}

int Estate::price()
{
	int price = getIntProperty("price");
	return (!price && m_group) ? m_group->price() : price;
}

int Estate::housePrice()
{
	int housePrice = getIntProperty("houseprice");
	return (!housePrice && m_group) ? m_group->housePrice() : housePrice;
}

bool Estate::canBeOwned()
{
	return price();
}

bool Estate::canToggleMortgage(Player *p)
{
	return (p == m_owner && !groupHasBuildings()) ? true : false;
}

bool Estate::groupHasBuildings()
{
	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates->begin(); it != m_estates->end() && (eTmp = *it) ; ++it) {
		if((eTmp->group() == m_group) && eTmp->getIntProperty("houses"))
			return true;
	}
	return false;
}

bool Estate::groupHasMortgages()
{
	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates->begin(); it != m_estates->end() && (eTmp = *it) ; ++it) {
		if((eTmp->group() == m_group) && eTmp->getBoolProperty("mortgaged"))
			return true;
	}
	return false;
}

bool Estate::estateGroupIsMonopoly()
{
	unsigned int size = 0, sameOwner = 0;

	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates->begin(); it != m_estates->end() && (eTmp = *it) ; ++it)
		if (eTmp->group() == m_group)
		{
			size++;
			if (eTmp->owner() == m_owner)
				sameOwner++;
		}

	return (sameOwner == size);
}

void Estate::addMoney(int money)
{
	setProperty("money", getIntProperty("money") + money);
}

void Estate::setTakeCardGroup(CardGroup *takeCardGroup)
{
	m_takeCardGroup = takeCardGroup;
}

CardGroup *Estate::takeCardGroup()
{
	return m_takeCardGroup;
}
