//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "Interface3ds.h"
#include <3ds.h>
#include <citro3d.h>

namespace Zap
{

Interface3ds interface3ds;

void Interface3ds::initConsole()
{
   gfxInitDefault();
   consoleInit(GFX_BOTTOM, consoleGetDefault());
   consoleDebugInit(debugDevice_CONSOLE);
}

}