//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

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

void Interface3ds::generateKeyDownEvent(SDL_Event *event, SDLKey key, char ascii)
{
   event->type = SDL_KEYDOWN;
   event->key.keysym.scancode = 0;
   event->key.keysym.mod = KMOD_NONE;
   event->key.keysym.sym = key;
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

Interface3ds::Interface3ds()
   : mKeysDown(0)
   , mKeysUp(0)
{
   // Do nothing
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
   C3D_Fini();
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
}

// Get events one-by-one
bool Interface3ds::pollEvent(SDL_Event *event)
{
   if(mKeysDown & KEY_A) {
      mKeysDown &= ~(KEY_A);
      generateKeyDownEvent(event, SDLK_RETURN, 'a');
   } else if(mKeysDown & KEY_B) {
      mKeysDown &= ~(KEY_B);
      generateKeyDownEvent(event, SDLK_ESCAPE, '\e');
   } else if(mKeysDown & KEY_SELECT) {
      mKeysDown &= ~(KEY_SELECT);
      generateKeyDownEvent(event, SDLK_ESCAPE, '\e');
   }
   else
      return false;

	//KEY_DRIGHT  = BIT(4),       ///< D-Pad Right
	//KEY_DLEFT   = BIT(5),       ///< D-Pad Left
	//KEY_DUP     = BIT(6),       ///< D-Pad Up
	//KEY_DDOWN   = BIT(7),       ///< D-Pad Down
	//KEY_R       = BIT(8),       ///< R
	//KEY_L       = BIT(9),       ///< L
	//KEY_X       = BIT(10),      ///< X
	//KEY_Y       = BIT(11),      ///< Y
	//KEY_ZL      = BIT(14),      ///< ZL (New 3DS only)
	//KEY_ZR      = BIT(15),      ///< ZR (New 3DS only)
	//KEY_TOUCH   = BIT(20),      ///< Touch (Not actually provided by HID)
	//KEY_CSTICK_RIGHT = BIT(24), ///< C-Stick Right (New 3DS only)
	//KEY_CSTICK_LEFT  = BIT(25), ///< C-Stick Left (New 3DS only)
	//KEY_CSTICK_UP    = BIT(26), ///< C-Stick Up (New 3DS only)
	//KEY_CSTICK_DOWN  = BIT(27), ///< C-Stick Down (New 3DS only)
	//KEY_CPAD_RIGHT = BIT(28),   ///< Circle Pad Right
	//KEY_CPAD_LEFT  = BIT(29),   ///< Circle Pad Left
	//KEY_CPAD_UP    = BIT(30),   ///< Circle Pad Up
	//KEY_CPAD_DOWN  = BIT(31),   ///< Circle Pad Down

   // This quits, but causes an illegal read exception somewhere; where/why??
   // if(kDown & KEY_START)
   //    return false;

   return true;
}

}