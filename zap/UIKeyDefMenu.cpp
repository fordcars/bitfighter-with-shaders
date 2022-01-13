//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIKeyDefMenu.h"

#include "UIManager.h"

#include "DisplayManager.h"
#include "Renderer.h"
#include "Joystick.h"
#include "JoystickRender.h"
#include "ClientGame.h"
#include "Cursor.h"

#include "Colors.h"
#include "Intervals.h"

#include "RenderUtils.h"

#include <string>
#include <math.h>
#include "FontManager.h"


namespace Zap
{

// Constructor
KeyDefMenuItem::KeyDefMenuItem(const char *text, U32 col, BindingNameEnum PC, string helpStr)
{
   this->text = text;
   column = col;
   primaryControl = PC;
   helpString = helpStr;
}


// Destructor
KeyDefMenuItem::~KeyDefMenuItem()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
KeyDefMenuUserInterface::KeyDefMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "Define Keys";
   mMenuSubTitle = "";
   mMenuFooter = "UP, DOWN, LEFT, RIGHT to choose | ENTER to select | ESC exits menu";

   errorMsgTimer.setPeriod(SIX_SECONDS);
}


// Destructor
KeyDefMenuUserInterface::~KeyDefMenuUserInterface()
{
   // Do nothing
}


// Some constants used for positioning menu items and coordinating mouse position
static S32 offset = 5; 
static S32 yStart = UserInterface::vertMargin + 115;
static S32 height = 30; 
static S32 firstItemInCol2 = 0;     // Set in onActivate()


