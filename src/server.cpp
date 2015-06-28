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

#include <dirent.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <utf8.h>

#include <string>

#include "config.h"
#include "socket.h"
#include "auction.h"
#include "event.h"
#include "game.h"
#include "gameobject.h"
#include "gameconfig.h"
#include "io.h"
#include "player.h"
#include "server.h"

#if USE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif /* USE_SYSTEMD_DAEMON */

#define METASERVER_PERIOD 180000

int main(int argc, char **argv)
{
	MonopdServer server(argc, argv);
	return 0;
}

MonopdServer::MonopdServer(int argc, char **argv) : GameObject(0)
{
	m_nextGameId = m_nextPlayerId = 1;
	m_port = 1234;
	m_metaserverHost = "meta.atlanticd.net";
	m_metaserverPort = 1240;
	m_metaserverIdentity = "";
	m_useMetaserver = false;
	m_metaserverEvent = 0;
	m_metaserverAddrinfo = NULL;
	m_metaserverBusy = false;

	openlog("monopd", LOG_PID, LOG_DAEMON);
	syslog(LOG_NOTICE, "monopd %s started", VERSION);

	srand((unsigned int)time(0));
	loadConfig();
	loadGameTemplates();

	if (argc > 1) {
		m_port = atoi(argv[1]);
	}

	m_listener = new Listener(this, m_port);

	// Indicate to systemd that we are ready to answer requests
	updateSystemdStatus();

	if (m_useMetaserver) {
		registerMetaserver();

		// Register Metaserver event
		m_metaserverEvent = newEvent(Event::Metaserver);
		m_metaserverEvent->setLaunchTime(METASERVER_PERIOD);
		m_metaserverEvent->setFrequency(METASERVER_PERIOD);
	}

	while(1) {
		// Check for network events
		m_listener->checkActivity();
		// Check for scheduled events
		processEvents();
	}
}

MonopdServer::~MonopdServer()
{
	// We are responsible for the objects allocated
	while (!m_events.empty()) { delete *m_events.begin(); m_events.erase(m_events.begin()); }
	while (!m_games.empty()) { delete *m_games.begin(); m_games.erase(m_games.begin()); }
	while (!m_gameConfigs.empty()) { delete *m_gameConfigs.begin(); m_gameConfigs.erase(m_gameConfigs.begin()); }
	while (!m_players.empty()) { delete *m_players.begin(); m_players.erase(m_players.begin()); }
}

Event *MonopdServer::newEvent(unsigned int eventType, Game *game, int id)
{
	Event *newEvent = new Event(id, (Event::EventType)eventType, game);
	m_events.push_back(newEvent);
	printf("newEvent %d/%d\n", id, (int)m_events.size());
	return newEvent;
}

void MonopdServer::delEvent(Event *event)
{
	for(std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (*it) ; ++it)
		if (*it == event)
		{
			printf("delEvent %d/%d\n", event->id(), (int)m_events.size()-1);
			delete event;
			m_events.erase(it);
			break;
		}
}

Event *MonopdServer::findEvent(Game *game, unsigned int eventType)
{
	Event *event = 0;
	for(std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (event = *it) ; ++it)
		if (event->game() == game && event->type() == eventType)
			return event;

	return 0;
}

Event *MonopdServer::findEvent(Game *game, GameObject *object)
{
	for(std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (*it) ; ++it)
		if ( (*it)->game() == game && (*it)->object() == object)
			return (*it);

	return 0;
}

void MonopdServer::newGame(Player *player, const std::string gameType)
{
	Game *game = new Game(m_nextGameId++);
	m_games.push_back(game);

	game->setProperty("name", (gameType.size() ? gameType : "atlantic"), this);
	game->setProperty("gametype", gameType, this);
	game->setProperty("status", "config", this);
	game->setProperty("turn", 0, this);

	// Hardcoded reasonable defaults, which also ensure the entire server is the scope
	game->setProperty("master", -1, this); // addPlayer will set the correct playerid
	game->setProperty("description", "", this);
	game->setProperty("minplayers", 1, this);
	game->setProperty("maxplayers", 1, this);
	game->setProperty("players", 0, this);
	game->setBoolProperty("canbejoined", true, this);
	game->setBoolProperty("canbewatched", false, this);

	game->loadGameType(gameType);

	if (!game->isValid())
	{
		delGame(game, false);
		player->ioWrite("<monopd><msg type=\"error\" value=\"Could not load valid configuration for gametype %s.\"/></monopd>\n", gameType.c_str());
		m_nextGameId--;
		return;
	}

	syslog( LOG_INFO, "new game: id=[%d], type=[%s], games=[%d]", game->id(), gameType.c_str(), (int)m_games.size() );

	game->addPlayer(player, false, true);

	updateSystemdStatus();
}

