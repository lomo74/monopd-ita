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

#include "config.h"

#define __USE_XOPEN
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
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
 :	GameObject(id, GGame), m_randomEngine(time(NULL))
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

	addBoolConfigOption( "collectfines", "Il Parcheggio Gratuito riscuote le multe", false, true );
	addBoolConfigOption( "alwaysshuffle", "Mescola sempre i mazzi prima di pescare una carta", false, true );
	addBoolConfigOption( "auctionsenabled", "Abilita vendite all'asta", true, true );
	addBoolConfigOption( "doublepassmoney", "Riscuoti il doppio in caso di fermata sul Via", false, true );
	addBoolConfigOption( "unlimitedhouses", "La Banca dispone di case/alberghi illimitati", false, true );
	addBoolConfigOption( "norentinjail", "I giocatori in Prigione non riscuotono affitti", false, true );
	addBoolConfigOption( "allowestatesales", "Consenti rivendita delle proprietà alla Banca", false, true );
	addBoolConfigOption( "automatetax", "Automatizza le decisioni sulle tasse", false, true );
	addBoolConfigOption( "allowspectators", "Consenti spettatori", true, true );
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

		// Reset estates.. so that the first player joining won't get estateupdates until init
		for (std::vector<Estate *>::iterator it = m_estates.begin(); it != m_estates.end() && (*it) ; ++it) {
			(*it)->unsetPropertiesChanged();
		}
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
		pInput->ioError("Solo il master può modificare la configurazione di gioco.");
		return;
	}

	if (m_status != Config)
	{
		pInput->ioError("Questo gioco è già iniziato.");
		return;
	}

	if (!strstr(data, ":"))
	{
		pInput->ioError("Input non valido per .gc, nessun separatore dopo configId");
		return;
	}
	int configId = atoi(strsep(&data, ":"));

	GameObject *configOption = findConfigOption(configId);
	if (!configOption)
	{
		pInput->ioError("configId inesistente: %d.", configId);
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
		pInput->ioError("Solo il master può iniziare un gioco.");
		return;
	}
	if (m_status != Config)
	{
		pInput->ioError("Questo gioco è già iniziato.");
		return;
	}
	int minPlayers = getIntProperty("minplayers");
	if (players() < minPlayers)
	{
		pInput->ioError("Questo gioco necessita almeno %d giocatori per iniziare.", minPlayers);
		return;
	}

	// Update status.
	sendStatus(Init);
	// Add notification of game start.
	m_status = Run;
	setProperty("status", "run");

	// Update whether players can join/watch.
	setBoolProperty("canbejoined", false);
	GameObject *config = findConfigOption("allowspectators");
	if (config && config->getBoolProperty("value")) {
		setBoolProperty("canbewatched", true);
	}

	// Shuffle players
	std::shuffle(m_players.begin(), m_players.end(), m_randomEngine);

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
		pTmp->setProperty("turnorder", ++turnorder, this);
		pTmp->setEstate(m_goEstate);
		pTmp->addMoney(m_startMoney);
	}

	// First turn
	setProperty("turn", 1);

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp = *it) ; ++it)
		sendFullUpdate(pTmp);

	Display display;
	display.resetText();
	display.resetEstate();
	display.setText("Gioco iniziato!");
	sendDisplayMsg(&display);

	// Turn goes to first player
	m_pTurn = *m_players.begin();
	m_pTurn->setTurn(true);
}

