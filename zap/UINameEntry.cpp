//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UINameEntry.h"

#include "FontManager.h"
#include "UIGame.h"
#include "UIEditor.h"      // Only used once, could probably be refactored out
#include "UIManager.h"

#include "ClientGame.h"
#include "DisplayManager.h"
#include "Renderer.h"

#include "Colors.h"

#include "stringUtils.h"
#include "RenderUtils.h"

#include <string>
#include <math.h>

namespace Zap
{
using namespace std;

// Constructor
TextEntryUserInterface::TextEntryUserInterface(ClientGame *game) : Parent(game)  
{
   title = "ENTER TEXT:";
   instr1 = "";
   instr2 = "Enter some text above";
   setSecret(false);
   cursorPos = 0;
   resetOnActivate = true;
   lineEditor = LineEditor(MAX_PLAYER_NAME_LENGTH);
}

// Constructor
TextEntryUserInterface::~TextEntryUserInterface()
{
   // Do nothing
}


void TextEntryUserInterface::onActivate()
{
   if(resetOnActivate)
      lineEditor.clear();
}


static const S32 fontSize = 20;
static const F32 fontSizeBig = 30.0f;
static const S32 TextEntryYPos = 325;


F32 TextEntryUserInterface::getFontSize()
{
   F32 maxLineLength = 750.0f;      // Pixels

   // Shrink the text to fit on-screen when text gets very long
   F32 w = (F32)getStringWidthf(fontSizeBig, lineEditor.getDisplayString().c_str());
   if(w > maxLineLength)
      return maxLineLength * fontSizeBig / w;
   else
      return fontSizeBig;
}


void TextEntryUserInterface::render()
{
   Renderer& r = Renderer::get();
   r.setColor(Colors::white);

   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   // Center vertically
   S32 y = TextEntryYPos - 45 ;

   drawCenteredString(y, fontSize, title);
   y += 45;

   r.setColor(Colors::green);
   drawCenteredString(canvasHeight - vertMargin - 2 * fontSize - 5, fontSize, instr1);
   drawCenteredString(canvasHeight - vertMargin - fontSize, fontSize, instr2);

   r.setColor(Colors::white);

   FontManager::pushFontContext(InputContext);

   TNLAssert(y == TextEntryYPos, "Something is off here!");

   S32 x = (S32)drawCenteredString(y, getFontSize(), lineEditor.getDisplayString().c_str());
   lineEditor.drawCursor(x, y, (S32)fontSizeBig);
   FontManager::popFontContext();
}


void TextEntryUserInterface::setSecret(bool secret)
{
   lineEditor.setSecret(secret);
}


string TextEntryUserInterface::getText()
{
   return lineEditor.getString();
}


bool TextEntryUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;

   switch (inputCode)
   {
      case KEY_ENTER:
      case KEY_KEYPAD_ENTER:
         onAccept(lineEditor.c_str());
         return true;
      case KEY_BACKSPACE:
         lineEditor.backspacePressed();
         return true;
      case KEY_DELETE:
         lineEditor.deletePressed();
         return true;
      case KEY_ESCAPE:
         onEscape();
         return true;
      default:
         return false;
   }
}


void TextEntryUserInterface::onTextInput(char ascii)
{
   lineEditor.addChar(ascii);
}


void TextEntryUserInterface::setString(string str)
{
   lineEditor.setString(str);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelNameEntryUserInterface::LevelNameEntryUserInterface(ClientGame *game) : Parent(game)     
{
   title = "ENTER LEVEL TO EDIT:";
   instr1 = "Enter an existing level, or create your own!";
   instr2 = "Arrows / wheel cycle existing levels | Tab completes partial name";
   resetOnActivate = false;
   lineEditor.setFilter(fileNameFilter);
   lineEditor.mMaxLen = MAX_FILE_NAME_LEN;

   mLevelIndex = 0;
   mFoundLevel = false;
}


// Destructor
LevelNameEntryUserInterface::~LevelNameEntryUserInterface()
{
   // Do nothing
}


void LevelNameEntryUserInterface::onEscape()
{
   playBoop();
   getUIManager()->reactivatePrevUI();      //gMainMenuUserInterface
}


void LevelNameEntryUserInterface::onActivate()
{
   Parent::onActivate();
   mLevelIndex = 0;

   mLevels = getGame()->getSettings()->getLevelList();

   // Remove the extension from the level file
   for(S32 i = 0; i < mLevels.size(); i++)
       mLevels[i] = stripExtension(mLevels[i]);

   mFoundLevel = setLevelIndex();
}


// See if the current level is on the list -- if so, set mLevelIndex to that level and return true
bool LevelNameEntryUserInterface::setLevelIndex()
{
   for(S32 i = 0; i < mLevels.size(); i++)
   {
      // Exact match
      if(mLevels[i] == lineEditor.getString())
      {
         mLevelIndex = i;
         return true;
      }
   }
   // is mLevels sorted correctly?
   for(S32 i = 0; i < mLevels.size(); i++)
   {
      // No exact match, but we just passed the item and have selected the closest one alphabetically following
      if(mLevels[i] > lineEditor.getString())
      {
         mLevelIndex = i;
         return false;
      }
   }

   mLevelIndex = 0;     // First item
   return false;
}


bool LevelNameEntryUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
   { 
      // Do nothing -- key handled
   }
   else if(inputCode == KEY_DOWN || inputCode == MOUSE_WHEEL_DOWN)
   {
      if(mLevels.size() == 0)
         return true;

      if(!mFoundLevel)           // If we have a partially entered name, just simulate hitting tab 
      {
         completePartial();      // Resets mFoundLevel
         if(!mFoundLevel)
            mLevelIndex--;       // Counteract increment below
      }

      mLevelIndex++;
      if(mLevelIndex >= mLevels.size())
         mLevelIndex = 0;

      lineEditor.setString(mLevels[mLevelIndex]);
   }

   else if(inputCode == KEY_UP || inputCode == MOUSE_WHEEL_UP)
   {
      if(mLevels.size() == 0)
         return true;

      if(!mFoundLevel)
         completePartial();

      mLevelIndex--;
      if(mLevelIndex < 0)
         mLevelIndex = mLevels.size() - 1;

      lineEditor.setString(mLevels[mLevelIndex]);
   }

   else if(inputCode == KEY_TAB)       // Tab will try to complete a name from whatever the user has already typed
      completePartial();

   else                                // Normal typed key - not handled
   {
      mFoundLevel = setLevelIndex();   // Update levelIndex to reflect current level
      return lineEditor.handleKey(inputCode);
   }

   // Something was handled!
   return true;
}


