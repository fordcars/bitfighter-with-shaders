//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "Joystick.h"
#include "GameSettings.h"
#include "IniFile.h"
#include "Colors.h"

#include "stringUtils.h"

#include "tnlLog.h"

#include "SDL.h"
#include "SDL_stdinc.h"

#include <map>


namespace Zap {


// Linker needs these declared like this, why?
// private
SDL_GameController *Joystick::sdlController = NULL;

// public
U32 Joystick::ButtonMask = 0;
#ifndef BF_PLATFORM_3DS
S16 Joystick::rawAxesValues[SDL_CONTROLLER_AXIS_MAX]; // Array of the current axes values
#endif
S16 Joystick::LowerSensitivityThreshold = 4900;   // out of 32767, ~15%, any less than this is ends up as zero
S16 Joystick::UpperSensitivityThreshold = 30000;  // out of 32767, ~91%, any more than this is full amount

// private
// Aligned with SDL_GameControllerButton.  For now this is just an XBox controller
Joystick::ButtonInfo Joystick::controllerButtonInfos[ControllerButtonMax] =
{
      // These first values must be aligned with SDL_GameControllerButton enum
      // and the ControllerButton enum
      { "A",   Colors::white, ButtonShapeRound, ButtonSymbolNone },
      { "B",   Colors::white, ButtonShapeRound, ButtonSymbolNone },
      { "X",   Colors::white, ButtonShapeRound, ButtonSymbolNone },
      { "Y",   Colors::white, ButtonShapeRound, ButtonSymbolNone },
      { "Ba",  Colors::white, ButtonShapeRoundedRect, ButtonSymbolNone },
      { "G",   Colors::white, ButtonShapeHorizEllipse, ButtonSymbolNone },
      { "St",  Colors::white, ButtonShapeRoundedRect, ButtonSymbolNone },
      { "9",   Colors::white, ButtonShapeRound, ButtonSymbolNone },
      { "10",  Colors::white, ButtonShapeRound, ButtonSymbolNone },
      { "L",   Colors::white, ButtonShapeRect, ButtonSymbolNone },
      { "R",   Colors::white, ButtonShapeRect, ButtonSymbolNone },
      { "",    Colors::white, ButtonShapeDPadUp, ButtonSymbolNone },
#ifndef BF_PLATFORM_3DS
      { "",    Colors::white, ButtonShapeDPadDown, ButtonSymbolNone },
      { "",    Colors::white, ButtonShapeDPadLeft, ButtonSymbolNone },
      { "",    Colors::white, ButtonShapeDPadRight, ButtonSymbolNone },
      // Additional hybrid buttons (start at index SDL_CONTROLLER_BUTTON_MAX)
      { "LT",  Colors::white, ButtonShapeRect, ButtonSymbolNone },
      { "RT",  Colors::white, ButtonShapeRect, ButtonSymbolNone },
#endif
};


// Constructor
Joystick::Joystick()
{
   // Do nothing
}


// Destructor
Joystick::~Joystick()
{
   // Do nothing
}


// Make sure "SDL_Init(0)" was done before calling this function, otherwise joystick will fail to work on windows.
bool Joystick::initJoystick(GameSettings *settings)
{
   GameSettings::DetectedControllerList.clear();
   GameSettings::UseControllerIndex = -1;

#ifndef BF_PLATFORM_3DS
   // Allows multiple joysticks with each using a copy of Bitfighter
   SDL_setenv("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1", 0);

   if(!SDL_WasInit(SDL_INIT_GAMECONTROLLER) &&
         SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))
   {
      logprintf("Unable to initialize the game controller subsystem");
      return false;
   }

   // Load the default controller database
   SDL_GameControllerAddMappingsFromFile(
         joindir(settings->getFolderManager()->iniDir, "gamecontrollerdb.txt").c_str()
         );

   // Load the user-specific gamecontroller database.  These will override any
   // in the main database since they're loaded afterwards
   SDL_GameControllerAddMappingsFromFile(
         joindir(settings->getFolderManager()->iniDir, "usergamecontrollerdb.txt").c_str()
         );

   // How many joysticks are there
   S32 joystickCount = SDL_NumJoysticks();

   // No joysticks found
   if(joystickCount <= 0)
      return false;

   logprintf("%d joystick(s) detected:", joystickCount);

