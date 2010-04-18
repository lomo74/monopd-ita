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

#include <cassert>

#include "gameobject.h"
#include "gameproperty.h"
#include "io.h"

GameObject::GameObject(int id, Type type, Game *game)
{
	m_id = id;
	m_type = type;
	m_game = game;
}

GameObject::~GameObject()
{
	while (!m_stringProperties.empty()) { delete *m_stringProperties.begin(); m_stringProperties.erase(m_stringProperties.begin()); }
	while (!m_intProperties.empty()) { delete *m_intProperties.begin(); m_intProperties.erase(m_intProperties.begin()); }
	while (!m_boolProperties.empty()) { delete *m_boolProperties.begin(); m_boolProperties.erase(m_boolProperties.begin()); }
}

int GameObject::id()
{
	return m_id;
}

std::string GameObject::name()
{
	return escapeXML(getStringProperty("name"));
}

const enum GameObject::Type GameObject::type()
{
	return m_type;
}

Game *GameObject::game()
{
	return m_game;
}

void GameObject::setProperty(const std::string &key, const std::string &value, GameObject *scope)
{
	GameStringProperty *stringProperty = 0;
	for(std::vector<GameStringProperty *>::iterator it = m_stringProperties.begin() ; it != m_stringProperties.end() && (stringProperty = *it) ; ++it)
		if (stringProperty->key() == key)
		{
			stringProperty->setValue(value);
			return;
		}

	stringProperty = new GameStringProperty(key, value, scope ? scope : this);
	m_stringProperties.push_back(stringProperty);
}

void GameObject::setProperty(const std::string &key, int value, GameObject *scope)
{
	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
	{
		if (intProperty->key() == key)
		{
			intProperty->setValue(value);
			return;
		}
	}
	intProperty = new GameIntProperty(key, value, scope ? scope : this);
	m_intProperties.push_back(intProperty);
}

void GameObject::setBoolProperty(const std::string &key, const bool &value, GameObject *scope)
{
	GameBoolProperty *boolProperty = 0;
	for(std::vector<GameBoolProperty *>::iterator it = m_boolProperties.begin() ; it != m_boolProperties.end() && (boolProperty = *it) ; ++it)
	{
		if (boolProperty->key() == key)
		{
			boolProperty->setValue(value);
			return;
		}
	}
	boolProperty = new GameBoolProperty(key, value, scope ? scope : this);
	m_boolProperties.push_back(boolProperty);
}

std::string GameObject::getStringProperty(const std::string &key)
{
	GameStringProperty *stringProperty = 0;
	for(std::vector<GameStringProperty *>::iterator it = m_stringProperties.begin() ; it != m_stringProperties.end() && (stringProperty = *it) ; ++it)
		if (stringProperty->key() == key)
			return stringProperty->value();
	return std::string();
}

bool GameObject::hasIntProperty(const std::string &key)
{
	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
		if (intProperty->key() == key)
			return true;
	return false;
}

int GameObject::getIntProperty(const std::string &key)
{
	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
		if (intProperty->key() == key)
			return intProperty->value();
	return 0;
}

bool GameObject::getBoolProperty(const std::string &key)
{
	GameBoolProperty *boolProperty = 0;
	for(std::vector<GameBoolProperty *>::iterator it = m_boolProperties.begin() ; it != m_boolProperties.end() && (boolProperty = *it) ; ++it)
		if (boolProperty->key() == key)
			return boolProperty->value();
	return 0;
}

void GameObject::removeProperty(const std::string &key)
{
	GameStringProperty *stringProperty = 0;
	for(std::vector<GameStringProperty *>::iterator it = m_stringProperties.begin() ; it != m_stringProperties.end() && (stringProperty = *it) ; ++it)
		if (stringProperty->key() == key)
		{
			m_stringProperties.erase(it);
			delete stringProperty;
			// FIXME: send xml update to erase stringProperty
			return;
		}

	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
		if (intProperty->key() == key)
		{
			m_intProperties.erase(it);
			delete intProperty;
			// FIXME: send xml update to erase intProperty
			return;
		}

	GameBoolProperty *boolProperty = 0;
	for(std::vector<GameBoolProperty *>::iterator it = m_boolProperties.begin() ; it != m_boolProperties.end() && (boolProperty = *it) ; ++it)
		if (boolProperty->key() == key)
		{
			m_boolProperties.erase(it);
			delete boolProperty;
			// FIXME: send xml update to erase boolProperty
			return;
		}
}

