#ident "%W% DWM:badIniFile delta: %G% %U% get: %H% %T%"

#include <iostream>

#include "badIniFile.h"

badIniFile::badIniFile(const char *sMsg)
{
	sErrMsg = std::string(sMsg);
}
		
badIniFile::~badIniFile()
{
}

const char * badIniFile::what() const throw()
{   
	return(sErrMsg.c_str());
}   

void badIniFile::setErrMsg(std::string sMsg)
{   
	sErrMsg = sMsg;
	sErrMsg += '\n';
}   

void badIniFile::setErrMsg(const char *sMsg)
{   
	sErrMsg = std::string(sMsg);
	sErrMsg += '\n';
}   

