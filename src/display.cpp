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

#include "display.h"
#include "estate.h"
#include "io.h"
#include "player.h"

DisplayButton::DisplayButton(const std::string command, const std::string caption, const bool enabled)
{
	m_command = command;
	m_caption = caption;
	m_enabled = enabled;
}

const std::string DisplayButton::command()
{
	return m_command;
}

const std::string DisplayButton::caption()
{
	return m_caption;
}

bool DisplayButton::enabled()
{
	return m_enabled;
}

Display::Display()
{
	m_estate = 0;
	m_text = "";
	m_clearText = m_clearButtons = false;
}

Display::~Display()
{
	reset();
}

void Display::reset()
{
	m_text = "";
	m_clearButtons = false;
	m_clearText = false;
	while (!m_buttons.empty()) { delete *m_buttons.begin(); m_buttons.erase(m_buttons.begin()); }
}

void Display::setEstate(Estate *estate)
{
	m_estate = estate;
}

Estate *Display::estate()
{
	return m_estate;
}

void Display::setText(const std::string text)
{
	m_text = text;
}

const std::string Display::text()
{
	return escapeXML(m_text);
}

void Display::addButton(const std::string command, const std::string caption, const bool enabled)
{
	DisplayButton *button = new DisplayButton(command, caption, enabled);
	m_buttons.push_back(button);
}

void Display::resetButtons()
{
	m_clearButtons = true;
}

void Display::setClearText(bool clearText)
{
	m_clearText = clearText;
}

bool Display::clearText()
{
	return m_clearText;
}

bool Display::clearButtons()
{
	return m_clearButtons;
}

std::vector<DisplayButton *> Display::buttons()
{
	return m_buttons;
}
