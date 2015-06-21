// Copyright © 2003 Rob Kaper <cap@capsi.com>
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

#ifndef ATLANTIC_GAMEPROPERTY_H
#define ATLANTIC_GAMEPROPERTY_H

#include <string>

class GameObject;

class GameProperty
{
public:
	GameProperty(const std::string &key, GameObject *m_scope);
	std::string key() const;
	// void setScope(GameObject *scope);
	GameObject *scope();
	void setChanged(const bool &changed);
	bool changed() const;

protected:
	std::string m_key;
	GameObject *m_scope;
	bool m_changed;
};

class GameIntProperty : public GameProperty
{
public:
	GameIntProperty(const std::string &key, int value, GameObject *scope);
	bool setValue(int value);
	int value();

private:
	int m_value;
};

class GameStringProperty : public GameProperty
{
public:
	GameStringProperty(const std::string &key, const std::string &value, GameObject *scope);
	bool setValue(const std::string &value);
	std::string value() const;

private:
	std::string m_value;
};

class GameBoolProperty : public GameProperty
{
public:
	GameBoolProperty(const std::string &key, const bool &value, GameObject *scope);
	bool setValue(const bool &value);
	bool value() const;

private:
	bool m_value;
};

#endif
