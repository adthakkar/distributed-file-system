#ifndef NET_H
#define NET_H

#include<netdb.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<cstring>
#include"logger.h"

/**********************************
 * PREPROCESSOR Directives
 **********************************/
#define	MAX_CONNECTIONS		11
#define MAX_NODES		7

/**********************************
 * GLOBAL VARIABLE Declaration
 **********************************/
static const int servPortNums[MAX_NODES] = {1126,
					    1127,
					    1128,
					    1129,
					    1130,
					    1131,
					    1132};
static const string servHostNames[MAX_NODES] = {"net21.utdallas.edu",
					 "net26.utdallas.edu",
					 "net27.utdallas.edu",
					 "net28.utdallas.edu",
					 "net29.utdallas.edu",
					 "net30.utdallas.edu",
					 "net31.utdallas.edu"};
/**********************************
 * STRUCTURE Declaration
 **********************************/
struct connection
{
	int			sockDesc;
	struct sockaddr_in	clientAddr;
	int			addrLen;
};

/**********************************
 * ENUM Declaration
 **********************************/
typedef enum
{
	TCP
}socketType;

/**********************************
 * FUNCTION Declaration
 **********************************/
int createSocket(socketType type);
int startServer(socketType type, int portNum);
int getNodeAddr(const char* hostName, int portNum, struct sockaddr_in* addr); 
int getNodeAddr(int port, struct sockaddr_in* address);

#endif
