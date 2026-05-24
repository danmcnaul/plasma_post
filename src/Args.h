#ident "%W% DWM:include delta: %G% %U% get: %H% %T%"

#ifndef _ARGS_H_
#define _ARGS_H_

#include <stdexcept>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <iterator>

#include "badCmdLine.h"

typedef struct
{
	bool bIsError;
	bool bShouldHaveOpt;
	bool bHasOpt;
	std::string sPattern;
	std::string sFlag;
	std::string sValue;
	std::string sAsIs;
} CLO;

#define AS_THEY_OCCUR	0
#define AFTER_LOAD		1
#define I_WILL_HANDLE	2

class Args
{
	public:

		Args()
		{
		};
		Args(int nArgCnt, char*const* argv, const char *cPattern, const struct option long_options[]);
		Args(const Args &old);
		Args operator= (const Args &old);
	
		
		~Args();

		bool isSet(std::string sFlag);
		std::vector<CLO *> getValues(std::string sFlag);
		std::vector<std::string> getNonOptArgs(void);
		
	protected:
		

	private:

		// A map of Command Line Options
		// 	key = the option flag example '-Q'
		// 	val = a vector of Command Line Options (CLO) in case > 1
		std::map<std::string, std::vector<CLO *> > optMap;
		std::map<std::string, std::vector<CLO *> >::iterator iterOptMap;

		// A vector of Command Line Options (CLO) in case > 1 is possible
		std::vector<CLO *> vectCLOs;
		std::vector<CLO *>::iterator iterCLOsVect;

		// A vector of Non Option Arguments (NOA) that may follow flags
		std::vector<std::string> vectNOAs;
		std::vector<std::string>::iterator iterNOAsVect;

//		bool parse(int argc, char* const* argv, const char *cPattern, const struct option long_options[]);
		void parse(int argc, char* const* argv, const char *cPattern, const struct option long_options[]);
};

#endif
