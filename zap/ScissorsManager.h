//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _SCISSORS_MANAGER_H_
#define _SCISSORS_MANAGER_H_

#include "Point.h"
#include "ConfigEnum.h"          // For DisplayMode def
#include "tnlTypes.h"
using namespace TNL; 


namespace Zap
{

class ClientGame;

// Class for managing scissor settings and reducing repeated code

class ScissorsManager
{
private:
   bool mScissorsWasEnabled;
   S32 mScissorBox[4];
   bool mManagerEnabled;

public:
   void enable(bool enable, DisplayMode displayMode, F32 x, F32 y, F32 width, F32 height);  // Store previous scissors settings
   void disable();                                                                          // Restore previous scissors settings
};


};

#endif

