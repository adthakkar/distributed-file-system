#ifndef SERVER_H
#define SERVER_H

#include<stdlib.h>
#include<pthread.h>
#include<map>
#include<time.h>
#include<sys/time.h>
#include<errno.h>
#include"logger.h"
#include"net.h"
#include"util.h"

#define	MAX_HOST_NAME_LENGTH	19
#define HELLO_INTERVAL		3000000
#define LOCK_MUTEX(type)	pthread_mutex_lock(&type)
#define UNLOCK_MUTEX(type)	pthread_mutex_unlock(&type)

/**********************************
 * GLOBAL VARIABLES Declaration
 **********************************/
pthread_t connThread;
pthread_t heartbeatThread;

pthread_mutex_t dataLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;

int servSockDesc[MAX_NODES];
int myId;
long int liveNodesTimer[MAX_NODES];

string debugFileName;

bool activeConnections[MAX_NODES];
bool startup, readyForRecovery, needRecovery;

struct sockaddr_in servAddress[MAX_NODES];

/**********************************
 * ENUM Declaration
 **********************************/
typedef enum
{
	SERVER_REQ_LOCK,
	SERVER_REQ_COMMIT,
	SERVER_REQ_RECOVERY,
	SERVER_RESP_RECOVERY,
	SERVER_ACK,
	SERVER_HELLO,
	SERVER_STARTUP
}serverMsgType;

/**********************************
 * STRUCTURE Declaration
 **********************************/
struct serverPkt
{
	serverMsgType		msgType;
	int 			hashNum;
	int 			serverId;
	int 			version;
	int 			lock;
	char 			fileName[MAX_FILE_NAME_LENGTH+1];
	char 			data[MAX_DATA_SIZE+1];
};

struct helloPkt
{
	serverMsgType		msgType;
	int 			serverId;
	int 			storeSizeHash;
	int 			storeSizeHashMinusOne;
	int 			storeSizeHashMinusTwo;
};

/**********************************
 * FUNCTION Declaration
 **********************************/
int initializeSystem();
void* processConnection(void* ptr);
void* initCommunication(void* ptr);
void* issueHeartbeat(void* ptr);
void* checkConnectivity(void* ptr);
string packetToMessage(struct serverPkt* servPkt);
string packetToMessage(struct helloPkt* hPkt);
struct serverPkt servMsgToPacket(string sMsg);
struct helloPkt helloMsgToPacket(string hMsg);
long int getCurTimeMilliSec();
void logToFile(logType type, string str);

#endif
