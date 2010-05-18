// Copyright (c) 2001-2004 Rob Kaper <cap@capsi.com>,
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

MonopdServer::MonopdServer() : GameObject(0)
{
	m_nextGameId = m_nextPlayerId = 1;
	m_port = 1234;
	m_metaserverHost = "meta.atlanticd.net";
	m_metaserverPort = 80;
	m_metaserverFrequency = 60;
	m_metaserverIdentity = "";
	m_useMetaserver = false;
	m_metaserverEvent = 0;
	m_metaserverSocket = -1;
	m_metaserverRefreshFrequency = 60;
	m_metaserverRefreshCount = 0;

	loadConfig();
	loadGameTemplates();
}

void MonopdServer::setPort(int port)
{
	m_port = port;
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
	printf("newEvent %d/%d\n", id, m_events.size());
	return newEvent;
}

void MonopdServer::delEvent(Event *event)
{
	for(std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (*it) ; ++it)
		if (*it == event)
		{
			printf("delEvent %d/%d\n", event->id(), m_events.size()-1);
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

	// Hardcoded reasonable defaults, which also ensure the entire server is the scope
	game->setProperty("master", -1, this); // addPlayer will set the correct playerid
	game->setProperty("description", "", this);
	game->setProperty("minplayers", 1, this);
	game->setProperty("maxplayers", 1, this);
	game->setProperty("players", 0, this);
	game->setBoolProperty("canbejoined", true, this);

	game->loadGameType(gameType);

	if (!game->isValid())
	{
		delGame(game, false);
		player->ioWrite("<monopd><msg type=\"error\" value=\"Could not load valid configuration for gametype %s.\"/></monopd>\n", gameType.c_str());
		m_nextGameId--;
		return;
	}

	syslog( LOG_INFO, "new game: id=[%d], type=[%s], games=[%d]", game->id(), gameType.c_str(), m_games.size() );

	game->addPlayer(player, true);

	// FIXME: DEPRECATED 1.0
	ioWrite(std::string("<monopd><updategamelist type=\"add\"><game id=\"") + itoa(game->id()) + "\" players=\"1\" gametype=\"" + game->gameType() + "\" name=\"" + game->name() + "\" description=\"" + game->getStringProperty("description") + "\" canbejoined=\"" + itoa(game->getBoolProperty("canbejoined")) + "\"/></updategamelist></monopd>\n");
}

void MonopdServer::joinGame(Player *pInput, unsigned int gameId, const bool &spectator)
{
	Game *game = findGame(gameId);
	if (!game)
	{
		pInput->ioError("There is no game with id %ld.", gameId);
		return;
	}

	if ( spectator )
	{
		GameObject *config = game->findConfigOption( "allowspectators" );
		if ( config && config->getBoolProperty( "value" ) && game->status() == Game::Run )
		{
			game->addPlayer( pInput, false, true );
			game->ioInfo("%s joins as spectator.", pInput->name().c_str());
		}
		else
			pInput->ioError("Game %ld doesn't allow spectators.", gameId);
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

	// FIXME: DEPRECATED 1.0
	ioWrite(std::string("<monopd><updategamelist type=\"edit\"><game id=\"") + itoa(game->id()) + "\" players=\"" + itoa(game->players()) + "\" canbejoined=\"" + itoa(game->getBoolProperty("canbejoined")) + "\"/></updategamelist></monopd>\n");
}

void MonopdServer::exitGame(Game *game, Player *pInput)
{
	game->removePlayer(pInput);
	if (game->connectedPlayers() == 0)
		delGame(game);

	pInput->reset();

	// Send new game list
	sendGameList(pInput);
}

Game *MonopdServer::findGame(unsigned int gameId)
{
	Game *game = 0;
	for(std::vector<Game *>::iterator it = m_games.begin(); it != m_games.end() && (game = *it) ; ++it)
		if (game->id() == gameId)
			return game;

	return 0;
}

void MonopdServer::delGame(Game *game, bool verbose)
{
	if (verbose)
		ioWrite("<monopd><deletegame gameid=\"" + itoa(game->id()) + "\"/></monopd>\n");

	// Remove everyone from the game
	while (game->players())
	{
		Player *player = 0;
		for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
			if (player->game() == game)
			{
				game->removePlayer(player);
				delPlayer(player);
				break;
			}
	}

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
			// FIXME: DEPRECATED 1.0
			if (verbose)
				ioWrite(std::string("<monopd><updategamelist type=\"del\"><game id=\"") + itoa(game->id()) + "\"/></updategamelist></monopd>\n");
			syslog( LOG_INFO, "del game: id=[%d], games=[%d]", game->id(), m_games.size() - 1 );
			m_games.erase(it);
			break;
		}
	delete game;
}

void MonopdServer::setGameDescription(Player *pInput, const std::string data)
{
	Game *game = pInput->game();
	if (pInput == game->master())
	{
		game->setProperty("description", data);

		// FIXME: DEPRECATED 1.0
		ioWrite("<monopd><updategamelist type=\"edit\"><game id=\"" + itoa(game->id()) + "\" gametype=\"" + game->gameType() + "\" name=\"" + game->name() + "\" description=\"" + game->getStringProperty("description") + "\"/></updategamelist></monopd>\n");
	}
	else
		pInput->ioError("Only the master can set the game description!");
}

Player *MonopdServer::newPlayer(Socket *socket, const std::string &name)
{
	// Players completed the handshake, delete socket timeout event
	delSocketTimeoutEvent( socket->fd() );

	Player *player = new Player(socket, m_nextPlayerId++);
	m_players.push_back(player);
	addToScope(player);

	player->setProperty("game", -1, this);
	player->setProperty("host", socket->fqdn(), this);
	setPlayerName(player, name);

	player->sendClientMsg();
	sendGameList(player, true);

	syslog( LOG_INFO, "new player: id=[%d], fd=[%d], name=[%s], players=[%d]", player->id(), socket->fd(), name.c_str(), m_players.size() );
	player = 0;
	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		printf("  player %16s %16s game %d bankrupt %d socket %d fd %d\n", player->name().c_str(), player->getStringProperty("host").c_str(), (player->game() ? player->game()->id() : -1), player->getBoolProperty("bankrupt"), player->socket(), player->socket() ? (int)player->socket()->fd() : -1);

	// Re-register to meta server with updated player count.
	registerMetaserver();
	if (m_metaserverEvent)
		m_metaserverEvent->setLaunchTime(time(0) + m_metaserverEvent->frequency() );

	return player;
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
	if (game)
	{
		if (player->socket())
			pInput->ioError("Cannot reconnect, target player already has a socket.");
		else
		{
			pInput->ioInfo("Reconnecting.");
			player->setSocket(pInput->socket());
			player->sendClientMsg();
			sendGameList(player, true);
			if (game->status() == Game::Run)
			{
				game->setStatus(Game::Init);
				game->sendFullUpdate(player);
				game->setStatus(Game::Run);
				game->sendStatus(player);
			}
			else
				game->sendFullUpdate(player);

			game->ioInfo("%s reconnected.", player->name().c_str());
			pInput->setSocket(0);
			delPlayer(pInput);
		}
	}
}

void MonopdServer::delPlayer(Player *player)
{
	// Delete timeout event, if present.
	Game *game = player ? player->game() : 0;
	if ( player && game )
	{
		Event *event = findEvent( game, dynamic_cast<GameObject *> (player) );
		if ( event )
		{
			printf("cleared event for player\n");
			event->setObject( 0 );
		}
		game->removePlayer( player );
	}

	ioWrite("<monopd><deleteplayer playerid=\"" + itoa(player->id()) + "\"/></monopd>\n");

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (*it) ; ++it)
		if (*it == player)
		{
			removeFromScope(player);
			syslog( LOG_INFO, "del player: id=[%d], fd=[%d], name=[%s], players=[%d]", player->id(), player->socket() ? player->socket()->fd() : 0, player->getStringProperty("name").c_str(), m_players.size() - 1 );
			printf("delPlayer %d/%d\n", player->id(), m_players.size()-1);
			delete player;
			m_players.erase(it);
			player = 0;
			break;
		}

	for(std::vector<Player *>::iterator it = m_players.begin(); it != m_players.end() && (player = *it) ; ++it)
		printf("  player %16s %16s game %d bankrupt %d socket %d fd %d\n", player->name().c_str(), player->getStringProperty("host").c_str(), (player->game() ? player->game()->id() : -1), player->getBoolProperty("bankrupt"), player->socket(), player->socket() ? (int)player->socket()->fd() : -1);

	// Re-register to meta server with updated player count.
	registerMetaserver();
	if (m_metaserverEvent)
		m_metaserverEvent->setLaunchTime(time(0) + m_metaserverEvent->frequency() );
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
	if (!pInput)
	{
		// Delete socket timeout event, socket is closed already
		delSocketTimeoutEvent( socket->fd() );
		return;
	}

	Game *game = pInput->game();
	if (!game)
	{
		delPlayer(pInput);
		return;
	}

	pInput->setSocket( 0 );
	printf("%s socket 0 spec %d, bank %d, gamerun %d\n", pInput->name().c_str(), pInput->getBoolProperty("spectator"), pInput->getBoolProperty("bankrupt"), game->status() == Game::Run );
	game->ioInfo("Connection with %s lost.", pInput->name().c_str());

	// Only remove from game when game not running, or when it's merely a spectator.
	bool exitFromGame = false;
	if (game->status() == Game::Run)
	{
		if ( pInput->getBoolProperty("spectator") )
			exitFromGame = true;
		else if ( !pInput->getBoolProperty("bankrupt") )
		{
			printf("may reconnect\n");
			unsigned int timeout = 180;
			game->ioInfo("Player has %ds to reconnect until bankruptcy.", timeout);
			Event *event = newEvent( Event::PlayerTimeout, game );
			event->setLaunchTime(time(0) + timeout);
			event->setObject( dynamic_cast<GameObject *> (pInput) );
		}
	}
	else
		exitFromGame = true;

	if (exitFromGame)
	{
		printf("exit from game %d: %d\n", game->id(), pInput->id());
		exitGame(game, pInput);
		printf("delplayer %d\n", pInput->id());
		delPlayer(pInput);
	}
}

