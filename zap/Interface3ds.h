//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _INTERFACE3DS_
#define _INTERFACE3DS_

#include <string>

namespace Zap
{

class Interface3ds
{
private:
   static std::string getResultSummary(int summaryCode);
   void initGFX();
   void initFS();
   void initSocket();

public:
   void init();
   void shutdown();
   bool doEvents();
};

extern Interface3ds interface3ds;
}

#endif // _INTERFACE3DS_
