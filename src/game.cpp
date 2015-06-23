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

#define __USE_XOPEN
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <algorithm> // libstdc++ from the gcc 2.95 has no #include <algo> yet :(

#include <map>
#include <string>
#include <iostream>
#include <fstream>

#include <utf8.h>

#include "auction.h"
#include "card.h"
#include "cardgroup.h"
#include "debt.h"
#include "estate.h"
#include "estategroup.h"
#include "game.h"
#include "io.h"
#include "player.h"
#include "trade.h"
#include "display.h"

Game::Game(int id)
 :	GameObject(id, GGame),
 	m_pWinner( 0 )
{
	m_status = Config;

	m_startMoney = 0;
	m_houses = 0;
	m_hotels = 0;
	m_master = m_pTurn = 0;
	m_goEstate =  0;
	m_gameType = m_bgColor = "";
	m_isValid = m_pausedForDialog = false;
	m_auction = 0;
	m_auctionDebt = 0;
	dice[0]=0;
	dice[1]=0;

//	addConfigOption( "test", "A test config option", "teststring" );
//	addConfigOption( "test2", "An int test config option", 23 );
//	addBoolConfigOption( "test3", "A bool test config option", true );

	m_nextCardGroupId = m_nextEstateId = m_nextEstateGroupId = m_nextTradeId = m_nextAuctionId = 0;

	addBoolConfigOption( "collectfines", "Free Parking collects fines", false, true );
	addBoolConfigOption( "alwaysshuffle", "Always shuffle decks before taking a card", false, true );
	addBoolConfigOption( "auctionsenabled", "Enable auctions", true, true );
	addBoolConfigOption( "doublepassmoney", "Double pass money on exact landings", false, true );
	addBoolConfigOption( "unlimitedhouses", "Bank provides unlimited amount of houses/hotels", false, true );
	addBoolConfigOption( "norentinjail", "Players in Jail get no rent", false, true );
	addBoolConfigOption( "allowestatesales", "Allow estates to be sold back to Bank", false, true );
	addBoolConfigOption( "automatetax", "Automate tax decisions", false, true );
	addBoolConfigOption( "allowspectators", "Allow spectators", false, true );
}

Game::~Game()
{
	// We are responsible for the objects allocated
	while (!m_cardGroups.empty()) { delete *m_cardGroups.begin(); m_cardGroups.erase(m_cardGroups.begin()); }
	while (!m_debts.empty()) { delete *m_debts.begin(); m_debts.erase(m_debts.begin()); }
	while (!m_estates.empty()) { delete *m_estates.begin(); m_estates.erase(m_estates.begin()); }
	while (!m_estateGroups.empty()) { delete *m_estateGroups.begin(); m_estateGroups.erase(m_estateGroups.begin()); }
	while (!m_trades.empty()) { delete *m_trades.begin(); m_trades.erase(m_trades.begin()); }
	while (!m_configOptions.empty()) { delete *m_configOptions.begin(); m_configOptions.erase(m_configOptions.begin()); }

	if (m_auction)
		delete m_auction;
}

void Game::ioWrite(const char *fmt, ...)
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

void Game::ioWrite(const std::string data)
{
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		pTmp->ioWrite(data);
}

void Game::ioInfo(const char *data, ...)
{
	va_list arg;
	char buf[2048];

	va_start(arg, data);
	vsnprintf(buf, sizeof(buf)-1, data, arg);
	va_end(arg);
	buf[sizeof(buf)-1] = 0;

	ioWrite("<monopd><msg type=\"info\" value=\"%s\"/></monopd>\n", buf);
}

void Game::ioInfo(const std::string data)
{
	ioWrite("<monopd><msg type=\"info\" value=\"" + data + "\"/></monopd>\n");
}

void Game::ioError(const std::string data)
{
	ioWrite("<monopd><msg type=\"error\" value=\"" + data + "\"/></monopd>\n");
}

void Game::loadGameType(const std::string &gameType)
{
	if (m_gameType != gameType)
	{
		m_gameType = gameType;
		loadConfig();

		unsetChildProperties(); // so that the first player joining won't get estateupdates until init
	}
}

void Game::loadConfig()
{
	enum ConfigGroup { Board, EstateGroups, Estates, CardGroups, Cards, Other };
	ConfigGroup configGroup = Other;
	std::map<Estate *, int> payTargets;
	EstateGroup *group = 0;
	Estate *eTmp = 0;
	CardGroup *cardGroup = 0;
	Card *card = 0;
	unsigned int goEstate = 0, nextCardId = 0;
	int pos = 0;

	std::string line, key, value;
	std::string filename = std::string(MONOPD_DATADIR "/games/") + m_gameType + ".conf";

	std::ifstream infile( filename.c_str() );
	if ( infile.bad() )
		return;

	while (getline( infile, line, '\n')) {

		if (!line.size() || line[0] == '#') {
			continue;
		}

		if (!utf8::is_valid(line.begin(), line.end())) {
			infile.close();
			return;
		}

		if (line[0] == '<') {
			value = line.substr( 1, line.size()-2 );
			if ( value == "Board" )
				configGroup = Board;
			else if ( value == "EstateGroups" )
				configGroup = EstateGroups;
			else if ( value == "Estates" )
				configGroup = Estates;
			else if ( value == "Cards" )
				configGroup = CardGroups;
			else
				configGroup = Other;

			continue;
		}

		if (line[0] == '[') {
			value = line.substr( 1, line.size()-2 );
			if (configGroup == EstateGroups)
				group = newGroup( value );
			else if (configGroup == Estates)
				eTmp = newEstate( value );
			else if (configGroup == Cards && cardGroup)
				card = cardGroup->newCard( nextCardId++, value );

			continue;
		}

		pos = line.find( "=" );
		if (pos == (int)std::string::npos) {
			continue;
		}

		key = line.substr( 0, pos );
		value = line.substr( pos+1 );

		if (configGroup == Board) {
			if ( key == "go" ) {
				goEstate = atoi( value.c_str() );
			} else if ( key == "bgcolor" ) {
				m_bgColor = value;
			}
		}
		else if (configGroup == EstateGroups) {
			parseConfigEntry( group, key, value );
		}
		else if (configGroup == Estates) {
			if ( key == "paytarget" ) {
				payTargets[eTmp] = atoi( value.c_str() );
			} else {
				parseConfigEntry( eTmp, key, value );
			}
		}
		else if (configGroup == CardGroups) {
			if ( key == "groupname" ) {
				cardGroup = newCardGroup( value );
				configGroup = Cards;
			}
		}
		else if (configGroup == Cards) {
			if ( key == "groupname" ) {
				cardGroup = newCardGroup( value );
			} else {
				parseConfigEntry( card, key, value );
			}
		}
		else if ( key == "name" || key == "description") {
			setProperty( key, value );
		}
		else if ( key == "minplayers" || key == "maxplayers" ) {
			setProperty( key, atoi( value.c_str() ) );
		}
		else if ( key == "houses" ) {
			m_houses = atoi( value.c_str() );
		}
		else if ( key == "hotels" ) {
			m_hotels = atoi( value.c_str() );
		}
		else if ( key == "startmoney" ) {
			m_startMoney = atoi( value.c_str() );
		}
	}
	infile.close();

	// Make sure defaults are within reasonable limits
	if (getIntProperty("minplayers") < 2)
		setProperty("minplayers", 2);
	if (getIntProperty("maxplayers") < getIntProperty("minplayers"))
		setProperty("maxplayers", getIntProperty("minplayers"));
	else if (getIntProperty("maxplayers") > 32)
		setProperty("maxplayers", 32);

	// Go estate is required, defaults to first specified
	if (!(m_goEstate = findEstate(goEstate)))
		m_goEstate = findEstate(0);
	if (!m_goEstate)
		return;

	// Fix defaults for estates
	for (std::map<Estate *, int>::iterator it = payTargets.begin() ; it != payTargets.end() && (eTmp = (*it).first) ; it++)
		eTmp->setPayTarget(findEstate((*it).second));
	payTargets.clear();

	eTmp = 0;
	for (std::vector<Estate *>::iterator eIt = m_estates.begin() ; eIt != m_estates.end() && (eTmp = *eIt) ; eIt++)
	{
		if (!eTmp->hasIntProperty("mortgageprice"))
			eTmp->setProperty("mortgageprice", (int)(eTmp->price()/2), this);
		if (!eTmp->hasIntProperty("unmortgageprice"))
			eTmp->setProperty("unmortgageprice", (int)((eTmp->price()/2)*1.1), this);
		if (!eTmp->hasIntProperty("sellhouseprice"))
			eTmp->setProperty("sellhouseprice", (int)(eTmp->housePrice()/2), this);
	}

	m_isValid = true;
}

