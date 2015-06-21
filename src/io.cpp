// Copyright © 2001-2002 Rob Kaper <cap@capsi.com>,
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

//#ifndef _GNU_SOURCE
//#define _GNU_SOURCE
//#endif
//#include <stdarg.h>
#include <stdio.h>

#include "io.h"

#define BUFSIZE	2048

std::string escapeXML(std::string data)
{
	stringReplace(data, "&", "\n");
	stringReplace(data, "<", "&lt;");
	stringReplace(data, ">", "&gt;");
	stringReplace(data, "\"", "&quot;");
	stringReplace(data, "\n", "&amp;");
	return data;
}

std::string escapeHTML(std::string data)
{
	stringReplace(data, " ", "+");
	return data;
}

std::string itoa(int number)
{
	char buf[32];
	snprintf(buf, sizeof(buf)-1, "%d", number);
	buf[sizeof(buf)-1] = 0;
	return std::string(buf);
}

void stringReplace( std::string & source, const std::string & find, const std::string & replace )
{
	size_t j;
	for ( ; ( j = source.find( find ) ) != std::string::npos ; )
		source.replace( j, find.length(), replace );
}