void MonopdServer::joinGame(Player *pInput, unsigned int gameId, const bool &spectator)
{
	Game *game = findGame(gameId);
	if (!game)
	{
		pInput->ioError("There is no game with id %d.", gameId);
		return;
	}

	if ( spectator )
	{
		GameObject *config = game->findConfigOption( "allowspectators" );
		if ( config && config->getBoolProperty( "value" ) && game->status() == Game::Run )
		{
			game->addPlayer(pInput, true);
			game->ioInfo("%s joins as spectator.", pInput->name().c_str());
		}
		else
			pInput->ioError("Game %d doesn't allow spectators.", gameId);
		return;
	}

	int maxPlayers = game->getIntProperty("maxplayers");
	if (game->players() >= maxPlayers)
	{
		pInput->ioError("This game already has the maximum of %d players.", maxPlayers);
		return;
	}

	if (game->status() != Game::Config)
	{
		pInput->ioError("You cannot join game %d, it is already in progress.", game->id());
		return;
	}

	game->addPlayer(pInput);
}

void MonopdServer::exitGame(Game *game, Player *pInput)
{
	game->removePlayer(pInput);
	game->ioInfo("%s left the game.", pInput->name().c_str());
	if (game->players(true) == 0) {
		delGame(game);
	}

	pInput->reset();
}

Game *MonopdServer::findGame(int gameId)
{
	Game *game = 0;
	for(std::vector<Game *>::iterator it = m_games.begin(); it != m_games.end() && (game = *it) ; ++it)
		if (game->id() == gameId)
			return game;

	return 0;
}

void MonopdServer::delGame(Game *game, bool verbose)
{
	if (game->players(true)) {
		return;
	}

	if (verbose)
		ioWrite("<monopd><deletegame gameid=\"" + itoa(game->id()) + "\"/></monopd>\n");

	// Remove events from game
	for(std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (*it) ; )
	{
		if ( (*it)->game() == game )
		{
			delEvent( (*it) );
			it = m_events.begin();
			continue;
		}
		++it;
	}

	// Remove game
	for(std::vector<Game *>::iterator it = m_games.begin(); it != m_games.end() && (*it) ; ++it)
		if (*it == game)
		{
			syslog( LOG_INFO, "del game: id=[%d], games=[%d]", game->id(), (int)m_games.size() - 1 );
			m_games.erase(it);
			break;
		}
	delete game;

	updateSystemdStatus();
}

void MonopdServer::setGameDescription(Player *pInput, std::string data)
{
	// truncate description to 64 characters
	try {
		std::string::iterator it = data.begin();
		utf8::advance(it, 64, data.end());
		data.erase(it, data.end());
	}
	catch (const utf8::not_enough_room &e) {
	}

	Game *game = pInput->game();
	if (pInput == game->master())
	{
		game->setProperty("description", data);
	}
	else
		pInput->ioError("Only the master can set the game description!");
}

void MonopdServer::reconnectPlayer(Player *pInput, const std::string &cookie)
{
	Player *player = findCookie(cookie);
	if (!player)
	{
		pInput->ioError("Invalid cookie.");
		return;
	}

	Game *game = player->game();
	if (!game) {
		pInput->ioError("Invalid cookie.");
		return;
	}

	if (game->status() != Game::Run) {
		pInput->ioError("Game is not running.");
		return;
	}

	if (player->socket()) {
		pInput->ioError("Cannot reconnect, target player already has a socket.");
		return;
	}

	pInput->ioInfo("Reconnecting.");
	player->setSocket(pInput->socket());
	player->sendClientMsg();

	game->sendStatus(Game::Init, player);
	game->sendFullUpdate(player);
	game->sendStatus(Game::Run, player);
	player->sendDisplayHistory();

	game->ioInfo("%s reconnected.", player->name().c_str());
	pInput->setSocket(0);
	delPlayer(pInput);
}