void Game::parseConfigEntry(Estate *es, const std::string &key, const std::string &value)
{
	if (!es)
		return;

	if ( key == "group" )
		es->setEstateGroup( findGroup( value ) );
	else if ( key == "color" || key == "bgcolor" || key == "icon" )
		es->setProperty( key, value, this );
	else if ( key == "price" || key == "mortgageprice" || key == "unmortgageprice" || key == "houseprice" || key == "sellhouseprice" ||
			  key == "payamount" || key == "passmoney" || key == "tax" || key == "taxpercentage"
			)
		es->setProperty( key, atoi( value.c_str() ), this );
	else if ( key == "rent0" )
		es->setRent(0, atoi( value.c_str() ) );
	else if ( key == "rent1" )
		es->setRent(1, atoi( value.c_str() ) );
	else if ( key == "rent2" )
		es->setRent(2, atoi( value.c_str() ) );
	else if ( key == "rent3" )
		es->setRent(3, atoi( value.c_str() ) );
	else if ( key == "rent4" )
		es->setRent(4, atoi( value.c_str() ) );
	else if ( key == "rent5" )
		es->setRent(5, atoi( value.c_str() ) );
	else if ( key == "tojail" || key == "jail" )
		es->setBoolProperty( key, atoi( value.c_str() ), this );
	else if ( key == "takecard" )
		es->setTakeCardGroup( findCardGroup( value ) );
}

void Game::parseConfigEntry(Card *card, const std::string &key, const std::string &value)
{
	if (!card)
		return;

	if ( key == "canbeowned" )
		card->setCanBeOwned( atoi( value.c_str() ) );
	else if ( key == "pay" )
		card->setPay( atoi( value.c_str() ) );
	else if ( key == "payhouse" )
		card->setPayHouse( atoi( value.c_str() ) );
	else if ( key == "payhotel" )
		card->setPayHotel( atoi( value.c_str() ) );
	else if ( key == "payeach" )
		card->setPayEach( atoi( value.c_str() ) );
	else if ( key == "tojail" )
		card->setToJail( atoi( value.c_str() ) );
	else if ( key == "outofjail" )
		card->setOutOfJail( atoi( value.c_str() ) );
	else if ( key == "advance" )
		card->setAdvance( atoi( value.c_str() ) );
	else if ( key == "advanceto" )
		card->setAdvanceTo( atoi( value.c_str() ) );
	else if ( key == "advancetonextof" )
		card->setAdvanceToNextOf( findGroup( value ) );
	else if ( key == "rentmath" )
		card->setRentMath( value );
}

void Game::parseConfigEntry(EstateGroup *group, const std::string &key, const std::string &value)
{
	 if (!group)
	 	return;

	if ( key == "price" )
		group->setPrice( atoi( value.c_str() ) );
	else if ( key == "houseprice" )
		group->setHousePrice( atoi( value.c_str() ) );
	else if ( key == "color" || key == "bgcolor" )
		group->setProperty( key, value, this );
	else if ( key == "rentmath" )
		group->setRentMath( value );
	else if ( key == "rentvar" )
		group->setRentVar( value );
}

void Game::editConfiguration(Player *pInput, char *data)
{
	if (pInput != m_master)
	{
		pInput->ioError("Only the master can edit the game configuration.");
		return;
	}

	if (m_status != Config)
	{
		pInput->ioError("This game has already been started.");
		return;
	}

	if (!strstr(data, ":"))
	{
		pInput->ioError("Invalid input for .gc, no separator after configId");
		return;
	}
	int configId = atoi(strsep(&data, ":"));

	GameObject *configOption = findConfigOption(configId);
	if (!configOption)
	{
		pInput->ioError("No such configId: %d.", configId);
		return;
	}

	std::string newValue = data;

	if ( configOption->getStringProperty("type") == "string" )
		configOption->setProperty( "value", newValue );
	else if ( configOption->getStringProperty("type") == "int" )
		configOption->setProperty( "value", atoi(newValue.c_str()) );
	else if ( configOption->getStringProperty("type") == "bool" )
		configOption->setBoolProperty( "value", atoi(newValue.c_str()) );
}

GameObject *Game::newConfigOption(const std::string &name, const std::string &description, bool editable)
{
	GameObject *config = new GameObject( m_configOptions.size()+1 , ConfigOption, this);
	m_configOptions.push_back(config);
	config->setProperty("name", name, this);
	config->setProperty("description", description, this);
	config->setBoolProperty("edit", editable, this);
	return config;
}

void Game::addConfigOption(const std::string &name, const std::string &description, const std::string &defaultValue, bool editable)
{
	GameObject *config = newConfigOption(name, description, editable);
	config->setProperty("type", "string", this);
	config->setProperty("value", defaultValue, this);
}

void Game::addConfigOption(const std::string &name, const std::string &description, int defaultValue, bool editable)
{
	GameObject *config = newConfigOption(name, description, editable);
	config->setProperty("type", "int", this);
	config->setProperty("value", defaultValue, this);
}

void Game::addBoolConfigOption(const std::string &name, const std::string &description, bool defaultValue, bool editable)
{
	GameObject *config = newConfigOption(name, description, editable);
	config->setProperty("type", "bool", this);
	config->setBoolProperty("value", defaultValue, this);
}

GameObject *Game::findConfigOption(int configId)
{
	for(std::vector<GameObject *>::iterator it = m_configOptions.begin(); it != m_configOptions.end() && (*it) ; ++it)
		if ( (*it)->id() == configId )
			return (*it);
	return 0;
}

GameObject *Game::findConfigOption(const std::string &name)
{
	for(std::vector<GameObject *>::iterator it = m_configOptions.begin(); it != m_configOptions.end() && (*it) ; ++it)
		if ( (*it)->getStringProperty("name") == name )
			return (*it);
	return 0;
}

bool Game::getBoolConfigOption(const std::string &name)
{
	GameObject *config = findConfigOption(name);
	return (config && config->getBoolProperty("value"));
}

void Game::start(Player *pInput)
{
	if (pInput != m_master)
	{
		pInput->ioError("Only the master can start a game.");
		return;
	}
	if (m_status != Config)
	{
		pInput->ioError("This game has already been started.");
		return;
	}
	int minPlayers = getIntProperty("minplayers");
	if ((int)m_players.size() < minPlayers)
	{
		pInput->ioError("This game requires at least %d players to be started.", minPlayers);
		return;
	}

	// Update status.
	m_status = Init;

	// Update whether players can join/watch.
	GameObject *config = findConfigOption( "allowspectators" );
	if ( config && config->getBoolProperty( "value" ) )
		setBoolProperty( "canbejoined", true );
	else
		setBoolProperty( "canbejoined", false );

	// Shuffle players
	random_shuffle(m_players.begin(), m_players.end());

	// Shuffle card decks.
	CardGroup *cardGroup = 0;
	for (std::vector<CardGroup *>::iterator it = m_cardGroups.begin() ; it != m_cardGroups.end() && (cardGroup = *it) ; ++it)
		cardGroup->shuffleCards();

	// Set all players at Go, give them money
	Player *pTmp = 0;
	int turnorder = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
	{
		pTmp->reset(false);
		pTmp->setProperty("turnorder", ++turnorder);
		pTmp->setEstate(m_goEstate);
		pTmp->addMoney(m_startMoney);
	}

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		sendFullUpdate(pTmp);

	// Add notification of game start.
	m_status = Run;
	ioWrite("<monopd><gameupdate gameid=\"%d\" status=\"%s\"/></monopd>\n", m_id, statusLabel().c_str());

	Display display;
	display.resetText();
	display.resetEstate();
	display.setText("Game started!");
	sendDisplayMsg(&display);

	// Turn goes to first player
	m_pTurn = *m_players.begin();
	m_pTurn->setTurn(true);
}

