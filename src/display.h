// Copyright © 2002 Rob Kaper <cap@capsi.com>
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

#ifndef __MONOPD_DISPLAY_H__
#define	__MONOPD_DISPLAY_H__

#include <string>
#include <vector>

class Estate;

class DisplayButton
{
public:
	DisplayButton(const std::string command, const std::string caption, const bool enabled);
	DisplayButton(const DisplayButton *button);
	const std::string command();
	const std::string caption();
	bool enabled();

private:
	std::string m_command, m_caption;
	bool m_enabled;
};

class Display
{
public:
	Display();
	Display(const Display *display);
	~Display();

	void setEstate(Estate *estate);
	Estate *estate();
	void setText(const std::string text);
	void setText(const char *fmt, ...);
	const std::string text();
	void addButton(const std::string command, const std::string caption, const bool enabled);
	void resetButtons();
	void resetText();
	void resetEstate();
	bool clearButtons();
	bool clearText();
	bool clearEstate();
	std::vector<DisplayButton *> buttons();

private:
	Estate *m_estate;
	std::string m_text;
	std::vector<DisplayButton *> m_buttons;
	bool m_clearText, m_clearButtons, m_clearEstate;
};


#endif // MONOP_ESTATE_H
