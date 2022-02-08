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

using namespace TNL;

class Interface3ds
{
private:
   U32 mKeysDown;
   U32 mKeysUp;

   static std::string getResultSummary(int summaryCode);
   static void createKeyEvent(SDL_Event *event, SDL_EventType eventType, SDLKey sdlKey, char ascii = '\0');

   void initGFX();
   void initFS();
   void initSocket();
   bool extractKeyEvent(U32 keyMask, SDL_Event *event, SDLKey sdlKey, char ascii = '\0');
   void updateCPad();

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