void Game::setTokenLocation(Player *pInput, unsigned int estateId)
{
	if (!clientsMoving() || !pInput || !m_pTurn)
		return;

	Estate *estateLoc = findEstate(estateId);

	if ( !estateLoc || !pInput->tokenLocation() || !m_pTurn->destination() || (pInput->tokenLocation() == estateLoc) )
		return;

	bool useNext = false;
	unsigned int money = 0;

//	printf("Game::setTokenLocation, P:%d PTL:%d EID:%d\n", pInput->id(), pInput->tokenLocation()->id(), estateId);

	Estate *estate = 0;
	for (std::vector<Estate *>::iterator it = m_estates.begin() ;;)
	{
		if (it == m_estates.end())
		{
//			printf("Game::setTokenLocation, reloop\n");
			it = m_estates.begin();
			continue;
		}
		if (!(estate = *it))
			break;

		if (useNext)
		{
			if (estate->getIntProperty("passmoney"))
			{
//				printf("Game::setTokenLocation, pass:%d\n", estate->id());
				Display display;
				display.setText("%s passes %s and gets %d.", m_pTurn->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), estate->getIntProperty("passmoney"));
				pInput->sendDisplayMsg(&display);
				money += estate->getIntProperty("passmoney");
				pInput->ioWrite("<monopd><playerupdate playerid=\"%d\" money=\"%d\"/></monopd>\n", m_pTurn->id(), m_pTurn->getIntProperty("money") + money);
			}
			if (estate == m_pTurn->destination())
			{
//				printf("Game::setTokenLocation, setPTL:0\n");
				pInput->setTokenLocation(0); // Player is done moving
				break;
			}
			if (estate == estateLoc)
			{
//				printf("Game::setTokenLocation, setPTL:%d\n", estateId);
				pInput->setTokenLocation(estate); // Player is not done moving
				break;
			}
		}
		else if (estate == pInput->tokenLocation())
		{
//			printf("Game::setTokenLocation, useNext:%d==PTL\n", estate->id());
			useNext = true;
		}

		++it;
	}

	// Find out if there are still clients busy with movement.
	if (clientsMoving())
		return;

	// Land player!
	bool endTurn = landPlayer(m_pTurn, false);

	if (clientsMoving())
	{
		// Don't do a thing.. just wait until they are done.
	}
	else if (m_pTurn->getBoolProperty("can_buyestate"))
	{
		// Don't do a thing..
	}
	else if (endTurn)
		m_pTurn->endTurn();
}

void Game::tokenMovementTimeout()
{
	// Mark all clients as non moving.
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		if (pTmp->tokenLocation())
		{
			if (m_pTurn && m_pTurn->destination())
			{
				m_pTurn->setProperty("location", m_pTurn->destination()->id(), this);
				m_pTurn->setBoolProperty("directmove", true, this);
				setTokenLocation(pTmp, m_pTurn->destination()->id());
			}
		}
}

unsigned int Game::auctionTimeout()
{
	if (!m_auction)
		return 0;

	printf("Game::auctionTimeout %d %d\n", m_id, m_auction->status());

	m_auction->setStatus(m_auction->status() +1);
	ioWrite("<monopd><auctionupdate auctionid=\"%d\" status=\"%d\"/></monopd>\n", m_auction->id(), m_auction->status());

	if (m_auction->status() == Auction::Sold) {
		printf("Game::auctionTimeout sold %d %d\n", m_id, m_auction->status());

		Player *pBid = m_auction->highBidder();
		int bid = m_auction->getIntProperty("highbid");

		if (!pBid->payMoney(bid)) {
			Display display;
			display.setText("%s has to pay %d. Game paused, %s is not solvent. Player needs to raise %d in cash first.", pBid->getStringProperty("name").c_str(), bid, pBid->getStringProperty("name").c_str(), (bid - pBid->getIntProperty("money")));
			sendDisplayMsg(&display);

			m_auctionDebt = newDebt(pBid, 0, 0, bid);
			return 0;
		}

		completeAuction();
		return 0;
	}

	return 4000;
}

void Game::newDebtToAll(Player *from, int amount)
{
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (pTmp = *it) ; ++it)
		if (pTmp != from)
			newDebt(from, pTmp, 0, amount);
}

Debt *Game::newDebt(Player *from, Player *toPlayer, Estate *toEstate, int amount)
{
	if (from && (from->getBoolProperty("bankrupt") || from->getBoolProperty("spectator")))
		return 0;
	if (toPlayer && (toPlayer->getBoolProperty("bankrupt") || toPlayer->getBoolProperty("spectator")))
		return 0;

	Debt *d = new Debt(from, toPlayer, toEstate, amount);
	m_debts.push_back(d);

	if (amount > from->assets())
	{
		from->setBoolProperty("hasdebt", true);

		Display display;
		display.setText("%s is bankrupt!", from->getStringProperty("name").c_str());
		sendDisplayMsg(&display, from);
		display.resetButtons(); /* Player is bankrupt, removed declare bankruptcy button */
		from->sendDisplayMsg(&display);

		bankruptPlayer(from);
		return 0;
	}

	if (!from->getBoolProperty("hasdebt")) {
		from->setBoolProperty("hasdebt", true);
		Display display;
		display.addButton(".D", "Declare bankruptcy", true);
		from->sendDisplayMsg(&display);
	}
	return d;
}

Debt *Game::findDebt(Player *p)
{
	Debt *dTmp = 0;
	for(std::vector<Debt *>::iterator it = m_debts.begin(); it != m_debts.end() && (dTmp = *it) ; ++it)
		if (p == dTmp->from())
			return dTmp;

	return 0;
}

Debt *Game::findDebtByCreditor(Player *p)
{
	Debt *dTmp = 0;
	for(std::vector<Debt *>::iterator it = m_debts.begin(); it != m_debts.end() && (dTmp = *it) ; ++it)
		if (p == dTmp->toPlayer())
			return dTmp;

	return 0;
}

void Game::delDebt(Debt *debt)
{
	for(std::vector<Debt *>::iterator it = m_debts.begin(); it != m_debts.end(); ++it)
		if (*it == debt)
		{
			m_debts.erase(it);
			break;
		}
	Player *pFrom = debt->from();
	if ( !findDebt(pFrom) )
		pFrom->setBoolProperty("hasdebt", false);

	delete debt;
}

void Game::solveDebts(Player *pInput, const bool &verbose)
{
	Debt *debt = findDebt(pInput);
	if (!debt)
	{
		if (verbose)
			ioError("You don't have any debts to pay off!");
		if (!clientsMoving() && !m_auction && !m_pausedForDialog)
			pInput->endTurn();
		return;
	}

	while ( (debt = findDebt(pInput)) )
	{
		if ( !solveDebt(debt) )
			break;
	}

	if (unsigned int debts = m_debts.size()) {
		Display display;
		display.setText("There are still %d debts, game still paused.", debts);
		sendDisplayMsg(&display);
	}
	else {
		Display display;
		display.setText("All debts are settled, game continues.");
		sendDisplayMsg(&display, pInput);
		display.resetButtons(); /* Remove declare bankruptcy button */
		pInput->sendDisplayMsg(&display);

		completeAuction();
	}

	if (!clientsMoving() && !m_auction && !m_pausedForDialog)
		pInput->endTurn();
}

bool Game::solveDebt( Debt *debt )
{
	Player *pFrom = debt->from();
	int payAmount = debt->amount();
	if ( !(pFrom->payMoney(payAmount)) )
		return false;

	Player *pCreditor = 0;
	Estate *eCreditor = 0;
	if ((pCreditor = debt->toPlayer()))
		pCreditor->addMoney(payAmount);
	else if ( ( eCreditor = debt->toEstate() )  && getBoolConfigOption("collectfines") )
		eCreditor->addMoney(payAmount);

	Display display;
	display.setText("%s pays off a %d debt to %s.", pFrom->getStringProperty("name").c_str(), debt->amount(), (pCreditor ? pCreditor->getStringProperty("name").c_str() : "Bank"));
	sendDisplayMsg(&display);
	if (debt == m_auctionDebt) {
		m_auctionDebt = 0;
	}
	delDebt(debt);
	return true;
}

void Game::enforceDebt(Player *pBroke, Debt *debt)
{
	// Find creditors
	Estate *eCreditor = debt ? debt->toEstate() : 0;
	Player *pCreditor = debt ? debt->toPlayer() : 0;

	// Give all cards to creditor or card stack
	while (Card *card = pBroke->findFirstCard())
		transferCard(card, pCreditor);

	// Sell all houses on property
	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates.begin() ; it != m_estates.end() && (eTmp = *it) ; ++it)
	{
		if (pBroke == eTmp->owner())
		{
			// Sell houses
			int eTmpHouses = eTmp->getIntProperty("houses");
			if (eTmpHouses)
			{
				// Get money
				int returnValue = eTmpHouses * eTmp->housePrice() / 2;
				pBroke->addMoney(returnValue);

				if (!getBoolConfigOption("unlimitedhouses"))
				{
					// Make houses available again
					if ( eTmpHouses == 5 )
						m_hotels++;
					else
						m_houses += eTmpHouses;
				}
				eTmp->setProperty("houses", 0);
			}

			// Transfer ownership to creditor
			if (pCreditor)
				transferEstate(eTmp, pCreditor);
			else
			{
				eTmp->setBoolProperty("mortgaged", false);

				// TODO: auction all estate when there is no pCreditor
				transferEstate(eTmp, pCreditor);
			}
			// TODO: send estateupdate?
		}
	}

	// Give all money to creditor
	int payAmount = pBroke->getIntProperty("money");
	pBroke->payMoney(payAmount);

	if (pCreditor)
		pCreditor->addMoney(payAmount);
	else if (eCreditor && getBoolConfigOption("collectfines"))
		eCreditor->addMoney(payAmount);

	if (debt)
		delDebt(debt);
}