void MonopdServer::delPlayer(Player *player)
{
	Game *game = player->game();
	if (game) {
		game->removePlayer(player);
		if (game->players(true) == 0) {
			delGame(game);
		}
	}

	if (player->id() > 0) {
		ioWrite("<monopd><deleteplayer playerid=\"" + itoa(player->id()) + "\"/></monopd>\n");
	}

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (*it) ; ++it)
		if (*it == player)
		{
			removeFromScope(player);
			syslog( LOG_INFO, "del player: id=[%d], fd=[%d], name=[%s], players=[%d]", player->id(), player->socket() ? player->socket()->fd() : 0, player->getStringProperty("name").c_str(), (int)m_players.size() - 1 );
			printf("delPlayer %d/%d\n", player->id(), (int)m_players.size()-1);
			delete player;
			m_players.erase(it);
			player = 0;
			break;
		}

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		printf("  player %16s %16s game %d bankrupt %d socket fd %d\n", player->name().c_str(), player->getStringProperty("host").c_str(), (player->game() ? player->game()->id() : -1), player->getBoolProperty("bankrupt"), player->socket() ? (int)player->socket()->fd() : -1);

	updateSystemdStatus();
}

Player *MonopdServer::findPlayer(int playerId)
{
	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		if (player->id() == playerId)
			return player;

	return 0;
}

Player *MonopdServer::findPlayer(Socket *socket)
{
	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		if (player->socket() == socket)
			return player;

	return 0;
}

Player *MonopdServer::findPlayer(const std::string &name)
{
	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		if (player->getStringProperty("name") == name)
			return player;

	return 0;
}

Player *MonopdServer::findCookie(const std::string &cookie)
{
	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		if (player->getStringProperty("cookie") == cookie)
			return player;

	return 0;
}

GameConfig *MonopdServer::newGameConfig(const std::string id, const std::string name, const std::string description)
{
	GameConfig *newGameConfig = new GameConfig(id, name, description);
	m_gameConfigs.push_back(newGameConfig);
	syslog( LOG_INFO, "loaded game configuration: game=[%s]", name.c_str() );
	return newGameConfig;
}

void MonopdServer::delGameConfig(GameConfig *gameConfig)
{
	for(std::vector<GameConfig *>::iterator it = m_gameConfigs.begin(); it != m_gameConfigs.end() && (*it) ; ++it)
		if (*it == gameConfig)
		{
			delete gameConfig;
			m_gameConfigs.erase(it);
			break;
		}
}

void MonopdServer::closedSocket(Socket *socket)
{
	Player *pInput = findPlayer(socket);

	Game *game = pInput->game();
	if (!game) {
		delPlayer(pInput);
		sendXMLUpdates();
		return;
	}

	printf("%s socket 0 spec %d, bankrupt %d, gamerun %d\n", pInput->name().c_str(), pInput->getBoolProperty("spectator"), pInput->getBoolProperty("bankrupt"), game->status() == Game::Run );

	// Only remove from game when game not running, or when it's merely a spectator.
	if (game->status() != Game::Run || pInput->getBoolProperty("spectator") || pInput->getBoolProperty("bankrupt")) {
		game->ioInfo("Connection with %s lost.", pInput->name().c_str());
		printf("exit from game %d: %d\n", game->id(), pInput->id());
		delPlayer(pInput);
		sendXMLUpdates();
		return;
	}

	printf("may reconnect\n");
	pInput->setSocket(NULL);
	unsigned int timeout = 180000;
	game->ioInfo("Connection with %s lost. Player has %ds to reconnect until bankruptcy.", pInput->name().c_str(), timeout/1000);
	Event *event = newEvent( Event::PlayerTimeout, game );
	event->setLaunchTime(timeout);
	event->setObject( dynamic_cast<GameObject *> (pInput) );
	sendXMLUpdates();
}

int MonopdServer::timeleftEvent()
{
	int timeleft = INT_MAX;
	struct timeval timenow;
	gettimeofday(&timenow, NULL);

	Event *event = NULL;
	for (std::vector<Event *>::iterator it = m_events.begin() ; it != m_events.end() && (event = *it) ; ++it) {
		struct timeval timeres;
		int timems;

		if (timercmp(&timenow, event->launchTime(), >)) {
			return 0;
		}

		timersub(event->launchTime(), &timenow, &timeres);
		// add 1ms to adjust for precision lost in tv_usec/10000
		timems = (timeres.tv_sec*1000 + timeres.tv_usec/1000) +1;
		if (timeleft > timems) {
			timeleft = timems;
		}
	}
	if (!event) {
		return -1;
	}

	return timeleft;
}

