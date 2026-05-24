#ident "%W% DWM:include delta: %G% %U% get: %H% %T%"

#ifndef _BADINIFILE_H_
#define _BADINIFILE_H_

#include <stdexcept>

class badIniFile
{
	public:

		badIniFile(const char *sMsg);
		~badIniFile();

		const char* what() const throw();

	private:
	
		std::string sErrMsg;

		void setErrMsg(std::string sMsg);
		void setErrMsg(const char *sMsg);
};

#endif