void Game::setTokenLocation(Player *pInput, unsigned int estateId)
{
	if (!clientsMoving())
		return;

	Estate *estateLoc = findEstate(estateId);

	if ( !estateLoc || !pInput->tokenLocation() || !m_pTurn->destination() || (pInput->tokenLocation() == estateLoc) )
		return;

//	printf("Game::setTokenLocation, P:%d PTL:%d EID:%d\n", pInput->id(), pInput->tokenLocation()->id(), estateId);

	std::vector<Estate *>::iterator it;
	// find current player estate
	for (it = m_estates.begin(); it != m_estates.end(); it++) {
		if (*it == pInput->tokenLocation()) {
//			printf("Game::setTokenLocation, %d==PTL\n", (*it)->id());
			it++;
			break;
		}
	}

	unsigned int money = 0;
	while (1) {
		if (it == m_estates.end()) {
//			printf("Game::setTokenLocation, reloop\n");
			it = m_estates.begin();
		}
		Estate *estate = *it;

		if (estate->getIntProperty("passmoney")) {
//			printf("Game::setTokenLocation, pass:%d\n", estate->id());
			Display display;
			display.setText("%s passa da %s e prende %d.", m_pTurn->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), estate->getIntProperty("passmoney"));
			pInput->sendDisplayMsg(&display);
			money += estate->getIntProperty("passmoney");
			pInput->ioWrite("<monopd><playerupdate playerid=\"%d\" money=\"%d\"/></monopd>\n", m_pTurn->id(), m_pTurn->getIntProperty("money") + money);
		}

		if (estate == m_pTurn->destination()) {
//			printf("Game::setTokenLocation, setPTL:0\n");
			pInput->setTokenLocation(0); // Player is done moving
			break;
		}

		if (estate == estateLoc) {
//			printf("Game::setTokenLocation, setPTL:%d\n", estateId);
			pInput->setTokenLocation(estate); // Player is not done moving
			break;
		}

		it++;
	}

	// Find out if there are still clients busy with movement.
	if (clientsMoving())
		return;

	// Land player!
	landPlayer(m_pTurn, false);
	// game might be ended if a player is bankrupt after landing (eg. could not pay rent), thus m_pTurn can be NULL here
	if (m_pTurn) m_pTurn->endTurn();
}

void Game::tokenMovementTimeout()
{
	// game might be ended if a player left while moving, thus m_pTurn can be NULL here
	if (!m_pTurn) {
		return;
	}

	if (!m_pTurn->destination()) {
		return;
	}

	// Mark all clients as non moving.
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->tokenLocation()) {
			setTokenLocation(*it, m_pTurn->destination()->id());
		}
	}
}

