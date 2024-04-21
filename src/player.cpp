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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "socket.h"
#include "card.h"
#include "display.h"
#include "estate.h"
#include "io.h"
#include "game.h"
#include "player.h"
#include "trade.h" // move trade commands to game/engine

Player::Player(Socket *socket) : GameObject(-1, GameObject::GPlayer)
{
	m_socket = socket;
	m_game = 0;
	m_estate = 0;
	m_destination = 0;
	m_tokenLocation = 0;
	m_identified = false;
}

Player::~Player()
{
	while (!m_cards.empty()) { delete *m_cards.begin(); m_cards.erase(m_cards.begin()); }
	while (!m_display.empty()) { delete *m_display.begin(); m_display.erase(m_display.begin()); }
}

void Player::reset(bool removeProperties)
{
	while (!m_cards.empty()) { delete *m_cards.begin(); m_cards.erase(m_cards.begin()); }
	while (!m_display.empty()) { delete *m_display.begin(); m_display.erase(m_display.begin()); }

	m_estate = m_destination = m_tokenLocation = 0;

	if (removeProperties) {
		removeProperty("money");
		removeProperty("doublecount");
		removeProperty("jailcount");
		removeProperty("jailed");
		removeProperty("bankrupt");
		removeProperty("hasturn");
		removeProperty("spectator");
		removeProperty("can_roll");
		removeProperty("canrollagain");
		removeProperty("can_buyestate");
		removeProperty("canauction");
		removeProperty("location");
		removeProperty("directmove");
		removeProperty("turnorder");
	}
}

void Player::identify(int id) {
	m_id = id;
	setProperty("cookie", std::string(itoa(m_id)) + "/" + itoa(rand()), this);
	m_identified = true;
}

void Player::ioWrite(const char *fmt, ...)
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

void Player::ioWrite(std::string data)
{
	if (m_socket)
		m_socket->ioWrite(data);
}

void Player::ioInfo(const char *data, ...)
{
	va_list arg;
	char buf[2048];

	va_start(arg, data);
	vsnprintf(buf, sizeof(buf)-1, data, arg);
	va_end(arg);
	buf[sizeof(buf)-1] = 0;

	ioWrite("<monopd><msg type=\"info\" value=\"%s\"/></monopd>\n", buf);
}

void Player::ioInfo(const std::string data)
{
	ioWrite("<monopd><msg type=\"info\" value=\"" + data + "\"/></monopd>\n");
}

void Player::ioError(const char *data, ...)
{
	va_list arg;
	char buf[2048];

	va_start(arg, data);
	vsnprintf(buf, sizeof(buf)-1, data, arg);
	va_end(arg);
	buf[sizeof(buf)-1] = 0;

	ioWrite("<monopd><msg type=\"error\" value=\"%s\"/></monopd>\n", buf);
}

void Player::ioError(const std::string data)
{
	ioWrite("<monopd><msg type=\"error\" value=\"" + data + "\"/></monopd>\n");
}

void Player::ioNoSuchCmd(const std::string data)
{
	if (data.size())
		ioError("Il comando non è disponibile allo stato attuale del gioco: " + data);
	else
		ioError("Comando inesistente.");
}

void Player::sendDisplay(Display *display)
{
	std::string text, estate;
	if (display->text().size()) {
		text = "text=\"" + display->text() + "\" ";
	}

	if (display->clearEstate()) {
		estate = "estateid=\"-1\" ";
	} else if (display->estate()) {
		estate = "estateid=\"" + itoa(display->estate()->id()) + "\" ";
	}

	ioWrite("<monopd><display %s%scleartext=\"%d\" clearbuttons=\"%d\"", estate.c_str(), text.c_str(), display->clearText(), display->clearButtons());

	std::vector<DisplayButton *> buttons = display->buttons();
	if (buttons.size() > 0) {
		ioWrite(">");
		DisplayButton *button;
		for (std::vector<DisplayButton *>::iterator it = buttons.begin() ; it != buttons.end() && (button = *it) ; ++it)
			ioWrite("<button command=\"%s\" caption=\"%s\" enabled=\"%d\"/>", button->command().c_str(), button->caption().c_str(), button->enabled());
		ioWrite("</display></monopd>\n");
	}
	else
		ioWrite("/></monopd>\n");
}

