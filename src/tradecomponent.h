// Copyright © 2002 Rob Kaper <cap@capsi.com>
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

#ifndef MONOP_TRADECOMPONENT_H
#define MONOP_TRADECOMPONENT_H

#include <vector>

class GameObject;
class Player;

class TradeComponent
{
public:
	TradeComponent(Player *from, Player *to, GameObject *gameObject);
	TradeComponent(Player *from, Player *to, unsigned int money);

	void updateTargetPlayer(Player *to);
	void updateMoney(unsigned int);

	void setObject(GameObject *object) { m_object = object; }
	GameObject *object() { return m_object; }
	Player *from() { return m_playerFrom; }
	Player *targetPlayer() { return m_playerTo; }
	unsigned int money() { return m_money; }

private:
	Player *m_playerFrom, *m_playerTo;
	unsigned int m_money;
	GameObject *m_object;
};

#endif // MONOP_TRADECOMPONENT_H
