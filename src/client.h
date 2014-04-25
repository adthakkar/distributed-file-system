#ifndef CLIENT_H
#define CLIENT_H

#include<stdlib.h>
#include<time.h>
#include<sys/time.h>
#include<errno.h>
#include"logger.h"
#include"net.h"
#include"util.h"

#define	CLIENT_RESP_TIME_LIMIT	12000000

/**********************************
 * ENUM Declaration
 **********************************/

/**********************************
 * GLOBAL VARIABLES Declaration
 **********************************/
struct sockaddr_in servAddress[MAX_NODES];

/**********************************
 * FUNCTION Declaration
 **********************************/
int initializeSystem();
void getUserInput(struct clientPkt* cPkt, int* rNum);
int sendRequestToServer(struct clientPkt* cPkt, int rNum);

#endif
