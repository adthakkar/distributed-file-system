#include"server.h"

std::map<string, serverPkt> storeHash;
std::map<string, serverPkt> storeHashMinusOne;
std::map<string, serverPkt> storeHashMinusTwo;

int main(int argc, char** argv)
{
	int 			socketDesc = -1;
	connection*		conn;
	std::stringstream	ss;

	//HACK
	printToConsole = true;

	if(0 >= initializeSystem())
	{
		log(ERROR, "System Initialization FAILED \n");
		return -1;
	}

	socketDesc = startServer(TCP, servPortNums[myId]);

	if(0 >= socketDesc)
	{
		log(ERROR, "UNABLE to start server \n");
		return -1;
	}

	pthread_create(&heartbeatThread, 0, issueHeartbeat, NULL);
	pthread_detach(heartbeatThread);

	while(1)
	{
		conn = (connection*)malloc(sizeof(connection));
		conn->sockDesc = accept(socketDesc, (struct sockaddr*)&conn->clientAddr, (socklen_t*)&conn->addrLen);
		if(0 >= conn->sockDesc)
		{
			free(conn);
			ss<<inet_ntoa(conn->clientAddr.sin_addr)<<"\n";
			log(ERROR, "UNABLE to accept client with IP "+ss.str());
		}
		else
		{
			ss<<inet_ntoa(conn->clientAddr.sin_addr)<<"\n";
			log(DEBUG, "client accepted with IP"+ss.str());
			pthread_create(&connThread, 0, processConnection, (void*)conn);
			pthread_detach(connThread);
		}
	}
}

int initializeSystem()
{
	char 			host[MAX_HOST_NAME_LENGTH];
	std::stringstream	ss;
	int			ret = 1;	

	startup = true;
	readyForRecovery = true;
	needRecovery = false;

	gethostname(host, sizeof(host));

	for(int i=0; i<MAX_NODES; i++)
	{
		if(servHostNames[i] == host)
			myId = i;

		//servSockDesc[i] = createSocket(TCP);

		ss.str(std::string());
		ss<<servHostNames[i]<<" sockDesc="<<servSockDesc[i]<<"portNum="<<servPortNums[i]<<"\n";
		log(DEBUG, "servHostName[i] = "+ss.str());
	}

	ss.str(std::string());
	ss<<"servDebugFile_"<<myId;
	debugFileName = ss.str(); 

	for(int i=0; i<MAX_NODES; i++)
	{
		if(i != myId)
		{
			if(0 > getNodeAddr(servHostNames[i].c_str(), servPortNums[i], &servAddress[i]))
			{
				log(ERROR, "getNodeAddr() FAILED \n");
				ret = -1;
			}
			else
			{
				ss.str(std::string());
				ss<<"IP of client that we are trying to connect: "<<inet_ntoa(servAddress[i].sin_addr)<<"\n";
				logToFile(DEBUG, ss.str());
			}
		}
	}

	return ret;
}

