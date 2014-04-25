#ifndef LOGGER_H
#define LOGGER_H

#include<iostream>
#include<fstream>
#include<sstream>

using namespace std;

/**********************************
 * GLOBAL Directives
 **********************************/
static bool printToConsole = false;

/**********************************
 * ENUM Declaration
 **********************************/
typedef enum
{
	DEBUG,
	ERROR
}logType;

/**********************************
 * FUNCTION Declaration
 **********************************/
void log(logType logType, string strToPrint);
void log(logType type, string str, const char* fileName);

#endif
