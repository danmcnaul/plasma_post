/*********************************************************************
**                    Author 2026 - Dan McNaul
**                   
** 	A command line program that will modify FreeCad linuxcnc
** 	G-Code to work with CNC plasma / laser engraver machines
**                   
*********************************************************************/
/***************
****************
**
**		UNLICENSE LICENSE
**
**	This is free and unencumbered software released into the public
**	domain.
**
**	Anyone is free to copy, modify, publish, use, compile, sell, or
**	distribute this software, either in source code form or as a
**	compiled binary, for any purpose, commercial or non-commercial,
**	and by any means.
**
**	In jurisdictions that recognize copyright laws, the author or
**	authors of this software dedicate any and all copyright interest
**	in the software to the public domain. We make this dedication
**	for the benefit of the public at large and to the detriment of
**	our heirs and successors. We intend this dedication to be an overt
**	act of relinquishment in perpetuity of all present and future
**	rights to this software under the copyright law.
**
**	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
**	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
**	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
**	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
**	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTACT,
**	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
**	SOFTWARE OR THE USE OF OTHER DEALINGS IN THE SOFTWARE.
**
*/
/***************
****************
**
** FILE:    plasma_post.cc
**
** CONTENTS:    This program will read a gcode file and modify
**				the file to work with a plasma/laser engraver.
**				In particular, the Titanium 45 plasma cutter 
**              and the Creality Falcon 10w engraver.
**
** REFERENCES:  
**
****************
***************/

#ident "%W% DWM:plasma_post delta: %G% %U% get: %H% %T%"

#include <string>
#include <iostream>
#include <iomanip>
#include <climits>
#include <bitset>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <fstream>
#include <regex>
#include <filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ftw.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <getopt.h>

#include "Args.h"
#include "badIniFile.h"
#include "version.h"

// Global Variable here.
bool bValidate = false;
bool bSpecific = false;
bool bUseIni = false;
std::string sOutFileName = "";
std::string sFileName = "";
std::string sGcodeIn = "";
std::string sGcodeOut = "";
std::string sSpeed = "";
std::string sCircSpeed = "";
std::string sPierce = "";
std::string sIntensity = "";
std::string sOutExt = "";
std::string sIniFile = "";
std::string sThickness = "";
struct stat statbuf;