int MonopdServer::processEvents()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	int returnvalue = -1;
	Event *event = 0;
	for (std::vector<Event *>::iterator it = m_events.begin() ; it != m_events.end() && (event = *it) ; ++it)
	{
		if (tv.tv_sec >= event->launchTime())
		{
			Game *game = event->game();
			switch(event->type())
			{
			case Event::TokenMovementTimeout:
				if (game)
				{
					game->tokenMovementTimeout();
					if ( game->clientsMoving() )
						event->setFrequency( 1 );
					else
						event->setFrequency( 0 );
				}
				else
					event->setFrequency( 0 );
				break;
			case Event::SocketTimeout:
				returnvalue = event->id();
				break;
			case Event::AuctionTimeout:
				if (game)
				{
					unsigned int frequency = game->auctionTimeout();
					event->setFrequency(frequency);
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
				if (player)
				{
					if (player->socket())
					break;

					Game *game = player->game();
					if (game->status() == Game::Run)
					{
						game->ioInfo("%s did not reconnect in time and is now bankrupt.", player->name().c_str());
						game->bankruptPlayer(player);
					}
					else
					{
						// Game might have ended, silently remove.
						exitGame(game, player);
						// Event might have been deleted now.
						event = 0;

						delPlayer(player);
					}
				}
				if ( event )
					event->setObject(0);
				break;
			}

			if ( event )
			{
				// Delete event from event list
				int frequency = event->frequency();
				if (frequency)
					event->setLaunchTime(time(0) + frequency);
				else
				{
					delEvent(event);
					break; // can't use continue, damn vectors
				}
			}
			else
				break;
		}
	}
	sendXMLUpdates();
	return returnvalue;
}