void KeyDefMenuUserInterface::onActivate()
{
   mDisableShipKeyboardInput = true;      // Keep keystrokes from getting to game
   selectedIndex = 0;                     // First item selected when we begin
   changingItem = NONE;                   // Not changing anything at the moment...

   // Display an intitial message to users
   errorMsgTimer.clear();
   errorMsg = "";

   GameSettings *settings = getGame()->getSettings();
   InputCodeManager *inputCodeManager = settings->getInputCodeManager();
   InputMode inputMode = inputCodeManager->getInputMode();

   if(inputMode == InputModeJoystick)
      mMenuTitle = "Define Keys: [Joystick]";
   else
      mMenuTitle = "Define Keys: [Keyboard]";

   mMenuSubTitleColor = Colors::white;   

   menuItems.clear();

   if(inputMode == InputModeJoystick)
   {
      // Col 1
      menuItems.push_back(KeyDefMenuItem("Advance Weapon",        1, BINDING_ADVWEAP,
                                         "Toggles your weapons, use as an alternative to Select Weapon commands"));
      menuItems.push_back(KeyDefMenuItem("Advance Weapon (alt.)", 1, BINDING_ADVWEAP2,
                                         "Alternate toggle key to give you more flexibility"));
      menuItems.push_back(KeyDefMenuItem("Previous Weapon",       1, BINDING_PREVWEAP,
                                         "Toggles your weapons, use as an alternative to Select Weapon commands"));
      menuItems.push_back(KeyDefMenuItem("Activate Module 1",     1, BINDING_MOD1,
                                         "Module 1 will be active while this key/button is held down"));
      menuItems.push_back(KeyDefMenuItem("Activate Module 2",     1, BINDING_MOD2,
                                         "Module 2 will be active while this key/button is held down"));
      menuItems.push_back(KeyDefMenuItem("Drop Flag",             1, BINDING_DROPITEM, ""));

      menuItems.push_back(KeyDefMenuItem("Configure Loadout",     1, BINDING_LOADOUT, ""));
      menuItems.push_back(KeyDefMenuItem("Toggle Map Mode",       1, BINDING_CMDRMAP, ""));
      menuItems.push_back(KeyDefMenuItem("Show Scoreboard",       1, BINDING_SCRBRD,
                                         "Scoreboard will be visible while this key/button is held down"));
      menuItems.push_back(KeyDefMenuItem("Toggle Level Rating",   1, BINDING_TOGGLE_RATING, ""));

      // Col 2
      firstItemInCol2 = menuItems.size();

      menuItems.push_back(KeyDefMenuItem("Select Weapon 1",       2, BINDING_SELWEAP1,
                                         "Use as an alternative to Advance Weapon"));
      menuItems.push_back(KeyDefMenuItem("Select Weapon 2",       2, BINDING_SELWEAP2,
                                         "Use as an alternative to Advance Weapon"));
      menuItems.push_back(KeyDefMenuItem("Select Weapon 3",       2, BINDING_SELWEAP3,
                                         "Use as an alternative to Advance Weapon"));
      menuItems.push_back(KeyDefMenuItem("Quick Chat",            2, BINDING_QUICKCHAT, ""));
      menuItems.push_back(KeyDefMenuItem("Team Chat",             2, BINDING_TEAMCHAT,  ""));
      menuItems.push_back(KeyDefMenuItem("Global Chat",           2, BINDING_GLOBCHAT,  ""));
      menuItems.push_back(KeyDefMenuItem("Enter Command",         2, BINDING_CMDCHAT,   ""));
      menuItems.push_back(KeyDefMenuItem("Record Voice Msg",      2, BINDING_TOGVOICE,  ""));
   }
   else     // Keyboard mode
   {
      // Col 1
      menuItems.push_back(KeyDefMenuItem("Ship Up",           1, BINDING_UP, ""));
      menuItems.push_back(KeyDefMenuItem("Ship Down",         1, BINDING_DOWN, ""));
      menuItems.push_back(KeyDefMenuItem("Ship Left",         1, BINDING_LEFT, ""));
      menuItems.push_back(KeyDefMenuItem("Ship Right",        1, BINDING_RIGHT,""));
      menuItems.push_back(KeyDefMenuItem("Fire",              1, BINDING_FIRE,
                                         "The mouse will always be used to aim your ship"));
      menuItems.push_back(KeyDefMenuItem("Activate Module 1", 1, BINDING_MOD1,
                                         "Module 1 will be active while this key/button is held down"));
      menuItems.push_back(KeyDefMenuItem("Activate Module 2", 1, BINDING_MOD2,
                                         "Module 2 will be active while this key/button is held down"));
      menuItems.push_back(KeyDefMenuItem("Drop Flag",         1, BINDING_DROPITEM,
                                         "Drop flag when this key is pressed"));
      menuItems.push_back(KeyDefMenuItem("Configure Ship Loadouts", 1, BINDING_LOADOUT,   ""));
      menuItems.push_back(KeyDefMenuItem("Toggle Map Mode",   1, BINDING_CMDRMAP, ""));
      menuItems.push_back(KeyDefMenuItem("Show Scoreboard",   1, BINDING_SCRBRD,
                                         "Scoreboard will be visible while this key/button is held down"));
      menuItems.push_back(KeyDefMenuItem("Toggle Level Rating", 1, BINDING_TOGGLE_RATING, ""));

      // Col 2
      firstItemInCol2 = menuItems.size();

      menuItems.push_back(KeyDefMenuItem("Select Weapon 1",         2, BINDING_SELWEAP1,
                                         "Use as an alternative to Advance Weapon"));
      menuItems.push_back(KeyDefMenuItem("Select Weapon 2",         2, BINDING_SELWEAP2,
                                         "Use as an alternative to Advance Weapon"));
      menuItems.push_back(KeyDefMenuItem("Select Weapon 3",         2, BINDING_SELWEAP3,
                                         "Use as an alternative to Advance Weapon"));
      menuItems.push_back(KeyDefMenuItem("Advance Weapon",          2, BINDING_ADVWEAP,
                                         "Toggles your weapons, use as an alternative to Select Weapon commands"));
      menuItems.push_back(KeyDefMenuItem("Advance Weapon (alt.)",   2, BINDING_ADVWEAP2,
                                         "Alternate toggle key to give you more flexibility"));
      menuItems.push_back(KeyDefMenuItem("Previous Weapon",         2, BINDING_PREVWEAP,
                                         "Toggles your weapons, use as an alternative to Select Weapon commands"));
      menuItems.push_back(KeyDefMenuItem("Quick Chat",              2, BINDING_QUICKCHAT, ""));
      menuItems.push_back(KeyDefMenuItem("Team Chat",               2, BINDING_TEAMCHAT,  ""));
      menuItems.push_back(KeyDefMenuItem("Global Chat",             2, BINDING_GLOBCHAT,  ""));
      menuItems.push_back(KeyDefMenuItem("Enter Command",           2, BINDING_CMDCHAT,   ""));

      menuItems.push_back(KeyDefMenuItem("Record Voice Msg",        2, BINDING_TOGVOICE,  ""));
   }

   S32 itemCount[] = { 0, 0 };

   for(S32 i = 0; i < menuItems.size(); i++)
      itemCount[menuItems[i].column - 1]++;

   maxMenuItemsInAnyCol = MAX(itemCount[0], itemCount[1]);
}


void KeyDefMenuUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);
   errorMsgTimer.update(timeDelta);
}


// Finds out if key is already assigned to something else
bool KeyDefMenuUserInterface::isDuplicate(S32 key, const Vector<KeyDefMenuItem> &menuItems)
{
   S32 size = menuItems.size();
   S32 count = 0;

   GameSettings *settings = getGame()->getSettings();

   InputCode targetInputCode = getInputCode(settings, menuItems[key].primaryControl);

   for(S32 i = 0; i < size && count < 2; i++)
      if(getInputCode(settings, menuItems[i].primaryControl) == targetInputCode)
         count++;

   return count >= 2;
}


void KeyDefMenuUserInterface::render()
{
   Renderer& r = Renderer::get();
   FontManager::pushFontContext(MenuContext);

   // Draw the game screen, then dim it out so you can still see it under our overlay
   if(getGame()->getConnectionToServer())
      getUIManager()->renderAndDimGameUserInterface();

   r.setColor(Colors::white);
   drawCenteredString(vertMargin, 30, mMenuTitle);
   drawCenteredString(vertMargin + 35, 18, mMenuSubTitle);

   r.setColor(Colors::menuHelpColor);
   drawCenteredString(vertMargin + 63, 14, "You can define different keys for keyboard or joystick mode.  Switch in Options menu.");

   r.setColor(Colors::white);
   drawCenteredString(DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - 20, 18, mMenuFooter);

   if(selectedIndex >= menuItems.size())
      selectedIndex = 0;

   S32 size = menuItems.size();

   for(S32 i = 0; i < size; i++)
   {
      S32 y = yStart + (i - ((i < firstItemInCol2) ? 0 : firstItemInCol2)) * height;

		S32 Column_Width = DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2 - horizMargin;
		S32 xPos = (menuItems[i].column - 1) * Column_Width + horizMargin * 2;

      if(selectedIndex == i)       // Highlight selected item
         drawFilledRect(xPos - horizMargin, y, 
                        xPos + Column_Width - horizMargin, y + height + 1, 
                        Colors::blue40, Colors::blue);

      // Draw item text
      r.setColor(Colors::cyan);
      drawString(xPos, y + offset, 15, menuItems[i].text);

		xPos += Column_Width * 14 / 20;

      if(changingItem == i)
      {
         r.setColor(Colors::red);
         const S32 size = 13;
         drawCenteredString_fixed(xPos, y + offset + 1 + size, size, "Press Key or Button");
      }
      else
      {
         bool dupe = isDuplicate(i, menuItems); 
         const Color *color = dupe ? &Colors::red : NULL;

         JoystickRender::renderControllerButton(F32(xPos), F32(y + offset), 
               getInputCode(getGame()->getSettings(), menuItems[i].primaryControl), color);
      }
   }

   S32 yPos = yStart + maxMenuItemsInAnyCol * height + 10;

   // Draw the help string
   r.setColor(Colors::green);
   drawCenteredString(yPos, 15, menuItems[selectedIndex].helpString.c_str());

   yPos += 20;

   // Draw some suggestions
   r.setColor(Colors::yellow);
   if(getGame()->getInputMode() == InputModeJoystick)
      drawCenteredString(yPos, 15, "HINT: You will be using the left joystick to steer, the right to fire");
   else
      drawCenteredString(yPos, 15, "HINT: You will be using the mouse to aim, so make good use of your mouse buttons");

   if(errorMsgTimer.getCurrent())
   {
      yPos += 20;
      F32 alpha = 1.0;
      if(errorMsgTimer.getCurrent() < 1000)
         alpha = (F32) errorMsgTimer.getCurrent() / 1000;

      r.setColor(Colors::red, alpha);
      drawCenteredString(yPos, 15, errorMsg.c_str());
   }

   FontManager::popFontContext();
}