std::string GameObject::xmlUpdate(GameObject *target, const bool &fullUpdate)
{
	std::string xmlString;
	bool emptyUpdate = true;

	GameStringProperty *stringProperty = 0;
	for(std::vector<GameStringProperty *>::iterator it = m_stringProperties.begin() ; it != m_stringProperties.end() && (stringProperty = *it) ; ++it)
		if ( (fullUpdate || stringProperty->changed()) && (stringProperty->scope()->hasInScope(target)) )
		{
			if (emptyUpdate)
			{
				xmlString = "<update object=\"" + itoa(m_type) + "\" id=\"" + itoa(m_id) + "\">";
				emptyUpdate = false;
			}
			xmlString += "<string key=\"" + stringProperty->key() + "\" value=\"" + escapeXML(stringProperty->value()) + "\"/>";
		}

	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
		if ( (fullUpdate || intProperty->changed()) && (intProperty->scope()->hasInScope(target)) )
		{
			if (emptyUpdate)
			{
				xmlString = "<update object=\"" + itoa(m_type) + "\" id=\"" + itoa(m_id) + "\">";
				emptyUpdate = false;
			}
			xmlString += "<int key=\"" + intProperty->key() + "\" value=\"" + itoa(intProperty->value()) + "\"/>";
		}

	GameBoolProperty *boolProperty = 0;
	for(std::vector<GameBoolProperty *>::iterator it = m_boolProperties.begin() ; it != m_boolProperties.end() && (boolProperty = *it) ; ++it)
		if ( (fullUpdate || boolProperty->changed()) && (boolProperty->scope()->hasInScope(target)) )
		{
			if (emptyUpdate)
			{
				xmlString = "<update object=\"" + itoa(m_type) + "\" id=\"" + itoa(m_id) + "\">";
				emptyUpdate = false;
			}
			xmlString += "<bool key=\"" + boolProperty->key() + "\" value=\"" + itoa(boolProperty->value()) + "\"/>";
		}

	if (!emptyUpdate)
		xmlString += "</update>";
	return xmlString;
}