void* processConnection(void* ptr)
{
	connection*			conn;
	struct serverPkt		sPkt;
	struct helloPkt			hPkt;
	int				noBytesRead;
	string 				sendMsg;
	char*				buffer;
	char*				msgTypeToken;
	std::stringstream		ss;
	serverMsgType			sMsgType;

	if(!ptr)
		pthread_exit(0);

	conn = (connection*)ptr;
	//ss<<inet_ntoa(conn->clientAddr.sin_addr)<<"\n";
	//log(DEBUG, "connection started from IP"+ss.str());

	while(1)
	{
		ss.str(std::string());
		buffer = new char[MAX_BUFFER_SIZE];
		noBytesRead = recv(conn->sockDesc, buffer, MAX_BUFFER_SIZE, 0);
		
		if(noBytesRead == 0)
		{
			ss.str(std::string());
			ss<<"breaking out = "<<EINTR<<"\n";
			log(ERROR, ss.str());
			close(conn->sockDesc);
			break;
		}	

		if(noBytesRead>0)
		{
			buffer[noBytesRead] = '\0';
			ss<<"processConnection() - received following message "<<buffer<<" size="<<noBytesRead<<"\n";
			logToFile(DEBUG, ss.str());
			
			ss.str(std::string());
			ss<<buffer;
			msgTypeToken = strtok(const_cast<char*>(ss.str().c_str()), "~");
			sMsgType = (serverMsgType)atoi(msgTypeToken);

			if(sMsgType == SERVER_HELLO || sMsgType == SERVER_STARTUP)
			{
				hPkt = helloMsgToPacket(buffer);

				switch(hPkt.msgType)
				{
					case SERVER_HELLO:
					case SERVER_STARTUP:
						LOCK_MUTEX(dataLock);
						if(activeConnections[hPkt.serverId])
						{
							liveNodesTimer[hPkt.serverId] = getCurTimeMilliSec();
							ss.str(std::string());
							ss<<"liveNodesTimer, serverId = "<<hPkt.serverId<<" is "<<liveNodesTimer[hPkt.serverId]<<"\n";
							log(DEBUG, ss.str());
						}
						else
						{
							servSockDesc[hPkt.serverId] = createSocket(TCP);
							if(-1 < connect(servSockDesc[hPkt.serverId], (struct sockaddr*)&servAddress[hPkt.serverId], sizeof(servAddress[hPkt.serverId])))
							{	
								ss.str(std::string());
								ss<<hPkt.serverId<<"\n";
								log(DEBUG, "processConnection() - connection successful to node "+ss.str());
								activeConnections[hPkt.serverId] = true;
								liveNodesTimer[hPkt.serverId] = getCurTimeMilliSec();
							}
						}
						UNLOCK_MUTEX(dataLock);
						break;
					default:
						break;
				}
			}
			else
			{
				sPkt = servMsgToPacket(buffer);

				switch(sPkt.msgType)
				{
					case SERVER_REQ_LOCK:
						break;
					case SERVER_ACK:
						break;
					case SERVER_REQ_COMMIT:
						break;
					case SERVER_REQ_RECOVERY:
						break;
					case SERVER_RESP_RECOVERY:
						break;
					default:
						break;
				}
			}
		}
	}

	pthread_exit(0);
}

void* issueHeartbeat(void* ptr)
{
	helloPkt 		hPkt;
	string 			sendMsg;
	std::stringstream 	ss;
	long int		endTime;

	while(1)
	{
		LOCK_MUTEX(dataLock);
		for(int i=0; i<MAX_NODES; i++)
		{
			if(i != myId && !activeConnections[i])
			{
				servSockDesc[i] = createSocket(TCP);
				if(-1 < connect(servSockDesc[i], (struct sockaddr*)&servAddress[i], sizeof(servAddress[i])))
				{	
					ss.str(std::string());
					ss<<i<<"\n";
					log(DEBUG, "issueHeartbeat() - connection successful to node "+ss.str());
					activeConnections[i] = true;
					liveNodesTimer[i] = getCurTimeMilliSec();
				}
			}
		}
		UNLOCK_MUTEX(dataLock);

		if(startup)
		{
			hPkt.msgType = SERVER_STARTUP;
			startup = false;
		}
		else
		{
			hPkt.msgType = SERVER_HELLO;
		}

		LOCK_MUTEX(dataLock);
		endTime = getCurTimeMilliSec();
		for(int i=0; i<MAX_NODES; i++)
		{
			if(i != myId && activeConnections[i])
			{
				ss.str(std::string());
				ss<<"endTime = "<<endTime<<" liveNodesTimer = "<<liveNodesTimer[i]<<"\n";
				log(DEBUG, ss.str());

				if((endTime - liveNodesTimer[i]) > HELLO_INTERVAL)
				{
					ss.str(std::string());
					ss<<"issueHeartBeat() - closing connection to node "<<i<<"\n";

					logToFile(DEBUG, ss.str());
					activeConnections[i] = false;
					close(servSockDesc[i]);
					servSockDesc[i] = 0;
				}
				else
				{
					hPkt.serverId = myId;
					hPkt.storeSizeHash = storeHash.size();
					hPkt.storeSizeHashMinusOne = storeHashMinusOne.size();
					hPkt.storeSizeHashMinusTwo = storeHashMinusTwo.size();
					sendMsg = packetToMessage(&hPkt);
					sendMsg += "\0";

					ss.str(std::string());
					ss<<"issueHeartBeat() - sending hello to node="<<i<<" msg length="
						<<strlen(sendMsg.c_str())+1<<" msg="<<sendMsg<<"\n";

					logToFile(DEBUG, ss.str());
					send(servSockDesc[i], sendMsg.c_str(), strlen(sendMsg.c_str())+1, MSG_NOSIGNAL);
					if(errno == EPIPE)
					{	
						ss.str(std::string());
						ss<<"issueHeartBeat() - received EPIPE, closing connection to node "<<i<<"\n";

						logToFile(DEBUG, ss.str());
						activeConnections[i] = false;
						close(servSockDesc[i]);
						servSockDesc[i] = 0;
					}
				}
			}
		}	
		UNLOCK_MUTEX(dataLock);
		usleep(2500000);
	}

}

