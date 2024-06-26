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

#ifndef __MONOPD_SERVER_H__
#define	__MONOPD_SERVER_H__

#include "gameobject.h"
#include "listener.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>

class Auction;
class GameConfig;
class Event;
class Game;
class Player;
class Socket;

class MonopdServer : public GameObject
{
public:
	MonopdServer(int argc, char **argv);
	~MonopdServer();

	void closedSocket(Socket *socket);
	Game *findGame(Player *player);
	void delGame(Game *game, bool verbose = true);
	Player *findPlayer(Socket *socket);
	void delPlayer(Player *player);

	void welcomeNew(Socket *socket);
	void welcomeMetaserver(Socket *socket);
	void closedMetaserver(Socket *socket);
	void processInput(Socket *socket, const std::string data2);
	int timeleftEvent();

private:
	void updateSystemdStatus();

	Event *newEvent(unsigned int eventType, Game *game = 0, int id = 0);
	Event *findEvent(Auction *auction);
	Event *findEvent(Game *game, unsigned int eventType);
	Event *findEvent(Game *game, GameObject *object);
	void delEvent(Event *event);
	void processEvents();
	Game *findGame(int gameId);
	GameConfig *newGameConfig(const std::string id, const std::string name, const std::string description);
	void delGameConfig(GameConfig *gameConfig);
	Player *findPlayer(int playerId);
	Player *findPlayer(const std::string &name);
	Player *findCookie(const std::string &cookie);

	void newGame(Player *player, const std::string gameType);
	void joinGame(Player *player, unsigned int gameId, const bool &specator = false);
	void exitGame(Game *game, Player *player);
	void setGameDescription(Player *pInput, std::string data);
	void reconnectPlayer(Player *player, const std::string &cookie);

	void registerMetaserver();
	void loadConfig();
	void loadGameTemplates();
	void ioWrite(const char *data, ...);
	void ioWrite(const std::string &data, const bool &noGameOnly = false);
	void sendGameTemplateList(Player *player);
	void sendXMLUpdates();
	void sendXMLUpdate(Player *player, bool fullUpdate = false, bool excludeSelf = false);
	void setPlayerName(Player *player, std::string name);
	void setPlayerImage(Player *player, std::string image);

	std::vector<Event *> m_events;
	std::vector<Game *> m_games;
	std::vector<GameConfig *> m_gameConfigs;
	std::vector<Player *> m_players;

	Listener *m_listener;
	unsigned int m_nextGameId, m_nextPlayerId;
	std::string m_metaserverIdentity, m_metaserverHost;
	int m_port, m_metaserverPort;
	bool m_useMetaserver;
	bool m_metaserverBusy;
	Event *m_metaserverEvent;
	struct addrinfo *m_metaserverAddrinfo;
};

#endif