void Player::sendDisplayMsg(Display *display)
{
	sendDisplay(display);
	if (display->clearText()) {
		while (!m_display.empty()) { delete *m_display.begin(); m_display.erase(m_display.begin()); }
	}
	m_display.push_back(new Display(display));
}

void Player::sendDisplayHistory()
{
	for(std::vector<Display *>::iterator it = m_display.begin(); it != m_display.end(); ++it) {
		sendDisplay(*it);
	}
}

void Player::sendClientMsg()
{
	ioWrite(std::string("<monopd><client playerid=\"" + itoa(m_id) + "\" cookie=\"" + getStringProperty("cookie").c_str() + "\"/></monopd>\n"));
}

void Player::sendCardList(Player *pOut)
{
	Card *card = 0;
	for(std::vector<Card *>::iterator it = m_cards.begin() ; it != m_cards.end() && (card = *it) ; ++it)
		pOut->ioWrite("<cardupdate cardid=\"%d\" title=\"%s\" owner=\"%d\"/>", card->id(), card->name().c_str(), m_id);
}

void Player::rollDice()
{
	if (getBoolProperty("jailed"))
	{
		ioError("Non puoi tirare i dadi mentre sei in prigione.");
		return;
	}
	if (getBoolProperty("can_buyestate"))
	{
		ioError("Devi comprare o mettere all'asta la proprietà corrente.");
		return;
	}

	m_game->rollDice();

	Display display;
	display.setText("%s lancia %d e %d.", getStringProperty("name").c_str(), m_game->dice[0], m_game->dice[1]);
	display.resetText();
	display.resetEstate();
	m_game->sendDisplayMsg(&display, this);
	display.resetButtons(); /* Remove Roll button */
	sendDisplayMsg(&display);

	// Take away many privileges
	setBoolProperty("can_roll", false);
	setBoolProperty("canrollagain", false);
	setBoolProperty("can_buyestate", false);
	setBoolProperty("canauction", false);

	if (m_game->dice[0] == m_game->dice[1])
	{
		int doubleCount = getIntProperty("doublecount");
		setProperty("doublecount", ++doubleCount);
		if(doubleCount < 3)
			setBoolProperty("canrollagain", true);
		else
		{
			Display display;
			display.setText("%s fa tre doppi e finisce in prigione.", getStringProperty("name").c_str());
			m_game->sendDisplayMsg(&display);

			if (Estate *eJail = m_game->findNextJailEstate(m_estate))
				toJail(eJail);
			else
				m_game->ioError("Questo tavolo da gioco non ha una prigione.");

			m_game->updateTurn();
			return;
		}
	}
	else
		setProperty("doublecount", 0);

	// Update player board position and nothing more. Other player
	// properties are not actually updated yet, this happens when all
	// clients have landed the player on the destination estate. Players do
	// get asynchronous updates, these are calculated per client.
	advance(m_game->dice[0] + m_game->dice[1], false);
}

