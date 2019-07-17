/*
 * SocketPort.h
 *
 *  Created on: 01-Jun-2019
 *      Author: aksonlyaks
 */

#ifndef SRC_SOCKETPORT_H_
#define SRC_SOCKETPORT_H_

#include "SerialPort.h"
class SocketPort: public SerialPort
{
public:
	SocketPort(const std::string& name, int dev_id);
	virtual ~SocketPort();

public:
    bool open(int baud = 115200,
              int data = 8,
              SerialPort::Parity parity = SerialPort::ParityNone,
              SerialPort::StopBit stop = SerialPort::StopBitOne);
    void close();

    bool isUsb() { return _isUsb; };

    int read(uint8_t* data, int size);
    int write(const uint8_t* data, int size);
    int get();
    int put(int c);

    bool timeout(int millisecs);
    void flush();
    void setAutoFlush(bool autoflush);

private:
    int _devfd;
    bool _isUsb;
    int _timeout;
    bool _autoFlush;
};

#endif /* SRC_SOCKETPORT_H_ */
