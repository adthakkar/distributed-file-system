#include"server.h"

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

	pthread_create(&readRequestThread, 0, processReadRequest, NULL);
	pthread_detach(readRequestThread);
	
	pthread_create(&writeRequestThread, 0, processWriteRequest, NULL);
	pthread_detach(writeRequestThread);
	
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

	ss.str(std::string());
	ss<<"logHash_"<<myId;
	logHash = ss.str(); 
	
	ss.str(std::string());
	ss<<"logHashMinusOne_"<<myId;
	logHashMinusOne = ss.str(); 
	
	ss.str(std::string());
	ss<<"logHashMinusTwo_"<<myId;
	logHashMinusTwo = ss.str(); 
	
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
	connection*					conn;
	struct serverPkt				sPkt;
	struct helloPkt					hPkt;
	struct clientPkt				cPkt;
	struct readRequest				rReq;
	struct writeRequest				wReq;
	int						noBytesRead;
	int						hNum;
	string 						sendMsg;
	char*						buffer;
	char*						msgTypeToken;
	std::stringstream				ss;
	clientServMsgType				sMsgType;
	std::map<string,storageLocation>::iterator	mIt;
	std::vector<serverPkt>::iterator		servIt;
	std::vector<writeRequest>::iterator		wqIt;

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
			sMsgType = (clientServMsgType)atoi(msgTypeToken);

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
			else if(sMsgType > CLIENT_SERV_NONE && sMsgType < CLIENT_SERV_RESP_SUCCESS)
			{
				cPkt = clientMsgToPacket(buffer);

				hNum = hashFileName(cPkt.fileName);
				if(!validateHash(hNum))
				{
					cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
					strncpy(cPkt.data, "Invalid File Name - validateHash() FAILED\0", MAX_DATA_SIZE);

					sendMsg = packetToMessage(&cPkt);
					ss<<"processConnection() - "<<sendMsg<<" message length="<<strlen(sendMsg.c_str())+1<<"\n";
					logToFile(ERROR, ss.str());

					sendMessage(conn->sockDesc, sendMsg.c_str(), strlen(sendMsg.c_str())+1);
				}
				else
				{
					switch(cPkt.msgType)
					{
						case CLIENT_REQ_READ:
							rReq.hashNum = hNum;
							rReq.fileName = cPkt.fileName;
							rReq.conn = conn;
							LOCK_MUTEX(dataLock);
							readQueue.push_back(rReq);
							UNLOCK_MUTEX(dataLock);
							break;
						case CLIENT_REQ_WRITE:
							wReq.hashNum = hNum;
							wReq.fileName = cPkt.fileName;
							wReq.conn = conn;
							wReq.lastMsg = CLIENT_SERV_NONE;
							wReq.serverRespTimer = getCurTimeMilliSec();
							wReq.numAck = 0;
							wReq.lockEnable = false;
							strncpy(wReq.data, cPkt.data, MAX_DATA_SIZE);
							LOCK_MUTEX(dataLock);
							mIt = directory.find(cPkt.fileName);
							if(mIt != directory.end())
							{
								servIt = findFile(cPkt.fileName, mIt->second);
					
								if(servIt != storeEndIndex(mIt->second))
								{
									servIt->lock = true;
									wReq.lockEnable = true;
								}
							}
							writeQueue.push_back(wReq);
							UNLOCK_MUTEX(dataLock);
							break;
						case CLIENT_REQ_ABORT:
							break;
						default:
							break;
					}
				}
			}
			else if(sMsgType > CLIENT_SERV_RESP_FAILURE && sMsgType < SERVER_HELLO)
			{
				sPkt = servMsgToPacket(buffer);

				switch(sPkt.msgType)
				{
					case SERVER_REQ_LOCK:
						LOCK_MUTEX(dataLock);
						mIt = directory.find(sPkt.fileName);
						if(mIt != directory.end())
						{
							servIt = findFile(sPkt.fileName, mIt->second);
					
							if(servIt != storeEndIndex(mIt->second))
							{
								servIt->lock = true;
							}
						}
						
						sPkt.msgType = SERVER_ACK;

						sendMsg = packetToMessage(&sPkt);
						ss<<"processConnection() - SERVER_ACK "<<sendMsg<<" message length="<<strlen(sendMsg.c_str())+1<<"\n";
						logToFile(DEBUG, ss.str());

						sendMessage(servSockDesc[sPkt.serverId], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
						UNLOCK_MUTEX(dataLock);
						break;
					case SERVER_ACK:
						LOCK_MUTEX(dataLock);
						wqIt = findWriteRequest(sPkt.fileName);
						if(wqIt != writeQueue.end())
						{
							wqIt->numAck += 1;
						}
						UNLOCK_MUTEX(dataLock);
						break;
					case SERVER_REQ_COMMIT:
						LOCK_MUTEX(dataLock);
						writeToFile(findStoreType(sPkt.hashNum), &sPkt);
						UNLOCK_MUTEX(dataLock);
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
				//ss.str(std::string());
				//ss<<"endTime = "<<endTime<<" liveNodesTimer = "<<liveNodesTimer[i]<<"\n";
				//log(DEBUG, ss.str());

				if((endTime - liveNodesTimer[i]) > HELLO_INTERVAL)
				{
					ss.str(std::string());
					ss<<"issueHeartBeat() - closing connection to node "<<i<<"\n";

					logToFile(DEBUG, ss.str());
					activeConnections[i] = false;
					close(servSockDesc[i]);
					servSockDesc[i] = -1;
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

						logToFile(ERROR, ss.str());
						activeConnections[i] = false;
						close(servSockDesc[i]);
						servSockDesc[i] = -1;
					}
				}
			}
		}	
		UNLOCK_MUTEX(dataLock);
		usleep(2500000);
	}

}

