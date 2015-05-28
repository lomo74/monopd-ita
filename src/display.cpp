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

#include <stdio.h>
#include <stdarg.h>

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

DisplayButton::DisplayButton(const DisplayButton *button) {
	m_command = button->m_command;
	m_caption = button->m_caption;
	m_enabled = button->m_enabled;
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
	m_estate = NULL;
	m_text = "";
	m_clearText = m_clearButtons = m_clearEstate = false;
}

Display::Display(const Display *display) {
	m_estate = display->m_estate;
	m_text = display->m_text;
	m_clearText = display->m_clearText;
	m_clearButtons = display->m_clearButtons;
	m_clearEstate = display->m_clearEstate;

	std::vector<DisplayButton *> buttons = display->m_buttons;
	for (std::vector<DisplayButton *>::iterator it = buttons.begin() ; it != buttons.end(); ++it) {
		DisplayButton *button = new DisplayButton(*it);
		m_buttons.push_back(button);
	}
}

Display::~Display()
{
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

void Display::setText(const char *fmt, ...)
{
	va_list arg;
	char buf[2048];

	va_start(arg, fmt);
	vsnprintf(buf, sizeof(buf), fmt, arg);
	va_end(arg);
	buf[sizeof(buf)-1] = 0;

	m_text = std::string(buf);
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

void Display::resetText()
{
	m_clearText = true;
}

void Display::resetEstate()
{
	m_clearEstate = true;
}

bool Display::clearButtons()
{
	return m_clearButtons;
}

bool Display::clearText()
{
	return m_clearText;
}

bool Display::clearEstate()
{
	return m_clearEstate;
}

std::vector<DisplayButton *> Display::buttons()
{
	return m_buttons;
}
