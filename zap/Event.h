//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _EVENT_H_
#define _EVENT_H_

#include "InputCode.h"
#include "ConfigEnum.h"
#include "tnlTypes.h"

union SDL_Event;     // Outside Zap namespace, please!

using namespace TNL;

namespace Zap {

class ClientGame;
class UserInterface;


class Event 
{
private:
   static bool mAllowTextInput;   // Flag to allow text translation pass-through

   static void setMousePos(UserInterface *currentUI, S32 x, S32 y, DisplayMode mode);

   static void inputCodeUp(UserInterface *currentUI, InputCode inputCode);
   static bool inputCodeDown(UserInterface *currentUI, InputCode inputCode);

   static void onKeyDown(ClientGame *game, SDL_Event *event);
   static void onKeyUp(UserInterface *currentUI, SDL_Event *event);
   static void onTextInput(UserInterface *currentUI, char unicode);
   static void onMouseMoved(UserInterface *currentUI, S32 x, S32 y, DisplayMode mode);
   static void onMouseWheel(UserInterface *currentUI, bool Up, bool Down);  //Not implemented
   static void onMouseButtonDown(UserInterface *currentUI, S32 x, S32 y, InputCode inputCode, DisplayMode mode);
   static void onMouseButtonUp(UserInterface *currentUI, S32 x, S32 y, InputCode inputCode, DisplayMode mode);
   static void onControllerButtonDown(UserInterface *currentUI, U8 deviceId, U8 button);
   static void onControllerButtonUp(UserInterface *currentUI, U8 deviceId, U8 button);
   static void onResized(ClientGame *game, S32 width, S32 height);
   static void onUser(U8 type, S32 code, void* data1, void* data2);
   static void onControllerAdded(S32 deviceId);
   static void onControllerRemoved(S32 deviceId);

public:
   Event();
   virtual ~Event();
   static void onMouseMoved(S32 x, S32 y, DisplayMode mode);
   static void onControllerAxis(ClientGame *game, U8 deviceId, U8 axis, S16 value);

   static void onEvent(ClientGame *game, SDL_Event *event);
};

}
#endif /* INPUT_H_ */
