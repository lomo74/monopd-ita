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

#include "config.h"

#include <stdlib.h>

#include "player.h"
#include "tradecomponent.h"

TradeComponent::TradeComponent(Player *from, Player *to, GameObject *gameObject)
{
	m_playerFrom = from;
	m_playerTo = to;
	m_object = gameObject;
	m_money = 0;
}

TradeComponent::TradeComponent(Player *from, Player *to, unsigned int money)
{
	m_object = 0;
	m_playerFrom = from;
	m_playerTo = to;
	m_money = money;
}

void TradeComponent::updateTargetPlayer(Player *to)
{
	m_playerTo = to;
}

void TradeComponent::updateMoney(unsigned int money)
{
	m_money = money;
}
