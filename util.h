#ifndef UTIL_H
#define UTIL_H

#include<iostream>
#include<sstream>
#include<cstring>
#include<stdlib.h>
#include"net.h"

#define MAX_FILE_NAME_LENGTH	24
#define MAX_DATA_SIZE		256
#define MAX_BUFFER_SIZE		312

/**********************************
 * ENUM Declaration
 **********************************/
typedef enum
{
	CLIENT_SERV_NONE,
	CLIENT_REQ_WRITE,
	CLIENT_REQ_READ,
	CLIENT_REQ_ABORT,
	CLIENT_SERV_RESP_SUCCESS,
	CLIENT_SERV_RESP_FAILURE,
	SERVER_REQ_LOCK,
	SERVER_REQ_COMMIT,
	SERVER_REQ_RECOVERY,
	SERVER_RESP_RECOVERY,
	SERVER_ACK,
	SERVER_HELLO,
	SERVER_STARTUP
}clientServMsgType;

/**********************************
 * STRUCTURE Declaration
 **********************************/
struct clientPkt
{
	clientServMsgType	msgType;
	std::string		fileName;
	char			data[MAX_DATA_SIZE];
};

/**********************************
 * FUNCTION Declaration
 **********************************/
std::string packetToMessage(struct clientPkt* cPkt);
clientPkt clientMsgToPacket(std::string str);
int hashFileName(string str);
#endif
