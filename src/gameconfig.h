// Copyright (c) 2002 Rob Kaper <cap@capsi.com>
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

#ifndef MONOP_GAMECONFIG_H
#define MONOP_GAMECONFIG_H

#include <string>

class GameConfig
{
public:
	GameConfig(const std::string &id, const std::string &name, const std::string &description);

	const std::string id() { return m_id; }
	const std::string name() { return m_name; }
	const std::string description() { return m_description; }

private:
	std::string m_id, m_name, m_description;
};

#endif // MONOP_GAMECONFIG_H