void MonopdServer::registerMetaserver()
{
	// refresh metaserver IP
	if( m_metaserverSocket>=0 && m_metaserverRefreshCount >= m_metaserverRefreshFrequency ) {

		struct sockaddr_in sin;
		struct hostent *hp = gethostbyname(m_metaserverHost.c_str());

		memcpy((char *) &sin.sin_addr, hp->h_addr, hp->h_length);

		if(m_metaserverSockaddr.sin_addr.s_addr != sin.sin_addr.s_addr) {

			close(m_metaserverSocket);
			m_metaserverSocket = -1;
		}

		m_metaserverRefreshCount = 0;
	}

	if( m_metaserverSocket<0 ) {

		int ircsock;
		struct sockaddr_in sin;
		struct hostent *hp = gethostbyname(m_metaserverHost.c_str());
		if (!hp)
			return;

		memset((char *) &sin, 0, sizeof(sin));
		memcpy((char *) &sin.sin_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = htons(m_metaserverPort);

		ircsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (ircsock<0)
			return;

		m_metaserverSocket = ircsock;
		memcpy(&m_metaserverSockaddr, &sin, sizeof(sin) );

		m_metaserverRefreshCount = 0;
	}

	if( m_metaserverSocket>=0 ) {

		std::string getStr = "<monopd><server host=\"" + m_metaserverIdentity + "\" port=\"" + itoa(m_port) + "\" version=\"" + VERSION + "\"/></monopd>";
		sendto(m_metaserverSocket, getStr.c_str(), strlen(getStr.c_str()), 0, (sockaddr*)&m_metaserverSockaddr, sizeof(m_metaserverSockaddr) );
		m_metaserverRefreshCount++;
	}
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
			if (!strcmp(buf, "metaserverhost"))
				m_metaserverHost = strtok(NULL, "\n\0");
			else if (!strcmp(buf, "metaserveridentity"))
			{
				m_metaserverIdentity = strtok(NULL, "\n\0");
				m_useMetaserver = true;
			}
			else if (!strcmp(buf, "port"))
				setPort(atoi(strtok(NULL, "\n\0")));
			else if (!strcmp(buf, "metaserverport"))
				m_metaserverPort = atoi(strtok(NULL, "\n\0"));
		}
		fgets(str, sizeof(str), f);
	}
	fclose(f);
}