unsigned int Game::auctionTimeout()
{
	// game might be ended while an auction was in progress (eg. a player left), thus m_auction can be NULL here
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
			display.setText("%s deve pagare %d. Gioco in pausa, %s è insolvente. Il giocatore deve prima recuperare %d in contanti.", pBid->getStringProperty("name").c_str(), bid, pBid->getStringProperty("name").c_str(), (bid - pBid->getIntProperty("money")));
			sendDisplayMsg(&display);

			m_auctionDebt = newDebt(pBid, 0, 0, bid);
			return 0;
		}

		completeAuction();
		m_pTurn->endTurn();
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
		display.setText("%s è in bancarotta!", from->getStringProperty("name").c_str());
		sendDisplayMsg(&display, from);
		display.resetButtons(); /* Player is bankrupt, removed declare bankruptcy button */
		from->sendDisplayMsg(&display);

		bankruptPlayer(from);
		return 0;
	}

	if (!from->getBoolProperty("hasdebt")) {
		from->setBoolProperty("hasdebt", true);
		Display display;
		display.addButton(".D", "Dichiara bancarotta", true);
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

	if (debt == m_auctionDebt) {
		m_auctionDebt = 0;
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
			ioError("Non hai alcun debito da saldare!");
		return;
	}

	while ( (debt = findDebt(pInput)) )
	{
		if ( !solveDebt(debt) )
			break;
	}

	if (unsigned int debts = m_debts.size()) {
		Display display;
		display.setText("Ci sono ancora debiti per %d, il gioco è ancora in pausa.", debts);
		sendDisplayMsg(&display);
	}
	else {
		Display display;
		display.setText("Tutti i debiti sono saldati, il gioco prosegue.");
		sendDisplayMsg(&display, pInput);
		display.resetButtons(); /* Remove declare bankruptcy button */
		pInput->sendDisplayMsg(&display);

		m_pTurn->endTurn();
	}
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
	display.setText("%s salda un debito di %d a %s.", pFrom->getStringProperty("name").c_str(), debt->amount(), (pCreditor ? pCreditor->getStringProperty("name").c_str() : "Bank"));
	sendDisplayMsg(&display);
	delDebt(debt);
	completeAuction();
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
		pInput->ioError("Le aste non sono abilitate.");
		return;
	}
	if (!(pInput->getBoolProperty("canauction")))
	{
		pInput->ioError("Non puoi mettere all'asta nulla al momento.");
		return;
	}

	Estate *estate = pInput->estate();

	m_auction = new Auction();
	m_auction->setEstate(estate);
	m_auction->addToScope(this);

	pInput->setBoolProperty("can_buyestate", false);
	pInput->setBoolProperty("canauction", false);

	Display display;
	display.setText("%s sceglie di mettere all'asta %s.", pInput->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
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
		pInput->ioError("Input non valido per .ab, nessun separatore dopo auctionId");
		return 1;
	}
	int auctionId = atoi(strsep(&data, ":"));
	int bid = atoi(data);

	if ( !m_auction || m_auction->id() != auctionId )
	{
		pInput->ioError("auctionId inesistente: %d.", auctionId);
		return 1;
	}

	if (m_auction->status() == Auction::Sold) {
		pInput->ioError( "Non puoi più fare offerte all'asta %d.", auctionId );
		return 1;
	}

	if (bid > pInput->assets())
	{
		pInput->ioError("Non possiedi %d.", bid);
		return 1;
	}

	int highestBid = m_auction->getIntProperty("highbid");
	if (bid <= highestBid)
	{
		pInput->ioError("L'offerta minima è %d.", highestBid+1);
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
		pInput->ioError("playerId inesistente: %d.", playerId);
		return;
	}
	if (player == pInput) {
		pInput->ioError("Vuoi trattare con te stesso?");
		return;
	}
	if (player->getBoolProperty("bankrupt")) {
		pInput->ioError("Non puoi trattare con giocatori in bancarotta.");
		return;
	}
	if (player->getBoolProperty("spectator")) {
		pInput->ioError("Non puoi trattare con gli spettatori.");
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
		ioError("Input non valido per .Ta, nessun separatore dopo tradeId");
		return;
	}
	tradeId = atoi(strsep(&data, ":"));
	revision = atoi(data);

	Trade *trade = findTrade(tradeId);
	if (!trade)
	{
		pInput->ioError("tradeId inesistente: %d.", tradeId);
		return;
	}
	if ( !(trade->hasPlayer(pInput)) )
	{
		pInput->ioError("Non fai parte della trattativa %d.", tradeId);
		return;
	}
	if (revision != trade->getIntProperty("revision"))
	{
		pInput->ioError("Accettazione ignorata per la trattativa %d poiché è stata cambiata.", tradeId);
		return;
	}

	trade->setPlayerAccept(pInput, true);

	if (trade->allAccept()) {
		completeTrade(trade);
	}
}