void Game::newAuction(Player *pInput)
{
	if (!getBoolConfigOption("auctionsenabled") || !totalAssets())
	{
		pInput->ioError("Auctions are not enabled.");
		return;
	}
	if (!(pInput->getBoolProperty("canauction")))
	{
		pInput->ioError("You cannot auction anything at the moment.");
		return;
	}

	Estate *estate = pInput->estate();

	m_auction = new Auction();
	m_auction->setEstate(estate);
	m_auction->addToScope(this);

	pInput->setBoolProperty("can_buyestate", false);
	pInput->setBoolProperty("canauction", false);

	Display display;
	display.setText("%s chooses to auction %s.", pInput->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
	sendDisplayMsg(&display, pInput);
	display.resetButtons(); /* Remove buy estate buttons: Buy, Auction, End Turn */
	pInput->sendDisplayMsg(&display);

	ioWrite("<monopd><auctionupdate auctionid=\"%d\" actor=\"%d\" estateid=\"%d\" status=\"0\"/></monopd>\n", m_auction->id(), pInput->id(), estate->id());
}

Auction *Game::auction()
{
	return m_auction;
}

// Returns 0 on successful bid, 1 on error.
int Game::bidInAuction(Player *pInput, char *data)
{
	if (!strstr(data, ":"))
	{
		pInput->ioError("Invalid input for .ab, no separator after auctionId");
		return 1;
	}
	int auctionId = atoi(strsep(&data, ":"));
	int bid = atoi(data);

	if ( !m_auction || m_auction->id() != auctionId )
	{
		pInput->ioError("No such auctionId: %d.", auctionId);
		return 1;
	}

	if (m_auction->status() == Auction::Sold) {
		pInput->ioError( "You can no longer bid in auction %d.", auctionId );
		return 1;
	}

	if (bid > pInput->assets())
	{
		pInput->ioError("You don't have %d.", bid);
		return 1;
	}

	int highestBid = m_auction->getIntProperty("highbid");
	if (bid <= highestBid)
	{
		pInput->ioError("Minimum bid is %d.", highestBid+1);
		return 1;
	}

	m_auction->setHighBid(pInput, bid);
	ioWrite("<monopd><auctionupdate auctionid=\"%d\" highbid=\"%d\" highbidder=\"%d\" status=\"%d\"/></monopd>\n", m_auction->id(), bid, pInput->id(), m_auction->status());
	return 0;
}

void Game::newTrade(Player *pInput, unsigned int playerId)
{
	Player *player = findPlayer(playerId);
	if (!player) {
		pInput->ioError("No such playerId: %d.", playerId);
		return;
	}
	if (player == pInput) {
		pInput->ioError("Trading with yourself?");
		return;
	}
	if (player->getBoolProperty("bankrupt")) {
		pInput->ioError("You cannot trade with bankrupt players.");
		return;
	}
	if (player->getBoolProperty("spectator")) {
		pInput->ioError("You cannot trade with spectators.");
		return;
	}

	// Create a new trade between us and player
	Trade *t = new Trade(pInput, player, m_nextTradeId++);
	m_trades.push_back(t);
}

Trade *Game::findTrade(Player *_p)
{
	Trade *tTmp = 0;
	for(std::vector<Trade *>::iterator it = m_trades.begin(); it != m_trades.end() && (tTmp = *it) ; ++it)
		if (tTmp->hasPlayer(_p))
			return tTmp;

	return 0;
}

Trade *Game::findTrade(int tradeId)
{
	Trade *tTmp = 0;
	for(std::vector<Trade *>::iterator it = m_trades.begin(); it != m_trades.end() && (tTmp = *it) ; ++it)
		if (tradeId == tTmp->id()) return tTmp;

	return 0;
}

void Game::delTrade(Trade *trade)
{
	for(std::vector<Trade *>::iterator it = m_trades.begin(); it != m_trades.end(); ++it)
		if (*it == trade)
		{
			delete trade;
			m_trades.erase(it);
			break;
		}
}

void Game::acceptTrade(Player *pInput, char *data)
{
	int tradeId = 0, revision = 0;

	if (!strstr(data, ":"))
	{
		ioError("Invalid input for .Ta, no separator after tradeId");
		return;
	}
	tradeId = atoi(strsep(&data, ":"));
	revision = atoi(data);

	Trade *trade = findTrade(tradeId);
	if (!trade)
	{
		pInput->ioError("No such tradeId: %d.", tradeId);
		return;
	}
	if ( !(trade->hasPlayer(pInput)) )
	{
		pInput->ioError("You are not part of trade %d.", tradeId);
		return;
	}
	if (revision != trade->getIntProperty("revision"))
	{
		pInput->ioError("Ignored accept for trade %d because it was changed.", tradeId);
		return;
	}

	trade->setPlayerAccept(pInput, true);

	if (trade->allAccept())
	{
		// Complete trade
		completeTrade(trade);
		delTrade(trade);
		if (!clientsMoving() && !m_auction && !m_pausedForDialog)
			m_pTurn->endTurn();
	}
}

void Game::rejectTrade(Player *pInput, unsigned int tradeId, const bool verbose)
{
	Trade *trade = findTrade(tradeId);
	if (!trade)
	{
		if (verbose) {
			pInput->ioError("No such tradeId: %d.", tradeId);
		}
		return;
	}

	if ( !(trade->hasPlayer(pInput)) )
	{
		if (verbose) {
			pInput->ioError("You are not part of trade %d.", tradeId);
		}
		return;
	}

	ioWrite("<monopd><tradeupdate type=\"rejected\" tradeid=\"%d\" actor=\"%d\"/></monopd>\n", trade->id(), pInput->id());
	delTrade(trade);
}

void Game::completeTrade(Trade *trade)
{
	ioWrite("<monopd><tradeupdate tradeid=\"%d\" type=\"accepted\"/></monopd>\n", trade->id());

	Player *pFrom, *pTo;
	GameObject *object;
	while((pTo = trade->firstTarget()))
	{
		Display display1, display2;
		pFrom = trade->firstFrom();
		object = trade->takeFirstObject();
		switch(object->type())
		{
		case GameObject::Money:
			unsigned int money;
			money = object->id();
			delete object; // was temporarily created to serve as trade object
			if (!pFrom->payMoney(money))
			{
				display2.setText("%s owes %d to %s. Game paused, %s is not solvent. Player needs to raise %d in cash first.", pFrom->getStringProperty("name").c_str(), money, pTo->getStringProperty("name").c_str(), pFrom->getStringProperty("name").c_str(), (money - pFrom->getIntProperty("money")));
				sendDisplayMsg(&display2);

				newDebt(pFrom, pTo, 0, money);
			}
			else
				pTo->addMoney(money);

			display1.setText("%s gets %d from %s.", pTo->getStringProperty("name").c_str(), money, pFrom->getStringProperty("name").c_str());
			sendDisplayMsg(&display1);
			break;
		default:
			transferObject(object->type(), object->id(), pTo, true);
		}
	}

	// Write "completed" to all players
	ioWrite("<monopd><tradeupdate tradeid=\"%d\" type=\"completed\"/></monopd>\n", trade->id());

	// Solve debts for trade players
	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		if (trade->hasPlayer(player))
			solveDebts(player);
}

void Game::abortAuction()
{
	if (!m_auction)
		return;

	printf("Game::abortAuction()\n");

	ioWrite("<monopd><auctionupdate auctionid=\"%d\" status=\"%d\"/></monopd>\n", m_auction->id(), Auction::Sold);

	delete m_auction;
	m_auction = NULL;

	m_pTurn->endTurn();
}

void Game::completeAuction()
{
	if (!m_auction || m_auction->status() != Auction::Sold || m_auctionDebt)
		return;

	printf("Game::completeAuction()\n");

	Player *pBid = m_auction->highBidder();
	Estate *estate = m_auction->estate();
	transferEstate(estate, pBid);

	Display display;
	display.setText("Purchased by %s in an auction for %d.", pBid->getStringProperty("name").c_str(), m_auction->getIntProperty("highbid"));
	sendDisplayMsg(&display);

	delete m_auction;
	m_auction = NULL;

	m_pTurn->endTurn();
}

void Game::sendDisplayMsg(Display *display, Player *except) {
	for (std::vector<Player *>::iterator pit = m_players.begin(); pit != m_players.end(); ++pit) {
		if (*pit != except) {
			(*pit)->sendDisplayMsg(display);
		}
	}
}

void Game::sendMsgEstateUpdate(Estate *e)
{
	Estate *eTmp = 0;
	Player *pTmp = 0;
	for (std::vector<Player *>::iterator pit = m_players.begin(); pit != m_players.end() && (pTmp = *pit) ; ++pit) {
		for(std::vector<Estate *>::iterator eit = m_estates.begin(); eit != m_estates.end() && (eTmp = *eit) ; ++eit) {
			if(eTmp->group() == e->group())
				pTmp->ioWrite("<monopd><estateupdate estateid=\"%d\" owner=\"%d\" can_toggle_mortgage=\"%d\" can_buy_houses=\"%d\" can_sell_houses=\"%d\"/></monopd>\n", eTmp->id(), (eTmp->owner() ? eTmp->owner()->id() : -1), eTmp->canToggleMortgage(pTmp), eTmp->canBuyHouses(pTmp), eTmp->canSellHouses(pTmp));
		}
	}
}

EstateGroup *Game::newGroup(const std::string &name)
{
	EstateGroup *group = new EstateGroup(m_nextEstateGroupId++, this, name);
	m_estateGroups.push_back(group);
	return group;
}

EstateGroup *Game::findGroup(const std::string &name)
{
	EstateGroup *group = 0;
	for(std::vector<EstateGroup *>::iterator it = m_estateGroups.begin(); it != m_estateGroups.end() && (group = *it) ; ++it)
		if (group->name() == name)
			return group;

	return 0;
}

unsigned int Game::estateGroupSize(EstateGroup *estateGroup)
{
	unsigned int size = 0;

	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator it = m_estates.begin(); it != m_estates.end() && (eTmp = *it) ; ++it)
		if (estateGroup == eTmp->group())
			size++;

	return size;
}