void MonopdServer::processEvents()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	Event *event = 0;
	for (std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (event = *it);) {
		if (timercmp(event->launchTime(), &tv, >)) {
			++it; // next event
			continue;
		}

		Game *game = event->game();
		Socket *delSocket;

		switch(event->type())
		{
		case Event::TokenMovementTimeout:
			event->setFrequency(0);
			if (game) {
				game->tokenMovementTimeout();
				if (game->clientsMoving()) {
					event->setFrequency(1000);
				}
			}
			break;
		case Event::SocketTimeout:
			delSocket = m_listener->findSocket(event->id());
			if (delSocket) {
				delSocket->setStatus(Socket::Close);
			}
			break;
		case Event::AuctionTimeout:
			if (game) {
				event->setFrequency(game->auctionTimeout());
			}
			break;
		case Event::Metaserver:
			registerMetaserver();
			break;
		case Event::PlayerTimeout:
			GameObject *object = event->object();
			if (!object)
				break;

			Player *player;
			player = findPlayer(object->id());
			if (player) {
				if (player->socket()) {
					break;
				}

				Game *game = player->game();
				if (game->status() == Game::Run) {
					game->ioInfo("%s did not reconnect in time and is now bankrupt.", player->name().c_str());
				}

				// delPlayer() might call delGame() if player is the last player in game, which remove
				// all events from game, therefore we don't know whether delPlayer() is going to
				// remove this event. Remove it before to prevent a double free.
				delEvent(event);

				delPlayer(player);

				// damn vectors
				it = m_events.begin();
				continue; // apply to for() loop
			}
			break;
		}

		if (event->frequency() == 0) {
			// Delete event from event list
			delEvent(event);
			// damn vectors
			it = m_events.begin();
			continue;
		}

		event->setLaunchTime(event->frequency());
		++it; // next event
	}

	sendXMLUpdates();
}

void MonopdServer::registerMetaserver()
{
	if (m_metaserverBusy)
		return;

	/* Don't ruin games, only try getaddrinfo() if no games are currently running */
	if (m_metaserverAddrinfo == NULL || m_games.size() == 0) {
		struct addrinfo hints;
		struct addrinfo *result;
		char port_str[6];
		int r;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		snprintf(port_str, sizeof(port_str), "%d", m_metaserverPort);
		/*
		 * getaddrinfo() is synchronous and might block, this is still less worse
		 * than a blocking connect(), DNS resolution are less likely to fail.
		 *
		 * Furthermore, we are only calling getaddrinfo() when no games are
		 * currently running.
		 */
		r = getaddrinfo(m_metaserverHost.c_str(), port_str, &hints, &result);
		if (r != 0) {
			syslog(LOG_INFO, "getaddrinfo() failed: error=[%s]", gai_strerror(r));
			/* use previous result */
		} else {
			if (m_metaserverAddrinfo)
				freeaddrinfo(m_metaserverAddrinfo);
			m_metaserverAddrinfo = result;
		}
	}

	Socket *socket = m_listener->connectSocket(m_metaserverAddrinfo);
	if (socket)
		m_metaserverBusy = true;
}

void MonopdServer::loadConfig()
{
	FILE *f = fopen(MONOPD_CONFIGDIR "/monopd.conf", "r");
	if (!f)
	{
		syslog( LOG_WARNING, "cannot open configuration: file=[%s]", MONOPD_CONFIGDIR "/monopd.conf" );
		return;
	}

	char str[1024], *buf;

	fgets(str, sizeof(str), f);
	while(!feof(f))
	{
		if (str[0]=='#') {}
		else if (strstr(str, "="))
		{
			buf = strtok(str, "=");
			if (!strcmp(buf, "port"))
				m_port = atoi(strtok(NULL, "\n\0"));
			else if (!strcmp(buf, "metaserverhost"))
				m_metaserverHost = strtok(NULL, "\n\0");
			else if (!strcmp(buf, "metaserverport"))
				m_metaserverPort = atoi(strtok(NULL, "\n\0"));
			else if (!strcmp(buf, "metaserveridentity")) {
				m_metaserverIdentity = strtok(NULL, "\n\0");
				m_useMetaserver = true;
			}
		}
		fgets(str, sizeof(str), f);
	}
	fclose(f);
}