void Game::rejectTrade(Player *pInput, unsigned int tradeId, const bool verbose)
{
	Trade *trade = findTrade(tradeId);
	if (!trade)
	{
		if (verbose) {
			pInput->ioError("tradeId inesistente: %d.", tradeId);
		}
		return;
	}

	if ( !(trade->hasPlayer(pInput)) )
	{
		if (verbose) {
			pInput->ioError("Non fai parte della trattativa %d.", tradeId);
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
				display2.setText("Gioco in pausa, %s deve %d a %s ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pFrom->getStringProperty("name").c_str(), money, pTo->getStringProperty("name").c_str(), (money - pFrom->getIntProperty("money")));
				sendDisplayMsg(&display2);

				newDebt(pFrom, pTo, 0, money);
			}
			else
				pTo->addMoney(money);

			display1.setText("%s prende %d da %s.", pTo->getStringProperty("name").c_str(), money, pFrom->getStringProperty("name").c_str());
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

	delTrade(trade);
}

void Game::abortAuction()
{
	if (!m_auction)
		return;

	printf("Game::abortAuction()\n");

	if (m_auction->status() != Auction::Sold) {
		ioWrite("<monopd><auctionupdate auctionid=\"%d\" status=\"%d\"/></monopd>\n", m_auction->id(), Auction::Sold);
	}

	delete m_auction;
	m_auction = NULL;
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
	display.setText("Comprata da %s all'asta per %d.", pBid->getStringProperty("name").c_str(), m_auction->getIntProperty("highbid"));
	sendDisplayMsg(&display);

	delete m_auction;
	m_auction = NULL;
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
	std::vector<Estate *>::iterator it = m_estates.begin();
	if (startEstate) {
		for (; it != m_estates.end(); it++) {
			if (startEstate == *it) {
				break;
			}
		}
	}
	for (; it != m_estates.end(); it++) {
		if ((*it)->group() == group) {
			return *it;
		}
	}
	// reloop
	if (startEstate) {
		for (it = m_estates.begin(); it != m_estates.end(); it++) {
			if ((*it)->group() == group) {
				return *it;
			}
		}
	}
	return NULL;
}

Estate *Game::findNextJailEstate(Estate *startEstate)
{
	std::vector<Estate *>::iterator it = m_estates.begin();
	if (startEstate) {
		for (; it != m_estates.end(); it++) {
			if (startEstate == *it) {
				break;
			}
		}
	}
	for (; it != m_estates.end(); it++) {
		if ((*it)->getBoolProperty("jail")) {
			return *it;
		}
	}
	// reloop
	if (startEstate) {
		for (it = m_estates.begin(); it != m_estates.end(); it++) {
			if ((*it)->getBoolProperty("jail")) {
				return *it;
			}
		}
	}
	return NULL;
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
		display.setText("%s non è il proprietario della proprietà %s.", player->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
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
		display.setText("%s non è il proprietario della carta %d.", player->getStringProperty("name").c_str(), card->id());
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

Player *Game::addPlayer(Player *p, const bool &isSpectator, const bool &isFirst)
{
	m_players.push_back(p);

	p->setGame(this);
	addToScope(p);

	// Properties with game as scope
	p->setProperty("money", 0, this);
	p->setBoolProperty("bankrupt", false, this);
	p->setBoolProperty("jailed", false, this);
	p->setBoolProperty("hasturn", 0, this);
	p->setBoolProperty("spectator", isSpectator, this);

	setProperty("players", players());
	if (players() == getBoolProperty("maxplayers")) {
		setBoolProperty("canbejoined", false);
	}

	if (isFirst) {
		m_master = p;
		setProperty("master", p->id());
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

	if (!isFirst) {
		if (m_status == Run) {
			sendStatus(Init, p);
			sendFullUpdate(p);
			sendStatus(Run, p);
		} else {
			sendFullUpdate(p);
		}
	}

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
	// note: m_pTurn is NULL when a game is finished
	if (p == m_pTurn) {
		// cancel any tax dialog
		m_pausedForDialog = false;
		// player left game during its turn, cancel any token movement
		setAllClientsMoving(0);
		// and update turn -before- removing player from players vector
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
	setProperty("players", players());

	// If in Config, canbejoined might become true again
	if (m_status == Config) {
		if (players() < getBoolProperty("maxplayers"))
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
		pInput->ioError("Solo il master può escludere i giocatori.");
		return 0;
	}

	if ( m_status != Config )
	{
		pInput->ioError("Puoi escludere i giocatori solo durante la configuraione del gioco.");
		return 0;
	}

	Player *player = findPlayer(playerId);
	if (!player)
	{
		pInput->ioError("playerId inesistente: %d.", playerId);
		return 0;
	}

	return player;
}

void Game::upgradePlayer(Player *pInput, int playerId)
{
	if ( pInput != m_master )
	{
		pInput->ioError("Solo il master può promuovere gli spettatori.");
		return;
	}

	if ( m_status != Run )
	{
		pInput->ioError("Puoi promuovere gli spettatori solo durante lo svolgimento del gioco.");
		return;
	}

	int maxPlayers = getIntProperty("maxplayers");
	if (players() >= maxPlayers)
	{
		pInput->ioError("Questo gioco ha già il numero massimo di giocatori pari a %d.", maxPlayers);
		return;
	}

	Player *player = findPlayer(playerId);
	if (!player)
	{
		pInput->ioError("playerId inesistente: %d.", playerId);
		return;
	}

	if ( !player->getBoolProperty("spectator") )
	{
		pInput->ioError( "%s non è uno spettatore in questo gioco.", player->name().c_str() );
		return;
	}

	// Reset player and remove spectator property.
	player->reset(false);

	int turnorder = 0;
	for (std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		int to = (*it)->getIntProperty("turnorder");
		if (to > turnorder) {
			turnorder = to;
		}
	}
	player->setProperty("turnorder", turnorder+1, this);

	// Find player and erase from std::vector
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if (*it == player) {
			m_players.erase(it);
			break;
		}
	}
	// Reinsert player to std::vector
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->getIntProperty("turnorder") == turnorder) {
			m_players.insert(it+1, player);
			break;
		}
	}

	player->setEstate(m_goEstate);
	player->addMoney(m_startMoney);
	player->setBoolProperty("spectator", 0, this);
	setProperty("players", players());

	ioInfo( "%s promuove %s a giocatore partecipante.", pInput->name().c_str(), player->name().c_str() );
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
	// Disable turn, roll and buy.
	m_pTurn->setTurn(false);

	std::vector<Player *>::iterator it;
	// find current player
	for(it = m_players.begin(); it != m_players.end(); it++) {
		if (*it == m_pTurn) {
			it++;
			break;
		}
	}

	// find next player
	while (1) {
		if (it == m_players.end()) {
			it = m_players.begin();
			setProperty("turn", getIntProperty("turn") +1);
		}
		Player *player = *it;
		if (!player->getBoolProperty("bankrupt") && !player->getBoolProperty("spectator")) {
			m_pTurn = player;
			break;
		}
		it++;
	}

	// Set turn.
	Display display;
	display.setText("Il turno passa a %s.", m_pTurn->getStringProperty("name").c_str());
	sendDisplayMsg(&display);

	m_pTurn->setTurn(true);
}

void Game::landPlayer(Player *pTurn, const bool directMove, const std::string &rentMath)
{
	Estate *destination = pTurn->destination();
	if (destination) {
		std::vector<Estate *>::iterator it;

		// find current player estate
		for (it = m_estates.begin(); it != m_estates.end(); it++) {
			if (*it == pTurn->estate()) {
				it++;
				break;
			}
		}

		unsigned int money = 0;
		while (1) {
			if (it == m_estates.end()) {
				it = m_estates.begin();
			}
			Estate *estate = *it;

			if (estate->getIntProperty("passmoney")) {
				money += estate->getIntProperty("passmoney");
				// Write incremental message for direct moves, token
				// confirmation or timeout didn't do it yet.
				if (directMove) {
					Display display;
					display.setText("%s passa da %s e prende %d.", pTurn->getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), estate->getIntProperty("passmoney"));
					sendDisplayMsg(&display);
				}
			}

			if (estate == destination) {
				break;
			}

			it++;
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
	Player *pOwner = 0;
	Estate *es = pTurn->estate();

	if (getBoolConfigOption("doublepassmoney") && es->getIntProperty("passmoney"))
	{
		Display display;
		display.setText("%s si ferma su %s e prende %d.", pTurn->getStringProperty("name").c_str(), es->getStringProperty("name").c_str(), es->getIntProperty("passmoney"));
		display.setEstate(es);
		sendDisplayMsg(&display);

		pTurn->addMoney(es->getIntProperty("passmoney"));
	}
	else {
		Display display;
		display.setText("%s si ferma su %s.", pTurn->getStringProperty("name").c_str(), es->getStringProperty("name").c_str());
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
			ioError("Questo tavolo da gioco non ha una prigione.");

		updateTurn();
		return;
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
			display.setText("%s prende %d dal piatto.", pTurn->getStringProperty("name").c_str(), estateMoney);
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
				display.setText("%s deve pagare %d ma è insolvente. Gioco in pausa, %s è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTurn->getStringProperty("name").c_str(), estateMoney, pTurn->getStringProperty("name").c_str(), (estateMoney - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebt(pTurn, 0, ePayTarget, estateMoney);
				return;
			}
			else if (ePayTarget && getBoolConfigOption("collectfines"))
				ePayTarget->addMoney(estateMoney);

			Display display;
			display.setText("%s paga %d.", pTurn->getStringProperty("name").c_str(), estateMoney);
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

				display1.setText("%s deve pagare %d oppure il %d percento dei suoi averi.", pTurn->getStringProperty("name").c_str(), tax, taxPercentage);
				sendDisplayMsg(&display1, pTurn);

				display2.setText("Paga %d oppure il %d percento dei tuoi averi.", tax, taxPercentage);
				display2.addButton(".T$", std::string("Paga ") + itoa(tax), 1);\
				display2.addButton(".T%", std::string("Paga ") + itoa(taxPercentage) + " Percento", 1);
				pTurn->sendDisplayMsg(&display2);

				// TODO: port this into a blocking bool in Display which we can check, will be more generic
				m_pausedForDialog = true;
				return;
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
			display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
			sendDisplayMsg(&display);

			newDebt(pTurn, 0, ePayTarget, payAmount);
			return;
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

		giveCard(pTurn, cardGroup->nextCard());
	}

	// Calculate rent for owned estates or offer them for sale.
	if ( es->canBeOwned() )
	{
		if ((pOwner = es->owner()))
		{
			// Owned.

			if (pOwner == pTurn) {
				Display display;
				display.setText("%s lo possiede già.", pTurn->getStringProperty("name").c_str());
				sendDisplayMsg(&display);
			} else if (es->getBoolProperty("mortgaged")) {
				Display display;
				display.setText("%s non frutta affitto perché è ipotecato.", pTurn->getStringProperty("name").c_str());
				sendDisplayMsg(&display);
			} else if (getBoolConfigOption("norentinjail") && pOwner->getBoolProperty("jailed")) {
				Display display;
				display.setText("%s non frutta affitto perché il proprietario %s è in prigione.", pTurn->getStringProperty("name").c_str(), pOwner->getStringProperty("name").c_str());
				sendDisplayMsg(&display);
			} else {
				// Pay payAmount owed.
				int payAmount = es->rent(pTurn, rentMath);

				if (!pTurn->payMoney(payAmount))
				{
					Display display;
					display.setText("Gioco in pausa, %s deve %d a %s ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTurn->getStringProperty("name").c_str(), payAmount, pOwner->getStringProperty("name").c_str(), (payAmount - pTurn->getIntProperty("money")));
					sendDisplayMsg(&display);

					newDebt(pTurn, pOwner, 0, payAmount);
					return;
				}
				Display display;
				display.setText("%s paga %d di affitto a %s.", pTurn->getStringProperty("name").c_str(), payAmount, pOwner->getStringProperty("name").c_str());
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
			display.setText("In vendita.");
			sendDisplayMsg(&display, pTurn);
			display.addButton(".eb", "Compra", 1);
			display.addButton(".ea", "Metti all'asta", (getBoolConfigOption("auctionsenabled") && totalAssets()));
			display.addButton(".E", "Passa", !getBoolConfigOption("auctionsenabled"));
			pTurn->sendDisplayMsg(&display);
			return;
		}
	}
}

void Game::giveCard(Player *pTurn, Card *card)
{
	if (!card)
	{
		ioError(pTurn->name() + " dovrebbe prendere una carta, ma sembra non ce ne siano disponibili!");
		return;
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
			ioError("Questo tavolo da gioco non ha una prigione.");

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
				display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebt(pTurn, 0, ePayTarget, payAmount);
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
				display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
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
						display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTmp->getStringProperty("name").c_str(), payAmount, (payAmount - pTmp->getIntProperty("money")));
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
				display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", pTurn->getStringProperty("name").c_str(), payAmount, (payAmount - pTurn->getIntProperty("money")));
				sendDisplayMsg(&display);

				newDebt(pTurn, 0, ePayTarget, payAmount);
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
				ioError("Impossibile trovare la prossima proprietà sul tavolo da gioco.");
		}
		landPlayer(pTurn, true, card->rentMath());
	}
}

void Game::declareBankrupt(Player *pInput)
{
	Display display;
	display.setText("%s dichiara bancarotta!", pInput->getStringProperty("name").c_str());
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

	// highest bidder declared bankruptcy (or left)
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
	enforceDebt(pBroke);

	// Count active (non-bankrupt) players
	int activePlayers = 0;
	Player *pTmp = 0;
	Player *pWinner = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (pTmp =  *it) ; ++it)
		if (pTmp && !pTmp->getBoolProperty("bankrupt") && !pTmp->getBoolProperty("spectator"))
		{
			activePlayers++;
			pWinner = pTmp;
		}

	if (activePlayers == 1) {
		// a user who disconnect might end the game while an auction was running
		abortAuction();
		// game might be ended if a player left while a player was moving
		setAllClientsMoving(0);

		m_status = End;
		setProperty("status", "end");
		syslog(LOG_INFO, "game %d ended: %s wins with a fortune of %d.", m_id, pWinner->name().c_str(), pWinner->assets());
		setBoolProperty("canbewatched", false);

		Display display;
		display.resetButtons();  /* Reset any button, game might end if a player left while we were asked to buy an estate for example */
		display.setText("Il gioco è concluso! %s vince con un capitale di %d!", pWinner->getStringProperty("name").c_str(), pWinner->assets());
		display.addButton(".gx", "Nuovo Gioco", true);
		sendDisplayMsg(&display);

		m_pTurn->setTurn(false);
		m_pTurn = NULL;
		return;
	}

	if (m_debts.size()) {
		return;
	}

	if (debtsWereSolved) {
		Display display;
		display.setText("Tutti i debiti sono saldati, il gioco prosegue.");
		sendDisplayMsg(&display);
	}

	if (pBroke == m_pTurn) {
		updateTurn();
		return;
	}

	// necessary when an auction bidder cannot pay and then declared bankruptcy
	m_pTurn->endTurn();
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

int Game::players(bool includeSpectators)
{
	if (includeSpectators == true) {
		return (int)m_players.size();
	}

	int count = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if (!(*it)->getBoolProperty("spectator")) {
			count++;
		}
	}
	return count;
}

int Game::debts()
{
	return m_debts.size();
}

unsigned int Game::clientsMoving()
{
	unsigned int moving = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->tokenLocation()) {
			moving++;
		}
	}
	return moving;
}

void Game::setAllClientsMoving(Estate *estate)
{
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->socket()) {
			(*it)->setTokenLocation(estate);
		}
	}
}

void Game::sendStatus(Status status, Player *p)
{
	const char *s = "unknown";

	switch(status) {
	case Config:
		s = "config";
		break;
	case Init:
		s = "init";
		break;
	case Run:
		s = "run";
		break;
	case End:
		s = "end";
		break;
	}

	if (p) {
		p->ioWrite("<monopd><gameupdate gameid=\"%d\" status=\"%s\"/></monopd>\n", m_id, s);
	} else {
		ioWrite("<monopd><gameupdate gameid=\"%d\" status=\"%s\"/></monopd>\n", m_id, s);
	}
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
