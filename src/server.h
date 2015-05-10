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

#ifndef __MONOPD_SERVER_H__
#define	__MONOPD_SERVER_H__

#include "gameobject.h"
#include "main.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

class Auction;
class GameConfig;
class Event;
class Game;
class Player;
class Socket;

class MonopdServer : public GameObject
{
public:
	MonopdServer();
	~MonopdServer();

	void setListener(MonopdListener *listener) { m_listener = listener; }
	MonopdListener *listener() { return m_listener; }

	void setPort(int port);
	int port() { return m_port; }

	void closedSocket(Socket *socket);
	Game *findGame(Player *player);
	void delGame(Game *game, bool verbose = true);
	void identifyPlayer(Player *player, const std::string &name);
	Player *findPlayer(Socket *socket);
	void delPlayer(Player *player);

	void initMetaserverEvent();
	void welcomeNew(Socket *socket);
	void welcomeMetaserver(Socket *socket);
	void closedMetaserver(Socket *socket);
	int processEvents(); /* returns -1 or socket fd in case of socket timeout */
	void processInput(Socket *socket, const std::string data);
	void updateSystemdStatus();

private:
	Event *newEvent(unsigned int eventType, Game *game = 0, int id = 0);
	Event *findEvent(Auction *auction);
	Event *findEvent(Game *game, unsigned int eventType);
	Event *findEvent(Game *game, GameObject *object);
	void delEvent(Event *event);
	Game *findGame(int gameId);
	GameConfig *newGameConfig(const std::string id, const std::string name, const std::string description);
	void delGameConfig(GameConfig *gameConfig);
	Player *findPlayer(int playerId);
	Player *findPlayer(const std::string &name);
	Player *findCookie(const std::string &cookie);

	void newGame(Player *player, const std::string gameType);
	void joinGame(Player *player, unsigned int gameId, const bool &specator = false);
	void exitGame(Game *game, Player *player);
	void setGameDescription(Player *pInput, const std::string data);
	void reconnectPlayer(Player *player, const std::string &cookie);

	void registerMetaserver();
	void loadConfig();
	void loadGameTemplates();
	void ioWrite(const char *data, ...);
	void ioWrite(const std::string &data, const bool &noGameOnly = false);
	void sendGameList(Player *player, const bool &sendTemplates = false);
	void processCommands(Player *pInput, const std::string data);
	void sendXMLUpdates();
	void sendXMLUpdate(Player *player, bool fullUpdate = false, bool excludeSelf = false);
	void setPlayerName(Player *player, const std::string &name);

	std::vector<Event *> m_events;
	std::vector<Game *> m_games;
	std::vector<GameConfig *> m_gameConfigs;
	std::vector<Player *> m_players;

	MonopdListener *m_listener;
	unsigned int m_nextGameId, m_nextPlayerId;
	std::string m_metaserverIdentity, m_metaserverHost;
	int m_port, m_metaserverPort;
	bool m_useMetaserver;
	bool m_metaserverBusy;
	Event *m_metaserverEvent;
	struct addrinfo *m_metaserverAddrinfo;
};

#endif