void MonopdServer::loadGameTemplates()
{
	DIR *dirp;
	struct dirent *direntp;

	dirp = opendir(MONOPD_DATADIR "/games/");
	if (!dirp) {
		syslog( LOG_ERR, "cannot open game directory, dir=[%s]", MONOPD_DATADIR "/games/" );
#if USE_SYSTEMD_DAEMON
		sd_notifyf(1, "STATUS=Failed to start: cannot open game directory, dir=[%s]\nERRNO=%d", MONOPD_DATADIR "/games/", -1 );
		usleep(100000);
#endif /* USE_SYSTEMD_DAEMON */
		exit(-1);
	}
	while ((direntp=readdir(dirp)) != NULL) {
		char str[256], *buf;
		std::string name, description;

		if (!strstr(direntp->d_name, ".conf")) {
			continue;
		}

		std::string filename = std::string(MONOPD_DATADIR) + "/games/" + direntp->d_name;
		FILE *f = fopen(filename.c_str(), "r");
		if (!f) {
			syslog( LOG_WARNING, "cannot open game configuration: file=[%s/%s]", MONOPD_DATADIR "/games", filename.c_str() );
			continue;
		}

		for (fgets(str, sizeof(str), f); !feof(f); fgets(str, sizeof(str), f)) {
			if (str[0]=='#') {
				continue;
			}

			if (!utf8::is_valid(str, str+strlen(str))) {
				syslog( LOG_WARNING, "cannot open game configuration, file is not proper UTF-8: file=[%s/%s]", MONOPD_DATADIR "/games", filename.c_str() );
				goto abort;
			}

			if (strstr(str, "="))
			{
				buf = strtok(str, "=");
				if (!strcmp(buf, "name"))
					name = strtok(NULL, "\n\0");
				else if (!strcmp(buf, "description"))
					description = strtok(NULL, "\n\0");
			}
		}
		newGameConfig(strtok(direntp->d_name, "."), (name.size() ? name : strtok(direntp->d_name, ".")), (description.size() ? description : "No description available"));
abort:
		fclose(f);
	}
	closedir(dirp);
}

void MonopdServer::welcomeNew(Socket *socket)
{
	Player *player = new Player(socket);
	m_players.push_back(player);
	addToScope(player);

	player->ioWrite( std::string("<monopd><server host=\"") + m_metaserverIdentity + "\" version=\"" VERSION "\"/></monopd>\n" );
	sendGameTemplateList(player);
	sendXMLUpdate(player, true, true); // give new player a full update (excluding self) so it knows who's in the lounge
}

void MonopdServer::welcomeMetaserver(Socket *socket)
{
	socket->ioWrite( std::string("REGISTER ") + m_metaserverIdentity + " " + itoa(m_port) + " " + VERSION + "\n");

	/* we can't set socket to Close state here, Listener is going to change state from New to Ok */
	Event *socketTimeout = newEvent(Event::SocketTimeout, 0, socket->fd());
	/*
	 * sockets are not blocking, write() may be delayed and data buffered so wait a little
	 * bit before closing the session. Sure this is not perfect, we don't have a close when
	 * output buffer empty trigger yet, is it worth complicating this?
	 */
	socketTimeout->setLaunchTime(100);
}

void MonopdServer::closedMetaserver(Socket *socket)
{
	(void)socket;
	m_metaserverBusy = false;
}

void MonopdServer::ioWrite(const char *fmt, ...)
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

void MonopdServer::ioWrite(const std::string &data, const bool &noGameOnly)
{
	Player *player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		if ( !(noGameOnly && player->game()) )
			player->ioWrite(data);
}

void MonopdServer::sendGameTemplateList(Player *pInput)
{
	pInput->ioWrite("<monopd>");

	// Supported game types for new games
	GameConfig *gcTmp = 0;
	for(std::vector<GameConfig *>::iterator it = m_gameConfigs.begin() ; it != m_gameConfigs.end() && (gcTmp = *it) ; ++it )
		pInput->ioWrite("<gameupdate gameid=\"-1\" gametype=\"%s\" name=\"%s\" description=\"%s\"/>", gcTmp->id().c_str(), gcTmp->name().c_str(), gcTmp->description().c_str());

	pInput->ioWrite("</monopd>\n");
}

