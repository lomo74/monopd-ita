// Copyright © 2001-2004 Rob Kaper <cap@capsi.com>,
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

#ifndef MONOP_GAME_H
#define MONOP_GAME_H

#include <vector>
#include <string>

#include "gameobject.h"
#include "display.h"

class Auction;
class Card;
class CardGroup;
class Debt;
class Estate;
class EstateGroup;
class Player;
class Trade;

class Game : public GameObject
{
	public:
		Game(int id);
		virtual ~Game();

		enum Status { Config, Init, Run, End };

		void ioWrite(const char *data, ...);
		void ioWrite(const std::string data);
		void ioInfo(const char *data, ...);
		void ioInfo(const std::string data);
		void ioError(const std::string data);

		void loadGameType(const std::string &gameType);

		GameObject *findConfigOption(const std::string &name);
		bool getBoolConfigOption(const std::string &name);
		void editConfiguration(Player *pInput, char *data);
		void start(Player *pInput);

		void setTokenLocation(Player *pInput, unsigned int estateId);
		void tokenMovementTimeout();
		unsigned int auctionTimeout();
		unsigned int clientsMoving();
		void setAllClientsMoving(Estate *estate);

		// Member management
		void setStatus(Status status) { m_status = status; }
		void setDescription(const std::string data);
		void setHouses(int _h) { m_houses = _h; }
		void setHotels(int _h) { m_hotels = _h; }

		Status status() { return m_status; }
		const std::string statusLabel();
		Player *master() { return m_master; }
		Estate *goEstate() { return m_goEstate; }
		const std::string gameType() { return m_gameType; }
		const std::string bgColor() { return m_bgColor; }
		int houses() { return m_houses; }
		int hotels() { return m_hotels; }
		bool isValid() { return m_isValid; }

		bool sendChildXMLUpdate(Player *pOutput, bool emptyUpdate);
		void unsetChildProperties();

		int players();
		Player *addPlayer(Player *, const bool &isMaster = false, const bool &isSpectator = false);
		Player *findPlayer(int playerId);
		Player *kickPlayer(Player *player, int playerId);
		void upgradePlayer(Player *player, int playerId);
		unsigned int playerAssets(Player *player);

		void updateTurn();
		bool landPlayer(Player *pTurn, const bool directMove, const std::string &rentmath = "");
		bool giveCard(Player *player, Card *card);
		void declareBankrupt(Player *player);
		void bankruptPlayer(Player *player);
		void sendDisplayMsg(Display *display, Player *except = NULL);
		void sendMsgEstateUpdate(Estate *);
		void sendStatus(Player *player);
		void sendEstateList(Player *player);
		void sendEstateGroupList(Player *player);
		void sendPlayerList(Player *player);
		void sendCardList(Player *pOut);
		void sendFullUpdate(Player *player);

		int debts();
		void newDebtToAll(Player *from, int amount);
		Debt *newDebt(Player *from, Player *toPlayer, Estate *toEstate, int amount);
		Debt *findDebt(Player *player);
		Debt *findDebtByCreditor(Player *player);
		void delDebt(Debt *);
		void solveDebts(Player *player, const bool &verbose = false);
		bool solveDebt(Debt *debt);
		void enforceDebt(Player *pBroke, Debt *debt = 0);
		void newAuction(Player *pInput);
		Auction *auction();
		// Returns 0 on successful bid, 1 on error.
		int bidInAuction(Player *pInput, char *data);

		void newTrade(Player *pInput, unsigned int playerId);
		Trade *findTrade(Player *player);
		Trade *findTrade(int tradeId);
		void delTrade(Trade *trade);
		void acceptTrade(Player *pInput, char *);
		void rejectTrade(Player *pInput, unsigned int tradeId, const bool verbose = true);
		void completeTrade(Trade *trade);
		void abortAuction();
		void completeAuction();

		EstateGroup *newGroup(const std::string &name);
		EstateGroup *findGroup(const std::string &name);
		unsigned int estateGroupSize(EstateGroup *estateGroup);
		Estate *newEstate(const std::string &name);
		Estate *findEstate(int estateId);
		Estate *findNextEstate(EstateGroup *group, Estate *startEstate = NULL);
		Estate *findNextJailEstate(Estate *startEstate = NULL);
		unsigned int estates() const { return m_estates.size(); }
		void setPausedForDialog(const bool paused);
		bool pausedForDialog();
		void transferEstate(Estate *estate, Player *player, const bool verbose = false);
		CardGroup *newCardGroup(const std::string name);
		CardGroup *findCardGroup(const std::string name);
		Card *newCard(CardGroup *cardGroup, const std::string name);
		Card *findCard(unsigned int id);
		void transferCard(Card *card, Player *player, const bool verbose = false);
		void transferObject(const enum GameObject::Type, unsigned int id, Player *player, const bool verbose = false);
		void removePlayer(Player *);
		int lowestFreeId();
		int totalAssets();
		void rollDice();
		int dice[2];

private:
	void loadConfig();
	void parseConfigEntry(Estate *es, const std::string &key, const std::string &value);
	void parseConfigEntry(Card *card, const std::string &key, const std::string &value);
	void parseConfigEntry(EstateGroup *group, const std::string &key, const std::string &value);

	void addConfigOption(const std::string &name, const std::string &description, const std::string &defaultValue, bool editable = false);
	void addConfigOption(const std::string &name, const std::string &description, int defaultValue, bool editable = false);
	void addBoolConfigOption(const std::string &name, const std::string &description, bool defaultValue, bool editable = false);
	GameObject *findConfigOption(int configId);
	void sendConfiguration(Player *player);

	/*
	 * Consider this method to be not just private, but to be very internal.
	 * It should only be called from the various addConfigOption
	 * implementations.
	 */
	GameObject *newConfigOption(const std::string &name, const std::string &description, bool editable);

	Status m_status;
	unsigned int m_nextCardGroupId, m_nextEstateId, m_nextEstateGroupId, m_nextTradeId, m_nextAuctionId;

	Player *m_master, *m_pTurn, *m_pWinner;
	Estate *m_goEstate;
	int m_houses, m_hotels, m_startMoney;
	bool m_isValid, m_pausedForDialog;

	Auction *m_auction;
	Debt *m_auctionDebt;
	std::vector<CardGroup *> m_cardGroups;
	std::vector<Debt *> m_debts;
	std::vector<EstateGroup *> m_estateGroups;
	std::vector<Estate *> m_estates;
	std::vector<Player *> m_players;
	std::vector<Trade *> m_trades;
	std::vector<GameObject *> m_configOptions;
	std::string m_gameType, m_bgColor;
};

#endif // MONOP_GAME_H
