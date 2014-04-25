#include"logger.h"

void log(logType logType, string strToPrint)
{
	switch(logType)
	{
		case DEBUG:
			cout<<"DEBUG - "+strToPrint;
			break;
		case ERROR:
			cout<<"ERROR - "+strToPrint;
			break;
		default:
			cout<<"log(): INVALID log type \n";
	}
}

void log(logType type, string str, const char* fileName)
{
	fstream oFile;
	
	if(printToConsole)
		log(type, str);
	
	oFile.open(fileName, ios::out | ios::app);
	if(oFile.is_open())
	{
		oFile<<str;
		oFile.close();
	}
}