void MonopdServer::loadGameTemplates()
{
	DIR *dirp;
	FILE *f;
	char str[256], *buf;
	struct dirent *direntp;
	std::string name = "", description = "";

	dirp = opendir(MONOPD_DATADIR "/games/");
	if (!dirp)
	{
		syslog( LOG_ERR, "cannot open game directory, dir=[%s]", MONOPD_DATADIR "/games/" );
		return;
	}
	while ((direntp=readdir(dirp)) != NULL)
	{
		if (strstr(direntp->d_name, ".conf"))
		{
			std::string filename = std::string(MONOPD_DATADIR) + "/games/" + direntp->d_name;
			f = fopen(filename.c_str(), "r");
			if (!f)
			{
				syslog( LOG_WARNING, "cannot open game configuration: file=[%s/%s]", MONOPD_DATADIR "/games", filename.c_str() );
				continue;
			}

			fgets(str, sizeof(str), f);
			while (!feof(f))
			{
				if (str[0]=='#') {}
				else if (strstr(str, "="))
				{
					buf = strtok(str, "=");
					if (!strcmp(buf, "name"))
						name = strtok(NULL, "\n\0");
					else if (!strcmp(buf, "description"))
						description = strtok(NULL, "\n\0");
				}
				fgets(str, sizeof(str), f);
			}
			fclose(f);

			newGameConfig(strtok(direntp->d_name, "."), (name.size() ? name : strtok(direntp->d_name, ".")), (description.size() ? description : "No description available"));
			name = "";
			description = "";
		}
	}
	closedir(dirp);
}

void MonopdServer::initMetaserverEvent()
{
	if (m_useMetaserver==true)
	{
		// Register Metaserver event
		m_metaserverEvent = newEvent(Event::Metaserver);
		m_metaserverEvent->setLaunchTime(time(0));
		m_metaserverEvent->setFrequency(m_metaserverFrequency);
	}
}

void MonopdServer::welcomeNew(Socket *socket)
{
	socket->ioWrite( std::string("<monopd><server version=\"") + VERSION "\" hostname=\"" + m_metaserverIdentity + "\"/></monopd>\n" );
}

void MonopdServer::initSocketTimeoutEvent(int socketFd)
{
		Event *socketTimeout = newEvent(Event::SocketTimeout, 0, socketFd);
		socketTimeout->setLaunchTime(time(0) + 30);
}