   for(S32 i = 0; i < joystickCount; i++)
   {
      // A GameController is a specific type of joystick
      if(SDL_IsGameController(i))
      {
         const char *controllerName = SDL_GameControllerNameForIndex(i);

         logprintf("  %d. [GameController] \"%s\"", i + 1, controllerName);
         GameSettings::DetectedControllerList.insert(pair<S32,string>(i,controllerName));
      }

      // Not detected as a game controller
      else
      {
         const char *joystickName = SDL_JoystickNameForIndex(i);

         logprintf("  %d. [Joystick] (not compatible) \"%s\"", i + 1, joystickName);
         logprintf("");
         logprintf("");
         logprintf("There are a couple of things you can do to fix this.  First, check the SDL database to see if your stick has been added.");
         logprintf("Copy the contents of https://github.com/gabomdq/SDL_GameControllerDB/blob/master/gamecontrollerdb.txt into the copy of");
         logprintf("gamecontrollerdb.txt that was shipped with Bitfighter(it should be in the install folder) and restart the game.  ");
         logprintf("");
         logprintf("If that doesn't help, you may need to create a new joystick definition for your controller. To do this, download the");
         logprintf("SDL2 Gamepad Tool from http://www.generalarcade.com/gamepadtool, uncompress it, and run the  executable.  This tool will");
         logprintf("help you create a definition string for your joystick.  Copy it to the clipboard, and add it to the");
         logprintf("usergamecontrollerdb.txt file in the Bitfighter install folder, and restart the game.  If you know how to use GitHub,");
         logprintf("you can also create a pull request to submit your definition to the https://github.com/gabomdq/SDL_GameControllerDB");
         logprintf("project.");

         // TODO: Do some sort of auto-detection and string output of the
         // joystick hardware mappings and create a gamecontroller out of this
         // joystick.  Maybe integrate SDL/test/controllermap.c from SDL hg
      }
   }

   // Set the controller number we will use during the game unless it was already
   // set by a command line arg in GameSettings.cpp.  This will be the first
   // detected controller from above.  Unless there were no controllers detected, in which
   // case we'll bail.
   if(GameSettings::UseControllerIndex == -1)
   {
      if(GameSettings::DetectedControllerList.size() == 0)
         return false;

      GameSettings::UseControllerIndex = GameSettings::DetectedControllerList.begin()->first;
   }
#endif

   return true;
}


bool Joystick::enableJoystick(GameSettings *settings, bool hasBeenOpenedBefore)
{
#ifndef BF_PLATFORM_3DS
   // Need to close the controller to avoid having 2 being active at the same time
   if(sdlController != NULL) {
      SDL_GameControllerClose(sdlController);
      sdlController = NULL;
   }

   // Check that there is a controller available
   if(GameSettings::DetectedControllerList.size() == 0)
      return false;

   // Don't enable controller at all in keyboard mode
   if(settings->getInputMode() == InputModeKeyboard &&
        (hasBeenOpenedBefore || settings->getIniSettings()->alwaysStartInKeyboardMode)) // Don't enable joystick at all in keyboard mode
      return true;

   // Enable controller events
   SDL_GameControllerEventState(SDL_ENABLE);

   // Start using the controller
   sdlController = SDL_GameControllerOpen(GameSettings::UseControllerIndex);
   string controllerName = SDL_GameControllerNameForIndex(GameSettings::UseControllerIndex);
   if(sdlController == NULL)
   {
      logprintf("Error opening controller %d \"%s\"", GameSettings::UseControllerIndex, controllerName.c_str());

      return false;
   }

   logprintf("Using controller %d \"%s\"", GameSettings::UseControllerIndex, controllerName.c_str());

   // Set primary input to joystick if any controllers were found
   if(!hasBeenOpenedBefore)
      settings->getInputCodeManager()->setInputMode(InputModeJoystick);
#endif

   return true;
}


void Joystick::shutdownJoystick()
{
#ifndef BF_PLATFORM_3DS
   if(sdlController != NULL) {
      SDL_GameControllerClose(sdlController);
      sdlController = NULL;
   }

   if(SDL_WasInit(SDL_INIT_GAMECONTROLLER))
      SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}


// This handles both the
Joystick::ButtonInfo Joystick::getButtonInfo(S16 button)
{
   static const ButtonInfo DefaultButtonInfo = { "", Colors::white, ButtonShapeRound, ButtonSymbolNone };

   if(button >= ControllerButtonMax)
      return DefaultButtonInfo;

   return controllerButtonInfos[button];
}


Joystick::ButtonSymbol Joystick::stringToButtonSymbol(const string &label)
{
   if(label == "PSCIRCLE")
      return ButtonSymbolPsCircle;
   else if(label == "PSCROSS")
      return ButtonSymbolPsCross;
   else if(label == "PSSQUARE")
      return ButtonSymbolPsSquare;
   else if(label == "PSTRIANGLE")
      return ButtonSymbolPsTriangle;
   else if(label == "SMALLLEFTTRIANGLE")
      return ButtonSymbolSmallLeftTriangle;
   else if(label == "SMALLRIGHTTRIANGLE")
      return ButtonSymbolSmallRightTriangle;

   return ButtonSymbolNone;
}


} /* namespace Zap */