void MonopdServer::updateSystemdStatus() {
#if USE_SYSTEMD_DAEMON
	/* Update systemd status string */
	sd_notifyf(0, "READY=1\nSTATUS=Running, %d players, %d games", (int)m_players.size(), (int)m_games.size() );
#endif /* USE_SYSTEMD_DAEMON */
}

void MonopdServer::processInput(Socket *socket, const std::string data2)
{
	char *data = (char*)data2.c_str();
	Player *pInput = findPlayer(socket);
	Game *game = pInput->game();

	if (!utf8::is_valid(data2.begin(), data2.end())) {
		pInput->ioError("Input is not proper UTF-8");
		return;
	}

	// The following commands are _always_ available.
	if (data[0] == '.') {
		switch (data[1]) {

		case 'n':
			// The 'n' name command is available even for non-players.
			//  In fact, it's considered to be the protocol handshake.
			setPlayerName(pInput, data2.substr(2));
			goto sendupdates;
		case 'g':
			switch (data[2]) {

			case 'l':
				sendGameTemplateList(pInput);
				goto sendupdates;
			}
			break;
		case 'd':
			if (game) {
				exitGame(game, pInput);
			}
			socket->setStatus(Socket::Close);
			goto sendupdates;
		case 'R':
			if (game) {
				pInput->ioNoSuchCmd("you are already within a game");
				return;
			}
			reconnectPlayer(pInput, data2.substr(2));
			goto sendupdates;
		}
	}

	if (!pInput->identified()) {
		pInput->ioNoSuchCmd("you are not identified");
		// The rest of the commands are only available if player is identified.
		return;
	}

	if (data[0] != '.') {
		if (utf8::distance(data2.begin(), data2.end()) > 256) {
			pInput->ioError("Chat messages are limited to 256 characters");
			return;
		}

		if (game) {
			game->ioWrite("<monopd><msg type=\"chat\" playerid=\"%d\" author=\"%s\" value=\"%s\"/></monopd>\n", pInput->id(), pInput->name().c_str(), escapeXML(data2).c_str());
		} else {
			ioWrite("<monopd><msg type=\"chat\" playerid=\"" + itoa(pInput->id()) + "\" author=\"" + pInput->name() + "\" value=\"" + escapeXML(data2) + "\"/></monopd>\n", true);
		}
		return;
	}

	// The following commands are always available if player is identified.
	switch (data[1]) {

	case 'p':
		switch (data[2]) {

		case 'i':
			setPlayerImage(pInput, data2.substr(3));
			goto sendupdates;
		}
		break;
	}

	// Commands available when player is not within a game.
	if (!game) {
		switch (data[1]) {

		case 'g':
			switch (data[2]) {

			case 'n':
				newGame(pInput, data2.substr(3));
				goto sendupdates;
			case 'j':
				joinGame(pInput, atol(data2.substr(3).c_str()));
				goto sendupdates;
			case 'S':
				joinGame( pInput, atol(data2.substr(3).c_str()), true );
				goto sendupdates;
			}
			break;
		}

		pInput->ioNoSuchCmd("you are not within a game");
		// The rest of the commands are only available within a game.
		return;
	}

	switch (data[1]) {

	case 'f':
		game->sendFullUpdate(pInput);
		goto sendupdates;
	case 'g':
		switch (data[2]) {

		// These commands are always available in a running game, no matter what.
		case 'd':
			setGameDescription(pInput, data2.substr(3));
			goto sendupdates;
		case 'x':
			exitGame(game, pInput);
			goto sendupdates;
		// The following commands have their own availability checks.
		case 'c':
			game->editConfiguration( pInput, data+3 );
			goto sendupdates;
		case 'k':
			Player *pKick;
			pKick = game->kickPlayer( pInput, atoi(data+3) );
			if (pKick) {
				exitGame(game, pKick);
			}
			goto sendupdates;
		case 'u':
			game->upgradePlayer( pInput, atoi(data+3) );
			goto sendupdates;
		case 's':
			game->start(pInput);
			goto sendupdates;
		}
		break;
	}

	if (game->status() == Game::End) {
		pInput->ioNoSuchCmd("this game has ended");
		// The rest of the commands are only available when the game has not ended.
		return;
	}

	switch (data[1]) {

	case 't':
		game->setTokenLocation(pInput, atoi(data+2));
		if (!game->clientsMoving()) {
			Event *event = findEvent(game, Event::TokenMovementTimeout);
			if (event) {
				delEvent(event);
			}
		}
		goto sendupdates;
	}

	if (pInput->getBoolProperty("spectator") || pInput->getBoolProperty("bankrupt")) {
		pInput->ioNoSuchCmd("you are only a spectator");
		// The rest of the commands are only available for participating players
		return;
	}

	if (game->status() != Game::Run) {
		pInput->ioNoSuchCmd("game is not running");
		// The rest of the commands are only available if game is running
		return;
	}

	switch (data[1]) {

	case 'T':
		switch (data[2]) {

		case 'c':
		case 'e':
			pInput->updateTradeObject(data+2);
			goto sendupdates;
		case 'm':
			pInput->updateTradeMoney(data+3);
			goto sendupdates;
		case 'n':
			game->newTrade(pInput, atol(data2.substr(3).c_str()));
			goto sendupdates;
		case 'a':
			game->acceptTrade(pInput, data+3);
			goto sendupdates;
		case 'r':
			game->rejectTrade(pInput, atol(data2.substr(3).c_str()));
			goto sendupdates;
		}
		break;
	// From the official rules: "may buy and erect at any time"
	case 'h':
		switch (data[2]) {

		case 'b':
			pInput->buyHouse(atoi(data+3));
			goto sendupdates;
		case 's':
			pInput->sellHouse(atoi(data+3));
			goto sendupdates;
		}
		break;
	// From official rules: "Unimproved properties can be mortgaged
	// through the Bank at any time"
	// Selling estates is not officially supported, but it makes most
	// sense here.
	case 'e':
		switch (data[2]) {

		case 'm':
			pInput->mortgageEstate(atoi(data+3));
			goto sendupdates;
		case 's':
			pInput->sellEstate(atoi(data+3));
			goto sendupdates;
		}
		break;
	}

	if (game->clientsMoving()) {
		pInput->ioNoSuchCmd("other clients are still moving");
		// The rest of the commands are only available when no clients are moving
		return;
	}

	// If we're in a tax dialog, we don't accept too many commands.
	if (game->pausedForDialog()) {
		switch (data[1]) {

		case 'T':
			switch (data[2]) {

			case '$':
				pInput->payTax();
				goto sendupdates;
			case '%':
				pInput->payTax(true);
				goto sendupdates;
			}
			break;
		}

		// The rest of the commands are not available during a tax dialog
		pInput->ioNoSuchCmd("a tax dialog is in progress");
		return;
	}

	// Declaring bankruptcy is only possible when a player is in debt.
	if (game->debts()) {
		if (game->findDebt(pInput)) {
			switch (data[1]) {

			case 'D':
				game->declareBankrupt(pInput);
				goto sendupdates;
			case 'p':
				game->solveDebts(pInput, true);
				goto sendupdates;
			}
		}

		pInput->ioNoSuchCmd("there are debts to be settled");
		// The rest of the commands are only available when there
		// are no debts to be settled.
		return;
	}

	// Auctions restrict movement and stuff.
	if (game->auction()) {
		switch (data[1]) {

		case 'a':
			switch (data[2]) {

			case 'b':
				if (!game->bidInAuction(pInput, data+3)) {
					Event *event;
					event = findEvent(game, Event::AuctionTimeout);
					if (!event)
						event = newEvent(Event::AuctionTimeout, game);
					event->setLaunchTime(4000);
				}
				goto sendupdates;
			}
			break;
		}

		pInput->ioNoSuchCmd("an auction is in progress.");
		// The rest of the commands are only available when there
		// is no auction in progress
		return;
	}

	if (!pInput->getBoolProperty("hasturn")) {
		pInput->ioNoSuchCmd("this is not your turn");
		// The rest of the commands are only available when it's the player's turn
		return;
	}

	switch (data[1]) {

	case 'E':
		pInput->endTurn(true);
		goto sendupdates;
	}

	if (pInput->getBoolProperty("can_buyestate")) {
		Event *event;

		switch (data[1]) {

		case 'e':
			switch (data[2]) {

			case 'b':
				pInput->buyEstate();
				goto sendupdates;
			case 'a':
				game->newAuction(pInput);
				// A AuctionTimeout event may exist if a player disconnected
				// while an auction was in progress, destroy previous event if necessary
				event = findEvent(game, Event::AuctionTimeout);
				if (event) {
					delEvent(event);
				}
				goto sendupdates;
			}
			break;
		}
	}

	if (pInput->getBoolProperty("jailed")) {

		switch (data[1]) {

		case 'j':
			switch (data[2]) {

			case 'c':
				pInput->useJailCard();
				goto sendupdates;
			case 'p':
				pInput->payJail();
				goto sendupdates;
			case 'r':
				pInput->rollJail();
				goto sendupdates;
			}
			break;
		}
	}

	if (pInput->getBoolProperty("can_roll")) {

		switch (data[1]) {

		case 'r':
			pInput->rollDice();
			// A TokenMovementTimeout event may exist if a player disconnected
			// while moving, destroy previous event if necessary
			Event *event = findEvent(game, Event::TokenMovementTimeout);
			if (event) {
				delEvent(event);
			}
			event = newEvent(Event::TokenMovementTimeout, game);
			event->setLaunchTime(10000);
			goto sendupdates;
		}
	}

	pInput->ioNoSuchCmd();
	return;

sendupdates:
	sendXMLUpdates();
}