static struct option long_options[] =
{ 
	{"help", no_argument, NULL, 'h'},
	{"speed", required_argument, NULL, 'F'},
	{"circle", required_argument, NULL, 'C'},
	{"pierce", required_argument, NULL, 'P'},
	{"intensity", required_argument, NULL, 'S'},
	{"outext", required_argument, NULL, 'O'},
	{"inifile", required_argument, NULL, 'I'},
	{"thickness", required_argument, NULL, 'T'},
	{"validate", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};

// A struct file defining a record in the 
// iniFile
typedef struct
{
	bool bSelected;
	double dThickness;
	int nSpeed;
	int nCircle;
	double dPierce;
	int nIntensity;
	std::string sOutExt;
} iniRecord;

// A struct file defining a record in the 
// gcode file
typedef struct
{
	bool bRecognize;
	bool bTorchOn;
	std::string sOrigRecord;
	std::string sOrigGcode;
} gcodeRecord;

// A struct file defining which iniRecs
// that qualify for output gcode files
typedef struct
{
	std::string sComparator;
	double dLookFor;
} selectors;

int fileHandler(std::string sFileName, const struct stat *);
int testUsage(char **argv, Args g);
void showUsage(char **argv, Args g);
bool isNumeric(const std::string& str);
int verifyIniFile(void);
std::vector<std::string> tokenizer(const std::string &sBundle, char delimiter);
std::string getCurrentDateTime(void);
std::vector<iniRecord *> loadIniRecords(const std::string &sFileName);
std::string wildcardToRegex(const std::string& sPattern);

int main(int argc, char **argv)
{
	int nRet = 0;
	std::size_t tStrtBeg = 0;
	std::size_t tStrtEnd = 0;

	Args g;
	std::string sCmdOpts = "hVF:C:P:S:O:I:T:";

	try
	{
		g = Args(argc, argv, sCmdOpts.c_str(), long_options);
	}
	catch (badCmdLine &e)
	{
		std::cerr << e.what() << std::endl;
		showUsage(argv, g);
		return -1;
	}

	try
	{
		nRet = testUsage(argv, g);
	}
	catch (badCmdLine &e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}
	if(nRet != 0)
	{
		showUsage(argv, g);
		return -1;
	}
	if(bValidate)
	{
		nRet = verifyIniFile();
		return nRet;
	}

	try
	{
		std::string sWildcardPattern = "";
		std::filesystem::path pPath(sFileName);
		std::filesystem::path pDirectory = pPath.parent_path();
		std::filesystem::path pFileName = pPath.filename();
		std::filesystem::path pStem = pPath.stem();
		std::filesystem::path pExtension = pPath.extension();
		tStrtBeg = sFileName.rfind('*');
		tStrtEnd = sFileName.rfind('?');
		if(tStrtBeg == std::string::npos 
			&& tStrtEnd == std::string::npos)
		{
			// The user gave us a filename to act on
			// meaning, no REGEX pattern in the name
			// Just process that G-Code file
			nRet = stat(sFileName.c_str(), &statbuf);
			if(nRet < 0)
			{
				std::cerr << "The GCode File named " << sFileName << " is not found." << std::endl;
				return -1;
			}
			nRet = fileHandler(sFileName, &statbuf);
		}
		else
		{
			// The user gave us a REGEX pattern to act on
			// Process all the files in that directory that
			// matches their file pattern
		    // Prepare regex if pattern is given
    		std::regex fileRegex(".*", std::regex_constants::icase); // default: match all
			sWildcardPattern = pFileName.string();
//			sWildcardPattern = pDirectory.string() + "/" + pFileName.string();
		    if (!sWildcardPattern.empty()) 
			{
        		try 
				{
            		fileRegex = std::regex(wildcardToRegex(sWildcardPattern), std::regex_constants::icase);
        		}
        		catch (const std::regex_error& e) 
				{
            		std::cerr << "Invalid pattern: " << e.what() << '\n';
            		return 1;
        		}
			}
			for(const auto &entry : std::filesystem::recursive_directory_iterator(pDirectory, std::filesystem::directory_options::skip_permission_denied))
			{
				try
				{
					if(std::filesystem::is_regular_file(entry.status()))
					{
						sFileName = entry.path().filename().string();
						if(std::regex_match(sFileName, fileRegex))
						{
							nRet = stat(entry.path().string().c_str(), &statbuf);
							if(nRet < 0)
							{
								std::cerr << "The GCode File named " << entry.path().string() << " is not found." << std::endl;
								return -1;
							}
							nRet = fileHandler(entry.path().string(), &statbuf);
						}
					}
				}
				catch (const std::filesystem::filesystem_error &e)
				{
					std::cerr << "Filesystem error: " << e.what() << std::endl;
					return -1;
				}
				catch (const std::exception &e)
				{
					std::cerr << "General error: " << e.what() << std::endl;
					return -1;
				}
			}
		}
	}
	catch (const std::filesystem::filesystem_error &e)
	{
		std::cerr << "Filesystem error: " << e.what() << std::endl;
		return -1;
	}

	return(nRet);
}

//  We read the entire file into memory and then move 
//	it to another portion of memory for writing.  
//	The output file area is also completely in memory

int fileHandler(std::string sGcodeFileName, const struct stat *stat)
{
	bool bTorchIsOn = false;
	std::size_t tStrtBeg = 0;
	std::size_t tStrtEnd = 0;
	int nSize = 0;
	FILE *fp = NULL;
	void *vpInBuf = NULL;
	iniRecord *iniRec = NULL;
	selectors *selector = NULL;
	std::string sDateTimeStamp = "";
	std::string sWrkStack = "";
	std::string sWrk = "";
	std::string sG4code = "G4 P";
	std::string sScode = "S";
	std::string sM3code = "M3";
	std::string sM5code = "M5";
	std::string sFcode = " F";
	std::string sFCirccode = " F";
	std::string sG28zcode = "G28 G91 Z0";
	std::string sG28xycode = "G28 X0 Y0";
	std::string sGcodeRecIn = "";
	// first character is a semi-colon so it is commented out in output
	std::string sG38code = ";G38.2 Z-25.4 F508\t(probe downward until the ohmic circuit closes)";
	std::string sG92code = ";G92 Z0\t\t\t(Instantly zero out the Z-axis at the exact touch point)";
	std::string sG1Zcode = ";G1 Z3.81 F1016\t\t(raise the torch to piercing height -- DO NOT SKIP THIS STEP)";
	std::string sOR = "; ** OR ** \t\t(if you are using a Z-axis proximity/limit switch)";
	std::string sG28code = ";G28.1 Z12.70\t\t(probe downward until the Z-switch trips)";
	std::string sG92SWcode = ";G92 Z-1.524\t\t(Offset Z zero to account for switch travel (1.524 mm), measure your switch)";
	std::string sG0Zcode = ";G0 Z3.810\t\t(raise the torch to piercing height -- DO NOT SKIP THIS STEP)";

	std::vector<std::string> vectGcodeRecsIn;
	std::vector<std::string>::iterator iterGcodeRecsIn;
	std::vector<std::string>::iterator iterGcodeInLookAhead;
	std::vector<std::string> vectGcodeRecsOut;
	std::vector<std::string>::iterator iterGcodeRecsOut;
	std::vector<std::string> vectWork;
	std::vector<std::string>::iterator iterWork;
	std::vector<std::string> vectNext;
	std::vector<std::string>::iterator iterNext;
	std::vector<iniRecord *> vectIniRecords;
	std::vector<iniRecord *>::iterator iterIniRecords;	
	std::vector<selectors *> vectSelectors;
	std::vector<selectors *>::iterator iterSelectors;	

	std::ostringstream oss;
	
	// Open up the file and read it into memory in one big read
	fp = fopen(sGcodeFileName.c_str(), "r");	
	if(!fp)
	{
		std::cerr << "Trouble opening file " << sGcodeFileName << " for input." << std::endl;
		return 1;
	}

	//  Allocate memory large enough to hold the entire file contents + rounded up to double word.
	nSize = stat->st_size + (8 - (stat->st_size % 8));
	vpInBuf = calloc(1, nSize);
	if(!vpInBuf)
	{
		fclose(fp);
		std::cerr << "Internal trouble. Can not allocate memory for Input " << sGcodeFileName << std::endl;
		return 1;
	}

	// Read the entire contents of the file into memory
	nSize = fread(vpInBuf, stat->st_size, 1, fp);
	if(ferror(fp))
	{
		fclose(fp);
		free(vpInBuf);
		std::cerr << "Internal trouble. Can not read contents of " << sGcodeFileName.c_str() << std::endl;
		std::cerr << "\terrno is -> " << errno << std::endl;
		return 1;
	}

	// Close and release the file.  We have what we need in memory
	fclose(fp);
	fp = NULL;

	// load up a string so we can search it and free the allocated memory
	sWrkStack = std::string((char *)vpInBuf);
	free(vpInBuf);
	vpInBuf = NULL;

	// tokenizer will put each record as a row in the vector/table
	vectGcodeRecsIn = tokenizer(sWrkStack, '\n');
	if(vectGcodeRecsIn.empty())
	{
		std::cerr << "G-Code file  " << sGcodeFileName << " is empty" << std::endl;
		return -1;
	}

	// bSpecific is TRUE when the user uses the "specific" form of
	// the command options/flags -F -C -P etc.
	if(bSpecific)
	{
		// Convert and place the "specific" values in
		// the first row of the iniRecs vectors and
		// then let it fall through and use the same
		// logic as the iniRecords version uses.
		iniRec = new iniRecord();
		iniRec->bSelected = true;
		iniRec->dThickness = 0.0;
		try
		{
			iniRec->nSpeed = std::stoi(sSpeed);
		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "Error -F Speed not numeric" << std::endl;
			return -1;
		}
		try
		{
			iniRec->nCircle = std::stoi(sCircSpeed);
		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "Error -C Circle not numeric" << std::endl;
			return -1;
		}
		try
		{
			iniRec->dPierce = std::stod(sPierce);
		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "Error -P Pierce not numeric" << std::endl;
			return -1;
		}
		try
		{
			iniRec->nIntensity = std::stoi(sIntensity);
		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "Error -S Intensity not numeric" << std::endl;
			return -1;
		}
		tStrtBeg = sOutExt.find_first_not_of(" \t\n\r\f\v");
		tStrtEnd = sOutExt.find_last_not_of(" \t\n\r\f\v");
		if(tStrtBeg == std::string::npos)
			sOutExt = "";
		iniRec->sOutExt = sOutExt.substr(tStrtBeg, tStrtEnd - tStrtBeg + 1);

		vectIniRecords.push_back(iniRec);
	}
	else
	{
		// Here we are using the iniRecs version of 
		// the command line options/flags.
		// We use the "selector" (==,>=,<=,!=)
		// to mark the iniRecords that will qualify
		// for a G-Code output file
		try
		{
			vectIniRecords = loadIniRecords(sIniFile);
		}
		catch (badIniFile &e)
		{
			std::cerr << e.what() << std::endl;
			return -1;
		}
		sWrk = sThickness.substr(0, 2);
		if(sWrk.compare("==") == 0
			|| sWrk.compare(">=") == 0
			|| sWrk.compare("<=") == 0
			|| sWrk.compare("!=") == 0)
		{
			selector = new selectors();
			selector->sComparator = sThickness.substr(0, 2);
			try
			{
				selector->dLookFor = stod(sThickness.substr(2));
			}
			catch (const std::invalid_argument& e)
			{
				std::cerr << "Error -T Thickness selector not numeric" << std::endl;
				return -1;
			}
			vectSelectors.push_back(selector);
		}
		else
		{
			std::cerr << "Error -T Thickness selector comparator not a valid choice" << std::endl;
			return -1;
		}

		// I have "Selectors" in a table even though there will only
		// ever be one. I thought in the future an improvement may
		// be to allow for multiple selectors on the command line
		for(iterSelectors = vectSelectors.begin(); iterSelectors != vectSelectors.end(); iterSelectors++)
		{
			selector = *iterSelectors;
			// loop through the iniRecords. All that match the selection
			// criteria will be marked as bSelected TRUE
			for(iterIniRecords = vectIniRecords.begin(); iterIniRecords != vectIniRecords.end(); iterIniRecords++)
			{
				iniRec = *iterIniRecords;
				if(selector->sComparator.compare("==") == 0)
				{
					if(iniRec->dThickness == selector->dLookFor)
					{
						iniRec->bSelected = true;
						*iterIniRecords = iniRec;
						continue;
					}
					else
					{
						continue;
					}
				}
				if(selector->sComparator.compare("<=") == 0)
				{
					if(iniRec->dThickness <= selector->dLookFor)
					{
						iniRec->bSelected = true;
						*iterIniRecords = iniRec;
						continue;
					}
					else
					{
						continue;
					}
				}
				if(selector->sComparator.compare(">=") == 0)
				{
					if(iniRec->dThickness >= selector->dLookFor)
					{
						iniRec->bSelected = true;
						*iterIniRecords = iniRec;
						continue;
					}
					else
					{
						continue;
					}
				}
				if(selector->sComparator.compare("!=") == 0)
				{
					if(iniRec->dThickness != selector->dLookFor)
					{
						iniRec->bSelected = true;
						*iterIniRecords = iniRec;
						continue;
					}
					else
					{
						continue;
					}
				}
			}
		}
	}

	// Loop through the iniRecords. The rows marked as bSelected=TRUE
	// will fall through and a G-Code output file will be generated
	// for that iniRecord row.
	for(iterIniRecords = vectIniRecords.begin(); iterIniRecords != vectIniRecords.end(); iterIniRecords++)
	{
		iniRec = *iterIniRecords;
		if(!iniRec->bSelected)
			continue;
		// Get ready to build the G-Code output file for
		// this iniRecord
		sWrk = getCurrentDateTime();
		oss << "(Post Processor: plasma_post v" << VERSION_MAJOR << "." <<  VERSION_MINOR << "." << VERSION_PATCH << " Build No: " << VERSION_BUILD << " Output Time:" << sWrk << ")";
		sDateTimeStamp = oss.str();

		oss.str("");
		oss.clear();
		oss << "S" << iniRec->nIntensity;
		sScode = oss.str();
		oss.str("");
		oss.clear();
		oss << "G4 P" << iniRec->dPierce;
		sG4code = oss.str();
		oss.str("");
		oss.clear();
		oss << " F" << iniRec->nSpeed;
		sFcode = oss.str();
		oss.str("");
		oss.clear();
		oss << " F" << iniRec->nCircle;
		sFCirccode = oss.str();
		oss.str("");
		oss.clear();

		vectGcodeRecsOut.push_back(sDateTimeStamp);
		vectGcodeRecsIn.push_back("");
		bTorchIsOn = false;
		// For each qualifying iniRecord we process the G-Code
		// in records to the G-Code out. We modify the G-Code
		// turning on the torch and shutting it off when appropriate
		// We also pause for material piercing and we control
		// the speed of the x-axis,y-axis movement
		for(iterGcodeRecsIn = vectGcodeRecsIn.begin(); iterGcodeRecsIn != vectGcodeRecsIn.end(); iterGcodeRecsIn++)
		{
			iterGcodeInLookAhead = iterGcodeRecsIn + 1;
			sGcodeRecIn = *iterGcodeRecsIn;
			if(sGcodeRecIn.empty())
				continue;
			vectWork.clear();
			vectNext.clear();
			vectWork = tokenizer(*iterGcodeRecsIn, ' ');
			vectNext = tokenizer(*iterGcodeInLookAhead, ' ');

			if(vectWork[0].compare("M5") == 0 || vectWork[0].compare("M05")== 0)
			{
				bTorchIsOn = false;
				vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
				continue;
			}
			if(vectWork[0].compare("M3") == 0 || vectWork[0].compare("M03") == 0)
			{
				bTorchIsOn = true;
				vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
				continue;
			}
			if(vectWork[0].compare("G4") == 0)
			{
				vectGcodeRecsOut.push_back(sG4code);
				continue;
			}
			if(vectWork[0].front() == 'S')
			{
				vectGcodeRecsOut.push_back(sScode);
				continue;
			}


			if(vectWork[0].front() != 'G')
			{
				vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
				continue;
			}
			if(bTorchIsOn)
			{
			}
			else
			{
				if((vectNext[0].compare("G1") == 0 && vectNext[1][0] == 'X')
					|| (vectNext[0].compare("G2") == 0 && vectNext[1][0] == 'X')
					|| (vectNext[0].compare("G3") == 0 && vectNext[1][0] == 'X'))
				{
					// The torch is off and looking ahead at
					// the next input G-Code we found cut instructions
					// we will write this record first. Then
					// we will send the power setting (S),
					// the Torch On setting (M3)
					// and the Pause/Pierce setting (G4)
					//
					// Unless the current input record is a "G1 Z-axis"
					// move command. We don't write it to the output
					// file. We also replace it with commented out
					// THC (Torch Height Controller) ohmic commands
					if((vectWork[0].compare("G1") == 0 && vectWork[1][0] == 'Z'))
					{
					}
					else
					{
						vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
					}
					vectGcodeRecsOut.push_back(sG38code);
					vectGcodeRecsOut.push_back(sG92code);
					vectGcodeRecsOut.push_back(sG1Zcode);
					vectGcodeRecsOut.push_back(sOR);
					vectGcodeRecsOut.push_back(sG28code);
					vectGcodeRecsOut.push_back(sG92SWcode);
					vectGcodeRecsOut.push_back(sG0Zcode);
					vectGcodeRecsOut.push_back(sScode);
					vectGcodeRecsOut.push_back(sM3code);
					vectGcodeRecsOut.push_back(sG4code);
					bTorchIsOn = true;
					continue;
				}
			}
			if(vectWork[0].compare("G1") == 0
				|| vectWork[0].compare("G2") == 0
				|| vectWork[0].compare("G3") == 0)
			{
				// Here we are changing or adding the
				// speed settings for these cut codes
				// We support different values for 
				// straight cuts or arcs (circles)
				tStrtBeg = iterGcodeRecsIn->find(" F");
				if(tStrtBeg != std::string::npos)
				{
					sWrk = iterGcodeRecsIn->substr(0, tStrtBeg);
					if(vectWork[0].compare("G1") == 0)
					{
						sWrk = sWrk + sFcode;
						*iterGcodeRecsIn = sWrk;
						vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
					}
					else
					{
						sWrk = sWrk + sFCirccode;
						*iterGcodeRecsIn = sWrk;
						vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
					}
				}
				else
				{
					if(vectWork[0].compare("G1") == 0)
					{
						if(vectWork[1][0] == 'Z')
						{
							vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
						}
						else
						{
							*iterGcodeRecsIn = *iterGcodeRecsIn + sFcode;
							vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
						}
					}
					else
					{
						*iterGcodeRecsIn = *iterGcodeRecsIn + sFCirccode;
						vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
					}
				}

				if(vectNext[0].compare("G0") == 0)
				{
					vectGcodeRecsOut.push_back(sM5code);
					bTorchIsOn = false;
				}
				continue;
			}
			if(vectNext[0].compare("M2") == 0)
			{
				// LaserGRBL errors out the G28 codes
				// I wish it didn't. I'm not sure if
				// there is a LaserGRBL setting that
				// would allow these to work.
				vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
//				vectGcodeRecsOut.push_back(sG28zcode);
//				vectGcodeRecsOut.push_back(sG28xycode);
				continue;
			}
			vectGcodeRecsOut.push_back(*iterGcodeRecsIn);
		}

		// OK - The G-Code output vector is complete.
		// Let's write it out
		std::filesystem::path pPath(sGcodeFileName);
		std::filesystem::path pDirectory = pPath.parent_path();
		std::filesystem::path pFileName = pPath.filename();
		std::filesystem::path pStem = pPath.stem();
		std::filesystem::path pExtension = pPath.extension();
		if(pDirectory.string().empty())
		{
			sOutFileName = pStem.string() + iniRec->sOutExt + pExtension.string();
		}
		else
		{
			sOutFileName = pDirectory.string() + "/" + pStem.string() + iniRec->sOutExt + pExtension.string();
		}
//		sWrk = sOutFileName + iniRec->sOutExt + ".gcode";
		std::ofstream outfile(sOutFileName, std::ios::out | std::ios::trunc);
		if(!outfile)
		{
			std::cerr << "Trouble opening output file '" << sOutFileName << "' for writing." << std::endl;
			return -1;
		}

		for(iterGcodeRecsOut = vectGcodeRecsOut.begin(); iterGcodeRecsOut != vectGcodeRecsOut.end(); iterGcodeRecsOut++)
		{
				sWrk = *iterGcodeRecsOut;
				outfile << sWrk << std::endl;
		}
		outfile.close();
		vectGcodeRecsOut.clear();
	}

	// Trying to be a good technician. Let's free up our memory
	// before we exit.
	for(iterIniRecords = vectIniRecords.begin(); iterIniRecords != vectIniRecords.end(); iterIniRecords++)
	{
			iniRec = *iterIniRecords;
			delete iniRec;
	}
	
	for(iterSelectors = vectSelectors.begin(); iterSelectors != vectSelectors.end(); iterSelectors++)
	{
		selector = *iterSelectors;
		delete selector;
	}

	vectGcodeRecsIn.clear();
	vectGcodeRecsOut.clear();
	vectIniRecords.clear();
	vectSelectors.clear();

	return 0;
}

//  This function will display a message on how to run this
//  tool if any type of error occurs.

void showUsage(char **argv, Args g)
{
	std::cerr << argv[0] << " v" << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << " (Build No: " << VERSION_BUILD << ")" << std::endl;
	std::cerr << "Usage: " << std::endl;
	std::cerr << "\t" << argv[0] << " will modify a linuxcnc_post gcode file to work on a plasma cutter / laser engraver." << std::endl;
	std::cerr << "\t\tYou should look at plasma_post.chm for better help." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--help [-h]\t\tprint this usage statement" << std::endl;
	std::cerr << "\tUse Case -> \t" << argv[0] << " -h" << std::endl;
	std::cerr << "\t      OR -> \t" << argv[0] << " --help" << std::endl;
	std::cerr << "\t      OR -> \t" << argv[0] << " --help 2>help_dump.txt" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t===========================================================================" << std::endl;
	std::cerr << "\t--validate [-V]\t\tThis will provide some rudimentary -I iniFile validation and help." << std::endl;
	std::cerr << "\t\tIn case you decide to write your own iniFile or you modify Titanium45.ini" << std::endl;
	std::cerr << "\tUse Case -> \t" << argv[0] << " -V -I myPlasmaCutter.ini" << std::endl;
	std::cerr << "\t      OR -> \t" << argv[0] << " --validate --inifile myPlasmaCutter.ini" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t===========================================================================" << std::endl;
	std::cerr << "\t" << "A quick word about placement of your gcode input file. It can be anywhere on the" << std::endl;
	std::cerr << "\tcommand line (beginning, middle, or end). The software is smart enough to find" << std::endl;
	std::cerr << "\tit. All that is important is that you don't confuse the program by placing it" << std::endl;
	std::cerr << "\tafter a parameter that is expecting a value." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t===========================================================================" << std::endl;
	std::cerr << "\t--speed=NNNN [-F <torch speed>]\t\tThis is the speed your torch should move for material thickness. This modifies the G1 F codes." << std::endl;
	std::cerr << "\t\tYou MUST provide the value in mm/min. Convert in/min to mm/min." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--circle=NNNN [-C <circle speed>]\tThis is the speed your torch should move for cutting circles. Slowing down a circle cut typically" << std::endl;
	std::cerr << "\t\t\tresults in a better circle. This modifies the G2 & G3 F codes." << std::endl;
	std::cerr << "\t\tYou MUST provide the value in mm/min. Convert in/min to mm/min." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--pierce=NN.NN [-P <pierce delay>]\tThis is the seconds to wait for the plasma to pierce the material. This modifies the G4 P codes." << std::endl;
	std::cerr << "\t\tSet this to a pause value to give your arc a chance to get through the material before moving." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--intensity=NNNN [-S <intensity value>]\tNNNN is the intensity to set for laser engraver. This modifies the -S codes." << std::endl;
	std::cerr << "\t\tSet the value to S1000 for plasma cutters. Plasma is always 100 percent intensity. I included this" << std::endl;
	std::cerr << "\t\t\tfeature so I can use this post processor for my Laser Engraver." << std::endl;
	std::cerr << "\t\t\t** Modern Laser Engravers use S1000 for max power **" << std::endl;
	std::cerr << "\t\t\t** Older Laser Engravers used S255 for max power **" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--outext=STRING [-O <string_value>]\tSTRING is a value to tack onto the end of the resulting output GCode filename to distinguish it from" << std::endl;
	std::cerr << "\t\t\tthe input GCode filename. It can be whatever you want as long as it is a valid filename." << std::endl;
	std::cerr << "\t\t\tYou might use something like \"_4.8mm\" or \"_3-16in\" to indicate this GCode will cut 3/16\" steel." << std::endl;
	std::cerr << "\t\t\tAssuming the latter, if my FreeCad part is called 2_inch_washer.gcode, my output file will be called" << std::endl;
	std::cerr << "\t\t\t2_inch_washer_3-16in.gcode." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\tUse Case -> \t" << argv[0] << " -F 2200 -C 1760 -P 0.8 -S 1000 -O \"_3.2mm\" myFreeCad.gcode" << std::endl;
	std::cerr << "\t      OR -> \t" << argv[0] << " myFreeCad.gcode --speed 2200 --circle 1760 --pierce 0.8 --intensity 1000 --outext \"_3.2mm\"" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t===========================================================================" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\tThese parameters are Mutually Exclusive with the above parameters. They do the same thing as the parameters" << std::endl;
	std::cerr << "\tabove, they however get the values from an iniFile. This way you can set your favorite settings and" << std::endl;
	std::cerr << "\thave the computer remember them for you." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--inifile=FILE [-I <where to find the ini file>]\tFILE is a simple, pipe ['|'] delimited CSV text file" << std::endl;
	std::cerr << "\t\twith the plasma cutter parameters. The pipe character is the shift-key above your enter key." << std::endl << std::endl;
	std::cerr << "\t\t**TIP -> Use LibreOffice Calc, MS Excel, or GoogleSheets to build your iniFile table. <- TIP**" << std::endl << std::endl;
	std::cerr << "\t\tSee the provided Titanium45.ini file for the settings I use on my Harbor Freight Titanium 45." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t\t\tThe 9 Columns(Fields) MUST BE as follows and in this order separated by a '|' (pipe) character" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t\t\tComment => Any value in comment causes this row to be skipped." << std::endl;
	std::cerr << "\t\t\tThickness => This is the search and selection field. See the -T flag." << std::endl;
	std::cerr << "\t\t\tThickness Description => A description field for you benefit. Not used by this program." << std::endl;
	std::cerr << "\t\t\tAmperage Description => A description field for you benefit. Not used by this program." << std::endl;
	std::cerr << "\t\t\tCut Speed => A straight line cut speed setting." << std::endl;
	std::cerr << "\t\t\tCircle Cut Speed => A curved line cut speed setting. I use 65% of Cut Speed." << std::endl;
	std::cerr << "\t\t\tPierce Delay => The seconds to pause to allow the plasma arc/Laser dot to pierce the material before moving." << std::endl;
	std::cerr << "\t\t\tLaser Intensity => Important for Laser Engravers. How much power to use during the cut. Set to 1000 for plasma." << std::endl;
	std::cerr << "\t\t\tOutput File Extender => An extender to put on the output file so the original GCode file isn't over-written." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t--thickness=VALUE [-T <VALUE>]\tVALUE can be \"==N.N\", \"<=N.N\", \">=N.N\", or \"!=N.N\"." << std::endl;
	std::cerr << "\t\tThe program will produce multiple GCode output files based on iniFile entries that meet the criteria. Refer to the \"Thickness\" column in the --inifile." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\tUse Case -> \t" << argv[0] << " -I ~user/Documents/FreeCad/Titanium45.ini -T \">=80.0\" myFreeCad.gcode" << std::endl;
	std::cerr << "\t      OR -> \t" << argv[0] << " myFreeCad.gcode --inifile ~user/Documents/FreeCad/Titanium45.ini --thickness \">=80.0\"" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t===========================================================================" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "Example 01: " << std::endl;
	std::cerr << argv[0] << " --iniFile=~user/Documents/FreeCad/Titanium45.ini --thickness=\">=220\" MyGcodeFile.gcode" << std::endl;
	std::cerr << "\tOR, if you don't like to type" << std::endl;
	std::cerr << argv[0] << " -I ~user/Documents/FreeCad/Titanium45.ini -T \">=200\" MyGcodeFile.gcode" << std::endl;

	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\tModify MyGcodeFile.gcode so that my plasma cutter can pierce and cut the part. All rows in the iniFile" << std::endl;
	std::cerr << "\tthat have a \"THICKNESS\" column that is >= 200 will be produced." << std::endl;
	std::cerr << "\tMultiple separate output files is likely." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "Example 02: " << std::endl;
	std::cerr << argv[0] << " MyGcodeFile.gcode --speed 760 --circle 608 --pierce 1.2 --intensity 1000 --outext \"_3-8in\"" << std::endl;
	std::cerr << "\tOR, if you don't like to type" << std::endl;
	std::cerr << argv[0] << " MyGcodeFile.gcode -F 760 -C 608 -P 1.2 -S 1000 -O \"_3-8in\"" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\tModify MyGcodeFile.gcode so that my plasma cutter can pierce and cut the part. One resulting GCode" << std::endl;
	std::cerr << "\tfile will be produced. This output file will be named MyGcodeFile_3-8in.gcode. The torch will move at" << std::endl;
	std::cerr << "\t760 mm/min (608 mm/min on circles) and will pause for 1.2 seconds to pierce the material before moving." << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	std::cerr << "\t" << std::endl;
	return;
}

//  This function will test whether command line parameters
//  are valid

int testUsage(char **argv, Args g)
{
	int nRet = 0;
	char cBuf[BUFSIZ];
	CLO *clo = NULL;
	std::string sCount = "";
	std::string sSkip = "";
	std::vector<CLO *> vectCLOs;
	std::vector<CLO *>::iterator iterVCLOs;
	std::vector<std::string> vectExtraStuf;

	memset(cBuf, '\0', sizeof(cBuf));

	vectExtraStuf = g.getNonOptArgs();

	if(g.isSet("-h"))
	{
		return 1;
	}

	if(g.isSet("-I"))
	{
		bUseIni = true;
		vectCLOs = g.getValues("-I");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sIniFile = clo->sValue;
		nRet = stat(sIniFile.c_str(), &statbuf);
		if(nRet < 0)
		{
			sprintf(cBuf, "%s -> %s not found.", clo->sAsIs.c_str(), sIniFile.c_str());
			throw badCmdLine(cBuf);
			return -1;
		}
	}

	if(g.isSet("-T"))
	{
		bUseIni = true;
		vectCLOs = g.getValues("-T");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sThickness = clo->sValue;
	}

	if(g.isSet("-V"))
	{
		if(g.isSet("-I"))
		{
			bValidate = true;
			return 0;
		}
		else
		{
			sprintf(cBuf, "Option --validate [-V] To validate your iniFile, you must provide the --inifile= [-I] parameter.\nSee --help for details");
			throw badCmdLine(cBuf);
			return -1;
		}
	}

	if(g.isSet("-F"))
	{
		bSpecific = true;
		vectCLOs = g.getValues("-F");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sSpeed = clo->sValue;
	}
	else
	{
		sSpeed = "450";			// default to very slow
	}

	if(g.isSet("-C"))
	{
		bSpecific = true;
		vectCLOs = g.getValues("-C");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sCircSpeed = clo->sValue;
	}
	else
	{
		sCircSpeed = "360";			// default to very slow
	}

	if(g.isSet("-P"))
	{
		bSpecific = true;
		vectCLOs = g.getValues("-P");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sPierce = clo->sValue;
	}
	else
	{
		sPierce = "0.5";		// default to half second
	}

	if(g.isSet("-S"))
	{
		bSpecific = true;
		vectCLOs = g.getValues("-S");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sIntensity = clo->sValue;
	}
	else
	{
		sIntensity = "1000";		
	}

	if(g.isSet("-O"))
	{
		bSpecific = true;
		vectCLOs = g.getValues("-O");
		iterVCLOs = vectCLOs.begin();
		clo = *iterVCLOs;
		sOutExt = clo->sValue;
	}
	else
	{
		sOutExt = "_new";
	}

	if(bSpecific && bUseIni)
	{
		sprintf(cBuf, "You are mixing Mutually Exclusive parameters.\nSee --help for details");
		throw badCmdLine(cBuf);
		return -1;
	}

	if(bSpecific || bUseIni)
	{
	}
	else
	{
		sprintf(cBuf, "No parameters provided. I don't know how to proceed.\nSee --help for details");
		throw badCmdLine(cBuf);
		return -1;
	}

	if(bUseIni)
	{
		if((g.isSet("-T")) && (g.isSet("-I")))
		{
		}
		else
		{
			sprintf(cBuf, "You must use the -I and -T parameters together.\nSee --help for details");
			throw badCmdLine(cBuf);
			return -1;
		}
	}

	if(bSpecific)
	{
		if((g.isSet("-F")) && (g.isSet("-C")) && (g.isSet("-P")) && (g.isSet("-S")) && (g.isSet("-O")))
		{
			if(isNumeric(sSpeed))
			{
			}
			else
			{
				sprintf(cBuf, "Option --speed [-F] must be numeric.\nSee --help for details");
				throw badCmdLine(cBuf);
				return -1;
			}
			if(isNumeric(sCircSpeed))
			{
			}
			else
			{
				sprintf(cBuf, "Option --speed [-C] must be numeric.\nSee --help for details");
				throw badCmdLine(cBuf);
				return -1;
			}
			if(isNumeric(sPierce))
			{
			}
			else
			{
				sprintf(cBuf, "Option --pierce [-P] must be numeric.\nSee --help for details");
				throw badCmdLine(cBuf);
				return -1;
			}
			if(isNumeric(sIntensity))
			{
			}
			else
			{
				sprintf(cBuf, "Option --intensity [-S] must be numeric.\nSee --help for details");
				throw badCmdLine(cBuf);
				return -1;
			}
		}
		else
		{
			sprintf(cBuf, "You must use the -F, -C, -P, -S and -O parameters together.\nSee --help for details");
			throw badCmdLine(cBuf);
			return -1;
		}
	}


	if(vectExtraStuf.empty())
	{
		sprintf(cBuf, "The name of the GCode file you want to modify is missing. The GCode file name goes on last.\nSee --help for details");
		throw badCmdLine(cBuf);
		return -1;
	}
	
	sFileName = vectExtraStuf[0];

	if(sFileName.empty())
	{
		sprintf(cBuf, "The name of the GCode file you want to modify is missing. The GCode file name goes on last.\nSee --help for details");
		throw badCmdLine(cBuf);
		return -1;
	}
/*
	nRet = stat(sFileName.c_str(), &statbuf);
	if(nRet < 0)
	{
		sprintf(cBuf, "The GCode File named %s not found.", sFileName.c_str());
		throw badCmdLine(cBuf);
		return -1;
	}
*/


	return 0;
}

// Our little numeric test function. Numerics in a string
bool isNumeric(const std::string& str)
{
	if(str.empty())
		return false;

	std::stringstream ss(str);
	double num = 0;
	char extra = ' ';

	// Let us try and read a number
	
	if(!(ss >> num))
		return false;

	// We have to ensure no extra fluff after the number
	if(ss >> extra)
		return false;

	return true;
}

// This routine will walk through a iniFile and
// do some basic edits, very little verification.
// The sIniFile is global, so no need to pass it
int verifyIniFile(void)
{
	int nRet = 0;
	std::string sGCodeIn = "myGCodeIn.gcode";

	iniRecord *iniRec = NULL;
	std::vector<iniRecord *> vectIniRecords;
	std::vector<iniRecord *>::iterator iterIniRecords;	

	std::cout << std::endl << "Validating contents of " << sIniFile << std::endl;
	std::cout << "\tContents are equivalent to running =>" << std::endl << std::endl;
	try
	{
		vectIniRecords = loadIniRecords(sIniFile);
	}
	catch (badIniFile &e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	for(iterIniRecords = vectIniRecords.begin(); iterIniRecords != vectIniRecords.end(); iterIniRecords++)
	{
		iniRec = *iterIniRecords;
		std::cout << "plasma_post " << sGCodeIn << " -F " << iniRec->nSpeed << " -C " << iniRec->nCircle << " -P " << iniRec->dPierce << " -S " << iniRec->nIntensity << " -O \"" << iniRec->sOutExt << "\"" << std::endl;
		delete iniRec;
	}
	std::cout << "\tNOTE => LaserGRBL expects speeds are given in mm/min. Do NOT specify in/min" << std::endl << std::endl;
	vectIniRecords.clear();

	return nRet;
}

std::vector<std::string> tokenizer(const std::string &sBundle, char delimiter)
{
	std::size_t tStrtBeg = 0;
	std::size_t tStrtEnd = 0;
	std::vector<std::string> vectParts;
	std::stringstream ss(sBundle);
	std::string sPart = "";
	std::string sWrk = "";
	while(std::getline(ss, sPart, delimiter))
	{
		if(sPart.empty())
		{
			sWrk = "";
		}
		else
		{
			tStrtBeg = sPart.find_first_not_of(" \t\n\r\f\v");
			tStrtEnd = sPart.find_last_not_of(" \t\n\r\f\v");
			if(tStrtBeg == std::string::npos)
			{
				sWrk = "";
			}
			else
			{
				sWrk = sPart.substr(tStrtBeg, tStrtEnd - tStrtBeg + 1);
			}
		}
		vectParts.push_back(sWrk);
	}

	return vectParts;

}

std::string getCurrentDateTime(void)
{
		auto now = std::chrono::system_clock::now();

		std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm local_tm{};
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&local_tm, &now_time_t); // for windows
#else
	localtime_r(&now_time_t, &local_tm); // for POSIX
#endif

	std::ostringstream oss;
	oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

std::vector<iniRecord *> loadIniRecords(const std::string &sFileName)
{
	bool bBadRec = false;
	bool bBadFile = false;
	int nRet = 0;
	int nSize = 0;
	int nRec = 0;
	FILE *fp = NULL;
	std::string sWrkStack = "";
	void *vpInBuf = NULL;
	iniRecord *iniRec = NULL;
	char cBuf[BUFSIZ];
	std::stringstream ss;
	std::vector<iniRecord *> vectIniRecords;
	std::vector<iniRecord *>::iterator iteriniRecords;
	std::vector<std::string> vectIniRecordsIn;
	std::vector<std::string>::iterator iterIniRecordsIn;
	std::vector<std::string> vectIniRecordsFields;
	std::vector<std::string>::iterator iterIniRecordsFields;

	// Open up the iniFile and read it into memory in one big read
	fp = fopen(sIniFile.c_str(), "r");	
	if(!fp)
	{
		sprintf(cBuf, "Trouble opening iniFile. FileName -> %s not found.",  sIniFile.c_str());
		throw badIniFile(cBuf);
		return vectIniRecords;
	}

	//  Allocate memory large enough to hold the entire file contents + rounded up to double word.
	nRet = stat(sIniFile.c_str(), &statbuf);
	if(nRet < 0)
	{
		sprintf(cBuf, "Trouble opening iniFile. FileName -> %s not found.",  sIniFile.c_str());
		throw badIniFile(cBuf);
		return vectIniRecords;
	}
	nSize = statbuf.st_size + (8 - (statbuf.st_size % 8));
	vpInBuf = calloc(1, nSize);
	if(!vpInBuf)
	{
		fclose(fp);
		sprintf(cBuf, "Internal trouble allocating memory for iniFile %s",  sIniFile.c_str());
		throw badIniFile(cBuf);
		return vectIniRecords;
	}

	// Read the entire contents of the file into memory
	nSize = fread(vpInBuf, statbuf.st_size, 1, fp);
	if(ferror(fp))
	{
		fclose(fp);
		free(vpInBuf);
		sprintf(cBuf, "Internal trouble reading iniFile %s into memory",  sIniFile.c_str());
		throw badIniFile(cBuf);
		return vectIniRecords;
	}

	// Close and release the file.  We have what we need in memory
	fclose(fp);
	fp = NULL;

	// load up a string so we can search it and free the allocated memory
	sWrkStack = std::string((char *)vpInBuf);
	free(vpInBuf);
	vpInBuf = NULL;
	vectIniRecordsIn = tokenizer(sWrkStack, '\n');

	for(iterIniRecordsIn = vectIniRecordsIn.begin(); iterIniRecordsIn != vectIniRecordsIn.end(); iterIniRecordsIn++)
	{
		if(bBadRec)
		{
			bBadFile = true;
		}
		bBadRec = false;
		nRec++;
		vectIniRecordsFields = tokenizer(*iterIniRecordsIn, '|');

		// If we didn't get 9 fields in the iniFile
		// We're not going any further
		if(vectIniRecordsFields.size() != 9)
		{
			bBadRec = true;
			ss << "Rec #" << nRec << " incorrect number of fields. Expecting 9. Found " << vectIniRecordsFields.size() << " SKIPPING" << std::endl;
			vectIniRecordsFields.clear();
			continue;
		}
		
		// If the 1st field has something in it
		// this means that this row/rec is a comment
		// We skip this record
		if(!vectIniRecordsFields[0].empty())
		{
			ss << "Rec #" << nRec << " is a comment record. SKIPPING" << std::endl;
			vectIniRecordsFields.clear();
			continue;
		}

		// OK - This row/rec may qualify for work
		// We will save it

		if(isNumeric(vectIniRecordsFields[1]))
		{
		}
		else
		{
			ss << "Rec #" << nRec << " THICKNESS or SELECTION field #2 is not numeric. ERROR value found=" << vectIniRecordsFields[1] << std::endl;
			bBadRec = true;
		}
		if(isNumeric(vectIniRecordsFields[4]))
		{
		}
		else
		{
			ss << "Rec #" << nRec << " SPEED field #5 is not numeric. ERROR value found=" << vectIniRecordsFields[4] << std::endl;
			bBadRec = true;
		}
		if(isNumeric(vectIniRecordsFields[5]))
		{
		}
		else
		{
			ss << "Rec #" << nRec << " CIRCLE SPEED field #6 is not numeric. ERROR value found=" << vectIniRecordsFields[5] << std::endl;
			bBadRec = true;
		}
		if(isNumeric(vectIniRecordsFields[6]))
		{
		}
		else
		{
			ss << "Rec #" << nRec << " PIERCE pause field #7 is not numeric. ERROR value found=" << vectIniRecordsFields[6] << std::endl;
			bBadRec = true;
		}
		if(isNumeric(vectIniRecordsFields[7]))
		{
		}
		else
		{
			ss << "Rec #" << nRec << " INTENSITY field #8 is not numeric. ERROR value found=" << vectIniRecordsFields[7] << std::endl;
			bBadRec = true;
		}
		if(bBadRec)
			continue;

		iniRec = new iniRecord();
		iniRec->bSelected = false;
		iniRec->dThickness = 0.0;
		iniRec->dThickness = std::stod(vectIniRecordsFields[1]);
		iniRec->nSpeed = std::stoi(vectIniRecordsFields[4]);
		iniRec->nCircle = std::stoi(vectIniRecordsFields[5]);
		iniRec->dPierce = std::stod(vectIniRecordsFields[6]);
		iniRec->nIntensity = std::stoi(vectIniRecordsFields[7]);
		iniRec->sOutExt = vectIniRecordsFields[8];

		vectIniRecords.push_back(iniRec);
		vectIniRecordsFields.clear();

	}

	if(bBadFile)
	{
		std::cerr << ss.str() << std::endl;
		ss.clear();
	}
	if(vectIniRecords.empty())
	{
		sprintf(cBuf, "Either your INI file %s is empty or all the records are bad. No records passed the edits.",  sIniFile.c_str());
		throw badIniFile(cBuf);
		return vectIniRecords;
	}
	return vectIniRecords;
}

// Convert wildcard pattern (*, ?) to a case-insensitive regex string
std::string wildcardToRegex(const std::string& sPattern) 
{
    std::string sRegexPattern;
    sRegexPattern.reserve(sPattern.size() * 2);
    sRegexPattern += "^"; // match from start

    for (char c : sPattern) 
	{
        switch (c) 
		{
            case '*': sRegexPattern += ".*"; break; // * → any sequence
            case '?': sRegexPattern += "."; break;  // ? → any single char
            case '.': sRegexPattern += "\\."; break; // escape dot
            default:
                if (std::isalnum(static_cast<unsigned char>(c)))
                    sRegexPattern += c;
                else 
				{
                    sRegexPattern += "\\";
                    sRegexPattern += c;
                }
        }
    }

    sRegexPattern += "$"; // match till end
    return sRegexPattern;
}
