/*
 * SocketPort.cpp
 *
 *  Created on: 01-Jun-2019
 *      Author: aksonlyaks
 */

#include "SocketPort.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <errno.h>
#include <sys/socket.h>

SocketPort::SocketPort(const std::string& name, int dev_id):
	SerialPort(name), _devfd(dev_id), _isUsb(1), _timeout(0),
    _autoFlush(false)
{
}

SocketPort::~SocketPort() {
    if (_devfd >= 0)
        ::close(_devfd);
}

bool SocketPort::open(int baud, int data, SerialPort::Parity parity, SerialPort::StopBit stop)
{
	fprintf(stderr, "Socket Open:%d\n", _devfd);
	return true;
}

void SocketPort::close()
{
    if (_devfd >= 0){
    	fprintf(stderr, "Socket Close\n");
    	::shutdown(_devfd, SHUT_RDWR);
        ::close(_devfd);
    }
    _devfd = -1;
}

int SocketPort::read(uint8_t* buffer, int len)
{
    fd_set fds;
    struct timeval tv;
    int numread = 0;
    int retval;

    if (_devfd == -1)
        return -1;
	fprintf(stderr, "Socket len read:%d\n", len);
    while (numread < len)
    {
        FD_ZERO(&fds);
        FD_SET(_devfd, &fds);

        tv.tv_sec  = _timeout / 1000;
        tv.tv_usec = (_timeout % 1000) * 1000;

        retval = select(_devfd + 1, &fds, NULL, NULL, &tv);

        if (retval < 0)
        {
        	perror("Error in read: ");
            return -1;
        }
        else if (retval == 0)
        {
            return numread;
        }
        else if (FD_ISSET(_devfd, &fds))
        {
            retval = ::read(_devfd, (uint8_t*) buffer + numread, len - numread);
            if (retval < 0)
                return -1;
            numread += retval;
        	fprintf(stderr, "********** Socket read:%d\n", numread);
        }
    }

    return numread;
}

int SocketPort::write(const uint8_t* buffer, int len)
{
    if (_devfd == -1)
        return -1;

    int res = ::write(_devfd, buffer, len);

    fprintf(stderr, "Socket Write:%d\n", res);
    // Used on macos to avoid upload errors
    if (_autoFlush)
        flush();
    return res;
}

int SocketPort::get()
{
    uint8_t byte;

    if (_devfd == -1)
        return -1;

    if (read(&byte, 1) != 1)
        return -1;

    return byte;
}

int SocketPort::put(int c)
{
    uint8_t byte;

    byte = c;
    return write(&byte, 1);
}

void SocketPort::flush()
{
    //::sync(_devfd);
	usleep(1000);
}

bool SocketPort::timeout(int millisecs)
{
    _timeout = millisecs;
    return true;
}

void SocketPort::setAutoFlush(bool autoflush)
{
    _autoFlush = autoflush;
}
