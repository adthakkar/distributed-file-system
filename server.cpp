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

	pthread_create(&partitionSimThread, 0, partitionSimulator, NULL);
	pthread_detach(partitionSimThread);
	
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
			ss<<inet_ntoa(conn->clientAddr.sin_addr)<<" sockDesc = "<<conn->sockDesc<<"\n";
			log(DEBUG, "client accepted with IP "+ss.str());
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
	
	numMsgSent = 0;
	numMsgReceived = 0;

	gethostname(host, sizeof(host));

	for(int i=0; i<MAX_NODES; i++)
	{
		if(servHostNames[i] == host)
			myId = i;
		
		//servSockDesc[i] = createSocket(TCP);

		//ss.str(std::string());
		//ss<<servHostNames[i]<<" sockDesc = "<<servSockDesc[i]<<" portNum = "<<servPortNums[i]<<"\n";
		//log(DEBUG, "servHostName[i] = "+ss.str());
	}

	ss.str(std::string());
	ss<<"servDebugFile_"<<myId;
	debugFileName = ss.str(); 

	ss.str(std::string());
	ss<<"heartbeatLog_"<<myId;
	heartbeatLog = ss.str(); 
	
	ss.str(std::string());
	ss<<"logHash_"<<myId;
	logHash = ss.str(); 
	
	ss.str(std::string());
	ss<<"logHashMinusOne_"<<myId;
	logHashMinusOne = ss.str(); 
	
	ss.str(std::string());
	ss<<"logHashMinusTwo_"<<myId;
	logHashMinusTwo = ss.str(); 

	partitionSimFile = "partitionSimulator";

	for(int i=0; i<MAX_NODES; i++)
	{
		if(i != myId)
		{
			allowedConnections[i] = true;
			if(0 > getNodeAddr(servHostNames[i].c_str(), servPortNums[i], &servAddress[i]))
			{
				log(ERROR, "getNodeAddr() FAILED \n");
				ret = -1;
			}
			else
			{
				ss.str(std::string());
				ss<<"IP of client that we are trying to connect: "<<inet_ntoa(servAddress[i].sin_addr)<<"\n";
				logToFile(DEBUG, ss.str(), debugFileName);
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
	int						id;
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

			ss.str(std::string());
			ss<<buffer;
			msgTypeToken = strtok(const_cast<char*>(ss.str().c_str()), "~");
			sMsgType = (clientServMsgType)atoi(msgTypeToken);

			if(sMsgType == SERVER_HELLO || sMsgType == SERVER_STARTUP)
			{
				ss.str(std::string());
				ss<<"processConnection() - received following message "<<buffer<<" size="<<noBytesRead<<"\n";
				logToFile(DEBUG, ss.str(), heartbeatLog);
				hPkt = helloMsgToPacket(buffer);

				switch(hPkt.msgType)
				{
					case SERVER_HELLO:
					case SERVER_STARTUP:	
						LOCK_MUTEX(dataLock);
						if(activeConnections[hPkt.serverId])
						{
							liveNodesTimer[hPkt.serverId] = getCurTimeMilliSec();
							
							//ss.str(std::string());
							//ss<<"liveNodesTimer, serverId = "<<hPkt.serverId<<" is "<<liveNodesTimer[hPkt.serverId]<<"\n";
							//log(DEBUG, ss.str());
						}
						else if(allowedConnections[hPkt.serverId])
						{
							servSockDesc[hPkt.serverId] = createSocket(TCP);
							if(-1 < connect(servSockDesc[hPkt.serverId], (struct sockaddr*)&servAddress[hPkt.serverId], sizeof(servAddress[hPkt.serverId])))
							{	
								ss.str(std::string());
								ss<<"processConnection() - connection successful to node "<<hPkt.serverId<<" sockDesc "<<servSockDesc[hPkt.serverId]<<"\n";
								log(DEBUG, ss.str());
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
				ss.str(std::string());
				ss<<"processConnection() - received following message "<<buffer<<" size="<<noBytesRead<<"\n";
				logToFile(DEBUG, ss.str(), debugFileName);
				cPkt = clientMsgToPacket(buffer);

				hNum = hashFileName(cPkt.fileName);
				if(!validateHash(hNum))
				{
					cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
					strncpy(cPkt.data, "Invalid File Name - validateHash() FAILED\0", MAX_DATA_SIZE);
					
					sendToClient(conn->sockDesc, &cPkt);
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
							LOCK_MUTEX(dataLock);
							if(hNum != myId && activeConnections[hNum])
							{
								cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
								strncpy(cPkt.data, "Send request to leader \0", MAX_DATA_SIZE);
								
								sendToClient(conn->sockDesc, &cPkt);
							}
							else
							{
								wReq.hashNum = hNum;
								wReq.fileName = cPkt.fileName;
								wReq.conn = conn;
								wReq.lastMsg = CLIENT_SERV_NONE;
								wReq.leaderId = myId;
								wReq.lockEnable = false;
								strncpy(wReq.data, cPkt.data, MAX_DATA_SIZE);
								writeQueue.push_back(wReq);
							}
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
				ss.str(std::string());
				ss<<"processConnection() - received following message "<<buffer<<" size="<<noBytesRead<<"\n";
				logToFile(DEBUG, ss.str(), debugFileName);
				sPkt = servMsgToPacket(buffer);

				switch(sPkt.msgType)
				{
					case SERVER_REQ_LOCK:
						LOCK_MUTEX(dataLock);
						if(validateHash(sPkt.hashNum))
						{
							wReq.hashNum = hNum;
							wReq.fileName = sPkt.fileName;
							wReq.conn = conn;
							wReq.leaderId = -1;
							wReq.lastMsg = SERVER_REQ_LOCK;
							wReq.lockEnable = true;
							wReq.serverRespTimer = getCurTimeMilliSec();
							strncpy(wReq.data, sPkt.data, MAX_DATA_SIZE);

							searchAndLock(sPkt.fileName, true);
							writeQueue.push_back(wReq);
								
							id = sPkt.serverId;
							sPkt.serverId = myId;
							sPkt.msgType = SERVER_ACK;

							sendMsg = packetToMessage(&sPkt);
							ss.str(std::string());
							ss<<"processConnection() - SERVER_ACK "<<sendMsg<<" message length="<<strlen(sendMsg.c_str())+1<<"\n";
							logToFile(DEBUG, ss.str(), debugFileName);
							
							sendMessage(servSockDesc[id], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
							
							numMsgSent++;
							numMsgReceived++;
						}
						UNLOCK_MUTEX(dataLock);
						break;
					case SERVER_ACK:
						LOCK_MUTEX(dataLock);
						wqIt = findWriteRequest(sPkt.fileName);
						if(wqIt != writeQueue.end())
						{
							wqIt->numAck += 1;
						}
						numMsgReceived++;
						UNLOCK_MUTEX(dataLock);
						break;
					case SERVER_REQ_COMMIT:
						LOCK_MUTEX(dataLock);
						if(validateHash(sPkt.hashNum))
						{
							numMsgReceived++;
							
							writeToFile(findStoreType(sPkt.hashNum), &sPkt);
							wqIt = findWriteRequest(sPkt.fileName);
						
							if(wqIt != writeQueue.end())
							{
								writeQueue.erase(wqIt);
							}

						}
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
			if(i != myId && !activeConnections[i] && allowedConnections[i])
			{
				servSockDesc[i] = createSocket(TCP);
				if(-1 < connect(servSockDesc[i], (struct sockaddr*)&servAddress[i], sizeof(servAddress[i])))
				{	
					ss.str(std::string());
					ss<<"issueHeartbeat() - connection successful to node "<<i<<" sockDesc "<<servSockDesc[i]<<"\n";
					log(DEBUG, ss.str());
					activeConnections[i] = true;
					liveNodesTimer[i] = getCurTimeMilliSec();
				}
			}
		}

		if(startup)
		{
			hPkt.msgType = SERVER_STARTUP;
			startup = false;
		}
		else
		{
			hPkt.msgType = SERVER_HELLO;
		}

		endTime = getCurTimeMilliSec();
		for(int i=0; i<MAX_NODES; i++)
		{
			if(i != myId && activeConnections[i])
			{
				if((endTime - liveNodesTimer[i]) > HELLO_INTERVAL)
				{
					ss.str(std::string());
					ss<<"issueHeartBeat() - closing connection to node "<<i<<"\n";

					logToFile(DEBUG, ss.str(), heartbeatLog);
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
						<<strlen(sendMsg.c_str())+1<<" msg="<<sendMsg<<" sockDesc "
						<<servSockDesc[i]<<"\n";

					logToFile(DEBUG, ss.str(), heartbeatLog);
					send(servSockDesc[i], sendMsg.c_str(), strlen(sendMsg.c_str())+1, MSG_NOSIGNAL);
					if(errno == EPIPE)
					{	
						ss.str(std::string());
						ss<<"issueHeartBeat() - received EPIPE, closing connection to node "<<i<<"\n";

						logToFile(ERROR, ss.str(), heartbeatLog);
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
				sendToClient(it->conn->sockDesc, &cPkt);
			}
		}
		UNLOCK_MUTEX(dataLock);
	}
}

void* processWriteRequest(void* ptr)
{
	std::vector<writeRequest>::iterator 		it;
	std::vector<writeRequest>::iterator 		wqIt;
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
						sPkt.lock = true; //to avoid garbage
						sPkt.version = 1; //to avoid garbage
						strncpy(sPkt.data, it->data, MAX_DATA_SIZE);
						
						if(it->leaderId == myId)
						{
							wqIt = findWriteRequest(it->fileName);
							if((wqIt != writeQueue.end() && !wqIt->lockEnable) || 
							   (wqIt == writeQueue.end()))
							{
								if(sendToReplica(it->leaderId, it->hashNum, &sPkt))
								{
									searchAndLock(it->fileName, true);
									it->numAck += 1;
									it->lastMsg = SERVER_REQ_LOCK;
									it->lockEnable = true;
									it->serverRespTimer = getCurTimeMilliSec();
									++it;
								}
								else
								{
									errToClient = true;
									strncpy(cPkt.data, "Consensus FAILED, unable to write object \0", MAX_DATA_SIZE);
								}	
							}
							else
							{
								++it;
								continue;
							}
						}
						break;
					case SERVER_REQ_LOCK:
						curTime = getCurTimeMilliSec();
						if(it->leaderId == myId)
						{
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
							
								if(sendToReplica(it->leaderId, it->hashNum, &sPkt))
								{
									writeToFile(findStoreType(it->hashNum), &sPkt);
								
									cPkt.msgType = CLIENT_SERV_RESP_SUCCESS;
									cPkt.fileName = it->fileName;
									strncpy(cPkt.data, "Data SUCCESSFULLY written to the file \0", MAX_DATA_SIZE);
									sendToClient(it->conn->sockDesc, &cPkt);

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
						}
						else if(it->leaderId != myId)
						{
							if((curTime - it->serverRespTimer) > SERVER_RESP_TIME_LIMIT)
							{
								searchAndLock(it->fileName, false);
								writeQueue.erase(it);
							}
							else
							{
								++it;
							}
						}
						break;
					default:
						break;
				}

				if(errToClient)
				{	
					searchAndLock(it->fileName, false);
					cPkt.msgType = CLIENT_SERV_RESP_FAILURE;
					cPkt.fileName = it->fileName;
					sendToClient(it->conn->sockDesc, &cPkt);
					writeQueue.erase(it);
				}
			}
		}

		UNLOCK_MUTEX(dataLock);
	}
}

void* partitionSimulator(void* ptr)
{
	ifstream			iStream;
	int				globalCount = 0;
	int				localCount;
	string				line;
	int				mId, nodeId, action;
	std::istringstream		iss;
	std::stringstream		ss;

	while(1)
	{
		if(!iStream.is_open())
		{
			iStream.open(partitionSimFile.c_str(), ios::in);
		}

		if(iStream.good())
		{
			localCount = 0;
			while(std::getline(iStream, line))
			{
				localCount++;
				if(localCount <= globalCount)
				{
					continue;
				}
				else
				{
					iss.str(line);
					globalCount++;

					if(iss >> mId >> nodeId >> action)
					{
						if(mId != myId)
						{
							continue;
						}
						else
						{
							LOCK_MUTEX(dataLock);
							if((action == 0) && (activeConnections[nodeId]))
							{
								ss.str(std::string());
								ss<<"partitionSimulator() - Closing conection to node "<<nodeId<<" sockDesc = "<<servSockDesc[nodeId]<<"\n";
								log(DEBUG, ss.str());
								
								//close conection
								close(servSockDesc[nodeId]);
								servSockDesc[nodeId] = -1;
								activeConnections[nodeId] = false;
								allowedConnections[nodeId] = false;
							}
							else if((action == 1) && (!activeConnections[nodeId]))
							{
								servSockDesc[nodeId] = createSocket(TCP);
								if(-1 < connect(servSockDesc[nodeId], (struct sockaddr*)&servAddress[nodeId], sizeof(servAddress[nodeId])))
								{	
									ss.str(std::string());
									ss<<"partitionSimulator() - connection successful to node "<<nodeId<<" sockDesc "<<servSockDesc[nodeId]<<"\n";
									log(DEBUG, ss.str());
									activeConnections[nodeId] = true;
									liveNodesTimer[nodeId] = getCurTimeMilliSec();
								}
								
							}
							UNLOCK_MUTEX(dataLock);
						}
					}
				}
			}
			iStream.close();
		}
	}
}

bool sendToReplica(int leader, int hash, serverPkt* sPkt)
{
	std::stringstream			ss;
	string					sendMsg;
	bool					ret = false;
	int					replica1;
	int					replica2;

	if(leader == myId)
	{
		sendMsg = packetToMessage(sPkt);

		ss.str(std::string());
		ss<<"sendToReplica() attempting to send message -  "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
		logToFile(DEBUG, ss.str(), debugFileName);
		
		replica1 = (myId + 1) % MAX_NODES;
		replica2 = (myId + 2) % MAX_NODES;
		if((hash == leader) &&  ((activeConnections[replica1])||(activeConnections[replica2])))
		{
			numMsgSent += 2;
			sendMessage(servSockDesc[replica1], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
			sendMessage(servSockDesc[replica2], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
			ret = true;
		}
		else if((hash+1 == leader) && (activeConnections[replica1]))
		{
			numMsgSent +=1;
			sendMessage(servSockDesc[replica1], sendMsg.c_str(), strlen(sendMsg.c_str())+1);
			ret = true;
		}
		else
		{			
			ss.str(std::string());
			ss<<"sendToReplica() FAILED to send message -  "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
			logToFile(DEBUG, ss.str(), debugFileName);
		}
	}

	return ret;
	
}

void sendToClient(int sockDesc, clientPkt* cPkt)
{
	std::stringstream			ss;
	string					sendMsg;
	
	sendMsg = packetToMessage(cPkt);
	ss.str(std::string());
	ss<<"sendToClient() sending message - "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
	logToFile(DEBUG, ss.str(), debugFileName);

	sendMessage(sockDesc, sendMsg.c_str(), strlen(sendMsg.c_str())+1);
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
		servIt->version = servIt->version + 1;
		strncpy(servIt->data,sPkt->data,MAX_DATA_SIZE);
		servIt->lock = false;
		
		ss<<myId<<"\t"<<servIt->fileName<<"\t"<<servIt->version<<"\t"<<servIt->data<<"\t"<<numMsgSent<<"\t"<<numMsgReceived<<"\n";
		log(DEBUG, "writeToFile() "+ss.str());
		logToFile(DEBUG, "writeToFile() "+ss.str(), debugFileName);
	}
	else
	{
		pkt = sPkt;
		pkt->msgType = CLIENT_SERV_NONE;
		pkt->version = 1;
		pkt->lock = false;
		
		ss<<myId<<"\t"<<pkt->fileName<<"\t"<<pkt->version<<"\t"<<pkt->data<<"\t"<<numMsgSent<<"\t"<<numMsgReceived<<"\n";
		log(DEBUG, "writeToFile() "+ss.str());
		logToFile(DEBUG, "writeToFile() "+ss.str(), debugFileName);
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
	
	if(oFile.is_open())
	{
		oFile<<ss.str();
		oFile.close();
	}
	numMsgSent = 0;
	numMsgReceived = 0;

}

void searchAndLock(string fileName, bool lockValue)
{
	std::map<string,storageLocation>::iterator	mIt;
	std::vector<serverPkt>::iterator		servIt;

	mIt = directory.find(fileName);
	if(mIt != directory.end())
	{
		servIt = findFile(fileName, mIt->second);
				
		if(servIt != storeEndIndex(mIt->second))
		{
			servIt->lock = lockValue;
		}
	}
	return;
}

storageLocation findStoreType(int hNum)
{
	if(hNum == myId)
		return STORE_HASH;
	else if(((hNum + 1)%7) == myId)
		return STORE_HASH_MINUS_ONE;
	else if(((hNum + 2)%7) == myId)
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
			endIt = storeHashMinusOne.end();
			retIt = storeHashMinusOne.end();
			break;
		case STORE_HASH_MINUS_TWO:
			startIt = storeHashMinusTwo.begin();
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
		ss<<"sendMessage() - sending to sockDesc "<<sockDesc<<" following message: "<<msg<<"\n";
		logToFile(DEBUG, ss.str(), debugFileName);

		ret = send(sockDesc, msg, msgLen, MSG_NOSIGNAL);
		if(errno == EPIPE)
		{
			ss.str(std::string());
			ss<<"sendMessage() - unable to send message \n";
			logToFile(ERROR, ss.str(), debugFileName);
		}
	}
	return ret;
}

bool validateHash(int hashNum)
{
	bool		ret = false;
	
	if(hashNum == myId)
		ret = true;
	else if(((hashNum+1)%MAX_NODES) == myId)
		ret = true;
	else if(((hashNum+2)%MAX_NODES) == myId)
		ret = true;
	
	return ret;
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

string packetToMessage(struct serverPkt* sPkt)
{
	std::stringstream ss;
	ss.str(std::string());

	if(sPkt)
	{
		ss<<sPkt->msgType<<"~"
			<<sPkt->hashNum<<"~"
			<<sPkt->serverId<<"~"
			<<sPkt->version<<"~"
			<<sPkt->lock<<"~"
			<<sPkt->fileName<<"~"
			<<sPkt->data;
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

void logToFile(logType type, string str, string fileName)
{
	LOCK_MUTEX(fileLock);
	log(type, str, fileName.c_str());
	UNLOCK_MUTEX(fileLock);
}
