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

#ifndef MONOP_TRADE_H
#define MONOP_TRADE_H

#include <map>
#include <string>
#include <vector>

#include "gameobject.h"

class GameObject;
class Player;
class TradeComponent;

enum Status { Setup, Active };

class Trade : public GameObject
{
public:
	Trade(Player *pFrom, Player *pTo, int id);
	virtual ~Trade();

	void ioWrite(const char *fmt, ...);
	void ioWrite(const std::string data);
	void writeComponentMsg(TradeComponent *tc, Player *player = 0);

	Player *firstFrom();
	Player *firstTarget();
	GameObject *takeFirstObject();
	GameObject *findMoneyObject(Player *pFrom, Player *targetPlayer);
	void updateObject(GameObject *gameObject, Player *pFrom, Player *targetPlayer);
	void updateMoney(Player *from, Player *targetPlayer, unsigned int money);
	void setPlayerAccept(Player *player, const bool acceptTrade);
	void delComponent(const enum GameObject::Type, int id);

	bool hasPlayer(Player *player);
	bool allAccept();
	int status;

private:
	void addPlayer(Player *player);
	void resetAcceptFlags();

	void delComponent(TradeComponent *tc);

	Player *m_actor;
	std::vector<TradeComponent *> m_components;
	std::vector<Player *> m_players;
  	std::map<Player *, bool> m_acceptMap;
};

#endif // MONOP_TRADE_H
