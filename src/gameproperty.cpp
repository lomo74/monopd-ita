// Copyright (c) 2003 Rob Kaper <cap@capsi.com>
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

#include "gameobject.h"
#include "gameproperty.h"

GameProperty::GameProperty(const std::string &key, GameObject *scope)
{
	m_key = key;
	m_scope = scope;
	m_changed = true;
}

std::string GameProperty::key() const
{
	return m_key;
}

GameObject *GameProperty::scope()
{
	return m_scope;
}

void GameProperty::setChanged(const bool &changed)
{
	m_changed = changed;
}

bool GameProperty::changed() const
{
	return m_changed;
}

GameIntProperty::GameIntProperty(const std::string &key, int value, GameObject *scope) : GameProperty(key, scope)
{
	m_value = value;
}


bool GameIntProperty::setValue(int value)
{
	if (m_value != value)
	{
		m_value = value;
		m_changed = true;
	}
	return m_changed;
}

int GameIntProperty::value()
{
	return m_value;
}

GameStringProperty::GameStringProperty(const std::string &key, const std::string &value, GameObject *scope) : GameProperty(key, scope)
{
	m_value = value;
}

bool GameStringProperty::setValue(const std::string &value)
{
	if (m_value != value)
	{
		m_value = value;
		m_changed = true;
	}
	return m_changed;
}

std::string GameStringProperty::value() const
{
	return m_value;
}

GameBoolProperty::GameBoolProperty(const std::string &key, const bool &value, GameObject *scope) : GameProperty(key, scope)
{
	m_value = value;
}

bool GameBoolProperty::setValue(const bool &value)
{
	if (m_value != value)
	{
		m_value = value;
		m_changed = true;
	}
	return m_changed;
}

bool GameBoolProperty::value() const
{
	return m_value;
}