void Player::endTurn(bool userRequest)
{
	if (!getBoolProperty("hasturn")) {
		if (userRequest)
			ioError("Non puoi passare il turno, non è il tuo turno.");
		return;
	}

	if (m_game->pausedForDialog()) {
		if (userRequest)
			ioError("Non puoi passare il turno, devi prima rispondere.");
		return;
	}

	if (m_game->auction()) {
		if (userRequest)
			ioError("Non puoi passare il turno, c'è un'asta in corso.");
		return;
	}

	if (m_game->clientsMoving()) {
		if (userRequest)
			ioError("Non puoi passare il turno, altri giocatori stanno ancora muovendo.");
		return;
	}

	if (m_game->debts()) {
		if (userRequest)
			ioError("Non puoi passare il turno, ci sono debiti da saldare.");
		return;
	}

	if (getBoolProperty("can_buyestate")) {
		if (!userRequest) {
			return;
		}

		if (m_game->getBoolConfigOption("auctionsenabled")) {
			ioError("Non puoi passare il turno, devi comprare o mettere all'asta la proprietà su cui ti trovi.");
			return;
		}

		setBoolProperty("can_buyestate", false);
		setBoolProperty("canauction", false);
		Display display;
		display.resetButtons(); /* Remove buy estate buttons: Buy, Auction, End Turn */
		sendDisplayMsg(&display);
	}

	if (getBoolProperty("canrollagain")) {
		Display display;
		display.setText("%s può tirare di nuovo.", getStringProperty("name").c_str());
		m_game->sendDisplayMsg(&display, this);
		display.addButton(".r", "Tira i dadi", true);
		sendDisplayMsg(&display);

		setBoolProperty("can_roll", true);
		setBoolProperty("canrollagain", false);
		return;
	}

	if (getBoolProperty("can_roll")) {
		if (userRequest)
			ioError("Non puoi passare il turno, devi prima tirare i dadi!");
		return;
	}

	if (getBoolProperty("jailed")) {
		if (userRequest)
			ioError("Non puoi passare il turno mentre sei in prigione.");
		return;
	}

	// Turn goes to next player
	m_game->updateTurn();
}

void Player::payJail()
{
	int payAmount = m_estate->getIntProperty("payamount");
	if (!payMoney(payAmount))
	{
		ioError("Uscire di prigione costa %d, tu possiedi solo %d.", payAmount, getIntProperty("money"));
		return;
	}
	else if (m_game->getBoolConfigOption("collectfines"))
	{
		Estate * const ePayTarget = m_estate->payTarget();
		if (ePayTarget)
			ePayTarget->addMoney(payAmount);
	}

	setBoolProperty("jailed", false);
	setBoolProperty("can_roll", true);
	setBoolProperty("canusecard", false);
	setProperty("jailcount", 0);

	Display display;
	display.setText("%s ha pagato %d ed è uscito di prigione, può ora tirare.", getStringProperty("name").c_str(), payAmount);
	m_game->sendDisplayMsg(&display, this);
	display.resetButtons(); /* Remove jail buttons: Pay, Use card, Roll */
	display.addButton(".r", "Tira i dadi", true);
	sendDisplayMsg(&display);
}

void Player::rollJail()
{
	m_game->rollDice();

	// Leave jail for free when doubles are rolled
	if (m_game->dice[0] == m_game->dice[1])
	{
		setBoolProperty("jailed", false);

		Display display;
		display.setText("Raddoppi, esci di prigione!");
		m_game->sendDisplayMsg(&display, this);
		display.resetButtons(); /* Remove jail buttons: Pay, Use card, Roll */
		sendDisplayMsg(&display);

		advance(m_game->dice[0] + m_game->dice[1], false);
		return;
	}

	// No doubles, stay in jail the first three turns
	int jailCount = getIntProperty("jailcount");
	if(jailCount < 3)
	{
		Display display;
		display.setText("Non raddoppi, resti in prigione.");
		m_game->sendDisplayMsg(&display, this);
		display.resetButtons(); /* Remove jail buttons: Pay, Use card, Roll */
		sendDisplayMsg(&display);

		m_game->updateTurn();
		setProperty("jailcount", jailCount+1);
		return;
	}

	// Third turn in jail, no doubles after three turns, must pay and leave.
	const int payAmount = m_estate->getIntProperty("payamount");
	setBoolProperty("jailed", false);
	setBoolProperty("canusecard", false);
	setProperty("jailcount", 0);

	Display display;
	display.setText("Nessun raddoppio, %s deve ora pagare %d e uscire di prigione.", getStringProperty("name").c_str(), payAmount);
	m_game->sendDisplayMsg(&display, this);
	display.resetButtons(); /* Remove jail buttons: Pay, Use card, Roll */
	sendDisplayMsg(&display);

	Estate * const ePayTarget = m_estate->payTarget();
	if (!payMoney(payAmount))
	{
		Display display;
		display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuperare %d in contanti.", getStringProperty("name").c_str(), payAmount, (payAmount - getIntProperty("money")));
		m_game->sendDisplayMsg(&display);

		setBoolProperty("canrollagain", true); // player can roll once debt is resolved
		m_game->newDebt(this, 0, ePayTarget, payAmount);
		return;
	}

	if (ePayTarget && m_game->getBoolConfigOption("collectfines"))
		ePayTarget->addMoney(payAmount);

	Display display2;
	display2.addButton(".r", "Tira i dadi", true);
	sendDisplayMsg(&display2);

	setBoolProperty("can_roll", true);
}

