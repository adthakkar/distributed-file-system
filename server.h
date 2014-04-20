#ifndef SERVER_H
#define SERVER_H

#include<stdlib.h>
#include<pthread.h>
#include<map>
#include<vector>
#include<time.h>
#include<sys/time.h>
#include<errno.h>
#include"logger.h"
#include"net.h"
#include"util.h"

#define	MAX_HOST_NAME_LENGTH	19
#define HELLO_INTERVAL		3000000
#define	SERVER_RESP_TIME_LIMIT	4000000
#define LOCK_MUTEX(type)	pthread_mutex_lock(&type)
#define UNLOCK_MUTEX(type)	pthread_mutex_unlock(&type)

/**********************************
 * ENUM Declaration
 **********************************/
typedef enum
{
	STORE_NONE,
	STORE_HASH,
	STORE_HASH_MINUS_ONE,
	STORE_HASH_MINUS_TWO
}storageLocation;

/**********************************
 * STRUCTURE Declaration
 **********************************/
struct serverPkt
{
	clientServMsgType	msgType;
	int 			hashNum;
	int 			serverId;
	int 			version;
	bool 			lock;
	string 			fileName;
	char 			data[MAX_DATA_SIZE];
};

struct helloPkt
{
	clientServMsgType	msgType;
	int 			serverId;
	int 			storeSizeHash;
	int 			storeSizeHashMinusOne;
	int 			storeSizeHashMinusTwo;
};

struct readRequest
{
	int			hashNum;
	string			fileName;
	connection*		conn;
};

struct writeRequest
{
	int 			hashNum;
	bool			lockEnable;
	connection*		conn;
	string			fileName;
	long int		serverRespTimer;
	int			numAck;
	clientServMsgType	lastMsg;
	char			data[MAX_DATA_SIZE];
};

/**********************************
 * GLOBAL VARIABLES Declaration
 **********************************/
pthread_t connThread;
pthread_t heartbeatThread;
pthread_t readRequestThread;
pthread_t writeRequestThread;

pthread_mutex_t dataLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;

int servSockDesc[MAX_NODES];
int myId;
long int liveNodesTimer[MAX_NODES];

string debugFileName;
string logHash, logHashMinusOne, logHashMinusTwo;

bool activeConnections[MAX_NODES];
bool startup, readyForRecovery, needRecovery;

struct sockaddr_in servAddress[MAX_NODES];

std::map<string, storageLocation> directory;

std::vector<serverPkt> storeHash;
std::vector<serverPkt> storeHashMinusOne;
std::vector<serverPkt> storeHashMinusTwo;
std::vector<readRequest> readQueue;
std::vector<writeRequest> writeQueue;

/**********************************
 * FUNCTION Declaration
 **********************************/
int initializeSystem();
void* processConnection(void* ptr);
void* issueHeartbeat(void* ptr);
void* processReadRequest(void* ptr);
void* processWriteRequest(void* ptr);
string packetToMessage(struct serverPkt* servPkt);
string packetToMessage(struct helloPkt* hPkt);
struct serverPkt servMsgToPacket(string sMsg);
struct helloPkt helloMsgToPacket(string hMsg);
long int getCurTimeMilliSec();
void logToFile(logType type, string str);
int validateHash(int hashNum);
std::vector<serverPkt>::iterator storeEndIndex(storageLocation storeType);
int sendMessage(int sockDesc, const char* msg, int msgLen);
std::vector<serverPkt>::iterator findFile(string fileName, storageLocation storeType);
void writeToFile(storageLocation storeType, struct serverPkt* sPkt);
storageLocation findStoreType(int hNum);
std::vector<writeRequest>::iterator findWriteRequest(string fileName);
#endif