void MonopdServer::delSocketTimeoutEvent(int socketFd)
{
	Event *event = 0;
	for(std::vector<Event *>::iterator it = m_events.begin(); it != m_events.end() && (event = *it) ; ++it)
		if (event->id() == socketFd)
		{
			delEvent(event);
			return;
		}
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

void MonopdServer::sendGameList(Player *pInput, const bool &sendTemplates)
{
	pInput->ioWrite("<monopd>");

	// Supported game types for new games
	GameConfig *gcTmp = 0;
	if (sendTemplates)
		for(std::vector<GameConfig *>::iterator it = m_gameConfigs.begin() ; it != m_gameConfigs.end() && (gcTmp = *it) ; ++it )
			pInput->ioWrite("<gameupdate gameid=\"-1\" gametype=\"%s\" name=\"%s\" description=\"%s\"/>", gcTmp->id().c_str(), gcTmp->name().c_str(), gcTmp->description().c_str());

	// FIXME: DEPRECATED 1.0
	pInput->ioWrite("<updategamelist type=\"full\">");

	// Supported game types for new games (always send, type=full)
	gcTmp = 0;
	for(std::vector<GameConfig *>::iterator it = m_gameConfigs.begin() ; it != m_gameConfigs.end() && (gcTmp = *it) ; ++it )
		pInput->ioWrite("<game id=\"-1\" gametype=\"%s\" name=\"%s\" description=\"%s\"/>", gcTmp->id().c_str(), gcTmp->name().c_str(), gcTmp->description().c_str());

	// Games currently under configuration, we can join these
	Game *gTmp = 0;
	for(std::vector<Game *>::iterator it = m_games.begin(); it != m_games.end() && (gTmp = *it) ; ++it)
		if (gTmp->status() == Game::Config)
			pInput->ioWrite("<game id=\"%ld\" players=\"%d\" gametype=\"%s\" name=\"%s\" description=\"%s\" canbejoined=\"%d\"/>", gTmp->id(), gTmp->players(), gTmp->gameType().c_str(), gTmp->name().c_str(), gTmp->getStringProperty("description").c_str(), gTmp->getBoolProperty("canbejoined"));

	pInput->ioWrite("</updategamelist>");
	// END FIXME: DEPRECATED 1.0

	pInput->ioWrite("</monopd>\n");
}

void MonopdServer::processInput(Socket *socket, const std::string data)
{
	Player *pInput = findPlayer(socket);
	if (!pInput)
	{
		// The 'n' name command is available even for non-players. In fact,
		// it's considered to be the protocol handshake.
		if (data[0] == '.')
		{
			Player *pNew = 0;
			switch(data[1])
			{
			case 'n':
				pNew = newPlayer(socket, data.substr(2, 16));
				sendXMLUpdates();
				sendXMLUpdate(pNew, true, true); // give new player a full update (excluding self) so it knows who's in the lounge
				return;
			case 'R':
				pNew = newPlayer(socket, "");
				reconnectPlayer(pNew, data.substr(2));
				return;
			}
		}
		return;
	}

	if (data[0] == '.')
	{
		processCommands(pInput, data.substr(1));
		sendXMLUpdates();
		return;
	}

	if (data.size() > 256)
		pInput->ioError("Chat messages are limited to 256 characters");
	else
	{
		if (Game *game = pInput->game())
			game->ioWrite("<monopd><msg type=\"chat\" playerid=\"%d\" author=\"%s\" value=\"%s\"/></monopd>\n", pInput->id(), pInput->name().c_str(), escapeXML(data).c_str());
		else
			ioWrite("<monopd><msg type=\"chat\" playerid=\"" + itoa(pInput->id()) + "\" author=\"" + pInput->name() + "\" value=\"" + escapeXML(data) + "\"/></monopd>\n", true);
	}
}

void MonopdServer::processCommands(Player *pInput, const std::string data2)
{
	char *data = (char *)data2.c_str();
	if (data[0] != 'f')
		pInput->setRequestedUpdate(false);

	// The following commands are _always_ available.
	switch(data[0])
	{
	case 'n':
		setPlayerName(pInput, data2.substr(1, 16));
		return;
	case 'p':
		switch(data[1])
		{
			case 'i':
				pInput->setProperty("image", data2.substr(2, 32), this);
				return;
		}
		break;
	}

	// Commands available when player is not within a game.
	Game *game = pInput->game();
	if (!game)
	{
		switch(data[0])
		{
		case 'g':
			switch(data[1])
			{
				case 'l':
					sendGameList(pInput, true);
					return;
				case 'n':
					newGame(pInput, data2.substr(2));
					return;
				case 'j':
					joinGame(pInput, atol(data2.substr(2).c_str()));
					return;
				case 'S':
					joinGame( pInput, atol(data2.substr(2).c_str()), true );
					return;
				default:
					pInput->ioNoSuchCmd("you are not within a game");
			}
			break;
		case 'R':
			reconnectPlayer(pInput, data2.substr(1));
			break;
		default:
			pInput->ioNoSuchCmd("you are not within a game");
		}
		// The rest of the commands are only available within a game.
		return;
	}

	// These commands are always available in a running game, no matter what.
	switch(data[0])
	{
	case 'f':
		game->sendFullUpdate(pInput, true);
		return;
	case 'g':
		switch(data[1])
		{
		case 'x':
			exitGame(game, pInput);
			return;
		}
		break;
	}

	switch(game->status())
	{
	case Game::End:
		pInput->ioNoSuchCmd("this game has ended");
		// The rest of the commands are only available when the game has not ended.
		return;
	default:;
	}

	if (game->status() != Game::Config && !pInput->getBoolProperty("bankrupt") && !pInput->getBoolProperty("spectator"))
		switch(data[0])
		{
			case 'T':
				switch(data[1])
				{
				case 'c':
				case 'e':
					pInput->updateTradeObject(data+1);
					return;
				case 'm':
					pInput->updateTradeMoney(data+2);
					return;
				case 'n':
					game->newTrade(pInput, atol(data2.substr(2).c_str()));
					return;
				case 'a':
					game->acceptTrade(pInput, data+2);
					return;
				case 'r':
					game->rejectTrade(pInput, atol(data2.substr(2).c_str()));
					return;
				}
				break;
		}

	switch(data[0])
	{
	case 't':
		game->setTokenLocation(pInput, atoi(data+1));
		if (!game->clientsMoving())
			if (Event *event = findEvent(game, Event::TokenMovementTimeout))
				delEvent(event);
		return;
	}

	if (pInput->getBoolProperty("spectator") || pInput->getBoolProperty("bankrupt"))
	{
		pInput->ioNoSuchCmd("you are only a spectator");
		// The rest of the commands are only available for participating players
		return;
	}

	if (game->clientsMoving())
	{
		pInput->ioNoSuchCmd("other clients are still moving");
		// The rest of the commands are only available when no clients are moving
		return;
	}

	// If we're in a tax dialog, we don't accept too many commands.
	if (game->pausedForDialog())
	{
		switch(data[0])
		{
		case 'T':
			switch(data[1])
			{
			case '$':
				pInput->payTax();
				return;
			case '%':
				pInput->payTax(true);
				return;
			default:
				return;
			}
		default:
			// The rest of the commands are not available during a tax dialog
			return;
		}
	}

	switch(data[0])
	{
		case 't': // client authors find the no such command message annoying.
			return;
		break;
		// From the official rules: "may buy and erect at any time"
		case 'h':
			switch(data[1])
			{
				case 'b':
					pInput->buyHouse(atoi(data+2));
					return;
				case 's':
					pInput->sellHouse(atoi(data+2));
					return;
			}
			break;
		// From official rules: "Unimproved properties can be mortgaged
		// through the Bank at any time"
		// Selling estates is not officially supported, but it makes most
		// sense here.
		case 'e':
			switch(data[1])
			{
				case 'm':
					pInput->mortgageEstate(atoi(data+2));
					return;
				case 's':
					pInput->sellEstate(atoi(data+2));
					return;
			}
	}

	// Auctions restrict movement and stuff.
	Auction *auction = game->auction();
	if (auction && auction->status() != Auction::Completed)
	{
		switch(data[0])
			{
			case 'a':
				switch(data[1])
				{
				case 'b':
					if (!game->bidInAuction(pInput, data+2))
					{
						Event *event;
						event = findEvent(game, Event::AuctionTimeout);
						if (!event)
							event = newEvent(Event::AuctionTimeout, game);
						event->setLaunchTime(time(0) + 4);
					}
					return;
				default:
					pInput->ioNoSuchCmd();
					return;
				}
			default:
				pInput->ioNoSuchCmd("An auction is in progress.");
				return;
			}
	}

	// Declaring bankruptcy is only possible when a player is in debt.
	if (game->debts())
	{
		if (game->findDebt(pInput))
			switch(data[0])
			{
				case 'D':
					game->declareBankrupt(pInput);
					break;
				case 'p':
					game->solveDebts(pInput, true);
					break;
				default:
					pInput->ioNoSuchCmd("there are debts to be settled");
			}
		else
			pInput->ioNoSuchCmd("there are debts to be settled");
		// The rest of the commands are only available when there
		// are no debts to be settled.
		return;
	}

	// These are only available when it's the player's turn
	if(pInput->getBoolProperty("hasturn"))
	{
		if(pInput->getBoolProperty("can_buyestate"))
		{
			switch(data[0])
			{
			case 'e':
				switch(data[1])
				{
				case 'b':
					pInput->buyEstate();
					return;
		        case 'a':
					game->newAuction(pInput);
					return;
				default:
					pInput->ioNoSuchCmd();
					return;
				}
				break;
			}
		}

		if(pInput->getBoolProperty("jailed"))
		{
			switch(data[0])
			{
				case 'j':
					switch(data[1])
					{
						case 'c':
							pInput->useJailCard();
							return;
						case 'p':
							pInput->payJail();
							return;
						case 'r':
							pInput->rollJail();
							return;
						default:
							pInput->ioNoSuchCmd();
					}
					break;
			}
		}

		if(pInput->getBoolProperty("can_roll"))
		{
			switch(data[0])
			{
				case 'r':
					pInput->rollDice();
					Event *event = newEvent(Event::TokenMovementTimeout, game);
					event->setLaunchTime(time(0) + 10);
					return;
			}

		}
	}

	// The following commands have their own availability checks.
	switch(data[0])
	{
		case 'E':
			pInput->endTurn(true);
			break;
		case 'g':
			switch(data[1])
			{
				case 'd':
					setGameDescription(pInput, data2.substr(2, 64));
					return;
				case 'c':
					game->editConfiguration( pInput, data+2 );
					return;
				case 'e':
					game->editConfig(pInput, data+2);
					return;
				case 'k':
					Player *pKick;
					pKick = game->kickPlayer( pInput, atoi(data+2) );
					if (pKick)
						exitGame(game, pKick);
					return;
				case 'u':
					game->upgradePlayer( pInput, atoi(data+2) );
					return;
				case 'p':
					// FIXME: DEPRECATED 1.0
					return;
				case 's':
					game->start(pInput);

					// FIXME: DEPRECATED 1.0
					if (game->status() == Game::Run)
						ioWrite("<monopd><updategamelist type=\"del\"><game id=\"" + itoa(game->id()) + "\"/></updategamelist></monopd>\n");
					return;
				default:
					pInput->ioNoSuchCmd();
			}
			break;
		case 'd':
			pInput->closeSocket();
			return;
		default:
			pInput->ioNoSuchCmd();
	}
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
					pOutput->ioWrite("<monopd> ");
					updateEmpty = false;
				}
				pOutput->ioWrite("%s", updateXML.c_str());
			}
		}
	}

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
				pOutput->ioWrite("<monopd> ");
				updateEmpty = false;
			}
			pOutput->ioWrite("%s", updateXML.c_str());
		}
	}

	// Let game handle updates *about* all of its objects ..
	if (Game *game = pOutput->game())
		updateEmpty = game->sendChildXMLUpdate(pOutput, updateEmpty);

	if (!updateEmpty)
		pOutput->ioWrite("</monopd>\n");
}

void MonopdServer::setPlayerName(Player *player, const std::string &name)
{
		std::string useName = ( name.size() ? name : "anonymous" );

		int i=1;
		while (findPlayer(useName))
			useName = ( name.size() ? name : "anonymous" ) + itoa( ++i );

		player->setProperty("name", useName, this);

		Game *game = player->game();
		if (game)
		{
			// FIXME: DEPRECATED 1.0
			if (game->status() == Game::Config && player == game->master())
				ioWrite(std::string("<monopd><updategamelist type=\"edit\"><game id=\"") + itoa(game->id()) + "\" players=\"" + itoa(game->getIntProperty("players")) + "\" gametype=\"" + game->gameType() + "\" name=\"" + game->name() + "\" description=\"" + game->getStringProperty("description") + "\" canbejoined=\"" + itoa(game->getBoolProperty("canbejoined")) + "\"/></updategamelist></monopd>\n");
		}
}

