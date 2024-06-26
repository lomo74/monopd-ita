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

#include "config.h"

#include <stdarg.h>
#include <stdio.h>

#include "player.h"
#include "trade.h"
#include "tradecomponent.h"

Trade::Trade(Player *pFrom, Player *pTo, int id) : GameObject(id)
{
	status = Setup;
	setProperty("revision", 1, this); // scope is trade once used
	m_actor = pFrom;

	addPlayer(pFrom);
	addPlayer(pTo);
	resetAcceptFlags();
}

Trade::~Trade()
{
	while (!m_components.empty()) { delete *m_components.begin(); m_components.erase(m_components.begin()); }
}

Player *Trade::firstFrom()
{
	if (m_components.size())
	{
		TradeComponent *component = *m_components.begin();
		return component->from();
	}
	return 0;
}

Player *Trade::firstTarget()
{
	if (m_components.size())
	{
		TradeComponent *component = *m_components.begin();
		return component->targetPlayer();
	}
	return 0;
}

GameObject *Trade::takeFirstObject()
{
	if (m_components.size())
	{
		TradeComponent *component = *m_components.begin();
		m_components.erase(m_components.begin());
		return component->object();
	}
	return 0;
}

GameObject *Trade::findMoneyObject(Player *pFrom, Player *pTarget)
{
	TradeComponent *tc = 0;
	for(std::vector<TradeComponent *>::iterator it = m_components.begin(); it != m_components.end() && (tc = *it) ; ++it)
	{
		GameObject *object = tc->object();
		if ( (object->type() == GameObject::Money) && (tc->from() == pFrom) && (tc->targetPlayer() == pTarget) )
			return object;
	}
	return 0;
}

void Trade::updateObject(GameObject *gameObject, Player *pFrom, Player *targetPlayer)
{
	TradeComponent *tc;
	for(std::vector<TradeComponent *>::iterator it = m_components.begin(); it != m_components.end() && (tc = *it) ; ++it)
	{
		if (tc->object() == gameObject)
		{
			// Target player is already owner, set to 0 here so we get a
			// correct update message later
			if (targetPlayer == pFrom)
				targetPlayer = 0;

			tc->updateTargetPlayer(targetPlayer);

			// Target is 0, delete component
			if (!targetPlayer || (gameObject->type() == GameObject::Money && !gameObject->id()) )
			{
				setProperty("revision", getIntProperty("revision") + 1);
				writeComponentMsg(tc);
				delComponent(tc);
				resetAcceptFlags();
				return;
			}

			setProperty("revision", getIntProperty("revision") + 1);
			writeComponentMsg(tc);

			// Make sure participating players are participating
			if (!hasPlayer(pFrom))
				addPlayer(pFrom);
			if (!hasPlayer(targetPlayer))
				addPlayer(targetPlayer);

			resetAcceptFlags();
			return;
		}
	}

	// No component found for this object, but don't create one if target
	// doesn't exist or is already the owner
	if (!targetPlayer || targetPlayer == pFrom)
		return;

	// New component
	tc = new TradeComponent(pFrom, targetPlayer, gameObject);
	m_components.push_back(tc);

	setProperty("revision", getIntProperty("revision") + 1);
	writeComponentMsg(tc);

	// Make sure participating players are participating
	if (!hasPlayer(pFrom))
		addPlayer(pFrom);
	if (!hasPlayer(targetPlayer))
		addPlayer(targetPlayer);

	resetAcceptFlags();
}

void Trade::setPlayerAccept(Player *player, const bool acceptTrade)
{
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		if (*it == player)
		{
			m_acceptMap[player] = acceptTrade;
			ioWrite("<monopd><tradeupdate tradeid=\"%d\"><tradeplayer playerid=\"%d\" accept=\"%d\"/></tradeupdate></monopd>\n", m_id, player->id(), acceptTrade);
			return;
		}
	}
}

bool Trade::hasPlayer(Player *player)
{
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		if (*it == player)
			return true;

	return false;
}

bool Trade::allAccept()
{
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		if (m_acceptMap[*it] == false)
			return false;

	return true;
}

void Trade::delComponent(const enum GameObject::Type type, int id)
{
	TradeComponent *component = 0;
	for(std::vector<TradeComponent *>::iterator it = m_components.begin(); it != m_components.end() && (component = *it) ; ++it)
	{
		GameObject *object = component->object();
		if (object && (object->type() == type) && (object->id() == id))
		{
			setProperty("revision", getIntProperty("revision") + 1);
			writeComponentMsg(component);
			delete *it;
			m_components.erase(it);
			return;
		}
	}
}

