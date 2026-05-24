#ident "%W% DWM:Args delta: %G% %U% get: %H% %T%"

#include <iostream>
#include <string>
#include <sstream>
#include <iterator>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "badCmdLine.h"
#include "Args.h"

// Our constructor.  

Args::Args(int argc, char* const* argv, const char *cPattern, const struct option long_options[])
{
//	bool bRet = false;

	parse(argc, argv, cPattern, long_options);
}

// Our copy constructor.  We need this so we can move pointers
// to things into the new object

Args::Args(const Args &old)
{
	CLO *clonew = NULL;
	CLO *cloold = NULL;
	std::map<std::string, std::vector<CLO *> >::iterator itOld;
	optMap = old.optMap;
	vectNOAs = old.vectNOAs;

	for(iterOptMap = optMap.begin(); iterOptMap != optMap.end(); iterOptMap++)
	{
		vectCLOs = iterOptMap->second;
		for(iterCLOsVect = vectCLOs.begin(); iterCLOsVect != vectCLOs.end(); iterCLOsVect++)
		{
			// We have to relocate these CLO structs
			// because the old Args object will call
			// it's destructor and delete them.
			cloold = *iterCLOsVect;
			clonew = new CLO();

			clonew->bIsError = cloold->bIsError;
			clonew->bShouldHaveOpt = cloold->bShouldHaveOpt;
			clonew->bHasOpt = cloold->bHasOpt;
			clonew->sPattern = cloold->sPattern;
			clonew->sFlag = cloold->sFlag;
			clonew->sValue = cloold->sValue;
			clonew->sAsIs = cloold->sAsIs;
			*iterCLOsVect = clonew;
		}
		optMap[iterOptMap->first] = vectCLOs;
	}
}

// One Args equals a different Args

Args Args::operator=(const Args &old)
{
	CLO *clonew = NULL;
	CLO *cloold = NULL;
	std::map<std::string, std::vector<CLO *> >::iterator itOld;
	optMap = old.optMap;
	vectNOAs = old.vectNOAs;

	for(iterOptMap = optMap.begin(); iterOptMap != optMap.end(); iterOptMap++)
	{
		vectCLOs = iterOptMap->second;
		for(iterCLOsVect = vectCLOs.begin(); iterCLOsVect != vectCLOs.end(); iterCLOsVect++)
		{
			// We have to relocate these CLO structs
			// because the old Args object will call
			// it's destructor and delete them.
			cloold = *iterCLOsVect;
			clonew = new CLO();

			clonew->bIsError = cloold->bIsError;
			clonew->bShouldHaveOpt = cloold->bShouldHaveOpt;
			clonew->bHasOpt = cloold->bHasOpt;
			clonew->sPattern = cloold->sPattern;
			clonew->sFlag = cloold->sFlag;
			clonew->sValue = cloold->sValue;
			clonew->sAsIs = cloold->sAsIs;
			*iterCLOsVect = clonew;
		}
		optMap[iterOptMap->first] = vectCLOs;
	}

	return *this;
}

//  Our destructor.  We have to delete all the CLOs
//  to avoid memory leaks		

Args::~Args()
{
	CLO *clo = NULL;
	for(iterOptMap = optMap.begin(); iterOptMap != optMap.end(); iterOptMap++)
	{
		vectCLOs = iterOptMap->second;
		for(iterCLOsVect = vectCLOs.begin(); iterCLOsVect != vectCLOs.end(); iterCLOsVect++)
		{
			clo = *iterCLOsVect;
			delete(clo);
			clo = NULL;
		}
		vectCLOs.clear();
		optMap[iterOptMap->first] = vectCLOs;
	}
	optMap.clear();
}

// This private function is called during construction.
// We parse the command line options and we load the 
// appropriate containers.

