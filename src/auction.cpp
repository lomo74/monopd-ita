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

#include <stdlib.h>

#include <string>

#include "auction.h"
#include "player.h"

Auction::Auction()
	: GameObject(0)
{
	setProperty("highbid", 0);
	m_highBidder = 0;
	m_estate = 0;
	m_status = Bidding;
}

void Auction::setHighBid(Player *player, unsigned int bid)
{
	setProperty("highbid", bid);
	m_highBidder = player;
	m_status = Bidding;
}