Estate *Game::newEstate(const std::string &name)
{
	Estate *es = new Estate(m_nextEstateId++, &m_estates);
	m_estates.push_back(es);

	// Set properties with game-wide scope
	es->setProperty("name", name, this);
	es->setProperty("houses", 0, this);
	es->setProperty("money", 0, this);
	es->setBoolProperty("mortgaged", false, this);

	return es;
}

Estate *Game::findEstate(int id)
{
	if (id >= 0 && id < (int)m_estates.size())
		return m_estates[id];
	else
		return NULL;
}

Estate *Game::findNextEstate(EstateGroup *group, Estate *startEstate)
{
	bool useNext = true;
	if (startEstate)
		useNext = false;

	Estate *eTmp = 0, *eFirst = 0;
	for (std::vector<Estate *>::iterator eIt = m_estates.begin() ; eIt != m_estates.end() && (eTmp = *eIt) ; eIt++)
	{
		if (startEstate && startEstate == eTmp)
			useNext = true;
		if (eTmp->group() == group)
		{
			if (useNext==true)
				return eTmp;
			else if (!eFirst)
				eFirst = eTmp;
		}
	}
	return eFirst;
}

Estate *Game::findNextJailEstate(Estate *startEstate)
{
	bool useNext = true;
	if (startEstate)
		useNext = false;

	Estate *eTmp = 0, *eFirst = 0;
	for (std::vector<Estate *>::iterator eIt = m_estates.begin() ; eIt != m_estates.end() && (eTmp = *eIt) ; eIt++)
	{
		if (startEstate && startEstate == eTmp)
			useNext = true;
		if (eTmp->getBoolProperty("jail"))
		{
			if (useNext==true)
				return eTmp;
			else if (!eFirst)
				eFirst = eTmp;
		}
	}
	return eFirst;
}

void Game::transferEstate(Estate *estate, Player *player, const bool verbose)
{
	if (estate->owner())
	{
		Trade *trade = 0;
		for(std::vector<Trade *>::iterator it = m_trades.begin(); it != m_trades.end() && (trade = *it) ; ++it)
			trade->delComponent(estate->type(), estate->id());
	}

	estate->setOwner(player);
	sendMsgEstateUpdate(estate);

	if (player && verbose) {
		Display display;
		display.setText("%s is now the owner of estate %s.", player->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
		sendDisplayMsg(&display);
	}
}

void Game::transferObject(const enum GameObject::Type type, unsigned int id, Player *player, const bool verbose)
{
	switch(type)
	{
	case GameObject::GCard:
		transferCard(findCard(id), player, verbose);
		break;
	case GameObject::GEstate:
		transferEstate(findEstate(id), player, verbose);
		break;
	default:;
	}
}

void Game::setPausedForDialog(const bool paused)
{
	m_pausedForDialog = paused;
}

bool Game::pausedForDialog()
{
	return m_pausedForDialog;
}

void Game::transferCard(Card *card, Player *player, const bool verbose)
{
	Player *owner = card->owner();
	if (owner)
	{
		owner->takeCard(card);
		card->setOwner(0);
		Trade *trade = 0;
		for(std::vector<Trade *>::iterator it = m_trades.begin(); it != m_trades.end() && (trade = *it) ; ++it)
			trade->delComponent(card->type(), card->id());
	}
	else
		card->group()->popCard();

	if (player)
	{
		player->addCard(card);
		card->setOwner(player);
	}
	else
		card->group()->pushCard(card);

	ioWrite("<monopd><cardupdate cardid=\"%d\" title=\"%s\" owner=\"%d\"/></monopd>\n", card->id(), card->name().c_str(), card->owner() ? card->owner()->id() : -1);
	if (verbose) {
		Display display;
		display.setText("%s is now the owner of card %d.", player->getStringProperty("name").c_str(), card->id());
		sendDisplayMsg(&display);
	}
}

CardGroup *Game::newCardGroup(const std::string name)
{
	CardGroup *cardGroup = new CardGroup(m_nextCardGroupId++, this, name);
	m_cardGroups.push_back(cardGroup);
	return cardGroup;
}

CardGroup *Game::findCardGroup(const std::string name)
{
	CardGroup *cardGroup = 0;
	for(std::vector<CardGroup *>::iterator it = m_cardGroups.begin(); it != m_cardGroups.end(); ++it)
		if ((cardGroup = *it) && cardGroup->name() == name)
			return cardGroup;

	return 0;
}

Card *Game::findCard(unsigned int id)
{
	Card *card = 0;

	CardGroup *cardGroup = 0;
	for(std::vector<CardGroup *>::iterator it = m_cardGroups.begin(); it != m_cardGroups.end() && (cardGroup = *it) ; ++it)
	{
		card = cardGroup->findCard(id);
		if (card)
			return card;
	}

	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
	{
		card = player->findCard(id);
		if (card)
			return card;
	}

	return 0;
}

Player *Game::addPlayer(Player *p, const bool &isMaster, const bool &isSpectator)
{
	m_players.push_back(p);
	setProperty( "players", m_players.size() );

	if ( m_players.size() == getBoolProperty("maxplayers") )
		setBoolProperty("canbejoined", false);

	p->setGame(this);
	addToScope(p);

	// Properties with game as scope
	p->setProperty("money", 0, this);
	p->setBoolProperty("bankrupt", false, this);
	p->setBoolProperty("jailed", false, this);
	p->setBoolProperty("hasturn", 0, this);
	p->setBoolProperty("spectator", isSpectator, this);

	if (isMaster)
	{
		m_master = p;
		setProperty( "master", p->id() );
	}

	// Properties with player as scope
	p->setProperty("doublecount", 0);
	p->setProperty("jailcount", 0);
	p->setBoolProperty("can_roll", false);
	p->setBoolProperty("canrollagain", false);
	p->setBoolProperty("can_buyestate", false);
	p->setBoolProperty("canauction", false);
	p->setBoolProperty("hasdebt", false);
	p->setBoolProperty("canusecard", false);


	// Temporarily set status at init for joining player
	if (m_status == Run)
	{
		m_status = Init;
		sendFullUpdate(p);
		m_status = Run;
	}
	else
		sendFullUpdate(p);

	if (p->getBoolProperty("spectator"))
		sendStatus(p);

	return p;
}

Player *Game::findPlayer(int playerId)
{
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		if (playerId == pTmp->id())
			return pTmp;

	return 0;
}

void Game::removePlayer(Player *p)
{
	if (p->getBoolProperty("hasturn")) {
		if (clientsMoving()) {
			tokenMovementTimeout();
		}
		updateTurn();
	}

	// Find player and erase from std::vector
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it)
		if (*it == p)
		{
			m_players.erase(it);
			break;
		}

	// Do everything after player is removed from list, player doesn't need the resulting messages
	setProperty( "players", m_players.size() );

	// If in Config, canbejoined might become true again
	if (m_status == Config) {
		if ( m_players.size() < getBoolProperty("maxplayers") )
			setBoolProperty("canbejoined", true);
	}
	else if (m_status != End) {
		bankruptPlayer(p);
	}

	p->setGame(0);
	removeFromScope(p);

	if (p == m_master && m_players.size() > 0)
	{
		m_master = m_players[0];
		setProperty( "master", m_master->id() );

		if (m_status == Config)
			sendConfiguration(m_master);
	}
}

Player *Game::kickPlayer(Player *pInput, int playerId)
{
	if ( pInput != m_master )
	{
		pInput->ioError("Only the master can kick players.");
		return 0;
	}

	if ( m_status != Config )
	{
		pInput->ioError("You can only kick players during game configuration.");
		return 0;
	}

	Player *player = findPlayer(playerId);
	if (!player)
	{
		pInput->ioError("No such playerId: %d.", playerId);
		return 0;
	}

	return player;
}