void* processReadRequest(void* ptr)
{
	std::vector<readRequest>::iterator 		it;
	std::vector<serverPkt>::iterator 		servIt;
	std::map<string,storageLocation>::iterator	mIt;
	struct clientPkt 				cPkt;
	string 						sendMsg;
	stringstream 					ss;

	while(1)
	{
		LOCK_MUTEX(dataLock);
		if(!readQueue.empty())
		{
			for(it = readQueue.begin(); it != readQueue.end();)
			{
				mIt = directory.find(it->fileName);
				if(mIt == directory.end())
				{
					cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
					cPkt.fileName = it->fileName;
					strncpy(cPkt.data, "DIR SEARCH FAILED - File Does Not Exist\0", MAX_DATA_SIZE);
						
					readQueue.erase(it);
				}
				else
				{
					servIt = findFile(it->fileName, mIt->second);
					
					if(servIt != storeEndIndex(mIt->second))
					{
						if(servIt->lock)
						{
							++it;
							continue;
						}
						else
						{
							cPkt.msgType = CLIENT_SERV_RESP_SUCCESS;
							cPkt.fileName = it->fileName;
							strncpy(cPkt.data, servIt->data, MAX_DATA_SIZE);	
						
							readQueue.erase(it);
						}
					}
					else
					{
						directory.erase(mIt);
						cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
						cPkt.fileName = it->fileName;
						strncpy(cPkt.data, "File Does Not Exist\0", MAX_DATA_SIZE);
						readQueue.erase(it);
					}
				}

				sendMsg = packetToMessage(&cPkt);
				ss<<"processReadRequest() - "<<sendMsg<<" message length="<<strlen(sendMsg.c_str())+1<<"\n";
				logToFile(DEBUG, ss.str());
						
				sendMessage(it->conn->sockDesc, sendMsg.c_str(), strlen(sendMsg.c_str())+1);
			}
		}
		UNLOCK_MUTEX(dataLock);
	}
}

