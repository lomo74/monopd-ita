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

#ifndef MONOP_GAMEOBJECT_H
#define MONOP_GAMEOBJECT_H

#include <string>
#include <vector>

class GameIntProperty;
class GameStringProperty;
class GameBoolProperty;
class Game;
class GameObject;

class GameObject
{
public:
 	// We can abuse a Money object with its id as amount in trades.
 	enum Type { Unknown = 0, GGame, GCard, GEstate, GEstateGroup, GPlayer, Money, ConfigOption };

	GameObject(int id, Type type = Unknown, Game *game = 0);
	~GameObject();

	void setId(int id) { m_id = id; }
	int id();
	std::string name();
	const enum Type type();
	Game *game();

	void setProperty(const std::string &key, const std::string &value, GameObject *scope = 0);
	void setProperty(const std::string &key, int value, GameObject *scope = 0);
	void setBoolProperty(const std::string &key, const bool &value, GameObject *scope = 0);
	std::string getStringProperty(const std::string &key);
	bool hasIntProperty(const std::string &key);
	int getIntProperty(const std::string &key);
	bool getBoolProperty(const std::string &key);

	void removeProperty(const std::string &key);

	void unsetPropertiesChanged();
	bool propertiesChanged() const;

	void addToScope(GameObject *object);
	bool hasInScope(GameObject *object);
	void removeFromScope(GameObject *object);

	bool propertiesChanged(GameObject *target);
	std::string oldXMLUpdate(GameObject *target, const bool &fullUpdate = false);
	std::string xmlUpdate(GameObject *target, const bool &fullUpdate = false);

protected:
	int m_id;
	Type m_type;
	Game *m_game;

private:
	std::vector<GameStringProperty *> m_stringProperties;
	std::vector<GameIntProperty *> m_intProperties;
	std::vector<GameBoolProperty *> m_boolProperties;
	std::vector<GameObject *> m_scope;
};

#endif
