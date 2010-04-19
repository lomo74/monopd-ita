// Copyright (c) 2002-2003 Rob Kaper <cap@capsi.com>
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

#include <algorithm>

#include "card.h"
#include "cardgroup.h"
#include "game.h"

CardGroup::CardGroup(int id, Game *game, std::string name) : GameObject(id, GameObject::Unknown, game)
{
	setProperty("name", name, game);
}

CardGroup::~CardGroup()
{
	while (!m_cards.empty()) { delete *m_cards.begin(); m_cards.erase(m_cards.begin()); }
}

Card *CardGroup::newCard(unsigned int id, const std::string name)
{
	Card *card = new Card(this, id);
	card->setProperty("name", name, m_game);
	m_cards.push_back(card);
	return card;
}

Card *CardGroup::nextCard()
{
	Card *card = 0;

	if (m_cards.size() > 0)
	{
		// Fetch first card
		card = m_cards[0];

		// Remove first card
		m_cards.erase(m_cards.begin());

		// Reinsert it at the back
		m_cards.push_back(card);
	}

	return card;
}

Card *CardGroup::findCard(unsigned int id)
{
	Card *card = 0;
	for (std::vector<Card *>::iterator it = m_cards.begin() ; it != m_cards.end() && (card = *it) ; ++it)
		if (card->id() == id)
			return card;
	return 0;
}

void CardGroup::pushCard(Card *card)
{
	m_cards.push_back(card);
}

void CardGroup::popCard()
{
	m_cards.pop_back();
}

void CardGroup::shuffleCards()
{
	random_shuffle(m_cards.begin(), m_cards.end());
}