void* checkConnectivity(void* ptr)
{

}

string packetToMessage(struct serverPkt* servPkt)
{
	std::stringstream ss;
	ss.str(std::string());

	if(servPkt)
	{
		ss<<servPkt->msgType<<"~"
			<<servPkt->hashNum<<"~"
			<<servPkt->serverId<<"~"
			<<servPkt->version<<"~"
			<<servPkt->lock<<"~"
			<<servPkt->fileName<<"~"
			<<servPkt->data;
	}
	return ss.str();
}

string packetToMessage(struct helloPkt* hPkt)
{
	std::stringstream ss;
	ss.str(std::string());

	if(hPkt)
	{
		ss<<hPkt->msgType<<"~"
			<<hPkt->serverId<<"~"
			<<hPkt->storeSizeHash<<"~"
			<<hPkt->storeSizeHashMinusOne<<"~"
			<<hPkt->storeSizeHashMinusTwo;
	}
	return ss.str();
}

struct serverPkt servMsgToPacket(string sMsg)
{
	serverPkt sPkt;
	string token;
	string toks[7];
	std::istringstream ss(sMsg);
	int i=0;

	while(std::getline(ss, token, '~'))
	{
		toks[i++] = token;
	}
	sPkt.msgType = (serverMsgType)atoi(toks[0].c_str());
	sPkt.hashNum = atoi(toks[1].c_str());
	sPkt.serverId = atoi(toks[2].c_str());
	sPkt.version = atoi(toks[3].c_str());
	sPkt.lock = atoi(toks[4].c_str());

	memcpy(sPkt.fileName, toks[5].c_str(), MAX_FILE_NAME_LENGTH);
	memcpy(sPkt.data, toks[6].c_str(), MAX_DATA_SIZE);

	return sPkt;
}

struct helloPkt helloMsgToPacket(string hMsg)
{
	helloPkt hPkt;
	string token;
	string toks[5];
	std::istringstream ss(hMsg);
	std::stringstream sStream;

	int i=0;
	
	while(std::getline(ss, token, '~'))
	{
		toks[i++] = token;
	}
	hPkt.msgType = (serverMsgType)atoi(toks[0].c_str());
	hPkt.serverId = atoi(toks[1].c_str());
	hPkt.storeSizeHash = atoi(toks[2].c_str());
	hPkt.storeSizeHashMinusOne = atoi(toks[3].c_str());
	hPkt.storeSizeHashMinusTwo = atoi(toks[4].c_str());

	return hPkt;
}

long int getCurTimeMilliSec()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

void logToFile(logType type, string str)
{
	LOCK_MUTEX(fileLock);
	log(type, str, debugFileName.c_str());
	UNLOCK_MUTEX(fileLock);
}