bool KeyDefMenuUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode)) { /* Do nothing */ }

   // InputCode entry
   else if(changingItem != NONE)
   {
      playBoop();

      if(inputCode == KEY_ESCAPE || inputCode == BUTTON_BACK)
      {
         changingItem = NONE;
         return true;
      }

      // Check for reserved keys (F1-F12, Ctrl, Esc, Button_Back)
      if(inputCode >= KEY_F1 && inputCode <= KEY_F12)
      {
         errorMsgTimer.reset();
         errorMsg = "Keys F1 - F12 are reserved.  You cannot redefine them.  Sorry!";
         return true;
      }
      else if(inputCode == KEY_CTRL)
      {
         errorMsgTimer.reset();
         errorMsg = "Control key is reserved.  You cannot use it for binding.  Sorry!";
         return true;
      }

      // Fail silently on joystick motion
      if(inputCode >= STICK_1_LEFT && inputCode <= STICK_2_DOWN)
         return true;

      // Assign key
      setInputCode(getGame()->getSettings(), menuItems[changingItem].primaryControl, inputCode);
      changingItem = NONE;

      if(inputCode == KEY_CTRL)
      {
         errorMsgTimer.reset();
         errorMsg = "Be careful when using the Ctrl key -- some keys will not work when Ctrl is pressed";
      }
      return true;
   }

   // We're not doing InputCode entry, so let's try menu navigation
   if(inputCode == KEY_SPACE || inputCode == KEY_ENTER  || inputCode == KEY_KEYPAD_ENTER ||
         inputCode == BUTTON_START || inputCode == MOUSE_LEFT)      // Set key for selected item
   {
      playBoop();
      changingItem = selectedIndex;
   }
   else if(inputCode == KEY_RIGHT  || inputCode == BUTTON_DPAD_RIGHT || inputCode == KEY_LEFT || inputCode == BUTTON_DPAD_LEFT)    // Change col
   {
      playBoop();

      if(menuItems[selectedIndex].column == 1)     // Switching to right column
         selectedIndex += firstItemInCol2;
      else                                         // Switching to left column
      {
         selectedIndex -= firstItemInCol2;
         while(menuItems[selectedIndex].column == 2)
            selectedIndex--;
      }

      Cursor::disableCursor();                    // Turn off cursor
   }
   else if(inputCode == KEY_ESCAPE || inputCode == BUTTON_BACK)   // Quit
   {
      playBoop();
      saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());

      getUIManager()->reactivatePrevUI();      // to gOptionsMenuUserInterface
   }
   else if(inputCode == KEY_UP || inputCode == BUTTON_DPAD_UP)    // Prev item
   {
      playBoop();

      selectedIndex--;
      if(selectedIndex < 0)
         selectedIndex = menuItems.size() - 1;

      Cursor::disableCursor();    // Turn off cursor
   }
   else if(inputCode == KEY_DOWN || inputCode == BUTTON_DPAD_DOWN)   // Next item
   {
      playBoop();

      selectedIndex++;
      if(selectedIndex >= menuItems.size())
         selectedIndex = 0;

      Cursor::disableCursor();                       // Turn off cursor
   }
   else              // No key has been handled
      return false;

   // A key was handled
   return true;
}


// Handle mouse input, figure out which menu item we're over, and highlight it
void KeyDefMenuUserInterface::onMouseMoved()
{
   Parent::onMouseMoved();

   Cursor::enableCursor();  // Show cursor when user moves mouse

   const Point *mousePos = DisplayManager::getScreenInfo()->getMousePos();

   // Which column is the mouse in?  Left half of screen = 0, right half = 1
   S32 col = (mousePos->x < (DisplayManager::getScreenInfo()->getGameCanvasWidth() - horizMargin) / 2) ? 0 : 1;
   S32 row = min(max(static_cast<int>(floor(( mousePos->y - yStart ) / height)), 0), menuItems.size() - 1);

   selectedIndex = min(max(row + firstItemInCol2 * col, 0), menuItems.size() - 1);    // Bounds checking

   // If we're in the wrong column, get back to the right one.  This can happen if there are more
   // items in the right column than the left.  Note that our column numbering scheme is different
   // between col and the column field.  Also corrects for that dummy null item in the joystick
   // section of the controls.
   if(col == 0)
      while(menuItems[selectedIndex].column == 2)
         selectedIndex--;
   else
      while(menuItems[selectedIndex].column == 1)
         selectedIndex++;
}


};


