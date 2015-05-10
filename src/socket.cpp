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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "socket.h"

Socket::Socket( int fd )
 :	m_status( Socket::New ),
	m_type( Socket::Player ),
	m_fd( fd ),
	m_addrinfoNext( NULL )
{
}

ssize_t Socket::ioWrite(const char *fmt, va_list args)
{
	int n, size = 256;
	char *buf = new char[size];
	static std::string ioStr;
	va_list arg;

	buf[0] = 0;

	while (1) {
		va_copy(arg, args);
		n = vsnprintf(buf, size, fmt, arg);
		va_end(arg);

		if (n > -1 && n < size) {
			ioStr = buf;
			delete[] buf;
			return ioWrite(ioStr);
		}

		if (n > -1)
			size = n+1;
		else
			size *= 2;

		delete[] buf;
		buf = new char[size];
	}
}

ssize_t Socket::ioWrite(const char *fmt, ...)
{
	va_list arg;
	ssize_t s;

	va_start(arg, fmt);
	s = ioWrite(fmt, arg);
	va_end(arg);
	return s;
}

ssize_t Socket::ioWrite(const std::string data)
{
	if (m_status == New || m_status == Ok)
		return write(m_fd, data.c_str(), strlen(data.c_str()));

	errno = EBADFD;
	return -1;
}

bool Socket::hasReadLine()
{
	static std::string newLine = "\r\n";
	std::string::size_type pos = m_ioBuf.find_first_of(newLine);

	return (!(pos == std::string::npos));
}

const std::string Socket::readLine()
{
	static std::string newLine = "\r\n";
	std::string::size_type pos = m_ioBuf.find_first_of(newLine);

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
