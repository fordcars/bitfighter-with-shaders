//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIGameParameters.h"

#include "UIEditor.h"
#include "UIManager.h"

#include "game.h"
#include "gameType.h"
#include "ClientGame.h"
#include "Cursor.h"

#include "stringUtils.h"


namespace Zap
{

// Default constructor
SavedMenuItem::SavedMenuItem()
{
   // Do nothing
}


SavedMenuItem::SavedMenuItem(MenuItem *menuItem)
{
   mParamName = menuItem->getPrompt();
   setValues(menuItem);
}


// Destructor
SavedMenuItem::~SavedMenuItem()
{
   // Do nothing
}


void SavedMenuItem::setValues(MenuItem *menuItem)
{
   mParamVal = menuItem->getValueForWritingToLevelFile();
}


string SavedMenuItem::getParamName()
{
   return mParamName;
}


string SavedMenuItem::getParamVal()
{
   return mParamVal;
}


////////////////////////////////////
////////////////////////////////////


// Constructor
GameParamUserInterface::GameParamUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "GAME PARAMETERS MENU";
   mMenuSubTitle = "";
   mMaxMenuSize = S32_MAX;                // We never want scrolling on this menu!

   mGameSpecificParams = 0;
   selectedIndex = 0;
   mQuitItemIndex = 0;
   changingItem = -1;

   mLevelFilename = "";
}


GameParamUserInterface::~GameParamUserInterface()
{
   // Do nothing
}


void GameParamUserInterface::onActivate()
{
   selectedIndex = 0;                          // First item selected when we begin

   // Force rebuild of all params for current gameType; this will make sure we have the latest info if we've loaded a new level,
   // but will also preserve any values entered for gameTypes that are not current.
   clearCurrentGameTypeParams();

   // Load filename from editor only when we activate the menu
   mLevelFilename = stripExtension(getUIManager()->getUI<EditorUserInterface>()->getLevelFileName());
   if(mLevelFilename == EditorUserInterface::UnnamedFile)
      mLevelFilename = "";

   updateMenuItems();
   origGameParams = getGame()->toLevelCode();   // Save a copy of the params coming in for comparison when we leave to see what changed
   Cursor::disableCursor();
}


// Find and delete any parameters associated with the current gameType
void GameParamUserInterface::clearCurrentGameTypeParams()
{
   const Vector<string> keys = getGame()->getGameType()->getGameParameterMenuKeys();

   for(S32 i = 0; i < keys.size(); i++)
   {
      MenuItemMap::iterator iter = mMenuItemMap.find(keys[i]);

      shared_ptr<MenuItem> menuItem;

      if(iter != mMenuItemMap.end())      
         mMenuItemMap.erase(iter);
   }
}


static Vector<string> gameTypes;

static void changeGameTypeCallback(ClientGame *game, U32 gtIndex)
{
   if(game->getGameType() != NULL)
      delete game->getGameType();

   // Instantiate our gameType object and cast it to GameType
   TNL::Object *theObject = TNL::Object::create(GameType::getGameTypeClassName(gameTypes[gtIndex]));
   GameType *gt = dynamic_cast<GameType *>(theObject);   

   gt->addToGame(game, NULL);    // GameType::addToGame() ignores database (and what would it do with one, anyway?), so we can pass NULL

   // If we have a new gameType, we might have new game parameters; update the menu!
   game->getUIManager()->getUI<GameParamUserInterface>()->updateMenuItems();
}


void GameParamUserInterface::updateMenuItems()
{
   GameType *gameType = getGame()->getGameType();
   TNLAssert(gameType, "Missing game type!");

   if(gameTypes.size() == 0)     // Should only be run once, as these gameTypes will not change during the session
   {
      gameTypes = GameType::getGameTypeNames();
      gameTypes.sort(alphaSort);
   }

   string filename = mLevelFilename;
   // Grab the level filename from the menuitem if it has been built already.
   // This let's us persist a changed filename without having to leave the menu first
   // The filename menuitem should be cleared before loading a new level!
   if(getMenuItemCount() > 0)
      filename = getMenuItem(1)->getValue();

   clearMenuItems();

   // Note that on some gametypes instructions[1] is NULL
   string instructs = string(gameType->getInstructionString()[0]) + 
                             (gameType->getInstructionString()[1] ?  string(" ") + gameType->getInstructionString()[1] : "");

   addMenuItem(new ToggleMenuItem("Game Type:",       
                                  gameTypes,
                                  gameTypes.getIndex(gameType->getGameTypeName()),
                                  true,
                                  changeGameTypeCallback,
                                  instructs));


   addMenuItem(new TextEntryMenuItem("Filename:",                         // name
                                     filename,                            // val
                                     EditorUserInterface::UnnamedFile,    // empty val
                                     "File where this level is stored (changing this will trigger a ""Save As"", not a rename)",   // help
                                     MAX_FILE_NAME_LEN));

   const Vector<string> keys = gameType->getGameParameterMenuKeys();

   for(S32 i = 0; i < keys.size(); i++)
   {
      MenuItemMap::iterator iter = mMenuItemMap.find(keys[i]);

      shared_ptr<MenuItem> menuItem;

      if(iter != mMenuItemMap.end())      // What is this supposed to do?  I can't seem to make this condition occur.
         menuItem = iter->second;
      else                 // Item not found
      {
         menuItem = gameType->getMenuItem(keys[i]);
         TNLAssert(menuItem, "Failed to make a new menu item!");

         mMenuItemMap.insert(pair<string, shared_ptr<MenuItem> >(keys[i], menuItem));
      }

      addWrappedMenuItem(menuItem);
   }
}


// Runs as we're exiting the menu
void GameParamUserInterface::onEscape()
{
   EditorUserInterface *ui = getUIManager()->getUI<EditorUserInterface>();

   string newFilename = getMenuItem(1)->getValue();

   bool filenameChanged = mLevelFilename != newFilename;
   if(filenameChanged)
      ui->setLevelFileName(newFilename);

   GameType *gameType = getGame()->getGameType();

   const Vector<string> keys = gameType->getGameParameterMenuKeys();

   for(S32 i = 0; i < keys.size(); i++)
   {
      MenuItemMap::iterator iter = mMenuItemMap.find(keys[i]);

      MenuItem *menuItem = iter->second.get();
      gameType->saveMenuItem(menuItem, keys[i]);
   }

   if(anythingChanged() || filenameChanged)
   {
      ui->setNeedToSave(true);       // Need to save to retain our changes
      ui->mAllUndoneUndoLevel = -1;  // This change can't be undone
      ui->validateLevel();
   }

   // Now back to our previously scheduled program...  (which will be the editor, of course)
   getUIManager()->reactivatePrevUI();

   // Finally clear the menu items, they'll be reloaded next time the menu opens
   clearMenuItems();
}


void GameParamUserInterface::processSelection(U32 index)
{
   // Do nothing
}


bool GameParamUserInterface::anythingChanged()
{
   return origGameParams != getGame()->toLevelCode();
}


S32 GameParamUserInterface::getTextSize(MenuItemSize size) const
{
   return 18;
}


S32 GameParamUserInterface::getGap(MenuItemSize size) const
{
   return 12;
}


S32 GameParamUserInterface::getYStart() const
{
   return 70;
}



};