void LevelNameEntryUserInterface::completePartial()
{
   mFoundLevel = setLevelIndex();
   lineEditor.completePartial(&mLevels, lineEditor.getString(), 0, "", false);
   setLevelIndex();   // Update levelIndex to reflect current level
}


void LevelNameEntryUserInterface::onAccept(const char *name)
{
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();
   ui->setLevelFileName(name);

   playBoop();
   getUIManager()->activate(ui, false);
   
   // Get that baby into the INI file
   getGame()->getSettings()->getIniSettings()->lastEditorName = name;
   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());             
   // Should be...
   //getGame()->getIniSettings()->saveSettingsToDisk();
}

extern void drawHorizLine(S32,S32,S32);

void LevelNameEntryUserInterface::render()
{
   static const S32 linesBefore = 6;
   static const S32 linesAfter = 3;

   S32 startIndex = MAX(0, mLevelIndex - linesBefore);
   S32 endIndex = MIN(mLevels.size() - 1, mLevelIndex + linesAfter);

   Renderer::get().setColor(Colors::gray20);
   FontManager::pushFontContext(MenuContext);

   for(S32 i = startIndex; i <= endIndex; i++)
   {
      if(i != mLevelIndex)
         drawCenteredString(TextEntryYPos + F32(i - mLevelIndex) * ((F32)fontSize * 2.0f), getFontSize(), mLevels[i].c_str());
//      drawHorizLine(100, 700, TextEntryYPos + F32(i - mLevelIndex) * ((F32)fontSize * 2.0f));
   }

//   drawHorizLine(100, 700, TextEntryYPos + F32(mLevels.size() - mLevelIndex) * ((F32)fontSize * 2.0f));

   Parent::render();

   FontManager::popFontContext();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PasswordEntryUserInterface::PasswordEntryUserInterface(ClientGame *game) : Parent(game)
{
   setSecret(true);
}


// Destructor
PasswordEntryUserInterface::~PasswordEntryUserInterface()
{
   // Do nothing
}


void PasswordEntryUserInterface::render()
{
   Renderer& r = Renderer::get();
   const S32 canvasWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   if(getGame()->getConnectionToServer())
   {
      getUIManager()->getUI<GameUserInterface>()->render();

      r.setColor(Colors::black, 0.5);

      F32 vertices[] = {
            0,                 0,
            (F32)canvasWidth,  0,
            (F32)canvasWidth, (F32)canvasHeight,
            0,                (F32)canvasHeight
      };
      r.renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, RenderType::TriangleFan);
   }

   Parent::render();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ServerAccessPasswordEntryUserInterface::ServerAccessPasswordEntryUserInterface(ClientGame *game) : Parent(game)
{
   /* Do nothing */
}


ServerAccessPasswordEntryUserInterface::~ServerAccessPasswordEntryUserInterface()
{
   // Do nothing
}


void ServerAccessPasswordEntryUserInterface::onAccept(const char *text)
{
   getGame()->submitServerAccessPassword(mConnectAddress, text);
}


void ServerAccessPasswordEntryUserInterface::onEscape()
{
   getUIManager()->activate<MainMenuUserInterface>();
}


void ServerAccessPasswordEntryUserInterface::setAddressToConnectTo(const Address &address)
{
   mConnectAddress = address;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ServerPasswordEntryUserInterface::ServerPasswordEntryUserInterface(ClientGame *game) : Parent(game)     
{
   title = "ENTER SERVER PASSWORD:";
   instr1 = "";
   instr2 = "Enter the password required for access to the server";
}

// Destructor
ServerPasswordEntryUserInterface::~ServerPasswordEntryUserInterface()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
LevelChangeOrAdminPasswordEntryUserInterface::LevelChangeOrAdminPasswordEntryUserInterface(ClientGame *game) : Parent(game)     
{
   title = "ENTER PASSWORD:";
   instr1 = "";
   instr2 = "Enter level change or admin password to change levels on this server";
}


// Destructor
LevelChangeOrAdminPasswordEntryUserInterface::~LevelChangeOrAdminPasswordEntryUserInterface()
{
   // Do nothing
}


void LevelChangeOrAdminPasswordEntryUserInterface::onAccept(const char *text)
{
   bool submitting = getGame()->submitServerPermissionsPassword(text);

   if(submitting)
   {
      getUIManager()->reactivatePrevUI();                                      // Reactivating clears subtitle message, so reactivate first...
      getUIManager()->getUI<GameMenuUserInterface>()->mMenuSubTitle = "** checking password **";     // ...then set the message
   }
   else
      getUIManager()->reactivatePrevUI();                                      // Otherwise, just reactivate the previous menu
}


void LevelChangeOrAdminPasswordEntryUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();
}


////////////////////////////////////////
////////////////////////////////////////
};