void* processWriteRequest(void* ptr)
{
	std::vector<writeRequest>::iterator 		it;
	std::vector<serverPkt>::iterator 		servIt;
	std::map<string,storageLocation>::iterator	mIt;
	struct clientPkt 				cPkt;
	struct serverPkt				sPkt;
	string 						sendMsg;
	stringstream 					ss;
	bool						errToClient;
	long int					curTime;

	while(1)
	{
		LOCK_MUTEX(dataLock);
		if(!writeQueue.empty())
		{
			for(it=writeQueue.begin(); it!=writeQueue.end();)
			{
				errToClient = false;
				switch(it->lastMsg)
				{
					case CLIENT_SERV_NONE:
						sPkt.msgType = SERVER_REQ_LOCK;
						sPkt.hashNum = it->hashNum;
						sPkt.serverId = myId;
						sPkt.fileName = it->fileName;
						strncpy(sPkt.data, it->data, MAX_DATA_SIZE);
						
						if(activeConnections[myId+1] || activeConnections[myId+2])
						{
							sendMsg = packetToMessage(&sPkt);

							ss.str(std::string());
							ss<<"processWriteRequest() - SEVER_REQ_LOCK msg sent "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
							logToFile(DEBUG, ss.str());

							sendMessage(servSockDesc[myId+1], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
							sendMessage(servSockDesc[myId+2], sendMsg.c_str(), strlen(sendMsg.c_str())+1);

							it->numAck += 1;
							it->lastMsg = SERVER_REQ_LOCK;
							it->serverRespTimer = getCurTimeMilliSec();
							++it;
						}
						else
						{
							errToClient = true;
							strncpy(cPkt.data, "Consensus FAILED, unable to write object \0", MAX_DATA_SIZE);
						}
						break;
					case SERVER_REQ_LOCK:
						curTime = getCurTimeMilliSec();
						if((it->numAck < 2) && ((curTime - it->serverRespTimer) > SERVER_RESP_TIME_LIMIT))
						{
							errToClient = true;
							strncpy(cPkt.data,"Replica response TIMEOUT, unable to write object \0", MAX_DATA_SIZE);
						}
						else if(it->numAck >= 2)
						{
							sPkt.msgType = SERVER_REQ_COMMIT;
							sPkt.hashNum = it->hashNum;
							sPkt.serverId = myId;
							sPkt.fileName = it->fileName;
							strncpy(sPkt.data, it->data, MAX_DATA_SIZE);
							
							if(activeConnections[myId+1] || activeConnections[myId+2])
							{
								sendMsg = packetToMessage(&sPkt);

								ss.str(std::string());
								ss<<"processWriteRequest() - SEVER_REQ_COMMIT msg sent "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
								logToFile(DEBUG, ss.str());

								sendMessage(servSockDesc[myId+1], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
								sendMessage(servSockDesc[myId+2], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
								
								writeToFile(findStoreType(it->hashNum), &sPkt);
								
								cPkt.msgType = CLIENT_SERV_RESP_SUCCESS;
								cPkt.fileName = it->fileName;
								strncpy(cPkt.data, "Data SUCCESSFULLY written to the file \0", MAX_DATA_SIZE);
								
								sendMsg = packetToMessage(&cPkt);
								ss.str(std::string());
								ss<<"processWriteRequest() - CLIENT_SERV_RESP_SUCCESS msg sent "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
								logToFile(DEBUG, ss.str().c_str());

								sendMessage(it->conn->sockDesc, sendMsg.c_str(), strlen(sendMsg.c_str())+1);
								writeQueue.erase(it);
							}
							else
							{
								errToClient = true;
								strncpy(cPkt.data,"Replicas NOT AVAILABLE, unable to write object \0",MAX_DATA_SIZE);
							}
						}
						else
						{
							++it;
						}
						break;
					default:
						break;
				}

				if(errToClient && it->lockEnable)
				{	
					mIt = directory.find(it->fileName);
					if(mIt != directory.end())
					{
						servIt = findFile(it->fileName, mIt->second);
					
						if(servIt != storeEndIndex(mIt->second))
							servIt->lock = false;
						else
							directory.erase(mIt);
					}
				}
				if(errToClient)
				{
					cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
					cPkt.fileName = it->fileName;
					
					sendMsg = packetToMessage(&cPkt);
					ss.str(std::string());
					ss<<"processWriteRequest() - CLIENT_SERV_RESP_FAILURE msg sent "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
					logToFile(DEBUG, ss.str());

					sendMessage(it->conn->sockDesc, sendMsg.c_str(), strlen(sendMsg.c_str())+1);
					writeQueue.erase(it);
				}
			}
		}
		UNLOCK_MUTEX(dataLock);
	}
}

void writeToFile(storageLocation storeType, struct serverPkt* sPkt)
{
	std::vector<serverPkt>::iterator 	servIt;
	struct serverPkt*			pkt = NULL;				
	fstream					oFile;
	std::stringstream			ss;

	servIt = findFile(sPkt->fileName, storeType);
				
	if(servIt != storeEndIndex(storeType))
	{
		servIt->version += 1;
		strncpy(servIt->data,sPkt->data,MAX_DATA_SIZE);
		servIt->lock = false;
	}
	else
	{
		pkt = sPkt;
		pkt->msgType = CLIENT_SERV_NONE;
		pkt->version = 1;
		pkt->lock = false;
	}

	switch(storeType)
	{
		case STORE_HASH:
			if(pkt)
				storeHash.push_back(*pkt);
			oFile.open(logHash.c_str(), ios::out | ios::app);
			break;
		case STORE_HASH_MINUS_ONE:
			if(pkt)
				storeHashMinusOne.push_back(*pkt);
			oFile.open(logHashMinusOne.c_str(), ios::out | ios::app);
			break;
		case STORE_HASH_MINUS_TWO:
			if(pkt)
				storeHashMinusTwo.push_back(*pkt);
			oFile.open(logHashMinusTwo.c_str(), ios::out | ios::app);
			break;
		default:
			break;
	}
	
	directory.insert(std::pair<string,storageLocation>(sPkt->fileName, storeType));
	ss<<sPkt->fileName<<"\t"<<sPkt->version<<"\t"<<sPkt->data<<"\n";
	if(oFile.is_open())
	{
		oFile<<ss.str();
		oFile.close();
	}

}

storageLocation findStoreType(int hNum)
{
	if(hNum == myId)
		return STORE_HASH;
	else if(hNum + 1 == myId)
		return STORE_HASH_MINUS_ONE;
	else if(hNum + 2 == myId)
		return STORE_HASH_MINUS_TWO;
	
	return STORE_NONE;
}

std::vector<writeRequest>::iterator findWriteRequest(string fileName)
{
	std::vector<writeRequest>::iterator		retIt = writeQueue.end();
	std::vector<writeRequest>::iterator		it;

	for(it = writeQueue.begin(); it != writeQueue.end(); ++it)
	{
		if(it->fileName == fileName)
		{
			retIt = it;
			break;
		}
	}
	return retIt;
}

std::vector<serverPkt>::iterator findFile(string fileName, storageLocation storeType)
{
	std::vector<serverPkt>::iterator 		retIt;
	std::vector<serverPkt>::iterator 		startIt;
	std::vector<serverPkt>::iterator 		endIt;

	switch(storeType)
	{
		case STORE_HASH:
			startIt = storeHash.begin();
			endIt = storeHash.end();
			retIt = storeHash.end();
			break;
		case STORE_HASH_MINUS_ONE:
			startIt = storeHashMinusOne.begin();
			endIt = storeHashMinusTwo.end();
			retIt = storeHashMinusOne.end();
			break;
		case STORE_HASH_MINUS_TWO:
			startIt = storeHashMinusOne.begin();
			endIt = storeHashMinusTwo.end();
			retIt = storeHashMinusTwo.end();
			break;
		default:
			break;
	}

	for(startIt; startIt != endIt; ++startIt)
	{
		if(startIt->fileName == fileName)
		{
			retIt = startIt;
			break;
		}
	}
	return retIt;
	
}

int sendMessage(int sockDesc, const char* msg, int msgLen)
{
	int 			ret = -1;
	std::stringstream 	ss;

	if(sockDesc >= 0 )
	{
		ret = send(sockDesc, msg, msgLen, MSG_NOSIGNAL);
		if(errno == EPIPE)
		{
			ss.str(std::string());
			ss<<"sendMessage() - unable to send message \n";
			logToFile(ERROR, ss.str());
		}
	}
	return ret;
}

int validateHash(int hashNum)
{
	return hashNum == myId;
	return ((hashNum+1)%MAX_NODES) == myId;
	return ((hashNum+2)%MAX_NODES) == myId;
	return 0;
}

std::vector<serverPkt>::iterator storeEndIndex(storageLocation storeType)
{
	std::vector<serverPkt>::iterator 		retIt;

	switch(storeType)
	{
		case STORE_HASH:
			retIt = storeHash.end();
			break;
		case STORE_HASH_MINUS_ONE:
			retIt = storeHashMinusOne.end();
			break;
		case STORE_HASH_MINUS_TWO:
			retIt = storeHashMinusTwo.end();
			break;
		default:
			break;
	}

	return retIt;
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
	sPkt.msgType = (clientServMsgType)atoi(toks[0].c_str());
	sPkt.hashNum = atoi(toks[1].c_str());
	sPkt.serverId = atoi(toks[2].c_str());
	sPkt.version = atoi(toks[3].c_str());
	sPkt.lock = atoi(toks[4].c_str());
	sPkt.fileName = toks[5];

	//memcpy(sPkt.fileName, toks[5].c_str(), MAX_FILE_NAME_LENGTH);
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
	hPkt.msgType = (clientServMsgType)atoi(toks[0].c_str());
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