void MonopdServer::sendXMLUpdates()
{
	// Send XML to all players
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (*it) ; ++it)
		sendXMLUpdate((*it));

	// Reset propertiesChanged for all player and game objects
	// TODO: cache whether they changed above and reuse here?
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (*it) ; ++it)
		(*it)->unsetPropertiesChanged();
	for(std::vector<Game *>::iterator it = m_games.begin(); it != m_games.end() && (*it) ; ++it)
	{
		(*it)->unsetPropertiesChanged();

		// Let games handle resets for its own objects ..
		(*it)->unsetChildProperties();
	}
}

void MonopdServer::sendXMLUpdate(Player *pOutput, bool fullUpdate, bool excludeSelf)
{
	bool updateEmpty = true;

	// Send updates *about* all games ..
	Game *gUpdate = 0;
	for(std::vector<Game *>::iterator uit = m_games.begin(); uit != m_games.end() && (gUpdate = *uit) ; ++uit)
	{
		// .. but only when changed (and in property scope)
		std::string updateXML = gUpdate->oldXMLUpdate(pOutput, fullUpdate);
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

	// Let game handle updates *about* all of its objects ..
	if (Game *game = pOutput->game())
		updateEmpty = game->sendChildXMLUpdate(pOutput, updateEmpty);

	// Send updates *about* all players ..
	Player *pUpdate = 0;
	for(std::vector<Player *>::iterator uit = m_players.begin(); uit != m_players.end() && (pUpdate = *uit) ; ++uit)
	{
		// .. but only when changed (and in property scope)
		if (!excludeSelf || pUpdate != pOutput)
		{
			std::string updateXML = pUpdate->oldXMLUpdate(pOutput, fullUpdate);
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
	}

	if (!updateEmpty)
		pOutput->ioWrite("</monopd>\n");
}

void MonopdServer::setPlayerName(Player *player, std::string name) {

	// truncate player name to 16 characters
	if (name.size()) {
		try {
			std::string::iterator it = name.begin();
			utf8::advance(it, 16, name.end());
			name.erase(it, name.end());
		}
		catch (const utf8::not_enough_room &e) {
		}
	} else {
		name = "anonymous";
	}

	std::string useName = name;
	int i=1;
	while (findPlayer(useName)) {
		useName = name + itoa(++i);
	}

	player->setProperty("name", useName, this);
	if (player->identified()) {
		return;
	}

	player->setProperty("game", -1, this);
	player->setProperty("host", player->socket()->ipAddr(), this);
	player->identify(m_nextPlayerId++);
	player->sendClientMsg();

	syslog(LOG_INFO, "new player: id=[%d], fd=[%d], name=[%s], players=[%d]", player->id(), player->socket()->fd(), name.c_str(), (int)m_players.size());
	player = 0;
	for (std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it) {
		printf("  player %16s %16s game %d bankrupt %d socket fd %d\n", player->name().c_str(), player->getStringProperty("host").c_str(), (player->game() ? player->game()->id() : -1), player->getBoolProperty("bankrupt"), player->socket() ? (int)player->socket()->fd() : -1);
	}

	updateSystemdStatus();
}

void MonopdServer::setPlayerImage(Player *player, std::string image) {
	// truncate image name to 32 characters
	try {
		std::string::iterator it = image.begin();
		utf8::advance(it, 32, image.end());
		image.erase(it, image.end());
	}
	catch (const utf8::not_enough_room &e) {
	}

	player->setProperty("image", image, this);
}
