//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _INTERFACE3DS_
#define _INTERFACE3DS_

#include "tnl.h"
#include "SDL/SDL.h"
#include <string>

namespace Zap
{

class Interface3ds
{
private:
   TNL::U32 mKeysDown;
   TNL::U32 mKeysUp;

   static std::string getResultSummary(int summaryCode);
   static void generateKeyDownEvent(SDL_Event *event, SDLKey key, char ascii = '\0');
   static void generateKeyUpEvent(SDL_Event *event);

   void initGFX();
   void initFS();
   void initSocket();

public:
   Interface3ds();

   void init();
   void shutdown();
   bool shouldDoMainLoop();
   void fetchEvents();
   bool pollEvent(SDL_Event *event);
};

extern Interface3ds interface3ds;
}

#endif // _INTERFACE3DS_
