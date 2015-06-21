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

#ifndef MONOP_CARDGROUP_H
#define MONOP_CARDGROUP_H

#include <vector>
#include <string>

#include "gameobject.h"

class Card;
class Game;

class CardGroup : public GameObject
{
public:
	CardGroup(int id, Game *game, std::string name);
	virtual ~CardGroup();

	Card *newCard(int id, const std::string name);
	Card *nextCard();
	Card *findCard(int id);
	void pushCard(Card *card);
	void popCard();
	void shuffleCards();

private:
	std::vector<Card *> m_cards;
};

#endif // MONOP_CARDGROUP_H