void Player::useJailCard()
{
	Card *card = findOutOfJailCard();
	if (!card)
	{
		ioError("Non possiedi alcuna carta 'esci di prigione'.");
		return;
	}
	m_game->transferCard(card, 0);

	setBoolProperty("can_roll", true);
	setBoolProperty("jailed", false);
	setBoolProperty("canusecard", false);
	setProperty("jailcount", 0);

	Display display;
	display.setText("%s ha usato la carta ed è uscito di prigione, può ora tirare.", getStringProperty("name").c_str());
	m_game->sendDisplayMsg(&display, this);
	display.resetButtons(); /* Remove jail buttons: Pay, Use card, Roll */
	display.addButton(".r", "Tira i dadi", true);
	sendDisplayMsg(&display);
}

void Player::buyEstate()
{
	if (!getBoolProperty("can_buyestate"))
	{
		ioError("Non puoi comprare nulla al momento.");
		return;
	}

	if (!payMoney(m_estate->price()))
	{
		ioError("Non possiedi abbastanza denaro per comprare questa proprietà, %s costa %d.", m_estate->name().c_str(), m_estate->price());
		return;
	}

	m_game->transferEstate(m_estate, this);

	Display display;
	display.setText("Comprata da %s per %d.", getStringProperty("name").c_str(), m_estate->price());
	m_game->sendDisplayMsg(&display, this);
	display.resetButtons(); /* Remove buy estate buttons: Buy, Auction, End Turn */
	sendDisplayMsg(&display);

	setBoolProperty("can_buyestate", false);
	setBoolProperty("canauction", false);
	endTurn();
}

