//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "Interface3ds.h"
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

void Interface3ds::initGFX()
{
   gfxInitDefault();
   C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
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

}