void Game::upgradePlayer(Player *pInput, int playerId)
{
	if ( pInput != m_master )
	{
		pInput->ioError("Only the master can upgrade spectators.");
		return;
	}

	if ( m_status != Run )
	{
		pInput->ioError("You can only upgrade spectators during a game in progress.");
		return;
	}

	Player *player = findPlayer(playerId);
	if (!player)
	{
		pInput->ioError("No such playerId: %d.", playerId);
		return;
	}

	if ( !player->getBoolProperty("spectator") )
	{
		pInput->ioError( "%s is not a spectator in this game.", player->name().c_str() );
		return;
	}

	// Reset player and remove spectator property.
	player->reset(false);
	player->setEstate(m_goEstate);
	player->addMoney(m_startMoney);
	player->setBoolProperty("spectator", 0, this);

	ioInfo( "%s upgrades %s to a participating player.", pInput->name().c_str(), player->name().c_str() );
}

unsigned int Game::playerAssets(Player *player)
{
	unsigned int assets = player->getIntProperty("money");
	Estate *estate = 0;
	for(std::vector<Estate *>::iterator it = m_estates.begin(); it != m_estates.end() && (estate = *it) ; ++it)
	{
		if ( player == estate->owner() )
		{
			if (!estate->getBoolProperty("mortgaged"))
			{
				assets += estate->getIntProperty("mortgageprice");
				assets += ( estate->getIntProperty("houses") * estate->getIntProperty("sellhouseprice"));
			}
		}
	}
	return assets;
}

void Game::updateTurn()
{
	if (m_status == Game::End)
		return;

	Player *pOldTurn = m_pTurn;
	// Disable turn, roll and buy.
	pOldTurn->setTurn(false);

	Player *player = 0, *pFirst = 0;
	bool useNext = false;
	for(std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (player = *it) ; ++it)
	{
		if (!pFirst && !player->getBoolProperty("bankrupt"))
			pFirst = player;

		if (player == pOldTurn)
			useNext = true;
		else if (useNext && !player->getBoolProperty("bankrupt") && !player->getBoolProperty("spectator"))
		{
			m_pTurn = player;
			useNext = false;
			break;
		}
	}
	if (useNext && pFirst)
		m_pTurn = pFirst;

	// Set turn.
	Display display;
	display.setText("Turn goes to %s.", m_pTurn->getStringProperty("name").c_str());
	sendDisplayMsg(&display);

	m_pTurn->setTurn(true);
}