std::string GameObject::oldXMLUpdate(GameObject *target, const bool &fullUpdate)
{
	std::string xmlString;
	bool emptyUpdate = true;

	GameStringProperty *stringProperty = 0;
	for(std::vector<GameStringProperty *>::iterator it = m_stringProperties.begin() ; it != m_stringProperties.end() && (stringProperty = *it) ; ++it)
		if ( (fullUpdate || stringProperty->changed()) && (stringProperty->scope()->hasInScope(target)) )
		{
			if (emptyUpdate)
			{
				if (m_type == GPlayer)
					xmlString = "<playerupdate playerid=\"" + itoa(m_id) + "\"";
				else if (m_type == GEstate)
					xmlString = "<estateupdate estateid=\"" + itoa(m_id) + "\"";
				else if (m_type == GEstateGroup)
					xmlString = "<estategroupupdate groupid=\"" + itoa(m_id) + "\"";
				else if (m_type == GGame)
					xmlString = "<gameupdate gameid=\"" + itoa(m_id) + "\"";
				else if (m_type == ConfigOption)
					xmlString = "<configupdate configid=\"" + itoa(m_id) + "\"";
				else
					assert(0); // don't use use this on non-supported objects yet! :-p~

				emptyUpdate = false;
			}
			xmlString += std::string(" ") + stringProperty->key() + "=\"" + escapeXML(stringProperty->value()) + "\"";
		}

	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
		if ( (fullUpdate || intProperty->changed()) && (intProperty->scope()->hasInScope(target)) )
		{
			if (emptyUpdate)
			{
				if (m_type == GPlayer)
					xmlString = "<playerupdate playerid=\"" + itoa(m_id) + "\"";
				else if (m_type == GEstate)
					xmlString = "<estateupdate estateid=\"" + itoa(m_id) + "\"";
				else if (m_type == GEstateGroup)
					xmlString = "<estategroupupdate groupid=\"" + itoa(m_id) + "\"";
				else if (m_type == GGame)
					xmlString = "<gameupdate gameid=\"" + itoa(m_id) + "\"";
				else if (m_type == ConfigOption)
					xmlString = "<configupdate configid=\"" + itoa(m_id) + "\"";
				else
					assert(0); // don't use use this on non-supported objects yet! :-p~

				emptyUpdate = false;
			}
			xmlString += std::string(" ") + intProperty->key() + "=\"" + itoa(intProperty->value()) + "\"";
		}

	GameBoolProperty *boolProperty = 0;
	for(std::vector<GameBoolProperty *>::iterator it = m_boolProperties.begin() ; it != m_boolProperties.end() && (boolProperty = *it) ; ++it)
		if ( (fullUpdate || boolProperty->changed()) && (boolProperty->scope()->hasInScope(target)) )
		{
			if (emptyUpdate)
			{
				if (m_type == GPlayer)
					xmlString = "<playerupdate playerid=\"" + itoa(m_id) + "\"";
				else if (m_type == GEstate)
					xmlString = "<estateupdate estateid=\"" + itoa(m_id) + "\"";
				else if (m_type == GEstateGroup)
					xmlString = "<estategroupupdate groupid=\"" + itoa(m_id) + "\"";
				else if (m_type == GGame)
					xmlString = "<gameupdate gameid=\"" + itoa(m_id) + "\"";
				else if (m_type == ConfigOption)
					xmlString = "<configupdate configid=\"" + itoa(m_id) + "\"";
				else
					assert(0); // don't use use this on non-supported objects yet! :-p~

				emptyUpdate = false;
			}
			xmlString += std::string(" ") + boolProperty->key() + "=\"" + itoa(boolProperty->value()) + "\"";
		}

	if (!emptyUpdate)
		xmlString += "/>";
	return xmlString;
}

void GameObject::unsetPropertiesChanged()
{
	GameStringProperty *stringProperty = 0;
	for(std::vector<GameStringProperty *>::iterator it = m_stringProperties.begin() ; it != m_stringProperties.end() && (stringProperty = *it) ; ++it)
		if (stringProperty->changed())
			stringProperty->setChanged(false);

	GameIntProperty *intProperty = 0;
	for(std::vector<GameIntProperty *>::iterator it = m_intProperties.begin() ; it != m_intProperties.end() && (intProperty = *it) ; ++it)
		if (intProperty->changed())
			intProperty->setChanged(false);

	GameBoolProperty *boolProperty = 0;
	for(std::vector<GameBoolProperty *>::iterator it = m_boolProperties.begin() ; it != m_boolProperties.end() && (boolProperty = *it) ; ++it)
		if (boolProperty->changed())
			boolProperty->setChanged(false);
}

void GameObject::addToScope(GameObject *object)
{
	m_scope.push_back(object);
}

bool GameObject::hasInScope(GameObject *object)
{
	if (object == this)
		return true;

	for(std::vector<GameObject *>::iterator it = m_scope.begin() ; it != m_scope.end() && (*it) ; ++it)
	{
		if ((*it) == object || (*it)->hasInScope(object))
			return true;
	}
	return false;
}

void GameObject::removeFromScope(GameObject *object)
{
	GameObject *oTmp = 0;
	for(std::vector<GameObject *>::iterator it = m_scope.begin() ; it != m_scope.end() && (oTmp = *it) ; ++it)
		if (oTmp == object)
		{
			m_scope.erase(it);
			return;
		}
}
