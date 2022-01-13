//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#ifdef _MSC_VER
#  pragma warning (disable: 4996)     // Disable POSIX deprecation, certain security warnings that seem to be specific to VC++
#endif


#include "ConfigEnum.h"
#include "tnlVector.h"     // For Vector
#include "tnlTypes.h"

#include <string>
#include <map>
#include <fstream>

namespace Zap
{

using namespace std;
using namespace TNL;

// From http://stackoverflow.com/questions/134569/c-exception-throwing-stdstring
struct SaveException : public exception
{
   string msg;

   explicit SaveException(string str);         // Constructor
   virtual ~SaveException() throw();  // Destructor, needed to avoid "looser throw specifier" errors with gcc
   const char* what() const throw();
};


// Collection of useful string things

string extractDirectory(const string &path);
string extractFilename(const string &path);
string extractExtension(const string &path);

string itos(S32 i);
string itos(U32 i);
string itos(U64 i);
string itos(S64 i);
string ftos(F32 f, int digits);
string ftos(F32 f);

F64 stof(const string &s);

string replaceString(const string &strString, const string &strOld, const string &strNew);
string stripExtension(string filename);

string listToString(const Vector<string> &words, const string &seperator);

// TODO: Merge these methods
Vector<string> parseString(const string &line);
void parseString(const char *inputString, Vector<string> &words, char seperator = ' ');
void parseString(const string &inputString, Vector<string> &words, char seperator = ' ');
Vector<string> parseStringAndStripLeadingSlash(const char *str);

const char *findPointerOfArg(const char *message, S32 count);

string concatenate(const Vector<string> &words, S32 startingWith = 0);

string lcase(string strToConvert);
string ucase(string strToConvert);

bool isInteger(const char *str);

bool isAlpha(char c);
bool isDigit(char c);
bool isAlNum(char c);

string sanitizeForJson(const char *value);
string sanitizeForSql(const string &value);

bool isControlCharacter(char ch);
bool containsControlCharacter(const char* str);

void s_fprintf(FILE *stream, const char *format, ...);      // throws SaveException

bool caseInsensitiveStringCompare(const string &str1, const string &str2);

// File utils
string getFileSeparator();
bool fileExists(const string &path);               // Does file exist?
bool makeSureFolderExists(const string &dir);      // Like the man said: Make sure folder exists
bool getFilesFromFolder(const string &dir, Vector<string> &files, const string extensions[] = 0, S32 extensionCount = 0);
bool safeFilename(const char *str);
bool copyFile(const string &sourceFilename, const string &destFilename);
bool copyFileToDir(const string &sourceFilename, const string &destDir);


// Different variations on joining file and folder names
string joindir(const string &path, const string &filename);
string strictjoindir(const string &part1, const string &part2);
string strictjoindir(const string &part1, const string &part2, const string &part3);

// By default we'll mimic the behavior or PHP.  Because that's something to aspire to!
// http://lu1.php.net/trim
#define DEFAULT_TRIM_CHARS " \n\r\t\0\x0B"   

string trim_right(const string &source, const string &t = DEFAULT_TRIM_CHARS);
string trim_left(const string &source, const string &t = DEFAULT_TRIM_CHARS);
string trim(const string &source, const string &t = DEFAULT_TRIM_CHARS);

void trim_right_in_place(string &source, const string &t = DEFAULT_TRIM_CHARS);
void trim_left_in_place(string &source, const string &t = DEFAULT_TRIM_CHARS);
void trim_in_place(string &source, const string &t = DEFAULT_TRIM_CHARS);

#undef DEFAULT_TRIM_CHARS

S32 countCharInString(const string &source, char search);

const U32 MAX_FILE_NAME_LEN = 128;     // Completely arbitrary
string makeFilenameFromString(const char *levelname, bool allowLastDot = false);

string ctos(char c);

string writeLevelString(const char *in);

string chopComment(const string &line);


bool writeFile(const string& path, const string& contents, bool append = false);
const string readFile(const string& path);

string getExecutableDir();

bool stringContainsAllTheSameCharacter(const string &str);

string toString(const string &val);      
string toString(S32 val);                
string toString(DisplayMode displayMode);
string toString(YesNo yesNo);            
string toString(RelAbs relAbs); 
string toString(ColorEntryMode colorMode);

bool isPrintable(char c);
bool isHex(char c);
bool isHex(const string &str);

bool alphaSort(const string &a, const string &b);
bool alphaNumberSort(const string &a, const string &b);

};

#endif