void Player::sellEstate(int estateId)
{
	if (!m_game->getBoolConfigOption("allowestatesales"))
	{
		ioError("La vendita di proprietà è stata disabilitata nella configurazione.");
		return;
	}

	Estate *estate = m_game->findEstate(estateId);
	if (!estate)
	{
		ioError("estateId inesistente: %d.", estateId);
		return;
	}

	if (this != estate->owner())
	{
		ioError("Non possiedi '" + estate->name() + "'.");
		return;
	}

	if (estate->getBoolProperty("mortgaged") || estate->groupHasBuildings())
	{
		ioError("Non puoi vendere questa proprietà, ci sono case sul suo gruppo, o è ipotecata.");
		return;
	}

	// Sell the estate
	int sellPrice = estate->price() / 2;

	addMoney(sellPrice);
	m_game->transferEstate(estate, 0);

	Display display;
	display.setText("%s venduta %s per %d.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), sellPrice);
	m_game->sendDisplayMsg(&display);

	if (getBoolProperty("hasdebt"))
		m_game->solveDebts(this);
}

void Player::mortgageEstate(int estateId)
{
	Estate *estate = m_game->findEstate(estateId);
	if (!estate)
	{
		ioError("estateId inesistente: %d.", estateId);
		return;
	}

	if (this != estate->owner())
	{
		ioError("Non possiedi '" + estate->name() + "'.");
		return;
	}

	if (estate->getBoolProperty("mortgaged"))
	{
		if (!payMoney(estate->getIntProperty("unmortgageprice")))
		{
			ioError("'Riscattare %s' costa %d, tu possiedi solo %d.", estate->name().c_str(), estate->getIntProperty("unmortgageprice"), getIntProperty("money"));
			return;
		}
		estate->setBoolProperty("mortgaged", false);
		m_game->sendMsgEstateUpdate(estate);

		Display display;
		display.setText("%s riscattata %s per %d.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), estate->getIntProperty("unmortgageprice"));
		m_game->sendDisplayMsg(&display);
	}
	else
	{
		// Mortgage the property, if possible!

		if (estate->groupHasBuildings())
		{
			ioError("Non puoi ipotecare '" + estate->name() + "', ci sono ancora case nell stesso gruppo.");
			return;
		}

		estate->setBoolProperty("mortgaged", true);
		m_game->sendMsgEstateUpdate(estate);

		addMoney((estate->getIntProperty("mortgageprice")));

		Display display;
		display.setText("%s ipotecata %s per %d.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str(), estate->getIntProperty("mortgageprice"));
		m_game->sendDisplayMsg(&display);

		if (getBoolProperty("hasdebt"))
			m_game->solveDebts(this);
	}
}

void Player::payTax(const bool percentage)
{
	if (!getBoolProperty("hasturn"))
		return;

	m_game->setPausedForDialog(false);

	int payAmount = 0;
	if (percentage)
		payAmount = (int)( m_estate->getIntProperty("taxpercentage") * assets() / 100 );
	else
		payAmount = m_estate->getIntProperty("tax");

	Display display;
	display.setText("%s sceglie di pagare la tassa %s, che ammonta a %d.", getStringProperty("name").c_str(), (percentage ? "in base alle proprietà" : "fissa"), payAmount);
	m_game->sendDisplayMsg(&display, this);
	display.resetButtons(); /* Remove pay tax buttons: Pay 200, Pay 10 Percent */
	sendDisplayMsg(&display);

	Estate * const ePayTarget = m_estate->payTarget();
	if (!payMoney(payAmount))
	{
		Display display;
		display.setText("Gioco in pausa, %s deve %d ma è insolvente. Il giocatore deve prima recuparare %d in contanti.", getStringProperty("name").c_str(), payAmount, (payAmount - getIntProperty("money")));
		m_game->sendDisplayMsg(&display);

		m_game->newDebt(this, 0, ePayTarget, payAmount);
		return;
	}

	if (ePayTarget && m_game->getBoolConfigOption("collectfines"))
		ePayTarget->addMoney(payAmount);

	endTurn();
}

void Player::buyHouse(int estateId)
{
	Estate *estate = m_game->findEstate(estateId);
	if (!estate)
	{
		ioError("estateId inesistente: %d.", estateId);
		return;
	}

	if (this != estate->owner())
	{
		ioError("Non possiedi '" + estate->name() + "'.");
		return;
	}
	else if (!estate->housePrice())
	{
		ioError("Non puoi costruire case su '" + estate->name() + "'.");
		return;
	}
	else if (!estate->estateGroupIsMonopoly())
	{
		ioError("Non possiedi tutte le proprietà nel gruppo di '" + estate->name() + "'.");
		return;
	}
	else if (estate->groupHasMortgages())
	{
		ioError("Una o più proprietà nel gruppo di '" + estate->name() + "' sono ipotecate, quindi non puoi costruire case lì.");
		return;
	}
	else if (estate->getIntProperty("houses")==5)
	{
		ioError("Hai già un albergo su '" + estate->name() + "'.");
		return;
	}
	else if (estate->getIntProperty("houses") >= estate->maxHouses())
	{
		ioError("La regola di edificazione uniforme ti impedisce di costruire altre case su '" + estate->name() + "'.");
		return;
	}

	bool unlimitedHouses = m_game->getBoolConfigOption("unlimitedhouses");

	if (!unlimitedHouses)
	{
		if (estate->getIntProperty("houses") < 4)
		{
			if (!m_game->houses())
			{
				ioError("Spiacente, non ci sono più case disponibili!");
				return;
			}
		}
		else if (!m_game->hotels())
		{
			ioError("Spiacente, non ci sono più alberghi disponibili!");
			return;
		}
	}

	int cost = estate->housePrice();
	if (!payMoney(cost))
	{
		ioError("Una casa per %s costa %d, tu possiedi solo %d.", estate->name().c_str(), cost, getIntProperty("money"));
		return;
	}

	// Wow, we met all the right criteria? Let's build then!
	estate->setProperty("houses", estate->getIntProperty("houses") + 1);
	if (estate->getIntProperty("houses") == 5)
	{
		if (!unlimitedHouses)
		{
			m_game->setHotels(m_game->hotels() - 1);
			m_game->setHouses(m_game->houses() + 4);
		}

		Display display;
		display.setText("%s compra un albergo per %s.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
		m_game->sendDisplayMsg(&display);
	}
	else
	{
		if (!unlimitedHouses)
			m_game->setHouses(m_game->houses() - 1);

		Display display;
		display.setText("%s compra una casa per %s.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
		m_game->sendDisplayMsg(&display);
	}

	m_game->sendMsgEstateUpdate(estate);
}

void Player::sellHouse(int estateId)
{
	Estate *estate = m_game->findEstate(estateId);
	if (!estate)
	{
		ioError("estateId inesistente: %d.", estateId);
		return;
	}

	if (this != estate->owner())
	{
		ioError("Non possiedi '" + estate->name() + "'.");
		return;
	}
	else if (estate->getIntProperty("houses")==0)
	{
		ioError("Non ci sono costruzioni su '" + estate->name() + "' da vendere.");
		return;
	}
	else if (estate->getIntProperty("houses") <= estate->minHouses())
	{
		ioError("Devi livellare il numero di case nel tuo monopolio prima di poterne vendere altre su '" + estate->name() + "'.");
		return;
	}

	bool unlimitedHouses = m_game->getBoolConfigOption("unlimitedhouses");
	if ( !unlimitedHouses && (estate->getIntProperty("houses") == 5) && (m_game->houses() < 4) )
	{
		ioError("L'albergo non può essere venduto, la Banca ha a disposizione solo %d case.", m_game->houses());
		return;
	}

	// Wow, we met all the right criteria? Let's sell!
	addMoney(estate->getIntProperty("sellhouseprice"));
	estate->setProperty("houses", estate->getIntProperty("houses") - 1);
	if (estate->getIntProperty("houses") == 4)
	{
		if (!unlimitedHouses)
		{
			m_game->setHotels(m_game->hotels() + 1);
			m_game->setHouses(m_game->houses() - 4);
		}

		Display display;
		display.setText("%s vende un albergo da %s.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
		m_game->sendDisplayMsg(&display);
	}
	else
	{
		if (!unlimitedHouses)
			m_game->setHouses(m_game->houses() + 1);

		Display display;
		display.setText("%s vende una casa da %s.", getStringProperty("name").c_str(), estate->getStringProperty("name").c_str());
		m_game->sendDisplayMsg(&display);
	}

	m_game->sendMsgEstateUpdate(estate);
	m_game->solveDebts(this);
}

void Player::updateTradeObject(char *data)
{
	bool useEstate = false;
	bool useCard = false;
	if (data[0] == 'c')
		useCard=true;
	else if (data[0] == 'e')
		useEstate = true;
	else
		return;

	data++;

	// data looks like "1:10:1", tradeid, objectid, playerid
	if (!strstr(data, ":"))
	{
		ioError("Input non valido, nessun separatore dopo tradeId");
		return;
	}
	int tradeId = atoi(strsep(&data, ":"));

	if (!strstr(data, ":"))
	{
		ioError("Input non valido, nessun separatore dopo objectId");
		return;
	}
	int objectId = atoi(strsep(&data, ":"));
	int playerId = atoi(data);

	Trade *trade = m_game->findTrade(tradeId);
	if (!trade)
	{
		ioError("tradeId inesistente: %d.", tradeId);
		return;

	}
	if (!trade->hasPlayer(this))
	{
		ioError("Non fai parte della trattativa %d.", tradeId);
		return;
	}

	GameObject *object = 0;
	Player *pFrom = 0;
	if (useEstate)
	{
		Estate *estate = m_game->findEstate(objectId);
		if (!estate)
		{
			ioError("Nessuna proprietà con estateId %d.", objectId);
			return;
		}
		if (estate->groupHasBuildings())
		{
			ioError("Non puoi trattare " + estate->name() + ", ci sono case nel medesimo gruppo.");
			return;
		}
		object = estate;
		pFrom = estate->owner();
		if (!pFrom)
			return;
	}
	else if (useCard)
	{
		Card *card = m_game->findCard(objectId);
		if (!card || !card->owner())
		{
			ioError("Nessuno possiede una carta con cardId %d.", objectId);
			return;
		}
		object = card;
		pFrom = card->owner();
	}

	// NULL player allowed, Trade will delete the component.
	Player *playerTo = m_game->findPlayer(playerId);
	if (playerTo) {
		if (playerTo->getBoolProperty("bankrupt")) {
			ioError("Non puoi trattare con giocatori in bancarotta.");
			return;
		}
		if (playerTo->getBoolProperty("spectator")) {
			ioError("Non puoi trattare con gli spettatori.");
			return;
		}
	}

	trade->updateObject(object, pFrom, playerTo);
}

void Player::updateTradeMoney(char *data)
{
	// data looks like "1:1:1:100", tradeid, playerfrom, playerto, money
	if (!strstr(data, ":"))
	{
		ioError("Input non valido per .Tm, nessun separatore dopo tradeId");
		return;
	}
	int tradeId = atoi(strsep(&data, ":"));

	if (!strstr(data, ":"))
	{
		ioError("Input non valido per .Tm, nessun separatore dopo playerFromId");
		return;
	}
	int playerFromId = atoi(strsep(&data, ":"));

	if (!strstr(data, ":"))
	{
		ioError("Input non valido per .Tm, nessun separatore dopo playerToId");
		return;
	}

	int playerToId = atoi(strsep(&data, ":"));
	int money = atoi(data);

	Trade *trade = m_game->findTrade(tradeId);
	if (!trade)
	{
		ioError("tradeId inesistente: %d.", tradeId);
		return;
	}

	if (!trade->hasPlayer(this))
	{
		ioError("Non fai parte della trattativa %d.", tradeId);
		return;
	}

	Player *pFrom = m_game->findPlayer(playerFromId);
	if (!pFrom)
	{
		ioError("playerFromId inesistente: %d.", playerFromId);
		return;
	}
	if (pFrom->getBoolProperty("bankrupt")) {
		ioError("Non puoi trattare con giocatori in bancarotta.");
		return;
	}
	if (pFrom->getBoolProperty("spectator")) {
		ioError("Non puoi trattare con gli spettatori.");
		return;
	}
	Player *pTo = m_game->findPlayer(playerToId);
	if (!pTo)
	{
		ioError("playerToId inesistente: %d.", playerToId);
		return;
	}
	if (pTo->getBoolProperty("bankrupt")) {
		ioError("Non puoi trattare con giocatori in bancarotta.");
		return;
	}
	if (pTo->getBoolProperty("spectator")) {
		ioError("Non puoi trattare con gli spettatori.");
		return;
	}

	// TODO: should also count money player owes in other trade components
	if(pFrom->getIntProperty("money") < money)
	{
		ioError("%s non ha da offrire %d.", pFrom->name().c_str(), money);
		return;
	}

	GameObject *object = trade->findMoneyObject(pFrom, pTo);
	if (object)
		object->setId(money);
	else
		object = new GameObject(money, GameObject::Money);

	trade->updateObject(object, pFrom, pTo);
}

void Player::advance(int pos, bool direct)
{
	int estates = m_game->estates();
	int location = m_estate ? m_estate->id() : 0;

	location += pos;
	if (location >= estates)
		location %= estates;
	else if (location < 0)
		location += estates;

	advanceTo(location, direct, (pos >= 0));
}

void Player::advanceTo(int pos, bool direct, bool passEstates)
{
	if (passEstates)
		m_destination = m_game->findEstate(pos);
	else
		m_estate = m_game->findEstate(pos);

	// Send message to clients to move token. The rest is done by token confirmation or timeout.
	setProperty("location", m_destination ? m_destination->id() : m_estate->id(), m_game);
	setBoolProperty("directmove", direct, m_game);

	if (!direct)
		m_game->setAllClientsMoving(m_estate);
}

void Player::toJail(Estate *jailEstate)
{
	setProperty("doublecount", 0);
	setProperty("jailcount", 1);
	setBoolProperty("can_roll", false);
	setBoolProperty("jailed", true);

	advanceTo(jailEstate->id(), true, false);
}

void Player::addMoney(int amount)
{
	setProperty("money", getIntProperty("money") + amount);
}

bool Player::payMoney(int amount)
{
	int money = getIntProperty("money");
	if (money < amount)
		return false;

	setProperty("money", money - amount);
	return true;
}

void Player::setSocket(Socket *socket)
{
	m_socket = socket;
	if (m_socket)
		setProperty( "host", socket->ipAddr() );
	else
		setProperty( "host", "" );
}

void Player::setTurn(const bool &turn)
{
	setBoolProperty("hasturn", turn);
	setBoolProperty("can_buyestate", false);
	setBoolProperty("canauction", false);

	if (turn)
	{
		if (getBoolProperty("jailed"))
		{
			Display display1;
			display1.setText("%s è in prigione, turno %d.", name().c_str(), getIntProperty("jailcount"));
			display1.setEstate(m_estate);
			m_game->sendDisplayMsg(&display1, this);

			Display display2;
			display2.setText("Sei in prigione, turno %d.", getIntProperty("jailcount"));
			display2.setEstate(m_estate);
			display2.addButton(".jp", "Paga", true);
			display2.addButton(".jc", "Usa la carta", findOutOfJailCard());
			display2.addButton(".jr", "Tira i dadi", true);
			sendDisplayMsg(&display2);

			setBoolProperty("canusecard", findOutOfJailCard());
		}
		else {
			Display display;
			display.addButton(".r", "Tira i dadi", true);
			sendDisplayMsg(&display);

			setBoolProperty("can_roll", true);
		}
	}
	else
	{
		setBoolProperty("can_roll", false);
		setBoolProperty("canrollagain", false);
	}
}

void Player::setGame(Game *game)
{
	if (m_game != game)
	{
		m_game = game;
		setProperty("game", (m_game ? m_game->id() : -1));
	}
}

void Player::setEstate(Estate *estate)
{
	if (m_estate != estate)
		m_estate = estate;
}

Estate *Player::estate()
{
	return m_estate;
}

void Player::setDestination(Estate *estate)
{
	if (m_destination != estate)
		m_destination = estate;
}

Estate *Player::destination()
{
	return m_destination;
}

void Player::setTokenLocation(Estate *estate)
{
	m_tokenLocation = estate;
}

Estate *Player::tokenLocation()
{
	return m_tokenLocation;
}

void Player::addCard(Card *card)
{
	m_cards.push_back(card);
}

Card *Player::findFirstCard()
{
	if (m_cards.size() > 0)
		return m_cards[0];

	return 0;
}

void Player::takeCard(Card *card)
{
	for(std::vector<Card *>::iterator it = m_cards.begin() ; it != m_cards.end() && (*it) ; ++it)
		if (*it == card)
		{
			m_cards.erase(it);
			return;
		}
}

Card *Player::findCard(int cardId)
{
	Card *card = 0;
	for(std::vector<Card *>::iterator it = m_cards.begin() ; it != m_cards.end() && (card = *it) ; ++it)
		if (card->id() == cardId)
			return card;
	return 0;
}

Card *Player::findOutOfJailCard()
{
	Card *card = 0;
	for(std::vector<Card *>::iterator it = m_cards.begin() ; it != m_cards.end() && (card = *it) ; ++it)
		if (card->outOfJail())
			return card;
	return 0;
}

int Player::assets()
{
	if (m_game)
		return m_game->playerAssets(this);
	else
		return 0;
}