void Args::parse(int argc, char* const* argv, const char *cPattern, const struct option long_options[])
{

	bool bOptFnd = false;
	std::string::size_type xPos = 0;
	int nN = 0;
//	int nPatChar = 0;
	int nOptNdx = 0;
	CLO *clo = NULL;
	std::string sPat = "";
	std::string sWrk = "";
	char cPatColon = '\0';
	char cBuf[BUFSIZ];
//	struct option sLongOpt;

	std::map<std::string, char> mapOptPat;
	std::map<std::string, char>::iterator iterMapOptPat;

	memset(cBuf, '\0', sizeof(cBuf));
	sPat = cPattern;

// First, look through the arg flag patterns and get them all
// secured and ready for processing.  These are the patterns
// we can expect to receive on the command line
	for(xPos = 0; xPos < sPat.length(); xPos++)
	{
		if((sPat[xPos] >= 'a' && sPat[xPos] <= 'z')
			|| (sPat[xPos] >= 'A' && sPat[xPos] <= 'Z')
			|| (sPat[xPos] >= '0' && sPat[xPos] <= '9'))
		{
			sWrk = sPat[xPos];
			if(sPat[xPos + 1] == ':')
			{
				cPatColon = ':';
			}
			else
			{
				cPatColon = '_';
			}
			mapOptPat[sWrk] = cPatColon;
		}
	}

// Now, look through the possible long options.  Put them 
// in the table along with the flag style options
	for(xPos = 0; long_options[xPos].name != NULL; xPos++)
	{
		sWrk = long_options[xPos].name;
		if(long_options[xPos].has_arg == required_argument)
		{
			cPatColon = ':';
		}
		else
		{
			cPatColon = '_';
		}
		mapOptPat[sWrk] = cPatColon;
	}

// Now go get the command line options the user put on there
// We'll check them and store them for later analysis
    while((nN = getopt_long(argc, argv, sPat.c_str(), long_options, &nOptNdx)) != -1)
    {

		if(nN == 0)		// If true, it is a long form parm
		{
			sWrk = long_options[nOptNdx].name;
		}
		else
		{
			sWrk = (char)nN;
		}

		bOptFnd = false;
		if(nN == '?')
		{
			sWrk = (char)optopt;
			iterMapOptPat = mapOptPat.find(sWrk);
			if(iterMapOptPat != mapOptPat.end())
			{
				sprintf(cBuf, "Option -%c requires an argument.", optopt);
				throw badCmdLine(cBuf);	// Bad Command Line -- throw exception
			}
			if(isprint(optopt))
			{
				sprintf(cBuf, "Unrecognized option -%c.", optopt);
				throw badCmdLine(cBuf);	// Bad Command Line -- throw exception
			}
			sprintf(cBuf, "Indeterminate option found or missing.");
			throw badCmdLine(cBuf);	// Bad Command Line -- throw exception
		}

		iterMapOptPat = mapOptPat.find(sWrk);
		if(iterMapOptPat != mapOptPat.end())
		{
			bOptFnd = true;
			clo = new CLO();
			clo->bIsError = false;
			clo->bShouldHaveOpt = false;
			clo->bHasOpt = false;
			clo->sPattern = "";
			clo->sFlag = "";
			clo->sValue = "";
			clo->sAsIs = "";
			clo->sPattern += iterMapOptPat->first;
			clo->sPattern += iterMapOptPat->second;
			if(nN == 0)
			{
				clo->sFlag += "--";
				clo->sAsIs += "--";
			}
			else
			{
				clo->sFlag += "-";
				clo->sAsIs += "-";
			}
			clo->sFlag += iterMapOptPat->first;
			clo->sValue = "";
			clo->sAsIs += iterMapOptPat->first;
			if(iterMapOptPat->second == ':')
			{
				clo->bShouldHaveOpt = true;
				clo->bHasOpt = true;
				clo->sValue += optarg;
				clo->sAsIs.append(" ");
				clo->sAsIs.append(clo->sValue);
			}
			if(clo->bShouldHaveOpt && clo->sValue.empty())
			{
				// Error -- Should have value, but doesn't
				clo->bIsError = true;
				clo->bHasOpt = false;
			}
			if(!clo->bShouldHaveOpt && !clo->sValue.empty())
			{
				// Error -- Should not have value, but does
				clo->bIsError = true;
				clo->bHasOpt = true;
			}
			iterOptMap = optMap.find(clo->sFlag);
			if(iterOptMap == optMap.end())
			{
				vectCLOs.clear();
				vectCLOs.push_back(clo);
				optMap[clo->sFlag] = vectCLOs;
			}
			else
			{
				vectCLOs = iterOptMap->second;
				vectCLOs.push_back(clo);
				optMap[clo->sFlag] = vectCLOs;
			}
		}
		if(bOptFnd)
		{
			continue;	// we found and processed the option
		}
		sprintf(cBuf, "Something unexpected happened with getopt.");
		throw badCmdLine(cBuf);	// Bad Command Line -- throw exception
	}

// Any remaining data at the very end of the command line go in
// this container.  The user can request these via the interface
	for(nN = optind; nN < argc; nN++)
	{
		sPat = std::string(argv[nN]);
		vectNOAs.push_back(sPat);
	}

//	return true;
}

// The caller passes in a flag or long option and we 
// return "true" or "false" if the flag was on the command line
//   for example = "-F fix" the user passes in "-F" for us to find
//     or long_option "--fix=yes" the user passes in "--fix"

bool Args::isSet(std::string sFlag)
{
	iterOptMap = optMap.find(sFlag);
	if(iterOptMap != optMap.end())
	{
		return true;
	}
	else
	{
		return false;
	}
}

// The caller passes in a flag or long option and we get the 
// vector of CLOs we found on the command line.
//   for example = "-F fix" the user passes in "-F" for us to find
//     or long_option "--fix=yes" the user passes in "--fix"

std::vector<CLO *> Args::getValues(std::string sFlag)
{
	vectCLOs.clear();
	iterOptMap = optMap.find(sFlag);
	if(iterOptMap == optMap.end())
	{
		return vectCLOs;
	}
	
	vectCLOs = iterOptMap->second;
	return vectCLOs;
}

// We return the vector container of the Non Option Args
// found at the end of the command line.  The caller
// can walk through the vector and look for things they 
// recognize

std::vector<std::string> Args::getNonOptArgs(void)
{
	return vectNOAs;
}
