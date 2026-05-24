#ident "%W% DWM:include delta: %G% %U% get: %H% %T%"

#ifndef _BADCMDLINE_H_
#define _BADCMDLINE_H_

#include <stdexcept>

class badCmdLine
{
	public:

		badCmdLine(const char *sMsg);
		~badCmdLine();

		const char* what() const throw();

	private:
	
		std::string sErrMsg;

		void setErrMsg(std::string sMsg);
		void setErrMsg(const char *sMsg);
};

#endif
