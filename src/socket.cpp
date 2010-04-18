// Copyright (c) 2002-2004 Rob Kaper <rob@unixcode.org>. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "socket.h"

extern int errno;

Socket::Socket( unsigned int fd )
 :	m_status( Socket::New ),
	m_fd( fd )
{
}

int Socket::ioWrite(const std::string data)
{
	if ( m_status == New || m_status == Ok )
	{
		write( m_fd, data.c_str(), strlen( data.c_str() ) );
		return 0;
	}
	return 1;
}

const bool Socket::hasReadLine()
{
	static std::string newLine = "\r\n";
	unsigned int pos = m_ioBuf.find_first_of(newLine);

	return (!(pos == std::string::npos));
}

const std::string Socket::readLine()
{
	static std::string newLine = "\r\n";
	unsigned int pos = m_ioBuf.find_first_of(newLine);

	if (pos != std::string::npos)
	{
		// Grab first part for the listener
           std::string data = m_ioBuf.substr(0, pos);

		// Remove grabbed part from buffer
		m_ioBuf.erase(0, pos);
			
		// Remove all subsequent newlines
		m_ioBuf.erase(0, m_ioBuf.find_first_not_of(newLine));

		return data;
	}
	return std::string();
}

void Socket::fillBuffer(const std::string data)
{
	if (m_ioBuf.size())
		m_ioBuf.append(data);
	else
	{
		m_ioBuf.erase();
		m_ioBuf.append(data);
	}
}
