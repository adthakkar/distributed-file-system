#ifndef UTIL_H
#define UTIL_H

#include<iostream>

using namespace std;

#define MAX_FILE_NAME_LENGTH	24
#define MAX_DATA_SIZE		256
#define MAX_BUFFER_SIZE		312

/**********************************
 * ENUM Declaration
 **********************************/
typedef enum
{
	CLIENT_REQ_WRITE,
	CLIENT_REQ_READ,
	CLIENT_REQ_ABORT,
	SERV_CLIENT_RESP_SUCCESS,
	SERV_CLIENT_RESP_FAILURE
}clientMsgType;

/**********************************
 * STRUCTURE Declaration
 **********************************/
struct clientPkt
{
	clientMsgType		msgType;
	char			fileName[MAX_FILE_NAME_LENGTH+1];
	char			data[MAX_DATA_SIZE+1];
};

/**********************************
 * FUNCTION Declaration
 **********************************/
string packetToMessage(struct clientPkt* cPkt);
clientPkt clientMsgToPacket(string str);
#endif
