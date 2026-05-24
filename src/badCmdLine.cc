#ident "%W% DWM:badCmdLine delta: %G% %U% get: %H% %T%"

#include <iostream>

#include "badCmdLine.h"

badCmdLine::badCmdLine(const char *sMsg)
{
	sErrMsg = std::string(sMsg);
}
		
badCmdLine::~badCmdLine()
{
}

const char * badCmdLine::what() const throw()
{   
	return(sErrMsg.c_str());
}   

void badCmdLine::setErrMsg(std::string sMsg)
{   
	sErrMsg = sMsg;
	sErrMsg += '\n';
}   

void badCmdLine::setErrMsg(const char *sMsg)
{   
	sErrMsg = std::string(sMsg);
	sErrMsg += '\n';
}   