bool Game::landPlayer(Player *pTurn, const bool directMove, const std::string &rentMath)
{
	Estate *destination = pTurn->destination();

	if (destination)
	{
		bool useNext = false;
		unsigned int money = 0;
		Estate *estate = 0;
		for (std::vector<Estate *>::iterator it = m_estates.begin() ;;)
		{
			if (it == m_estates.end())
			{
				it = m_estates.begin();
				continue;
			}

			if (!(estate = *it))
				break;

			if (useNext)
			{
				if (estate->getIntProperty("passmoney"))
				{
					money += estate->getIntProperty("passmoney");
					// Write incremental message for direct moves, token
					// confirmation or timeout didn't do it yet.
					if (directMove) {
						Display display;
						display.setText("%s passes %s and gets %d.", pTurn->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), estate->getIntProperty("passmoney"));
						sendDisplayMsg(&display);
					}
				}
				if (estate == destination)
					break;
			}
			else if (estate == pTurn->estate())
				useNext = true;

			++it;
		}
		pTurn->setEstate(destination);
		pTurn->setDestination(0);

		// Store and write final data.
		pTurn->addMoney(money);
	}

	// Hm, we wait for all tokens to have landed, this is exactly what we
	// were trying to avoid with async updates! So some of this stuff can be
	// sent earlier.

	// What properties does the estate we are landing on have?
	bool endTurn = true;
	Player *pOwner = 0;
	Estate *es = pTurn->estate();

	if (getBoolConfigOption("doublepassmoney") && es->getIntProperty("passmoney"))
	{
		Display display;
		display.setText("%s lands on %s and gets %d.", pTurn->getStringProperty("name").c_str(), es->getStringProperty("name").c_str(), es->getIntProperty("passmoney"));
		display.setEstate(es);
		sendDisplayMsg(&display);

		pTurn->addMoney(es->getIntProperty("passmoney"));
	}
	else {
		Display display;
		display.setText("%s lands on %s.", pTurn->getStringProperty("name").c_str(), es->getStringProperty("name").c_str());
		display.setEstate(es);
		sendDisplayMsg(&display);
	}

	// toJail is really serious: if an estate has this property, all other
	// properties are ignored when landing it. TODO: Don't ignore and allow
	// weird combinations?
	if (es->getBoolProperty("tojail"))
	{
		if ( Estate *eTmp = findNextJailEstate(es) )
			pTurn->toJail(eTmp);
		else
			ioError("This gameboard does not have a jail estate.");

		updateTurn();
		return true;
	}

	// Any estate can have a pot of gold. This is handled before all other
	// properties except toJail and works in combination with any of those
	// properties.
	if ( int estateMoney = es->getIntProperty("money") )
	{
		if ( estateMoney > 0)
		{
			// Good for you!
			pTurn->addMoney(estateMoney);
			es->setProperty("money", 0);

			Display display;
			display.setText("%s gets %d from the fine pot.", pTurn->getStringProperty("name").c_str(), estateMoney);
			sendDisplayMsg(&display);
		}
		else
		{
			// Pay up!
			Estate * const ePayTarget = es->payTarget();
			if (!pTurn->payMoney(estateMoney))
			{
				// TODO: If we go into debt here, we'll never enter rent
				// calculation. But we'll need negative getIntProperty("money") piles for
				// Estates because that's basically what tax estates are.
				Display display;
				display.setText("%s has to pay %d but is not solvent. Game paused, %s is not solvent. Player needs to raise %d in cash first.", pTurn->getStringProperty("name").c_str(), estateMoney, pTurn->getStringProperty("name").c_str(), (estateMoney - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebt(pTurn, 0, ePayTarget, estateMoney);
				return false;
			}
			else if (ePayTarget && getBoolConfigOption("collectfines"))
				ePayTarget->addMoney(estateMoney);

			Display display;
			display.setText("%s pays %d.", pTurn->getStringProperty("name").c_str(), estateMoney);
			sendDisplayMsg(&display);
		}
	}

	// Any estate can have taxes on them. This is handled before rent and purchase opportunities.
	// TODO: Merge with getIntProperty("money") stuff to avoid duplication?
	int tax = es->getIntProperty("tax"), taxPercentage = es->getIntProperty("taxpercentage");

	if (tax || taxPercentage)
	{
		int payAmount = 0;
		if ( tax && taxPercentage )
		{
			GameObject *config = findConfigOption("automatetax");
			if (config && config->getBoolProperty("value") )
			{
				payAmount = tax;
				tax = (taxPercentage * pTurn->assets() / 100);
				if (tax < payAmount)
						payAmount = tax;
			}
			else
			{
				Display display1, display2;

				display1.setText("%s must pay either %d or %d percent of his/her assets.", pTurn->getStringProperty("name").c_str(), tax, taxPercentage);
				sendDisplayMsg(&display1, pTurn);

				display2.setText("Pay either %d or %d percent of your assets.", tax, taxPercentage);
				display2.addButton(".T$", std::string("Pay ") + itoa(tax), 1);\
				display2.addButton(".T%", std::string("Pay ") + itoa(taxPercentage) + " Percent", 1);
				pTurn->sendDisplayMsg(&display2);

				// TODO: port this into a blocking bool in Display which we can check, will be more generic
				m_pausedForDialog = true;
				return false;
			}
		}
		else
			payAmount = ( tax ? tax : (int)(taxPercentage * pTurn->assets() / 100 ) );

		Estate * const ePayTarget = es->payTarget();

		if (!pTurn->payMoney(payAmount))
		{
			// TODO: If we go into debt here, we'll never enter rent
			// calculation. So, estates with tax shouldn't be ownable
			// ever.
			Display display;
			display.setText("Game paused, %s owes %d but is not solvent. Player needs to raise %d in cash first.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
			sendDisplayMsg(&display);

			newDebt(pTurn, 0, ePayTarget, payAmount);
			return false;
		}
		else if (ePayTarget && getBoolConfigOption("collectfines"))
			ePayTarget->addMoney(payAmount);

		Display display;
		display.setText("%s pays %d.", pTurn->getStringProperty("name").c_str(), payAmount);
		sendDisplayMsg(&display);
	}

	// Some estates have cards. Handle them before we do rent and purchasing.
	if ( CardGroup *cardGroup = es->takeCardGroup() )
	{
		if (getBoolConfigOption("alwaysshuffle"))
			cardGroup->shuffleCards();

		endTurn = giveCard(pTurn, cardGroup->nextCard());
	}

	// Calculate rent for owned estates or offer them for sale.
	if ( es->canBeOwned() )
	{
		if ((pOwner = es->owner()))
		{
			// Owned.

			if (pOwner == pTurn) {
				Display display;
				display.setText("%s already owns it.", pTurn->getStringProperty("name").c_str());
				sendDisplayMsg(&display);
			} else if (es->getBoolProperty("mortgaged")) {
				Display display;
				display.setText("%s pays no rent because it's mortgaged.", pTurn->getStringProperty("name").c_str());
				sendDisplayMsg(&display);
			} else if (getBoolConfigOption("norentinjail") && pOwner->getBoolProperty("jailed")) {
				Display display;
				display.setText("%s pays no rent because owner %s is in jail.", pTurn->getStringProperty("name").c_str(), pOwner->getStringProperty("name").c_str());
				sendDisplayMsg(&display);
			} else {
				// Pay payAmount owed.
				int payAmount = es->rent(pTurn, rentMath);

				if (!pTurn->payMoney(payAmount))
				{
					Display display;
					display.setText("Game paused, %s owes %d to %s but is not solvent. Player needs to raise %d in cash first.", pTurn->getStringProperty("name").c_str(), payAmount, pOwner->getStringProperty("name").c_str(), (payAmount - pTurn->getIntProperty("money")));
					sendDisplayMsg(&display);

					newDebt(pTurn, pOwner, 0, payAmount);
					return false;
				}
				Display display;
				display.setText("%s pays %d rent to %s.", pTurn->getStringProperty("name").c_str(), payAmount, pOwner->getStringProperty("name").c_str());
				sendDisplayMsg(&display);

				pOwner->addMoney(payAmount);
			}
		}
		else
		{
			// Unowned, thus for sale.
			pTurn->setBoolProperty("can_roll", false);
			pTurn->setBoolProperty("can_buyestate", true);
			pTurn->setBoolProperty("canauction", (getBoolConfigOption("auctionsenabled") && totalAssets()));

			Display display;
			display.setText("For sale.");
			sendDisplayMsg(&display, pTurn);
			display.addButton(".eb", "Buy", 1);
			display.addButton(".ea", "Auction", (getBoolConfigOption("auctionsenabled") && totalAssets()));
			display.addButton(".E", "End Turn", !getBoolConfigOption("auctionsenabled"));
			pTurn->sendDisplayMsg(&display);
			return false;
		}
	}
	return endTurn;
}
bool Game::giveCard(Player *pTurn, Card *card)
{
	if (!card)
	{
		ioError(pTurn->name() + " should get a card, but there don't seem to be any available!");
		return true;
	}

	Display display;
	display.setText("%s", card->getStringProperty("name").c_str());
	sendDisplayMsg(&display);

	// If canBeOwned, remove from stack and give to player. Card can still
	// have actions afterwards.
	if (card->canBeOwned())
		transferCard(card, pTurn);

	// Actions
	if (card->toJail())
	{
		if ( Estate *eTmp = findNextJailEstate(pTurn->estate()) )
			pTurn->toJail(eTmp);
		else
			ioError("This gameboard does not have a jail estate.");

		updateTurn();
	}
	else if (card->pay())
	{
		int payAmount = card->pay();
		Estate * const ePayTarget = pTurn->estate()->payTarget();
		if (payAmount > 0)
		{
			if (!pTurn->payMoney(payAmount))
			{
				Display display;
				display.setText("Game paused, %s owes %d but is not solvent. Player needs to raise %d in cash first.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebt(pTurn, 0, ePayTarget, payAmount);
				return false;
			}
			else if (ePayTarget && getBoolConfigOption("collectfines"))
				ePayTarget->addMoney(payAmount);
		}
		else if (payAmount < 0)
			pTurn->addMoney(-payAmount);
	}
	else if (card->payEach())
	{
		Player *pTmp = 0;
		int payAmount = 0;
		for (std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (pTmp = *it) ; ++it)
		if (pTmp != pTurn && !pTmp->getBoolProperty("bankrupt") && !pTmp->getBoolProperty("spectator"))
			payAmount += card->payEach();

		if (payAmount > 0)
		{
			if (!pTurn->payMoney(payAmount))
			{
				Display display;
				display.setText("Game paused, %s owes %d but is not solvent. Player needs to raise %d in cash first.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebtToAll(pTurn, card->payEach());
			}
			else
			{
				for (std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (pTmp = *it) ; ++it)
					if (pTmp != pTurn && !pTmp->getBoolProperty("bankrupt") && !pTmp->getBoolProperty("spectator"))
						pTmp->addMoney(card->payEach());
			}
		}
		else
		{
			payAmount = -(card->payEach());
			for (std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (pTmp = *it) ; ++it)
				if (pTurn != pTmp && !pTmp->getBoolProperty("bankrupt") && !pTmp->getBoolProperty("spectator"))
				{
					if (!pTmp->payMoney(payAmount))
					{
						Display display;
						display.setText("Game paused, %s owes %d but is not solvent. Player needs to raise %d in cash first.", pTmp->getStringProperty("name").c_str(), payAmount, (payAmount - pTmp->getIntProperty("money")));
						sendDisplayMsg(&display);

						newDebt(pTmp, pTurn, 0, payAmount);
					}
					else
						pTurn->addMoney(payAmount);
				}
			}
	}
	else if (card->payHouse() && card->payHotel())
	{
		int payAmount = 0;

		Estate *eTmp = 0;
		for (std::vector<Estate *>::iterator eIt = m_estates.begin() ; eIt != m_estates.end() && (eTmp = *eIt) ; eIt++)
		{
			if (pTurn == eTmp->owner())
			{
				if (eTmp->getIntProperty("houses") == 5)
					payAmount += card->payHotel();
				else if (eTmp->getIntProperty("houses"))
					payAmount += (eTmp->getIntProperty("houses") * card->payHouse());
			}
		}

		if (payAmount > 0)
		{
			Estate * const ePayTarget = pTurn->estate()->payTarget();
			if (!pTurn->payMoney(payAmount))
			{
				Display display;
				display.setText("Game paused, %s owes %d but is not solvent. Player needs to raise %d in cash first.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebt(pTurn, 0, ePayTarget, payAmount);
				return false;
			}
			else if (ePayTarget && getBoolConfigOption("collectfines"))
				ePayTarget->addMoney(payAmount);
		}
		else if (payAmount < 0)
			pTurn->addMoney(-payAmount);
	}
	else if (card->advance() || card->advanceTo()!=-1 || card->advanceToNextOf())
	{
		// Duplicated from cmd_roll. :-(
		if (card->advance())
			pTurn->advance(card->advance(), true);
		else if (card->advanceTo()!=-1)
			pTurn->advanceTo(card->advanceTo(), true);
		else if (EstateGroup *group = card->advanceToNextOf())
		{
			if (Estate *eTmp = findNextEstate(group, pTurn->estate()))
				pTurn->advanceTo(eTmp->id(), true);
			else
				ioError("Could not find next estate on gameboard.");
		}
		return landPlayer(pTurn, true, card->rentMath());
	}
	return true;
}

void Game::declareBankrupt(Player *pInput)
{
	Display display;
	display.setText("%s declares bankruptcy!", pInput->getStringProperty("name").c_str());
	sendDisplayMsg(&display, pInput);
	display.resetButtons(); /* Player declared bankruptcy, removed declare bankruptcy button */
	pInput->sendDisplayMsg(&display);

	bankruptPlayer(pInput);
}

void Game::bankruptPlayer(Player *pBroke)
{
	if (pBroke->getBoolProperty("bankrupt") || pBroke->getBoolProperty("spectator"))
		return;

	// abort any trade where this player is participating
	for (std::vector<Trade *>::iterator it = m_trades.begin(); it != m_trades.end() && (*it);) {
		unsigned int prevsize = m_trades.size();
		rejectTrade(pBroke, (*it)->id(), false);
		if (prevsize != m_trades.size()) {
			it = m_trades.begin();
			continue;
		}
		++it;
	}

	// highest bidder might be the player who just left
	if (m_auction && m_auction->highBidder() == pBroke) {
		abortAuction();
	}

	// Set bankrupt flag
	pBroke->setBoolProperty("bankrupt", true);

	Debt *debt = 0;
	bool debtsWereSolved = false;

	// Find debts we are creditor in and remove them
	while ((debt = findDebtByCreditor(pBroke)))
	{
		delDebt(debt);
		debtsWereSolved = true;
	}

	// Find debts we are debitor in and pay them off
	while ((debt = findDebt(pBroke)))
	{
		enforceDebt(pBroke, debt);
		debtsWereSolved = true;
	}

	// Player might still have assets, when he goes bankrupt without debts
	// (disconnection timeout), so we'll enforce giving assets to the Bank.
	if (pBroke->assets())
		enforceDebt(pBroke);

	// Count active (non-bankrupt) players
	int activePlayers = 0;
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp =  *it) ; ++it)
		if (pTmp && !pTmp->getBoolProperty("bankrupt") && !pTmp->getBoolProperty("spectator"))
		{
			activePlayers++;
			m_pWinner = pTmp;
		}

	if (activePlayers == 1)
	{
		// a user who disconnect might end the game while an auction was running
		abortAuction();

		m_status = End;
		syslog(LOG_INFO, "game %d ended: %s wins with a fortune of %d.", m_id, m_pWinner->name().c_str(), m_pWinner->assets());

		Display display;
		display.resetButtons();  /* Reset any button, game might end if a player left while we were asked to buy an estate for example */
		display.setText("The game has ended! %s wins with a fortune of %d!", m_pWinner->getStringProperty("name").c_str(), m_pWinner->assets());
		display.addButton(".gx", "New Game", true);
		sendDisplayMsg(&display);

		for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
			sendStatus(*it);
		}
	}
	else
	{
		m_pWinner = 0;

		if (activePlayers && !m_debts.size())
		{
			if (debtsWereSolved) {
				Display display;
				display.setText("All debts are settled, game continues.");
				sendDisplayMsg(&display);
			}

			// Update turn. Always when this function is called as command
			// and possibly necessary when this function is called for a
			// disconnected user.

			if (pBroke == m_pTurn)
				updateTurn();
		}
	}
}

int Game::lowestFreeId()
{
	int lowest = 0;

	// Find lowest free id in std::vector.
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (*it) ; ++it)
		if ( (*it)->id() >= lowest)
			lowest = (*it)->id() + 1;

	return lowest;
}

int Game::totalAssets()
{
	int assets = 0;

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (*it) ; ++it)
		assets += (*it)->assets();

	return assets;
}

