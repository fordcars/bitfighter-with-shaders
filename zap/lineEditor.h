//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _LINE_EDITOR_H_
#define _LINE_EDITOR_H_

#include "tnlTypes.h"
#include "tnlVector.h"
#include "LineEditorFilterEnum.h"
#include "Timer.h"
#include "InputCodeEnum.h"

#include <string>

using namespace std;

namespace Zap
{


//
// Class to manage all sorts of single-line editing tasks
//
class LineEditor
{
private:
   // Don't put another string after this or you'll get a weird memory bug that will
   // drain precious hours of your life away trying to figure out...
   string mLine;

   bool mMasked;

   string mPrompt;

   static Timer mBlinkTimer;

   // For tab expansion 
   Vector<string> mMatchList;
   S32 mMatchIndex;
   void buildMatchList(const Vector<string> *candidates, const string &partial);

   static const char MASK_CHAR = '*';

public:
   U32 mMaxLen;
   U32 mDisplayedCharacters;
   U32 mCursorOffset;

   LineEditor(U32 maxLength = 256, string value = "", U32 displayedCharacters = 0xFFFF);   // Constructor
   virtual ~LineEditor();

   U32 length();                                // Returns line length in chars
   bool addChar(char c);                        // Returns true if char was added to line
   void backspacePressed();                     // User hit Backspace
   void deletePressed();                        // User hit Delete
   bool handleKey(InputCode inputCode);
   void clear();                                // Clear the string and tab-expansion matchlist
   char at(U32 pos) const;                      // Get char at pos
   bool isEmpty() const;                        // Is string empty

   void setSecret(bool secret);

   LineEditorFilter mFilter;
   void setFilter(LineEditorFilter filter);

   string getString() const;                    // Return the string in string format
   const string *getStringPtr() const;
   string getDisplayString() const;
   string getStringBeforeCursor() const;
   S32 getCursorOffset() const;

   void setString(const string &str);           // Set the string
   void setPrompt(const string &prompt);
   string getPrompt() const;
   const char *c_str() const;                   // Return the string in c_str format

   void drawCursor(S32 x, S32 y, S32 fontSize);                             // Draw our cursor, assuming string is drawn at x,y
   void drawCursorAngle(F32 x, F32 y, F32 fontSize, F32 angle);             // Draw our cursor, assuming string is drawn at x,y at specified angle
   void drawCursor(S32 x, S32 y, S32 fontSize, S32 startingWidth);   // Draw cursor starting at a given width

   // For tab expansion 
   void completePartial(const Vector<string> *candidates, const string &partial, std::size_t replacePos, const string &appender, bool wrapQuotes = true);

   S32 getMaxLen();

   // LineEditors are equal if their values are equal
   bool operator==(LineEditor &lineEditor) const;

};


};

#endif
