#include"client.h"

int main(int argc, char** argv)
{
	struct clientPkt 	cPkt;
	int			ret;

	if(0 > initializeSystem())
		return -1;

	getUserInput(&cPkt);
		
	ret = sendRequestToServer(&cPkt);	
	
	if(ret < 0)
	{
		log(ERROR, "sendRequestToServer() FAILED \n");
	}

	return 0;
}

int initializeSystem()
{
	std::stringstream	ss;
	int			ret = 1;	

	for(int i=0; i<MAX_NODES; i++)
	{
		if(0 > getNodeAddr(servHostNames[i].c_str(), servPortNums[i], &servAddress[i]))
		{
			log(ERROR, "getNodeAddr() FAILED \n");
			ret = -1;
		}
		else
		{
			ss.str(std::string());
			ss<<"IP of server "<<i<<" is "<<inet_ntoa(servAddress[i].sin_addr)<<"\n";
			log(DEBUG, ss.str());
		}
	}

	return ret;
}

void getUserInput(clientPkt* cPkt)
{
	string input;

	cout<<"Enter r for read request and w for write request \n";
	cout<<"Enter request type:";
	cin>>input;

	if(input == "r")
		cPkt->msgType = CLIENT_REQ_READ;
	else
		cPkt->msgType = CLIENT_REQ_WRITE;

	cout<<"\n Enter file name:";
	cin>>cPkt->fileName;

	if(cPkt->msgType == CLIENT_REQ_WRITE)
	{
		cout<<"\n Enter data to be written:";
		cin>>input;
		strncpy(cPkt->data, input.c_str(), MAX_DATA_SIZE -1);
		cPkt->data[MAX_DATA_SIZE-1] = '\0';
	}
	else
	{
		strncpy(cPkt->data, "\0", MAX_DATA_SIZE);
	}
	cout<<endl;
	return;
}

int sendRequestToServer(struct clientPkt* cPkt)
{
	int		 	sockDesc;
	int		 	hash;
	int 			serverToConn;
	std::stringstream	ss;
	string			sendMsg;
	long int		timer;
	char*			buffer;
	int			noBytesRead;
	struct clientPkt	clientRet;

	if(cPkt)
	{
		hash = hashFileName(cPkt->fileName);
		serverToConn = hash;
		sockDesc = createSocket(TCP);

		if(sockDesc < 0)
			return -1;

	
		if(-1 < connect(sockDesc, (struct sockaddr*)&servAddress[serverToConn], sizeof(servAddress[serverToConn])))
		{
			ss.str(std::string());
			ss<<"sendRequestToServer() - Cannot connect to hash server with Id "<<serverToConn<<"\n";
			log(ERROR, ss.str());
			
			serverToConn = (hash + 1) % 7;
			if(-1 < connect(sockDesc, (struct sockaddr*)&servAddress[serverToConn], sizeof(servAddress[serverToConn])))
			{
				ss<<"sendRequestToServer() - Cannot connect to hash+1 server with Id "<<serverToConn<<"\n";
				log(ERROR, ss.str());
				return -1;
			}
			ss.str(std::string());
			ss<<"sendRequestToServer() - connected to hash+1 server with Id "<<serverToConn<<"\n";
			log(DEBUG, ss.str());

		}	
		ss.str(std::string());
		ss<<"sendRequestToServer() - connected to hash server with Id "<<serverToConn<<"\n";
		log(DEBUG, ss.str());
		
		sendMsg = packetToMessage(cPkt);
		ss.str(std::string());
		ss<<"sendRequestToServer() sending message - "<<sendMsg<<" msgLen = "<<strlen(sendMsg.c_str())+1<<"\n";
		log(DEBUG, ss.str());
		
		timer = getCurTimeMilliSec();
		send(sockDesc, sendMsg.c_str(), strlen(sendMsg.c_str())+1, 0);

		while(1)
		{
			ss.str(std::string());
			if((getCurTimeMilliSec() - timer) > CLIENT_RESP_TIME_LIMIT)
			{
				ss<<"sendRequestToServer() - Waiting for server response, TIMEOUT occured \n";
				log(ERROR, ss.str());
				close(sockDesc);
				sockDesc = -1;
				break;
			}

			ss.str(std::string());
			buffer = new char[MAX_BUFFER_SIZE];
			noBytesRead = recv(sockDesc, buffer, MAX_BUFFER_SIZE, 0);

			if(noBytesRead == 0)
			{
				ss<<"sendRequestToServer() breaking out EINTR = "<<EINTR<<"\n";
				log(ERROR, ss.str());
				close(sockDesc);
				sockDesc = -1;
				break;
			}	

			if(noBytesRead>0)
			{
				buffer[noBytesRead] = '\0';
				ss<<"sendRequestToServer() response received from server - "<<buffer<<"\n";	
				clientRet = clientMsgToPacket(buffer);
				cPkt->msgType = clientRet.msgType;
				cPkt->fileName = clientRet.fileName;
				strncpy(cPkt->data, clientRet.data, MAX_DATA_SIZE);
				break;
			}
		}
	}
	return sockDesc;
}