void Trade::delComponent(TradeComponent *tc)
{
	for(std::vector<TradeComponent *>::iterator it = m_components.begin(); it != m_components.end() && (*it) ; ++it)
		if (*it == tc)
		{
			delete *it;
			m_components.erase(it);
			return;
		}
}

void Trade::addPlayer(Player *player)
{
	m_players.push_back(player);
	m_acceptMap[player] = false;

	player->ioWrite("<monopd><tradeupdate type=\"new\" tradeid=\"%d\" actor=\"%d\" revision=\"%d\"/></monopd>\n", m_id, m_actor->id(), getIntProperty("revision"));

	TradeComponent *tc = 0;
	for(std::vector<TradeComponent *>::iterator it = m_components.begin(); it != m_components.end() && (tc = *it) ; ++it)
		writeComponentMsg(tc, player);
}

void Trade::resetAcceptFlags()
{
	// Reset accept flag for all players
	Player *pTmp;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		if ((pTmp = *it))
		{
			m_acceptMap[pTmp] = false;
			ioWrite("<monopd><tradeupdate tradeid=\"%d\"><tradeplayer playerid=\"%d\" accept=\"0\"/></tradeupdate></monopd>\n", m_id, pTmp->id());
		}
}

void Trade::ioWrite(const char *fmt, ...)
{
	int n, size = 256;
	char *buf = new char[size];
	static std::string ioStr;
	va_list arg;

	buf[0] = 0;

	while (1)
	{
		va_start(arg, fmt);
		n = vsnprintf(buf, size, fmt, arg);
		va_end(arg);

		if (n > -1 && n < size)
		{
			ioStr = buf;
			delete[] buf;
			ioWrite(ioStr);
			return;
		}

		if (n > -1)
			size = n+1;
		else
			size *= 2;

		delete[] buf;
		buf = new char[size];
	}
}

void Trade::ioWrite(const std::string data)
{
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		pTmp->ioWrite(data);
}

void Trade::writeComponentMsg(TradeComponent *tc, Player *player)
{
	GameObject *object = tc->object();
	Player *targetPlayer = tc->targetPlayer();

	switch(object->type())
	{
	case GameObject::GCard:
		if (player)
			player->ioWrite("<monopd><tradeupdate tradeid=\"%d\" revision=\"%d\"><tradecard cardid=\"%d\" targetplayer=\"%d\"/></tradeupdate></monopd>\n", m_id, getIntProperty("revision"), object->id(), targetPlayer ? targetPlayer->id() : -1);
		else
			ioWrite("<monopd><tradeupdate tradeid=\"%d\" revision=\"%d\"><tradecard cardid=\"%d\" targetplayer=\"%d\"/></tradeupdate></monopd>\n", m_id, getIntProperty("revision"), object->id(), targetPlayer ? targetPlayer->id() : -1);
		break;
	case GameObject::GEstate:
		if (player)
			player->ioWrite("<monopd><tradeupdate tradeid=\"%d\" revision=\"%d\"><tradeestate estateid=\"%d\" targetplayer=\"%d\"/></tradeupdate></monopd>\n", m_id, getIntProperty("revision"), object->id(), targetPlayer ? targetPlayer->id() : -1);
		else
			ioWrite("<monopd><tradeupdate tradeid=\"%d\" revision=\"%d\"><tradeestate estateid=\"%d\" targetplayer=\"%d\"/></tradeupdate></monopd>\n", m_id, getIntProperty("revision"), object->id(), targetPlayer ? targetPlayer->id() : -1);
		break;
	case GameObject::Money:
		Player *pFrom;
		pFrom = tc->from();
		if (player)
			player->ioWrite("<monopd><tradeupdate tradeid=\"%d\" revision=\"%d\"><trademoney playerfrom=\"%d\" playerto=\"%d\" money=\"%d\"/></tradeupdate></monopd>\n", m_id, getIntProperty("revision"), pFrom->id(), targetPlayer->id(), object->id());
		else
			ioWrite("<monopd><tradeupdate tradeid=\"%d\" revision=\"%d\"><trademoney playerfrom=\"%d\" playerto=\"%d\" money=\"%d\"/></tradeupdate></monopd>\n", m_id, getIntProperty("revision"), pFrom->id(), targetPlayer->id(), object->id());
		break;
	default:;
	}
}
