#include"util.h"

using namespace std;

string packetToMessage(struct clientPkt* cPkt)
{
	std::stringstream ss;
	ss.str(std::string());

	if(cPkt)
	{
		ss<<cPkt->msgType<<"~"
			<<cPkt->fileName<<"~"
			<<cPkt->data<<"~";
	}
	return ss.str();
}

struct clientPkt clientMsgToPacket(string sMsg)
{
	struct clientPkt cPkt;
	string token;
	string toks[3];
	std::istringstream ss(sMsg);
	int i=0;

	while(std::getline(ss, token, '~'))
	{
		toks[i++] = token;
	}
	cPkt.msgType = (clientServMsgType)atoi(toks[0].c_str());
	cPkt.fileName = toks[1];
	
	//memcpy(cPkt.fileName, toks[1].c_str(), MAX_FILE_NAME_LENGTH);
	memcpy(cPkt.data, toks[2].c_str(), MAX_DATA_SIZE);

	return cPkt;
}

int hashFileName(string str)
{
	unsigned hash = 31;

	for(int i = 0; i<str.length(); i++)
	{
		hash = (hash * HASH_PRIME_SEED_A) ^ (str[i] * HASH_PRIME_SEED_B);
	}
	return hash % MAX_NODES;
}

long int getCurTimeMilliSec()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