void Game::rollDice()
{
	// TODO: decent dice display
	dice[0] = 1 + (int) (6.0 * rand()/(RAND_MAX + 1.0 ));
	dice[1] = 1 + (int) (6.0 * rand()/(RAND_MAX + 1.0 ));
}

int Game::players()
{
	return (int)m_players.size();
}

int Game::debts()
{
	return m_debts.size();
}

unsigned int Game::clientsMoving()
{
	unsigned int moving = 0;
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		if (pTmp->tokenLocation())
			moving++;
	return moving;
}

void Game::setAllClientsMoving(Estate *estate)
{
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
	if (pTmp->socket())
		pTmp->setTokenLocation(estate);
}

void Game::sendStatus(Player *p)
{
	p->ioWrite("<monopd><gameupdate gameid=\"%d\" status=\"%s\"/></monopd>\n", m_id, statusLabel().c_str());
}

void Game::sendEstateList(Player *p)
{
	Estate *eTmp = 0;
	for(std::vector<Estate *>::iterator eIt = m_estates.begin(); eIt != m_estates.end() && (eTmp = *eIt) ; eIt++)
	{
		std::string bgColor = eTmp->getStringProperty("bgcolor");
		if (!bgColor.size())
		{
			EstateGroup *estateGroup = eTmp->group();
			if (estateGroup)
				bgColor = estateGroup->getStringProperty("bgcolor");

			if (!bgColor.size())
				bgColor = m_bgColor;
		}

		std::string color = eTmp->getStringProperty("color");
		if (!color.size())
		{
			EstateGroup *estateGroup = eTmp->group();
		 	if (estateGroup)
				color = estateGroup->getStringProperty("color");
		}

		p->ioWrite("<estateupdate estateid=\"%d\" color=\"%s\" bgcolor=\"%s\" owner=\"%d\" houseprice=\"%d\" group=\"%d\" can_be_owned=\"%d\" can_toggle_mortgage=\"%d\" can_buy_houses=\"%d\" can_sell_houses=\"%d\" price=\"%d\" rent0=\"%d\" rent1=\"%d\" rent2=\"%d\" rent3=\"%d\" rent4=\"%d\" rent5=\"%d\"/>", eTmp->id(), color.c_str(), bgColor.c_str(), (eTmp->owner() ? eTmp->owner()->id() : -1), eTmp->housePrice(), eTmp->group() ? eTmp->group()->id() : -1, eTmp->canBeOwned(), eTmp->canToggleMortgage(p), eTmp->canBuyHouses(p), eTmp->canSellHouses(p), eTmp->price(), eTmp->rentByHouses(0), eTmp->rentByHouses(1), eTmp->rentByHouses(2), eTmp->rentByHouses(3), eTmp->rentByHouses(4), eTmp->rentByHouses(5));
		p->ioWrite(eTmp->oldXMLUpdate(p, true));
	}
}

void Game::sendEstateGroupList(Player *p)
{
	EstateGroup *egTmp = 0;
	for(std::vector<EstateGroup *>::iterator it = m_estateGroups.begin(); it != m_estateGroups.end() && (egTmp = *it) ; ++it)
		p->ioWrite("<estategroupupdate groupid=\"%d\" name=\"%s\"/>", egTmp->id(), egTmp->name().c_str());
}

void Game::sendPlayerList(Player *pOut)
{
	Player *pTmp = 0;
	for(std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (pTmp = *it) ; ++it)
	{
		pOut->ioWrite("<playerupdate playerid=\"%d\" name=\"%s\" location=\"%d\" jailed=\"%d\" directmove=\"%d\" hasturn=\"%d\" can_roll=\"%d\" turnorder=\"%d\"/>", pTmp->id(), pTmp->name().c_str(), pTmp->getIntProperty("location"), pTmp->getBoolProperty("jailed"), 1, pTmp->getBoolProperty("hasturn"), pTmp->getBoolProperty("can_roll"), pTmp->getIntProperty("turnorder"));
		pOut->ioWrite(pTmp->oldXMLUpdate(pOut, true));
	}
}

void Game::sendCardList(Player *pOut)
{
	for(std::vector<Player *>::iterator it = m_players.begin() ; it != m_players.end() && (*it) ; ++it) {
		(*it)->sendCardList(pOut);
	}
}

void Game::sendFullUpdate(Player *p)
{
	sendStatus(p);

	if (m_status == Config) {
		sendConfiguration(p);
		return;
	}

	p->ioWrite("<monopd>");
	sendPlayerList(p);
	sendEstateGroupList(p);
	sendEstateList(p);
	sendCardList(p);
	p->ioWrite("</monopd>\n");
}

void Game::sendConfiguration(Player *p)
{
	p->ioWrite("<monopd>");
	for(std::vector<GameObject *>::iterator it = m_configOptions.begin(); it != m_configOptions.end() && (*it) ; ++it)
		p->ioWrite( (*it)->oldXMLUpdate(p, true) );
	p->ioWrite("</monopd>\n");
}

const std::string Game::statusLabel()
{
	switch(m_status)
	{
	case Config:
		return "config";
	case Init:
		return "init";
	case Run:
		return "run";
	case End:
		return "end";
	default:
		return "default";
	}
}

bool Game::sendChildXMLUpdate(Player *pOutput, bool updateEmpty)
{
	// Send updates about all config options
	GameObject *configOption = 0;
	for(std::vector<GameObject *>::iterator it = m_configOptions.begin() ; it != m_configOptions.end() && (configOption = *it) ; ++it)
	{
		// .. but only when changed (and in property scope)
		std::string updateXML = configOption->oldXMLUpdate(pOutput);
		if (updateXML.size())
		{
			if (updateEmpty)
			{
				pOutput->ioWrite("<monopd>");
				updateEmpty = false;
			}
			pOutput->ioWrite("%s", updateXML.c_str());
		}
	}

	// Send updates *about* all estates ..
	Estate *eUpdate = 0;
	for(std::vector<Estate *>::iterator uit = m_estates.begin(); uit != m_estates.end() && (eUpdate = *uit) ; ++uit)
	{
		// .. but only when changed (and in property scope)
		std::string updateXML = eUpdate->oldXMLUpdate(pOutput);
		if (updateXML.size())
		{
			if (updateEmpty)
			{
				pOutput->ioWrite("<monopd>");
				updateEmpty = false;
			}
			pOutput->ioWrite("%s", updateXML.c_str());
		}
	}
	return updateEmpty;
}

void Game::unsetChildProperties()
{
	// Reset config options ..
	for(std::vector<GameObject *>::iterator it = m_configOptions.begin(); it != m_configOptions.end() && (*it) ; ++it)
		(*it)->unsetPropertiesChanged();

	// Reset estates ..
	for(std::vector<Estate *>::iterator it = m_estates.begin(); it != m_estates.end() && (*it) ; ++it)
		(*it)->unsetPropertiesChanged();
}
