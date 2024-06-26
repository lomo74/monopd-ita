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

#ifndef __MONOPD_DEBT_H__
#define	__MONOPD_DEBT_H__

class Estate;
class Player;

class Debt
{
	public:
		Debt(Player *from, Player *toPlayer, Estate *toEstate, unsigned int amount);

		Player *from() { return m_from; }
		Player *toPlayer() { return m_toPlayer; }
		Estate *toEstate() { return m_toEstate; }
		unsigned int amount() { return m_amount; }

	private:
		Player *m_from, *m_toPlayer;
		Estate *m_toEstate;
		unsigned int m_amount;
};

#endif // MONOP_DEBT_H
