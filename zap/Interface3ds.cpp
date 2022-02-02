//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifdef BF_PLATFORM_3DS

#include "Interface3ds.h"
#undef BIT
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <malloc.h>

// From https://github.com/devkitPro/3ds-examples/blob/master/network/sockets/source/sockets.c
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

namespace Zap
{

Interface3ds interface3ds;

struct KeyMapping3ds
{
   U32 keyMask;
   SDLKey sdlKey;
   char ascii;

   KeyMapping3ds(U32 keyMask, SDLKey sdlKey, char ascii = '\0')
      : keyMask(keyMask), sdlKey(sdlKey), ascii(ascii) {}
};

static KeyMapping3ds keyMappings[] = {
   { KEY_A,          SDLK_RETURN, '\r' },
   { KEY_B,          SDLK_ESCAPE, '\e' },
   { KEY_X,          SDLK_TAB,    '\t' },
   { KEY_Y,          SDLK_z,       'z' },
   { KEY_R,          SDLK_SPACE,       },
   { KEY_L,          SDLK_SPACE,       },
   { KEY_DRIGHT,     SDLK_RIGHT        },
   { KEY_DLEFT,      SDLK_LEFT         },
   { KEY_DUP,        SDLK_UP           },
   { KEY_DDOWN,      SDLK_DOWN         },
   { KEY_CPAD_RIGHT, SDLK_d,       'd' },
   { KEY_CPAD_LEFT,  SDLK_a,       'a' },
   { KEY_CPAD_UP,    SDLK_w,       'w' },
   { KEY_CPAD_DOWN,  SDLK_s,       's' },
   { KEY_SELECT,     SDLK_c,       'c' }
};

Interface3ds::Interface3ds()
   : mKeysDown(0)
   , mKeysUp(0)
{
   // Do nothing
}

// Static
std::string Interface3ds::getResultSummary(int summaryCode)
{
   switch(summaryCode)
   {
   case RS_SUCCESS:
      return "RS_SUCCESS";

   case RS_NOP:
      return "RS_NOP";

   case RS_WOULDBLOCK:
      return "RS_WOULDBLOCK";

   case RS_OUTOFRESOURCE:
      return "RS_OUTOFRESOURCE";

   case RS_NOTFOUND:
      return "RS_NOTFOUND";

   case RS_INVALIDSTATE:
      return "RS_INVALIDSTATE";

   case RS_NOTSUPPORTED:
      return "RS_NOTSUPPORTED";

   case RS_INVALIDARG:
      return "RS_INVALIDARG";

   case RS_WRONGARG:
      return "RS_WRONGARG";

   case RS_CANCELED:
      return "RS_CANCELED";

   case RS_STATUSCHANGED:
      return "RS_STATUSCHANGED";

   case RS_INTERNAL:
      return "RS_INTERNAL";

   case RS_INVALIDRESVAL:
      return "RS_INVALIDRESVAL";

   default:
      return "Unknown summary code: " + std::to_string(summaryCode);
   }
}

// Static
void Interface3ds::createKeyEvent(SDL_Event *event, SDL_EventType eventType, SDLKey sdlKey, char ascii)
{
   event->type = eventType;
   event->key.keysym.scancode = 0;
   event->key.keysym.mod = KMOD_NONE;
   event->key.keysym.sym = sdlKey;
   event->key.keysym.unicode = ascii;
}

void Interface3ds::initGFX()
{
   gfxInitDefault();
   consoleInit(GFX_BOTTOM, consoleGetDefault());
   consoleDebugInit(debugDevice_CONSOLE);
}

void Interface3ds::initFS()
{
   Result rc = romfsInit();
   if(rc)
   {
      std::string msg = "romfsInit error: " + getResultSummary(R_SUMMARY(rc)) + "\n";
      printf(msg.c_str());
   }
}

void Interface3ds::initSocket()
{
   static u32 *SOC_buffer = NULL;
   SOC_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
   Result rc = socInit(SOC_buffer, SOC_BUFFERSIZE);
   
   if(rc)
   {
      std::string msg = "socInit error: " + getResultSummary(R_SUMMARY(rc)) + "\n";
      printf(msg.c_str());
   }
}

// Returns true if key event was found.
// Make sure to call fetchEvents() before this!
bool Interface3ds::extractKeyEvent(U32 keyMask, SDL_Event *event, SDLKey sdlKey, char ascii)
{
   if(mKeysDown & keyMask)
   {
      mKeysDown &= ~(keyMask);
      createKeyEvent(event, SDL_KEYDOWN, sdlKey, ascii);
   }
   else if(mKeysUp & keyMask)
   {
      mKeysUp &= ~(keyMask);
      createKeyEvent(event, SDL_KEYUP, sdlKey, ascii);
   }
   else
      return false;

   return true;
}

void Interface3ds::init()
{
   initGFX();
   initFS();
   initSocket();
}

void Interface3ds::shutdown()
{
   socExit();
   romfsExit();
   gfxExit();
}

bool Interface3ds::shouldDoMainLoop()
{
   return aptMainLoop();
}

// Call once per frame
void Interface3ds::fetchEvents()
{
   hidScanInput();
   mKeysDown = hidKeysDown();
   mKeysUp = hidKeysUp();
}

// Get events one-by-one. Make sure to call fetchEvents() before this!
bool Interface3ds::pollEvent(SDL_Event *event)
{
   for(unsigned i = 0; i < sizeof(keyMappings) / sizeof(KeyMapping3ds); ++i)
   {
      KeyMapping3ds &entry = keyMappings[i];
      if(extractKeyEvent(entry.keyMask, event, entry.sdlKey, entry.ascii))
         return true;
   }

   return false;
}

} // namespace Zap

#endif // BF_PLATFORM_3DS