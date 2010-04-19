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

#ifndef MONOP_AUCTION_H
#define MONOP_AUCTION_H

#include "gameobject.h"

class Estate;
class Player;

class Auction : public GameObject
{
 public:
 	enum Status { Bidding = 0, GoingOnce = 1, GoingTwice = 2, Sold = 3, PaymentDue = 4, Completed = 5 };

	Auction();
	void setEstate(Estate *estate) { m_estate = estate; }
	void setHighBid(Player *player, unsigned int bid);

	void setStatus(unsigned int status) { m_status = status; }
	unsigned int status() { return m_status; }
	Player *highBidder() { return m_highBidder; }
	Estate *estate() { return m_estate; }

 private:
	Player *m_highBidder;
	Estate *m_estate;
	unsigned int m_status;
};

#endif
