//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UIMSG_H_
#define _UIMSG_H_

#include "UI.h"
#include "Color.h"

namespace Zap
{

class Game;

class MessageUserInterface : public UserInterface
{
   typedef UserInterface Parent;

private:
   static const S32 mNumLines = 9;
   char *mTitle;
   U32 mHeight;
   U32 mWidth;
   U32 mFadeTime;    // Time message will be displayed (ms)   0 -> "Hit any key to continue"
   bool mBox;        // Draw box around message?
   Color mMessageColor;
   Timer mFadeTimer;
   U32 mVertOffset;

public:
   explicit MessageUserInterface(ClientGame *game);     // Constructor
   virtual ~MessageUserInterface();

   char *mMessage[mNumLines];
   void onActivate();
   void setMessage (S32 id, char *message);  // Set a line of message
   void setTitle(char *message);             // Set menu title
   void setSize(U32 width, U32 height);      // Set size of menu
   void setFadeTime(U32 time);
   void setStyle(U32 style);                 // Use a preset menu style
   void reset();
   void idle(U32 t);
   void render();
   void quit();
   bool onKeyDown(InputCode inputCode);
};


}

#endif


