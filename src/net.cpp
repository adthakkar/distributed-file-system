#include"net.h"

int createSocket(socketType type)
{
	int sDesc = -1;

	switch(type)
	{
		case TCP:
			sDesc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			break;
		default:
			log(ERROR, "createSocket() - INVALID socket type \n");
	}

	if(sDesc <=0)
		log(ERROR, "createSocket() - CANNOT create socket \n");
	
	return sDesc;
}

int startServer(socketType type, int portNum)
{
	int 			sockDesc = -1;
	sockaddr_in		servAdd;

	sockDesc = createSocket(TCP);
	
	if(0 >= sockDesc)
	{
		log(ERROR,"startServer() - createSocket() FAILED \n");
		return -1;
	}
	if(0 >= getNodeAddr(portNum, &servAdd))
	{
		log(ERROR, "startServer() - getNodeAddr() FAILED \n");
		return -1;
	}
	if(0 > bind(sockDesc, (struct sockaddr*)&servAdd, sizeof(sockaddr_in)))
	{
		log(ERROR, "startServer() - bind() to server socket FAILED \n");
		return -1;
	}
	if(0 > listen(sockDesc, MAX_CONNECTIONS))
	{
		log(ERROR, "startServer() - UNABLE to listen to server socket \n");
		return -1;
	}
	return sockDesc;
}

int getNodeAddr(const char* hostName, int portNum, struct sockaddr_in* addr)
{
	hostent* host;
	std::stringstream ss;

	if(addr != NULL)
	{
		host = gethostbyname(hostName);
		if(!host)
		{
			ss<<hostName;
			log(ERROR, "getNodeAddr() - host= "+ss.str()+" NOT FOUND \n");
			return -1;
		}

		memcpy(&addr->sin_addr, host->h_addr_list[0], host->h_length);
		addr->sin_family = AF_INET;
		addr->sin_port = htons(portNum);

		ss<<hostName<<" has IP="<<inet_ntoa(addr->sin_addr)<<"\n";
		log(DEBUG,"getNodeAddr() - host= "+ss.str());

		return 1;
	}
	return -1;
}

int getNodeAddr(int port, struct sockaddr_in* address)
{
	if(address != NULL)
	{
		address->sin_family = AF_INET;
		address->sin_port = htons(port);
		(address->sin_addr).s_addr = INADDR_ANY;

		return 1;
	}

	return -1;
}

