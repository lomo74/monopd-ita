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

#ifndef MONOP_PLAYER_H
#define MONOP_PLAYER_H

#include <vector>
#include <string>

#include "gameobject.h"

class Card;
class Client;
class Display;
class Estate;
class Game;
class Socket;

class Player : public GameObject
{
	public:
		Player(Socket *socket);
		virtual ~Player();

		void reset(bool removeProperties = true);
		void identify(int id);
		bool identified() { return m_identified; };
		void ioWrite(const char *, ...);
		void ioWrite(std::string data);
		void ioInfo(const char *data, ...);
		void ioInfo(const std::string data);
		void ioError(const char *data, ...);
		void ioError(const std::string data);
		void ioNoSuchCmd(const std::string data = "");

		void sendDisplayMsg(Display *display);
		void sendClientMsg();
		void sendCardList(Player *pOut);

		void rollDice();
		void endTurn(bool userRequest = false);
		void payJail();
		void rollJail();
		void useJailCard();
		void buyEstate();
		void sellEstate(int estateId);
		void mortgageEstate(int estateId);
		void payTax(const bool percentage = false);
		void buyHouse(int estateId);
		void sellHouse(int estateId);
		void updateTradeObject(char *data);
		void updateTradeMoney(char *data);

		void setTurn(const bool& turn);
		void setGame(Game *game);
		void setEstate(Estate *estate);
		Estate *estate();
		void setDestination(Estate *estate);
		Estate *destination();
		void setTokenLocation(Estate *estate);
		Estate *tokenLocation();
		void setRequestedUpdate(const bool request) { m_requestedUpdate = request; }
		bool requestedUpdate() { return m_requestedUpdate; }
		void addCard(Card *card);
		void takeCard(Card *card);
		Card *findCard(int cardId);
		Card *findOutOfJailCard();
		Card *findFirstCard();
		int assets();

		void closeSocket();
		void setSocket(Socket *socket);
		Socket *socket() { return m_socket; }

		void toJail(Estate *jailEstate);

		void advance(int pos, bool direct);
		void advanceTo(int pos, bool direct, bool passEstates = true);

		void addMoney(int);
		bool payMoney(int);

private:
	// Connection related
	Socket *m_socket;

	// Game related
	Estate *m_estate, *m_destination, *m_tokenLocation;
	std::vector<Card *> m_cards;
	std::vector<Display *> m_display;

	bool m_requestedUpdate;
	bool m_identified;
};

#endif // MONOP_PLAYER_H
