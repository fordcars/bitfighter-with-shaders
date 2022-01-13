//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIEditor.h"

#include "UIEditorMenus.h"       // For access to menu methods such as setObject
#include "UIEditorInstructions.h"
#include "UIErrorMessage.h"
#include "UIGameParameters.h"
#include "UINameEntry.h"         // For LevelnameEntryUI
#include "UITeamDefMenu.h"
#include "UIManager.h"

#include "gridDB.h"
#include "WallSegmentManager.h"

#include "ClientGame.h"  
#include "CoreGame.h"            // For CoreItem def
#include "NexusGame.h"           // For NexusZone def
#include "soccerGame.h"          // For Soccer ball radius

#include "barrier.h"             // For DEFAULT_BARRIER_WIDTH
#include "EngineeredItem.h"      // For Turret properties
#include "goalZone.h"
#include "item.h"                // For Asteroid defs
#include "loadoutZone.h"         // For LoadoutZone def
#include "PickupItem.h"          // For RepairItem
#include "projectile.h"

#include "Spawn.h"
#include "speedZone.h"           // For Speedzone def
#include "Teleporter.h"          // For Teleporter def
#include "TextItem.h"            // For MAX_TEXTITEM_LEN and MAX_TEXT_SIZE

#include "config.h"
#include "Cursor.h"              // For various editor cursor
#include "Colors.h"
#include "Intervals.h"
#include "EditorTeam.h"

#include "gameLoader.h"          // For LevelLoadException def
#include "LevelSource.h"
#include "LevelDatabase.h"

#include "luaLevelGenerator.h"
#include "LevelDatabaseUploadThread.h"
#include "HttpRequest.h"
#include "gameObjectRender.h"
#include "SystemFunctions.h"

#include "Console.h"          // Our console object
#include "DisplayManager.h"
#include "Renderer.h"
#include "VideoSystem.h"

#include "stringUtils.h"
#include "GeomUtils.h"
#include "RenderUtils.h"
#include "ScreenShooter.h"

#include <cmath>
#include <set>

namespace Zap
{

// Dock widths in pixels
const S32 ITEMS_DOCK_WIDTH = 50;
const S32 PLUGINS_DOCK_WIDTH = 150;
const U32 PLUGIN_LINE_SPACING = 20;

const F32 MIN_SCALE = .02f;         // Most zoomed-out scale
const F32 MAX_SCALE = 10.0f;        // Most zoomed-in scale
const F32 STARTING_SCALE = 0.5;

static GridDatabase *mLoadTarget;

const string EditorUserInterface::UnnamedFile = "unnamed_file";      // When a file has no name, this is its name!


static void backToMainMenuCallback(ClientGame *game)
{
   UIManager *uiManager = game->getUIManager();

   uiManager->getUI<EditorUserInterface>()->onQuitted();
   uiManager->reactivate(uiManager->getUI<MainMenuUserInterface>());
}


static void saveLevelCallback(ClientGame *game)
{
   UIManager *uiManager = game->getUIManager();

   if(uiManager->getUI<EditorUserInterface>()->saveLevel(true, true))
      backToMainMenuCallback(game);
   else
      uiManager->reactivate(uiManager->getUI<EditorUserInterface>());
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
PluginInfo::PluginInfo(string prettyName, string fileName, string description, string requestedBinding)
   : prettyName(prettyName), fileName(fileName), description(description), requestedBinding(requestedBinding)
{
   bindingCollision = false;
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
EditorUserInterface::EditorUserInterface(ClientGame *game) : Parent(game)
{
   mWasTesting = false;
   mouseIgnore = false;

   clearSnapEnvironment();
   mCurrentScale = STARTING_SCALE;

   mHitItem     = NULL;
   mNewItem     = NULL;
   mDockItemHit = NULL;
   mDockWidth = ITEMS_DOCK_WIDTH;
   mDockMode = DOCKMODE_ITEMS;
   mDockPluginScrollOffset = 0;
   mCurrentTeam = 0;

   mHitVertex = NONE;
   mEdgeHit   = NONE;

   mEditorDatabase = shared_ptr<GridDatabase>(new GridDatabase());

   setNeedToSave(false);

   mLastUndoStateWasBarrierWidthChange = false;

   mUndoItems.resize(UNDO_STATES);     // Create slots for all our undos... also creates a ton of empty dbs.  Maybe we should be using pointers?
   mAutoScrollWithMouse = false;
   mAutoScrollWithMouseReady = false;

   mEditorAttributeMenuItemBuilder.initialize(game);

   mPreviewMode = false;
   mNormalizedScreenshotMode = false;

   mSaveMsgTimer.setPeriod(FIVE_SECONDS);    

   mGridSize = game->getSettings()->getIniSettings()->mSettings.getVal<U32>("EditorGridSize");

   mQuitLocked = false;
   mVertexEditMode = true;
   mDraggingObjects = false;
}


GridDatabase *EditorUserInterface::getDatabase() const
{ 
   return mEditorDatabase.get();
}  


F32 EditorUserInterface::getGridSize() const
{
   return F32(mGridSize);
}


void EditorUserInterface::setDatabase(shared_ptr<GridDatabase> database)
{
   TNLAssert(database.get(), "Database should not be NULL!");
   mEditorDatabase = dynamic_pointer_cast<GridDatabase>(database);
}


// Really quitting... no going back!
void EditorUserInterface::onQuitted()
{
   cleanUp(false);
   getGame()->clearAddTarget();
}


void EditorUserInterface::addDockObject(BfObject *object, F32 xPos, F32 yPos)
{
   object->prepareForDock(getGame(), Point(xPos, yPos), mCurrentTeam);     // Prepare object   
   mDockItems.push_back(shared_ptr<BfObject>(object));          // Add item to our list of dock objects
}


void EditorUserInterface::populateDock()
{
   mDockItems.clear();

   F32 xPos = (F32)DisplayManager::getScreenInfo()->getGameCanvasWidth() - horizMargin - ITEMS_DOCK_WIDTH / 2;
   F32 yPos = 35;
   const F32 spacer = 35;

   addDockObject(new RepairItem(), xPos - 10, yPos);
   addDockObject(new EnergyItem(), xPos + 10, yPos);
   yPos += spacer;

   addDockObject(new Spawn(), xPos, yPos);
   yPos += spacer;

   addDockObject(new ForceFieldProjector(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Turret(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Teleporter(), xPos, yPos);
   yPos += spacer;

   addDockObject(new SpeedZone(), xPos, yPos);
   yPos += spacer;

   addDockObject(new TextItem(), xPos, yPos);
   yPos += spacer;

   if(getGame()->getGameType()->getGameTypeId() == SoccerGame)
      addDockObject(new SoccerBallItem(), xPos, yPos);
   else if(getGame()->getGameType()->getGameTypeId() == CoreGame)
      addDockObject(new CoreItem(), xPos, yPos);
   else
      addDockObject(new FlagItem(), xPos, yPos);
   yPos += spacer;

   addDockObject(new FlagSpawn(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Mine(), xPos - 10, yPos);
   addDockObject(new SpyBug(), xPos + 10, yPos);
   yPos += spacer;

   // These two will share a line
   addDockObject(new Asteroid(), xPos - 10, yPos);
   addDockObject(new AsteroidSpawn(), xPos + 10, yPos);
   yPos += spacer;

   //addDockObject(new CircleSpawn(), xPos - 10, yPos);
//   addDockObject(new Core(), xPos /*+ 10*/, yPos);
//   yPos += spacer;


   // These two will share a line
   addDockObject(new TestItem(), xPos - 10, yPos);
   addDockObject(new ResourceItem(), xPos + 10, yPos);
   yPos += 25;

      
   addDockObject(new LoadoutZone(), xPos, yPos);
   yPos += 25;

   if(getGame()->getGameType()->getGameTypeId() == NexusGame)
   {
      addDockObject(new NexusZone(), xPos, yPos);
      yPos += 25;
   }
   else
   {
      addDockObject(new GoalZone(), xPos, yPos);
      yPos += 25;
   }

   addDockObject(new PolyWall(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Zone(), xPos, yPos);
   yPos += spacer;

}


// Destructor -- unwind things in an orderly fashion.  Note that mLevelGenDatabase will clear itself as the referenced object is deleted.
EditorUserInterface::~EditorUserInterface()
{
   mDockItems.clear();
   mClipboard.clear();

   delete mNewItem.getPointer();
}


// Removes most recent undo state from stack --> won't actually delete items on stack until we need the slot, or we quit
void EditorUserInterface::deleteUndoState()
{
   mLastUndoIndex--;
   mLastRedoIndex--; 
}


// Save the current state of the editor objects for later undoing
void EditorUserInterface::saveUndoState(bool forceSelectionOfTargetObject)
{
   // Use case: We do 5 actions, save, undo 2, redo 1, then do some new action.  
   // Our "no need to save" undo point is lost forever.
   if(mAllUndoneUndoLevel > mLastRedoIndex)     
      mAllUndoneUndoLevel = NONE;


   // Select item so when we undo, it will be selected, which looks better
   bool unselHitItem = false;
   if(forceSelectionOfTargetObject && mHitItem && !mHitItem->isSelected())
   {
      mHitItem->setSelected(true);
      unselHitItem = true;
   }


   GridDatabase *newDB = new GridDatabase();    // Make a copy

   newDB->copyObjects(getDatabase());

   mUndoItems[mLastUndoIndex % UNDO_STATES] = shared_ptr<GridDatabase>(newDB);  

   mLastUndoIndex++;
   mLastRedoIndex = mLastUndoIndex;

   if(mLastUndoIndex % UNDO_STATES == mFirstUndoIndex % UNDO_STATES)           // Undo buffer now full...
   {
      mFirstUndoIndex++;
      mAllUndoneUndoLevel -= 1;     // If this falls below 0, then we can't undo our way out of needing to save
   }
   
   setNeedToSave(mAllUndoneUndoLevel != mLastUndoIndex);
   mRedoingAnUndo = false;
   mLastUndoStateWasBarrierWidthChange = false;

   if(unselHitItem)
      mHitItem->setSelected(false);
}


// Remove and discard the most recently saved undo state 
void EditorUserInterface::removeUndoState()
{
   mLastUndoIndex--;
   mLastRedoIndex = mLastUndoIndex;

   if(mLastUndoIndex % UNDO_STATES == mFirstUndoIndex % UNDO_STATES)           // Undo buffer now full...
   {
      mFirstUndoIndex++;
      mAllUndoneUndoLevel -= 1;     // If this falls below 0, then we can't undo our way out of needing to save
   }
   
   setNeedToSave(mAllUndoneUndoLevel != mLastUndoIndex);
}


void EditorUserInterface::clearSnapEnvironment()
{
   mSnapObject = NULL;
   mSnapVertexIndex = NONE;
}


void EditorUserInterface::undo(bool addToRedoStack)
{
   if(!undoAvailable())
      return;

   clearSnapEnvironment();

   if(mLastUndoIndex == mLastRedoIndex && !mRedoingAnUndo)
   {
      saveUndoState();
      mLastUndoIndex--;
      mLastRedoIndex--;
      mRedoingAnUndo = true;
   }

   mLastUndoIndex--;

   setDatabase(mUndoItems[mLastUndoIndex % UNDO_STATES]);
   GridDatabase *database = getDatabase();
   mLoadTarget = database;

   rebuildEverything(database);    // Well, rebuild segments from walls at least

   onSelectionChanged();

   mLastUndoStateWasBarrierWidthChange = false;
   validateLevel();
}
   

void EditorUserInterface::redo()
{
   if(mLastRedoIndex != mLastUndoIndex)      // If there's a redo state available...
   {
      clearSnapEnvironment();

      mLastUndoIndex++;

      // Perform a little trick here... if we're redoing, and it's our final step of the redo tree, and there's only one item selected,
      // we'll make sure that same item is selected when we're finished.  That will help keep the focus on the item that's being modified
      // during the redo step, and make the redo feel more natural.
      // Act I:
      S32 selectedItem = NONE;

      if(mLastRedoIndex == mLastUndoIndex && getItemSelectedCount() == 1)
      {
         const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

         for(S32 i = 0; i < objList->size(); i++)
         {
            BfObject *obj = static_cast<BfObject *>(objList->get(i));

            if(obj->isSelected())
            {
               selectedItem = obj->getSerialNumber();
               break;
            }
         }
      }

      setDatabase(mUndoItems[mLastUndoIndex % UNDO_STATES]);
      GridDatabase *database = mUndoItems[mLastUndoIndex % UNDO_STATES].get();
      mLoadTarget = database;

      // Act II:
      if(selectedItem != NONE)
      {
         clearSelection(getDatabase());

         BfObject *obj = findObjBySerialNumber(getDatabase(), selectedItem);

         if(obj)
            obj->setSelected(true);
      }


      TNLAssert(mUndoItems[mLastUndoIndex % UNDO_STATES], "null!");

      rebuildEverything(database);  // Needed?  Yes, for now, but theoretically no, because we should be restoring everything fully reconstituted...
      onSelectionChanged();
      validateLevel();

      onMouseMoved();               // If anything gets undeleted under the mouse, make sure it's highlighted
   }
}


// Find specified object in specified database
BfObject *EditorUserInterface::findObjBySerialNumber(const GridDatabase *database, S32 serialNumber) const
{
   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->getSerialNumber() == serialNumber)
         return obj;
   }

   return NULL;
}


void EditorUserInterface::rebuildEverything(GridDatabase *database)
{
   database->getWallSegmentManager()->recomputeAllWallGeometry(database);
   resnapAllEngineeredItems(database, false);

   // If we're rebuilding items in our levelgen database, no need to save anything!
   if(database != &mLevelGenDatabase)
   {
      setNeedToSave(mAllUndoneUndoLevel != mLastUndoIndex);
      autoSave();
   }
}


// Resnaps all engineered items in database
void EditorUserInterface::resnapAllEngineeredItems(GridDatabase *database, bool onlyUnsnapped)
{
   fillVector.clear();
   database->findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrObj = dynamic_cast<EngineeredItem *>(fillVector[i]);

      // Skip already snapped items if only processing unsnapped ones
      if(onlyUnsnapped && engrObj->isSnapped())
         continue;

      engrObj->mountToWall(engrObj->getPos(), database->getWallSegmentManager(), NULL);
   }
}


bool EditorUserInterface::undoAvailable()
{
   return mLastUndoIndex - mFirstUndoIndex != 1;
}


// Wipe undo/redo history
void EditorUserInterface::clearUndoHistory()
{
   mFirstUndoIndex = 0;
   mLastUndoIndex = 1;
   mLastRedoIndex = 1;
   mRedoingAnUndo = false;
}


extern TeamPreset gTeamPresets[];

void EditorUserInterface::setLevelFileName(string name)
{
   if(name == "")
      mEditFileName = "";
   else
      if(name.find('.') == string::npos)      // Append extension, if one is needed
         mEditFileName = name + ".level";
}


void EditorUserInterface::makeSureThereIsAtLeastOneTeam()
{
   if(getTeamCount() == 0)
   {
      EditorTeam *team = new EditorTeam(gTeamPresets[0]);

      getGame()->addTeam(team);
   }
}


void EditorUserInterface::cleanUp(bool isReload)
{
   ClientGame *game = getGame();

   game->resetRatings();

   if(!isReload)
      clearUndoHistory();  // Clear up a little memory, but don't blow away our history if this is a reload

   mDockItems.clear();     // Free a little more -- dock will be rebuilt when editor restarts
   
   mLoadTarget = getDatabase();
   mLoadTarget->removeEverythingFromDatabase();    // Deletes all objects

   mRobotLines.clear();    // Clear our special Robot lines

   game->clearTeams();
   
   clearSnapEnvironment();
   
   mAddingVertex = false;
   clearLevelGenItems();
   mGameTypeArgs.clear();

   mHitItem = NULL;

   game->resetLevelInfo();

   if(game->getGameType())
      delete game->getGameType();
}


// Loads a level
void EditorUserInterface::loadLevel(bool isReload)
{
   string filename = getLevelFileName();
   TNLAssert(filename != "", "Need file name here!");

   ClientGame *game = getGame();

   cleanUp(isReload);

   FolderManager *folderManager = game->getSettings()->getFolderManager();
   string fileName = joindir(folderManager->levelDir, filename).c_str();


   // Process level file --> returns true if file found and loaded, false if not (assume it's a new level)
   bool levelLoaded = game->loadLevelFromFile(fileName, mLoadTarget);

   if(!game->getGameType())  // make sure we have GameType
   {
      GameType *gameType = new GameType;
      gameType->addToGame(game, mLoadTarget);   
   }

   makeSureThereIsAtLeastOneTeam(); // Make sure we at least have one team

   if(levelLoaded)   
   {
      // Loaded a level!
      validateTeams();                 // Make sure every item has a valid team
      validateLevel();                 // Check level for errors (like too few spawns)
   }
   else     
   {
      // New level!
      game->getGameType()->setLevelCredits(getGame()->getClientInfo()->getName());  // Set default author
   }

   //// If we have a level in the database, let's ping the database to make sure it's really still there
   //if(game->isLevelInDatabase() && game->getConnectionToMaster())
   //{
   //   game->getConnectionToMaster()->c2mRequestLevelRating(getLevelDatabaseId());
   //}

   clearSelection(mLoadTarget);        // Nothing starts selected
   setNeedToSave(false);               // Why save when we just loaded?

   mAllUndoneUndoLevel = mLastUndoIndex;

   // Add game-specific items to the dock.
   // We'll want to do this even if isReload is true because the GameType might have changed.
   populateDock();

   // Bulk-process new items, walls first
   mLoadTarget->getWallSegmentManager()->recomputeAllWallGeometry(mLoadTarget);
   
   // Snap all engineered items to the closest wall, if one is found
   resnapAllEngineeredItems(mLoadTarget, false);
}


void EditorUserInterface::clearLevelGenItems()
{
   mLevelGenDatabase.removeEverythingFromDatabase();
}


void EditorUserInterface::copyScriptItemsToEditor()
{
   if(mLevelGenDatabase.getObjectCount() == 0)
      return;     

   // Duplicate EditorObject pointer list to avoid unsynchronized loop removal
   Vector<DatabaseObject *> tempList(*mLevelGenDatabase.findObjects_fast());

   saveUndoState();

   // We can't call addToEditor immediately because it calls addToGame which will trigger
   // an assert since the levelGen items are already added to the game.  We must therefore
   // remove them from the game first
   for(S32 i = 0; i < tempList.size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(tempList[i]);

      obj->removeFromGame(false);     // False ==> do not delete object
      addToEditor(obj);
   }
      
   mLevelGenDatabase.removeEverythingFromDatabase();    // Don't want to delete these objects... we just handed them off to the database!

   rebuildEverything(getDatabase());

   mLastUndoStateWasBarrierWidthChange = false;
}


void EditorUserInterface::addToEditor(BfObject *obj)
{
   obj->addToGame(getGame(), getDatabase());     
   obj->onGeomChanged();                        // Generic way to get PolyWalls to build themselves after being dragged from the dock
}


// User has pressed Ctrl+K -- run the levelgen script and insert any resulting items into the editor in a separate database
void EditorUserInterface::runLevelGenScript()
{
   GameType *gameType = getGame()->getGameType();
   string scriptName = gameType->getScriptName();

   if(scriptName == "")      // No script included!!
      return;

   logprintf(LogConsumer::ConsoleMsg, "Running script %s", gameType->getScriptLine().c_str());

   const Vector<string> *scriptArgs = gameType->getScriptArgs();

   clearLevelGenItems();      // Clear out any items from the last run

   FolderManager *folderManager = getGame()->getSettings()->getFolderManager();
   runScript(&mLevelGenDatabase, folderManager, scriptName, *scriptArgs);
}


// game is an unused parameter needed to make the method fit the signature of the callbacks used by UIMenus
static void openConsole(ClientGame *game)
{
   if(gConsole.isOk())
   {
      gConsole.show();
      return;
   }
   // else show error message  <== TODO DO ThiS!
}


// Runs an arbitrary lua script.  Command is first item in cmdAndArgs, subsequent items are the args, if any
void EditorUserInterface::runScript(GridDatabase *database, const FolderManager *folderManager, 
                                    const string &scriptName, const Vector<string> &args)
{
   string name = folderManager->findLevelGenScript(scriptName);  // Find full name of levelgen script

   if(name == "")
   {
      logprintf(LogConsumer::ConsoleMsg, "Could not find script %s; looked in folders: %s",
                     scriptName.c_str(), concatenate(folderManager->getScriptFolderList()).c_str());
      return;
   }
   
   // Load the items
   LuaLevelGenerator levelGen(getGame(), name, args, database);

   // Error reporting handled within -- we won't cache these scripts for easier development   
   bool error = !levelGen.runScript(false);      

   if(error)
   {
      ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

      ui->reset();
      ui->setTitle("SCRIPT ERROR");

#ifndef BF_NO_CONSOLE
      ui->setMessage("The levelgen script you ran encountered an error.\n\n"
                     "See the console (press [[/]]) or the logfile for details.");
#else
      ui->setMessage("The levelgen script you ran encountered an error.\n\n"
                     "See the logfile for details.");
#endif

      ui->setInstr("Press [[Esc]] to return to the editor");

      ui->registerKey(KEY_SLASH, &openConsole);
      getUIManager()->activate(ui);
   }

   // Even if we had an error, continue on so we can process what does work -- this will make it more consistent with how the script will 
   // perform in-game.  Though perhaps we should show an error to the user...


   // Process new items that need it (walls need processing so that they can render properly).
   // Items that need no extra processing will be kept as-is.
   fillVector.clear();
   database->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *obj = dynamic_cast<BfObject *>(fillVector[i]);

      if(obj->getVertCount() < 2)      // Invalid item; delete  --> aren't 1 point walls already excluded, making this check redundant?
         database->removeFromDatabase(obj, true);
   }

   // Also find any teleporters and make sure their destinations are in order.  Teleporters with no dests will be deleted.
   // Those with multiple dests will be broken down into multiple single dest versions.
   fillVector.clear();
   database->findObjects(TeleporterTypeNumber, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Teleporter *teleporter = static_cast<Teleporter *>(fillVector[i]);
      if(teleporter->getDestCount() == 0)
         database->removeFromDatabase(teleporter, true);
      else
      {
         for(S32 i = 1; i < teleporter->getDestCount(); i++)
         {
            Teleporter *newTel = new Teleporter;
            newTel->setPos(teleporter->getPos());
            newTel->setEndpoint(teleporter->getDest(i));
            newTel->addDest(teleporter->getDest(i));

            newTel->addToGame(getGame(), database);     
         }

         // Delete any destinations past the first one
         for(S32 i = 1; i < teleporter->getDestCount(); i++)
            teleporter->delDest(i);
      }
   }

   rebuildEverything(database);
}


void EditorUserInterface::showPluginError(const string &msg)
{
   Vector<string> messages;
   messages.push_back("Problem With Plugin");
   messages.push_back("Press [[Esc]] to return to the editor");

   messages.push_back("This plugin encountered an error " + msg + ".\n"
                      "It has probably been misconfigured.\n\n"

#ifndef BF_NO_CONSOLE
                      "See the Bitfighter logfile or console ([[/]]) for details.");
#else
                      "See the Bitfighter logfile for details.");
#endif

   mMessageBoxQueue.push_back(messages);
}


// Try to create some sort of unique-ish signature for the plugin
string EditorUserInterface::getPluginSignature()
{
   string key = mPluginRunner->getScriptName();

   if(mPluginMenu)
      for(S32 i = 0; i < mPluginMenu->getMenuItemCount(); i++)
      {
         MenuItem *menuItem = mPluginMenu->getMenuItem(i);
         key += itos(menuItem->getItemType()) + "-";
      }

   return key;
}


void EditorUserInterface::runPlugin(const FolderManager *folderManager, const string &scriptName, const Vector<string> &args)
{
   string fullName = folderManager->findPlugin(scriptName);     // Find full name of plugin script

   if(fullName == "")
   {
      showCouldNotFindScriptMessage(scriptName);
      return;
   }


   // Create new plugin, will be deleted by shared_ptr
   EditorPlugin *plugin = new EditorPlugin(fullName, args, mLoadTarget, getGame());

   mPluginRunner = shared_ptr<EditorPlugin>(plugin);

   // Loads the script and runs it to get everything loaded into memory.  Does not run main().
   // We won't cache scripts here because the performance impact should be relatively small, and it will
   // make it easier to develop them.  If circumstances change, we might want to start caching.
   if(!mPluginRunner->prepareEnvironment() || !mPluginRunner->loadScript(false)) 
   {
      showPluginError("during loading");
      mPluginRunner.reset();
      return;
   }

   string title;
   Vector<shared_ptr<MenuItem> > menuItems;

   bool error = plugin->runGetArgsMenu(title, menuItems);     // Fills menuItems

   if(error)
   {
      showPluginError("configuring its options menu.");
      mPluginRunner.reset();
      return;
   }


   if(menuItems.size() == 0)
   {
      onPluginExecuted(Vector<string>());        // No menu items?  Let's run the script directly!
      mPluginRunner.reset();
      return;     
   }


   // There are menu items!
   // Build a menu from the menuItems returned by the plugin
   mPluginMenu.reset(new PluginMenuUI(getGame(), title));      // Using a smart pointer here, for auto deletion

   for(S32 i = 0; i < menuItems.size(); i++)
      mPluginMenu->addWrappedMenuItem(menuItems[i]);

   
   mPluginMenu->addSaveAndQuitMenuItem("Run plugin", "Saves values and runs plugin");

   mPluginMenu->setMenuCenterPoint(Point(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2));  

   // Restore previous values, if available
   string key = getPluginSignature();

   if(mPluginMenuValues.count(key) == 1)    // i.e. the key exists; use count to avoid creating new entry if it does not exist
      for(S32 i = 0; i < mPluginMenuValues[key].size(); i++)
         mPluginMenu->getMenuItem(i)->setValue(mPluginMenuValues[key].get(i));

   getGame()->getUIManager()->activate(mPluginMenu.get());
}


void EditorUserInterface::onPluginExecuted(const Vector<string> &args)
{
   TNLAssert(mPluginRunner, "NULL PluginRunner!");
   
   saveUndoState();

   // Save menu values for next time -- using a key that includes both the script name and the type of menu items
   // provides some protection against the script being changed while Bitfighter is running.  Probably not realy
   // necessary, but we can afford it here.
   string key = getPluginSignature();

   mPluginMenuValues[key] = args;

   if(!mPluginRunner->runMain(args))
      setSaveMessage("Plugin Error: press [/] for details", false);

   rebuildEverything(getDatabase());
   findSnapVertex();

   mPluginRunner.reset();
}


void EditorUserInterface::showCouldNotFindScriptMessage(const string &scriptName)
{
   string pluginDir = getGame()->getSettings()->getFolderManager()->pluginDir;

   Vector<string> messages;
   messages.push_back("Plugin not Found");
   messages.push_back("Press [[Esc]] to return to the editor");

   messages.push_back("Could not find the plugin called " + scriptName + "\n"
                      "I looked in the " + pluginDir + " folder.\n\n"
                      "You likely have a typo in the [EditorPlugins] section of your INI file.");

   mMessageBoxQueue.push_back(messages);
}


void EditorUserInterface::showUploadErrorMessage(S32 errorCode, const string &errorBody)
{
   Vector<string> messages;
   messages.push_back("Error Uploading Level");
   messages.push_back("Press [[Esc]] to return to the editor");

   messages.push_back("Error uploading level.\n\nServer responded with error code " + itos(errorCode) + "." +
                      (errorBody != "" ? "\n\n\"" + errorBody + "\"" : ""));

   mMessageBoxQueue.push_back(messages);
}


static bool TeamListToString(string &output, Vector<bool> teamVector)
{
   string teamList;
   bool hasError = false;
   char buf[16];

   // Make sure each team has a spawn point
   for(S32 i = 0; i < (S32)teamVector.size(); i++)
      if(!teamVector[i])
      {
         dSprintf(buf, sizeof(buf), "%d", i+1);

         if(!hasError)     // This is our first error
         {
            output = "team ";
            teamList = buf;
         }
         else
         {
            output = "teams ";
            teamList += ", ";
            teamList += buf;
         }
         hasError = true;
      }
   if(hasError)
   {
      output += teamList;
      return true;
   }
   return false;
}


static bool hasTeamFlags(GridDatabase *database)
{
   const Vector<DatabaseObject *> *flags = database->findObjects_fast(FlagTypeNumber);

   for(S32 i = 0; i < flags->size(); i++)
      if(static_cast<FlagItem *>(flags->get(i))->getTeam() > TEAM_NEUTRAL)
         return true;

   return false;     
}


static bool hasTeamSpawns(GridDatabase *database)
{
   fillVector.clear();
   database->findObjects(FlagSpawnTypeNumber, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
      if(dynamic_cast<FlagSpawn *>(fillVector[i])->getTeam() >= 0)
         return true;

   return false;
}


void EditorUserInterface::validateLevel()
{
   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   bool foundNeutralSpawn = false;

   Vector<bool> foundSpawn;

   string teamList;

   // First, catalog items in level
   S32 teamCount = getTeamCount();
   foundSpawn.resize(teamCount);

   for(S32 i = 0; i < teamCount; i++)      // Initialize vector
      foundSpawn[i] = false;

   GridDatabase *gridDatabase = getDatabase();
      
   fillVector.clear();
   gridDatabase->findObjects(ShipSpawnTypeNumber, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Spawn *spawn = static_cast<Spawn *>(fillVector[i]);
      const S32 team = spawn->getTeam();

      if(team == TEAM_NEUTRAL)
         foundNeutralSpawn = true;
      else if(team > TEAM_NEUTRAL && team < teamCount)
         foundSpawn[team] = true;
   }

   bool foundSoccerBall = gridDatabase->hasObjectOfType(SoccerBallItemTypeNumber);
   bool foundNexus      = gridDatabase->hasObjectOfType(NexusTypeNumber);
   bool foundFlags      = gridDatabase->hasObjectOfType(FlagTypeNumber);

   bool foundTeamFlags      = hasTeamFlags (gridDatabase);
   bool foundTeamFlagSpawns = hasTeamSpawns(gridDatabase);

   // "Unversal errors" -- levelgens can't (yet) change gametype

   GameType *gameType = getGame()->getGameType();

   // Check for soccer ball in a a game other than SoccerGameType. Doesn't crash no more.
   if(foundSoccerBall && gameType->getGameTypeId() != SoccerGame)
      mLevelWarnings.push_back("WARNING: Soccer ball can only be used in soccer game.");

   // Check for the nexus object in a non-hunter game. Does not affect gameplay in non-hunter game.
   if(foundNexus && gameType->getGameTypeId() != NexusGame)
      mLevelWarnings.push_back("WARNING: Nexus object can only be used in Nexus game.");

   // Check for missing nexus object in a hunter game.  This cause mucho dolor!
   if(!foundNexus && gameType->getGameTypeId() == NexusGame)
      mLevelErrorMsgs.push_back("ERROR: Nexus game must have a Nexus.");

   if(foundFlags && !gameType->isFlagGame())
      mLevelWarnings.push_back("WARNING: This game type does not use flags.");

   // Check for team flag spawns on games with no team flags
   if(foundTeamFlagSpawns && !foundTeamFlags)
      mLevelWarnings.push_back("WARNING: Found team flag spawns but no team flags.");

   // Errors that may be corrected by levelgen -- script could add spawns
   // Neutral spawns work for all; if there's one, then that will satisfy our need for spawns for all teams
   if(getGame()->getGameType()->getScriptName() == "" && !foundNeutralSpawn)
   {
      if(TeamListToString(teamList, foundSpawn))     // Compose error message
         mLevelErrorMsgs.push_back("ERROR: Need spawn point for " + teamList);
   }


   if(gameType->getGameTypeId() == CoreGame)
   {
      for(S32 i = 0; i < teamCount; i++)      // Initialize vector
         foundSpawn[i] = false;

      fillVector.clear();
      gridDatabase->findObjects(CoreTypeNumber, fillVector);
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         CoreItem *core = static_cast<CoreItem *>(fillVector[i]);
         const S32 team = core->getTeam();
         if(U32(team)< U32(foundSpawn.size()))
            foundSpawn[team] = true;
      }
      if(TeamListToString(teamList, foundSpawn))     // Compose error message
         mLevelErrorMsgs.push_back("ERROR: Need Core for " + teamList);
   }
}


void EditorUserInterface::validateTeams()
{
   validateTeams(getDatabase()->findObjects_fast());
}


// Check that each item has a valid team  (fixes any problems it finds)
void EditorUserInterface::validateTeams(const Vector<DatabaseObject *> *dbObjects)
{
   S32 teams = getTeamCount();

   for(S32 i = 0; i < dbObjects->size(); i++)
   {
      BfObject *obj = dynamic_cast<BfObject *>(dbObjects->get(i));
      S32 team = obj->getTeam();

      if(obj->hasTeam() && ((team >= 0 && team < teams) || team == TEAM_NEUTRAL || team == TEAM_HOSTILE))  
         continue;      // This one's OK

      if(team == TEAM_NEUTRAL && obj->canBeNeutral())
         continue;      // This one too

      if(team == TEAM_HOSTILE && obj->canBeHostile())
         continue;      // This one too

      if(obj->hasTeam())
         obj->setTeam(0);               // We know there's at least one team, so there will always be a team 0
      else if(obj->canBeHostile() && !obj->canBeNeutral())
         obj->setTeam(TEAM_HOSTILE); 
      else
         obj->setTeam(TEAM_NEUTRAL);    // We won't consider the case where hasTeam == canBeNeutral == canBeHostile == false
   }
}


// Search through editor objects, to make sure everything still has a valid team.  If not, we'll assign it a default one.
// Note that neutral/hostile items are on team -1/-2, and will be unaffected by this loop or by the number of teams we have.
void EditorUserInterface::teamsHaveChanged()
{
   bool teamsChanged = false;

   if(getTeamCount() != mOldTeams.size())     // Number of teams has changed
      teamsChanged = true;
   else
      for(S32 i = 0; i < getTeamCount(); i++)
      {
         EditorTeam *team = getTeam(i);

         if(mOldTeams[i].color != *team->getColor() || mOldTeams[i].name != team->getName().getString()) // Color(s) or names(s) have changed
         {
            teamsChanged = true;
            break;
         }
      }

   if(!teamsChanged)       // Nothing changed, we're done here
      return;

   validateTeams();

   // TODO: I hope we can get rid of this in future... perhaps replace with mDockItems being stored in a database, and pass the database?
   Vector<DatabaseObject *> hackyjunk;
   hackyjunk.resize(mDockItems.size());
   for(S32 i = 0; i < mDockItems.size(); i++)
      hackyjunk[i] = mDockItems[i].get();

   validateTeams(&hackyjunk);

   validateLevel();          // Revalidate level -- if teams have changed, requirements for spawns have too
   markLevelPermanentlyDirty();
   autoSave();
}


void EditorUserInterface::markLevelPermanentlyDirty()
{
   setNeedToSave(true);
   mAllUndoneUndoLevel = -1; // This change can't be undone
}


string EditorUserInterface::getLevelFileName()
{
   return mEditFileName != "" ? mEditFileName : UnnamedFile;
}


void EditorUserInterface::onSelectionChanged()
{
   GridDatabase *database = getDatabase();
   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   wallSegmentManager->clearSelected();
   
   // Update wall segment manager with what's currently selected
   fillVector.clear();
   database->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      TNLAssert(dynamic_cast<BfObject *>(fillVector[i]), "Bad cast!");
      BfObject *obj = static_cast<BfObject *>(fillVector[i]);

      if(obj->isSelected())
         wallSegmentManager->setSelected(obj->getSerialNumber(), true);
   }

   wallSegmentManager->rebuildSelectedOutline();
}


void EditorUserInterface::onBeforeRunScriptFromConsole()
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   // Use selection as a marker -- will have to change in future
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->setSelected(true);
   }
}


void EditorUserInterface::onAfterRunScriptFromConsole()
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   // Since all our original objects were marked as selected before the script was run, and since the objects generated by
   // the script are not selected, if we invert the selection, our script items will now be selected.
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->setSelected(!obj->isSelected());
   }

   rebuildEverything(getDatabase());
   onSelectionChanged();
}


void EditorUserInterface::onActivate()
{
   mDelayedUnselectObject = NULL;

   // Check if we have a level name:
   if(getLevelFileName() == UnnamedFile)     // We need to take a detour to get a level name
   {
      // Don't save this menu (false, below).  That way, if the user escapes out, and is returned to the "previous"
      // UI, they will get back to where they were before (prob. the main menu system), not back to here.
      getUIManager()->activate<LevelNameEntryUserInterface>(false);

      return;
   }

   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   mSaveMsgTimer.clear();

   mGameTypeArgs.clear();

   onActivateReactivate();

   loadLevel(false);
   setCurrentTeam(0);

   mSnapContext = FULL_SNAPPING;      // Hold [space/shift+space] to temporarily disable snapping

   // Reset display parameters...
   mDragSelecting = false;

   mCreatingPoly = false;
   mCreatingPolyline = false;
   mDraggingDockItem = NULL;
   mCurrentTeam = 0;
   mPreviewMode = false;
   mDragCopying = false;
   mJustInsertedVertex = false;


   centerView();
   findPlugins();
}


void EditorUserInterface::renderMasterStatus()
{
   // Do nothing, don't render this in editor 
}


bool EditorUserInterface::usesEditorScreenMode() const
{
   return true;
}


// Stuff to do when activating or reactivating
void EditorUserInterface::onActivateReactivate()
{
   mDraggingObjects = false;
   mUp = mDown = mLeft = mRight = mIn = mOut = false;
   getGame()->setAddTarget();    // When a Lua script does an addToGame(), objects should be added to this game
   mDockItemHit = NULL;

   getGame()->setActiveTeamManager(&mTeamManager);

   Cursor::enableCursor();
}


void EditorUserInterface::onReactivate()     // Run when user re-enters the editor after testing, among other things
{
   onActivateReactivate();

   if(mWasTesting)
   {
      mWasTesting = false;
      mSaveMsgTimer.clear();

      getGame()->setGameType(mEditorGameType); 

      remove("editor.tmp");      // Delete temp file
   }


   if(mCurrentTeam >= getTeamCount())
      mCurrentTeam = 0;

   if(UserInterface::getUIManager()->getPrevUI()->usesEditorScreenMode() != usesEditorScreenMode())
      VideoSystem::updateDisplayState(getGame()->getSettings(), VideoSystem::StateReasonInterfaceChange);
}


S32 EditorUserInterface::getTeamCount()
{
   return getGame()->getTeamCount();
}


EditorTeam *EditorUserInterface::getTeam(S32 teamId)
{
   TNLAssert(dynamic_cast<EditorTeam *>(getGame()->getTeam(teamId)), "Expected a EditorTeam");
   return static_cast<EditorTeam *>(getGame()->getTeam(teamId));
}


void EditorUserInterface::clearTeams()
{
   getGame()->clearTeams();
}


bool EditorUserInterface::getNeedToSave() const
{
   return mNeedToSave;
}


void EditorUserInterface::setNeedToSave(bool needToSave)
{
   mNeedToSave = needToSave;
}


void EditorUserInterface::addTeam(EditorTeam *team)
{
   getGame()->addTeam(team);
}


void EditorUserInterface::addTeam(EditorTeam *team, S32 teamIndex)
{
   getGame()->addTeam(team, teamIndex);
}


void EditorUserInterface::removeTeam(S32 teamIndex)
{
   getGame()->removeTeam(teamIndex);
}


Point EditorUserInterface::convertCanvasToLevelCoord(Point p)
{
   return (p - mCurrentOffset) / mCurrentScale;
}


Point EditorUserInterface::convertLevelToCanvasCoord(Point p, bool convert)
{
   return convert ? p * mCurrentScale + mCurrentOffset : p;
}


// Called when we shift between windowed and fullscreen mode, after change is made
void EditorUserInterface::onDisplayModeChange()
{
   static S32 previousXSize = -1;
   static S32 previousYSize = -1;

   if(previousXSize != DisplayManager::getScreenInfo()->getGameCanvasWidth() || 
      previousYSize != DisplayManager::getScreenInfo()->getGameCanvasHeight())
   {
      // Recenter canvas -- note that canvasWidth may change during displayMode change
      mCurrentOffset.set(mCurrentOffset.x - previousXSize / 2 + DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, 
                         mCurrentOffset.y - previousYSize / 2 + DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2);
   }

   // Need to populate the dock here because dock items are tied to a particular screen x,y; 
   // maybe it would be better to give them a dock x,y instead?
   if(getGame()->getGameType())
      populateDock();               // If game type has changed, items on dock will change

   previousXSize = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   previousYSize = DisplayManager::getScreenInfo()->getGameCanvasHeight();
}


Point EditorUserInterface::snapPointToLevelGrid(Point const &p)
{
   if(mSnapContext != FULL_SNAPPING)
      return p;

   // First, find a snap point based on our grid
   F32 factor = (showMinorGridLines() ? 0.1f : 0.5f) * mGridSize;     // Tenths or halves -- major gridlines are gridsize pixels apart

   return Point(floor(p.x / factor + 0.5) * factor, floor(p.y / factor + 0.5) * factor);
}


Point EditorUserInterface::snapPoint(GridDatabase *database, Point const &p, bool snapWhileOnDock)
{
   if(mouseOnDock() && !snapWhileOnDock) 
      return p;      // No snapping!

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   Point snapPoint(p);

   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   if(mDraggingObjects)
   {  
      // Turrets & forcefields: Snap to a wall edge as first (and only) choice, regardless of whether snapping is on or off
      if(isEngineeredType(mSnapObject->getObjectTypeNumber()))
         return snapPointToLevelGrid(p);
   }

   F32 minDist = 255 / mCurrentScale;    // 255 just seems to work well, not related to gridsize; only has an impact when grid is off

   if(mSnapContext == FULL_SNAPPING)     // Only snap to grid when full snapping is enabled; lowest priority snaps go first
   {
      snapPoint = snapPointToLevelGrid(p);
      minDist = snapPoint.distSquared(p);
   }

   if(mSnapContext != NO_SNAPPING)
   {
      // Where will we be snapping things?
      bool snapToWallCorners = getSnapToWallCorners();

      // Now look for other things we might want to snap to
      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         // Don't snap to selected items or items with selected verts (keeps us from snapping to ourselves, which is usually trouble)
         if(obj->isSelected() || obj->anyVertsSelected())    
            continue;

         for(S32 j = 0; j < obj->getVertCount(); j++)
         {
            F32 dist = obj->getVert(j).distSquared(p);
            if(dist < minDist)
            {
               minDist = dist;
               snapPoint.set(obj->getVert(j));
            }
         }
      }

      // Search for a corner to snap to - by using wall edges, we'll also look for intersections between segments
      if(snapToWallCorners)   
         checkCornersForSnap(p, wallSegmentManager->getWallEdgeDatabase()->findObjects_fast(), minDist, snapPoint);
   }

   return snapPoint;
}


// The purpose of the following code is to intelligently mark selected objects as unsnapped so that they will behave better
// while being dragged or pasted.  For example, if we are moving a turret, we want it to snap to anything nearby.  However, if
// that turret is attached to a wall that is also being moved, we don't want the turret going off and snapping to something
// else.  To prevent that, we keep the turret's status as being snapped, even though things are usually marked as unsnapped when
// they are being moved.
//
// The logic here is basically to loop through all selected objects in the passed set, keeping a list of engineer items, and of
// the ids of various walls that are also selected.  Everything that is not an engineered object gets marked as being unsnapped.
// After the loop, engineered items are reviewed; if the wall they were snapped to was not found during the first pass, the 
// item is marked as no longer snapped, so it will start snapping as it moves.  If the wall was found, then the item is marked as
// happily snapped, so it will not try to find another partner during the move.  
//
// The promiscuousSnapper vector keeps track of whether an engr. item is trying to snap to other things.  It is only set on the first
// pass through here, and is based on whether the item it is snapped to (if any) is included in the selection.
//
// We have broken the logic up into a series of functions to facilitate processing different kinds of lists.  Kind of annoying... but
// it seemed the best way to avoid repeating very similar logic.

static Vector<EngineeredItem *> selectedSnappedEngrObjects;
static Vector<S32> selectedSnappedEngrObjectIndices;
static Vector<bool> promiscuousSnapper;
static Vector<S32> selectedWalls;

static void markSelectedObjectsAsUnsnapped_init(S32 itemCount, bool calledDuringDragInitialization)
{
   selectedSnappedEngrObjects.clear();
   selectedWalls.clear();
   selectedSnappedEngrObjectIndices.clear();

   if(calledDuringDragInitialization)
   {
      promiscuousSnapper.resize(itemCount);
      for(S32 i = 0; i < itemCount; i++)
         promiscuousSnapper[i] = true;    // They're all a bit loose to begin with!
   }
}


static void markSelectedObjectAsUnsnapped_body(BfObject *obj, S32 index, bool calledDuringDragInitialization)
{
   if(obj->isSelected())
   {
      if(isEngineeredType(obj->getObjectTypeNumber()))
      {
         EngineeredItem *engrObj = static_cast<EngineeredItem *>(obj);
         if(engrObj->getMountSegment() && engrObj->isSnapped())
         {
            selectedSnappedEngrObjects.push_back(engrObj);
            selectedSnappedEngrObjectIndices.push_back(index);
         }
         else
            obj->setSnapped(false);
      }
      else        // Not an engineered object
      {
         if(isWallType(obj->getObjectTypeNumber()))      // Wall or polywall in this context
         {
            BfObject *bfObj = static_cast<BfObject *>(obj);
            selectedWalls.push_back(bfObj->getSerialNumber());
         }

         obj->setSnapped(false);
      }
   }
}


static void markSelectedObjectAsUnsnapped_done(bool calledDuringDragInitialization)
{
   // Now review all the engineer items that are being dragged and see if the wall they are mounted to is being
   // dragged as well.  If it is, keep them snapped; if not, mark them as unsnapped. 
   for(S32 i = 0; i < selectedSnappedEngrObjects.size(); i++)
   {
      bool snapped = selectedWalls.contains(selectedSnappedEngrObjects[i]->getMountSegment()->getOwner());
      selectedSnappedEngrObjects[i]->setSnapped(snapped);

      if(snapped && calledDuringDragInitialization)
         promiscuousSnapper[selectedSnappedEngrObjectIndices[i]] = false;
   }
}


void EditorUserInterface::markSelectedObjectsAsUnsnapped(const Vector<shared_ptr<BfObject> > &objList)
{
   markSelectedObjectsAsUnsnapped_init(objList.size(), true);

   // Mark all items being dragged as no longer being snapped -- only our primary "focus" item will be snapped
   for(S32 i = 0; i < objList.size(); i++)
      markSelectedObjectAsUnsnapped_body(objList[i].get(), i, true);

   markSelectedObjectAsUnsnapped_done(true);
}


void EditorUserInterface::markSelectedObjectsAsUnsnapped(const Vector<DatabaseObject *> *objList)
{
   markSelectedObjectsAsUnsnapped_init(objList->size(), true);

   // Mark all items being dragged as no longer being snapped -- only our primary "focus" item will be snapped
   for(S32 i = 0; i < objList->size(); i++)
      markSelectedObjectAsUnsnapped_body(static_cast<BfObject *>(objList->get(i)), i, true);

   markSelectedObjectAsUnsnapped_done(true);
}


bool EditorUserInterface::getSnapToWallCorners()
{
   // Allow snapping to wall corners when we're dragging items.  Disallow for all wall types other than PolyWall
   return mSnapContext != NO_SNAPPING && mDraggingObjects &&
         (mSnapObject->getObjectTypeNumber() == PolyWallTypeNumber ||  // Allow PolyWall
          mSnapObject->getObjectTypeNumber() == WallItemTypeNumber ||  // Allow WallItem
          !isWallType(mSnapObject->getObjectTypeNumber()));            // Disallow other Wall-related parts (would
                                                                       // these even ever appear as a snap object?)
}


static bool checkPoint(const Point &clickPoint, const Point &point, F32 &minDist, Point &snapPoint)
{
   F32 dist = point.distSquared(clickPoint);
   if(dist < minDist)
   {
      minDist = dist;
      snapPoint = point;
      return true;
   }

   return false;
}


S32 EditorUserInterface::checkCornersForSnap(const Point &clickPoint, const Vector<DatabaseObject *> *edges, F32 &minDist, Point &snapPoint)
{
   const Point *vert;

   for(S32 i = 0; i < edges->size(); i++)
      for(S32 j = 0; j < 1; j++)
      {
         WallEdge *edge = static_cast<WallEdge *>(edges->get(i));
         vert = (j == 0) ? edge->getStart() : edge->getEnd();
         if(checkPoint(clickPoint, *vert, minDist, snapPoint))
            return i;
      }

   return NONE;
}


////////////////////////////////////
////////////////////////////////////
// Rendering routines


bool EditorUserInterface::showMinorGridLines()
{
   return mCurrentScale >= .5;
}

static S32 QSORT_CALLBACK sortByTeam(DatabaseObject **a, DatabaseObject **b)
{
   TNLAssert(dynamic_cast<BfObject *>(*a), "Not a BfObject");
   TNLAssert(dynamic_cast<BfObject *>(*b), "Not a BfObject");
   return ((BfObject *)(*b))->getTeam() - ((BfObject *)(*a))->getTeam();
}


void EditorUserInterface::renderTurretAndSpyBugRanges(GridDatabase *editorDb)
{
   Renderer &r = Renderer::get();
   fillVector = *editorDb->findObjects_fast(SpyBugTypeNumber);  // This will actually copy vector of pointers to fillVector
                                                                // so we can sort by team, this is still faster then findObjects.
   if(fillVector.size() != 0)
   {
      // Use Z Buffer to make use of not drawing overlap visible area of same team SpyBug, but does overlap different team
      fillVector.sort(sortByTeam); // Need to sort by team, or else won't properly combine the colors.
      r.clearDepth();
      r.enableDepthTest();
      r.pushMatrix();
      r.translate(0, 0, -0.95f);

      r.useSpyBugBlending();

      S32 prevTeam = -10;

      // Draw spybug visibility ranges first, underneath everything else
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         BfObject *editorObj = dynamic_cast<BfObject*>(fillVector[i]);

         if(i != 0 && editorObj->getTeam() != prevTeam)
            r.translate(0, 0, 0.05f);
         prevTeam = editorObj->getTeam();

         Point pos = editorObj->getPos();
         pos *= mCurrentScale;
         pos += mCurrentOffset;
         renderSpyBugVisibleRange(pos, *editorObj->getColor(), mCurrentScale);
      }

      r.useDefaultBlending();
      r.popMatrix();
      r.disableDepthTest();
   }

   // Next draw turret firing ranges for selected or highlighted turrets only
   fillVector.clear();
      
   editorDb->findObjects(TurretTypeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *editorObj = dynamic_cast<BfObject *>(fillVector[i]);
      if(editorObj->isSelected() || editorObj->isLitUp())
      {
         Point pos = editorObj->getPos();
         pos *= mCurrentScale;
         pos += mCurrentOffset;
         renderTurretFiringRange(pos, *editorObj->getColor(), mCurrentScale);
      }
   }
}


S32 getDockHeight()
{
   return DisplayManager::getScreenInfo()->getGameCanvasHeight() - 2 * EditorUserInterface::vertMargin;
}


void EditorUserInterface::renderDock()    
{
   // Render item dock down RHS of screen
   const S32 canvasWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   Color fillColor;

   switch(mDockMode)
   {
      case DOCKMODE_ITEMS:
         fillColor = Colors::red30;
         break;

      case DOCKMODE_PLUGINS:
         fillColor = Colors::blue40;
         break;
   }

   S32 dockHeight = getDockHeight();

   drawFilledFancyBox(canvasWidth - mDockWidth - horizMargin, canvasHeight - vertMargin - dockHeight,
                  canvasWidth - horizMargin,              canvasHeight - vertMargin,
                  8, fillColor, .7f, (mouseOnDock() ? Colors::yellow : Colors::white));

   switch(mDockMode)
   {
      case DOCKMODE_ITEMS:
         renderDockItems();
         break;

      case DOCKMODE_PLUGINS:
         renderDockPlugins();
         break;
   }
}


const S32 PANEL_TEXT_SIZE = 10;
const S32 PANEL_SPACING = S32(PANEL_TEXT_SIZE * 1.3);

static S32 PanelBottom, PanelTop, PanelLeft, PanelRight, PanelInnerMargin;

void EditorUserInterface::renderInfoPanel() 
{
   Renderer& r = Renderer::get();

   // Recalc dimensions in case screen mode changed
   PanelBottom = DisplayManager::getScreenInfo()->getGameCanvasHeight() - EditorUserInterface::vertMargin;
   PanelTop    = PanelBottom - (4 * PANEL_SPACING + 9);
   PanelLeft   = EditorUserInterface::horizMargin;
   PanelRight  = PanelLeft + 180;      // left + width
   PanelInnerMargin = 4;

   drawFilledFancyBox(PanelLeft, PanelTop, PanelRight, PanelBottom, 6, Colors::richGreen, .7f, Colors::white);


   // Draw coordinates on panel -- if we're moving an item, show the coords of the snap vertex, otherwise show the coords of the
   // snapped mouse position
   Point pos;

   if(mSnapObject)
      pos = mSnapObject->getVert(mSnapVertexIndex);
   else
      pos = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos));


   r.setColor(Colors::white);
   renderPanelInfoLine(1, "Cursor X,Y: %2.1f,%2.1f", pos.x, pos.y);

   // And scale
   renderPanelInfoLine(2, "Zoom Scale: %2.2f", mCurrentScale);

   // Show number of teams
   renderPanelInfoLine(3, "Team Count: %d", getTeamCount());

   r.setColor(mNeedToSave ? Colors::red : Colors::green);     // Color level name by whether it needs to be saved or not

   // Filename without extension
   string filename = getLevelFileName();
   renderPanelInfoLine(4, "Filename: %s%s", mNeedToSave ? "*" : "", filename.substr(0, filename.find_last_of('.')).c_str());
}


void EditorUserInterface::renderPanelInfoLine(S32 line, const char *format, ...)
{
   const S32 xpos = horizMargin + PanelInnerMargin;

   va_list args;
   static char text[512];  // reusable buffer

   va_start(args, format);
   vsnprintf(text, sizeof(text), format, args); 
   va_end(args);

   drawString(xpos, DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - PANEL_TEXT_SIZE - line * PANEL_SPACING + 6, PANEL_TEXT_SIZE, text);
}


// Helper to render attributes in a colorful and lady-like fashion
static void renderAttribText(S32 xpos, S32 ypos, S32 textsize, 
                             const Color &keyColor, const Color &valColor, 
                             const Vector<string> &keys, const Vector<string> &vals)
{
   Renderer& r = Renderer::get();

   TNLAssert(keys.size() == vals.size(), "Expected equal number of keys and values!");
   for(S32 i = 0; i < keys.size(); i++)
   {
      r.setColor(keyColor);
      xpos += drawStringAndGetWidth(xpos, ypos, textsize, keys[i].c_str());
      xpos += drawStringAndGetWidth(xpos, ypos, textsize, ": ");

      r.setColor(valColor);
      xpos += drawStringAndGetWidth(xpos, ypos, textsize, vals[i].c_str());
      if(i < keys.size() - 1)
         xpos += drawStringAndGetWidth(xpos, ypos, textsize, "; ");
   }
}


// Shows selected item attributes, or, if we're hovering over dock item, shows dock item info string
void EditorUserInterface::renderItemInfoPanel()
{
   Renderer& r = Renderer::get();
   string itemName;     // All intialized to ""

   S32 hitCount = 0;
   bool multipleKindsOfObjectsSelected = false;

   static Vector<string> keys, values;    // Reusable containers
   keys.clear();
   values.clear();

   const char *instructs = "";

   S32 xpos = PanelRight + 9;
   S32 ypos = PanelBottom - PANEL_TEXT_SIZE - PANEL_SPACING + 6;
   S32 upperLineTextSize = 14;

   // Render information when hovering over a dock item
   if(mDockItemHit)
   {
      itemName    = mDockItemHit->getOnScreenName();

      r.setColor(Colors::green);
      drawString(xpos, ypos, 12, mDockItemHit->getEditorHelpString());

      ypos -= S32(upperLineTextSize * 1.3);

      r.setColor(Colors::white);
      drawString(xpos, ypos, upperLineTextSize, itemName.c_str());
   }

   // Handle everything else
   else
   {
      // Cycle through all our objects to find the selected ones
      const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->isSelected())
         {
            if(hitCount == 0)       // This is the first object we've hit
            {
               itemName   = obj->getOnScreenName();
               obj->fillAttributesVectors(keys, values);
               instructs  = obj->getInstructionMsg(keys.size());      // Various objects have different instructions

               S32 id = obj->getUserAssignedId();
               keys.push_back("Id");
               values.push_back(id > 0 ? itos(id) : "Unassigned");
            }
            else                    // Second or subsequent selected object found
            {
               if(multipleKindsOfObjectsSelected || itemName != obj->getOnScreenName())    // Different type of object
               {
                  itemName = "Multiple object types selected";
                  multipleKindsOfObjectsSelected = true;
               }
            }

            hitCount++;
         }  // end if obj is selected

         else if(obj->isLitUp() && !mouseOnDock())
            mInfoMsg = string("Hover: ") + obj->getOnScreenName();
      }

      /////
      // Now render the info we collected above

      if(hitCount == 1)
      {
         r.setColor(Colors::yellow);
         S32 w = drawStringAndGetWidth(xpos, ypos, PANEL_TEXT_SIZE, instructs);
         if(w > 0)
            w += drawStringAndGetWidth(xpos + w, ypos, PANEL_TEXT_SIZE, "; ");
         drawString(xpos + w, ypos, PANEL_TEXT_SIZE, "[#] to edit Id");

         renderAttribText(xpos, ypos - PANEL_SPACING, PANEL_TEXT_SIZE, Colors::cyan, Colors::white, keys, values);
      }

      ypos -= PANEL_SPACING + S32(upperLineTextSize * 1.3);
      if(hitCount > 0)
      {
         if(!multipleKindsOfObjectsSelected)
            itemName = (mDraggingObjects ? "Dragging " : "Selected ") + itemName;

         if(hitCount > 1)
            itemName += " (" + itos(hitCount) + ")";

         r.setColor(Colors::yellow);
         drawString(xpos, ypos, upperLineTextSize, itemName.c_str());
      }

      ypos -= S32(upperLineTextSize * 1.3);
      if(mInfoMsg != "")
      {
         r.setColor(Colors::white);
         drawString(xpos, ypos, upperLineTextSize, mInfoMsg.c_str());
      }
   }
}


void EditorUserInterface::renderReferenceShip()
{
   Renderer& r = Renderer::get();
   // Render ship at cursor to show scale
   static F32 thrusts[4] =  { 1, 0, 0, 0 };

   r.pushMatrix();
      r.translate(mMousePos);
      r.scale(mCurrentScale);
      r.rotate(90, 0, 0, 1);
      renderShip(ShipShape::Normal, &Colors::red, Colors::red, 1, thrusts, 1, 5, 0, false, false, false, false);
      r.rotate(-90, 0, 0, 1);

      // Draw collision circle
      const F32 spaceAngle = 0.0278f * FloatTau;
      r.setColor(Colors::green, 0.35f);
      r.setLineWidth(gLineWidth1);
      drawDashedCircle(Point(0,0), Ship::CollisionRadius, 10, spaceAngle, 0);
      r.setLineWidth(gDefaultLineWidth);

      // And show how far it can see
      const S32 horizDist = Game::PLAYER_VISUAL_DISTANCE_HORIZONTAL;
      const S32 vertDist  = Game::PLAYER_VISUAL_DISTANCE_VERTICAL;

      r.setColor(Colors::paleBlue, 0.35f);
      drawFilledRect(-horizDist, -vertDist, horizDist, vertDist);

   r.popMatrix();
}


static F32 getRenderingAlpha(bool isScriptItem)
{
   return isScriptItem ? .4f : 1;     // Script items will appear somewhat translucent
}


void EditorUserInterface::render()
{
   Renderer& r = Renderer::get();
   GridDatabase *editorDb = getDatabase();
   mInfoMsg = "";

   mouseIgnore = false;                // Avoid freezing effect from too many mouseMoved events without a render in between (sam)

   // Render bottom-most layer of our display
   if(mPreviewMode)
      renderTurretAndSpyBugRanges(editorDb);    // Render range of all turrets and spybugs in editorDb
   else
      renderGrid(mCurrentScale, mCurrentOffset, convertLevelToCanvasCoord(Point(0,0)), 
                 F32(mGridSize), mSnapContext == FULL_SNAPPING, showMinorGridLines());

   r.pushMatrix();
      r.translate(getCurrentOffset());
      r.scale(getCurrentScale());

      // mSnapDelta only gets recalculated during a dragging event -- if an item is no longer being dragged, we
      // don't want to use the now stale value in mSnapDelta, but rather (0,0) to reflect the rahter obvoius fact
      // that walls that are not being dragged should be rendered in place.
      static Point delta;
      delta = mDraggingObjects ? mSnapDelta : Point(0,0);

      // == Render walls and polyWalls ==
      renderWallsAndPolywalls(&mLevelGenDatabase, delta, false, true );
      renderWallsAndPolywalls(editorDb, delta, false, false);

      // == Normal, unselected items ==
      // Draw map items (teleporters, etc.) that are not being dragged, and won't have any text labels  (below the dock)
      renderObjects(editorDb, RENDER_UNSELECTED_NONWALLS, false);             // Render our normal objects
      renderObjects(&mLevelGenDatabase, RENDER_UNSELECTED_NONWALLS, true);    // Render any levelgen objects being overlaid

      // == Selected items ==
      // Draw map items (teleporters, etc.) that are are selected and/or lit up, so label is readable (still below the dock)
      // Do this as a separate operation to ensure that these are drawn on top of those drawn above.
      // We do render polywalls here because this is what draws the highlighted outline when the polywall is selected.
      renderObjects(editorDb, RENDER_SELECTED_NONWALLS, false);               // Render selected objects 

      renderWallsAndPolywalls(editorDb, delta, true, false);   

      // == Draw geomPolyLine features under construction ==
      if(mCreatingPoly || mCreatingPolyline)    
         renderObjectsUnderConstruction();

      // Since we're not constructing a barrier, if there are any barriers or lineItems selected, 
      // get the width for display at bottom of dock
      else  
      {
         fillVector.clear();
         editorDb->findObjects((TestFunc)isLineItemType, fillVector);

         for(S32 i = 0; i < fillVector.size(); i++)
         {
            LineItem *obj = dynamic_cast<LineItem *>(fillVector[i]);   // Walls are a subclass of LineItem, so this will work for both

            if(obj && (obj->isSelected() || (obj->isLitUp() && obj->isVertexLitUp(NONE))))
               break;
         }
      }

      // Render our snap vertex as a hollow magenta box...
      if(mVertexEditMode &&                                                                                 // Must be in vertex-edit mode
            !mPreviewMode && mSnapObject && mSnapObject->isSelected() && mSnapVertexIndex != NONE &&        // ...but not in preview mode...
            mSnapObject->getGeomType() != geomPoint &&                                                      // ...and not on point objects...
            !mSnapObject->isVertexLitUp(mSnapVertexIndex) && !mSnapObject->vertSelected(mSnapVertexIndex))  // ...or selected vertices
      {
         renderVertex(SnappingVertex, mSnapObject->getVert(mSnapVertexIndex), NO_NUMBER, mCurrentScale/*, alpha*/);  
      }

   r.popMatrix();

   if(!mNormalizedScreenshotMode)
   {
      if(mPreviewMode)
         renderReferenceShip();
      else
      {
         // The following items are hidden in preview mode:
         renderDock();

         renderInfoPanel();
         renderItemInfoPanel();

         if(mouseOnDock() && mDockItemHit)
            mDockItemHit->setLitUp(true);       // Will trigger a selection highlight to appear around dock item
      }
   }

   renderDragSelectBox();

   if(mAutoScrollWithMouse)
   {
      r.setColor(Colors::white);
      drawFourArrows(mScrollWithMouseLocation);
   }

   if(!mNormalizedScreenshotMode)
   {
      renderSaveMessage();
      renderWarnings();
      renderLingeringMessage();
   }

   renderConsole();        // Rendered last, so it's always on top
}


static void setColor(bool isSelected, bool isLitUp, bool isScriptItem)
{
   Renderer& r = Renderer::get();
   F32 alpha = isScriptItem ? .6f : 1;     // So script items will appear somewhat translucent

   if(isSelected)
      r.setColor(Colors::EDITOR_SELECT_COLOR, alpha);       // yellow
   else if(isLitUp)
      r.setColor(Colors::EDITOR_HIGHLIGHT_COLOR, alpha);    // white
   else  // Normal
      r.setColor(Colors::EDITOR_PLAIN_COLOR, alpha);
}


// Render objects in the specified database
void EditorUserInterface::renderObjects(GridDatabase *database, RenderModes renderMode, bool isLevelgenOverlay)
{
   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   bool wantSelected = (renderMode == RENDER_SELECTED_NONWALLS || renderMode == RENDER_SELECTED_WALLS);
   bool wantWalls =    (renderMode == RENDER_UNSELECTED_WALLS  || renderMode == RENDER_SELECTED_WALLS);

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      bool isSelected = obj->isSelected() || obj->isLitUp();
      bool isWall = isWallType(obj->getObjectTypeNumber());

      if(isSelected == wantSelected && isWall == wantWalls)     
      {
         // Items are rendered in index order, so those with a higher index get drawn later, and hence, on top
         setColor(obj->isSelected(), obj->isLitUp(), isLevelgenOverlay);

         if(mPreviewMode)
            obj->render();
         else
         {
            obj->renderEditor(mCurrentScale, getSnapToWallCorners(), mVertexEditMode);
            obj->renderAndLabelHighlightedVertices(mCurrentScale);
         }
      }
   }
}


// Render walls (both normal walls and polywalls, outlines and fills) and centerlines
void EditorUserInterface::renderWallsAndPolywalls(GridDatabase *database, const Point &offset, bool drawSelected, bool isLevelGenDatabase)
{
   GameSettings *settings = getGame()->getSettings();

   WallSegmentManager *wsm = database->getWallSegmentManager();

   // Guarantee walls are a standard color for editor screenshot uploads to the level database
   const Color &fillColor = mNormalizedScreenshotMode ? Colors::DefaultWallFillColor :
         mPreviewMode ? *settings->getWallFillColor() : Colors::EDITOR_WALL_FILL_COLOR;

   const Color &outlineColor = mNormalizedScreenshotMode ? Colors::DefaultWallOutlineColor : *settings->getWallOutlineColor();

   renderWalls(wsm->getWallSegmentDatabase(), *wsm->getWallEdgePoints(), *wsm->getSelectedWallEdgePoints(), outlineColor,
               fillColor, mCurrentScale, mDraggingObjects, drawSelected, offset, mPreviewMode, 
               getSnapToWallCorners(), getRenderingAlpha(isLevelGenDatabase));


   // Render walls as ordinary objects; this will draw wall centerlines
   if(!isLevelGenDatabase)
      renderObjects(database, drawSelected ? RENDER_SELECTED_WALLS : RENDER_UNSELECTED_WALLS, false);  
}


void EditorUserInterface::renderObjectsUnderConstruction()
{
   Renderer& r = Renderer::get();
   // Add a vert (and deleted it later) to help show what this item would look like if the user placed the vert in the current location
   mNewItem->addVert(snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)));
   r.setLineWidth(gLineWidth3);

   if(mCreatingPoly) // Wall
      r.setColor(Colors::EDITOR_SELECT_COLOR);
   else              // LineItem --> Caution! we're rendering an object that doesn't exist yet; its game is NULL
      r.setColor(*getGame()->getTeamColor(mCurrentTeam));

   r.renderPointVector(mNewItem->getOutline(), RenderType::LineStrip);
   r.setLineWidth(gDefaultLineWidth);

   for(S32 j = mNewItem->getVertCount() - 1; j >= 0; j--)      // Go in reverse order so that placed vertices are drawn atop unplaced ones
   {
      Point v = mNewItem->getVert(j);
            
      // Draw vertices
      if(j == mNewItem->getVertCount() - 1)                    // This is our most current vertex
         renderVertex(HighlightedVertex, v, NO_NUMBER, mCurrentScale);
      else
         renderVertex(SelectedItemVertex, v, j, mCurrentScale);
   }
   mNewItem->deleteVert(mNewItem->getVertCount() - 1); 
}


// Draw box for selecting items
void EditorUserInterface::renderDragSelectBox()
{
   if(!mDragSelecting)   
      return;
   
   Renderer::get().setColor(Colors::white);
   Point downPos = convertLevelToCanvasCoord(mMouseDownPos);
   drawHollowRect(downPos, mMousePos);
}


static const S32 DOCK_LABEL_SIZE = 9;      // Size to label items on the dock

// Forward declarations to allow us to put functions in human readable order
static void renderDockItemLabel(const Point &pos, const char *label);
static void renderDockItem(BfObject *object, F32 currentScale, S32 snapVertexIndex);

void EditorUserInterface::renderDockItems()
{
   for(S32 i = 0; i < mDockItems.size(); i++)
      renderDockItem(mDockItems[i].get(), mCurrentScale, mSnapVertexIndex);
}


static void renderDockItem(BfObject *object, F32 currentScale, S32 snapVertexIndex)
{
   Renderer::get().setColor(Colors::EDITOR_PLAIN_COLOR);

   object->renderDock();
   renderDockItemLabel(object->getDockLabelPos(), object->getOnDockName());

   if(object->isLitUp())
     object->highlightDockItem();

   object->setLitUp(false);
}


static void renderDockItemLabel(const Point &pos, const char *label)
{
   F32 xpos = pos.x;
   F32 ypos = pos.y - DOCK_LABEL_SIZE / 2;
   Renderer::get().setColor(Colors::white);
   drawStringc(xpos, ypos + (F32)DOCK_LABEL_SIZE, (F32)DOCK_LABEL_SIZE, label);
}


void EditorUserInterface::renderDockPlugins()
{
   S32 hoveredPlugin = mouseOnDock() ? findHitPlugin() : -1;
   S32 maxPlugins = getDockHeight() / PLUGIN_LINE_SPACING;
   for(S32 i = mDockPluginScrollOffset; i < mPluginInfos.size() && (i - mDockPluginScrollOffset) < maxPlugins; i++)
   {
      if(hoveredPlugin == i)
      {
         S32 x = DisplayManager::getScreenInfo()->getGameCanvasWidth() - mDockWidth - horizMargin;
         F32 y = 1.5f * vertMargin + PLUGIN_LINE_SPACING * (i - mDockPluginScrollOffset);
         drawHollowRect(x + horizMargin / 3, y, x + mDockWidth - horizMargin / 3, y + PLUGIN_LINE_SPACING, Colors::white);
         mInfoMsg = mPluginInfos[i].description;
      }

      Renderer::get().setColor(Colors::white);
      S32 y = (S32) (1.5 * vertMargin + PLUGIN_LINE_SPACING * (i - mDockPluginScrollOffset + 0.33));
      drawString((S32) (DisplayManager::getScreenInfo()->getGameCanvasWidth() - mDockWidth - horizMargin / 2), y, DOCK_LABEL_SIZE, mPluginInfos[i].prettyName.c_str());
      S32 bindingWidth = getStringWidth(DOCK_LABEL_SIZE, mPluginInfos[i].binding.c_str());
      drawString((S32) (DisplayManager::getScreenInfo()->getGameCanvasWidth() - bindingWidth - horizMargin * 1.5), y, DOCK_LABEL_SIZE, mPluginInfos[i].binding.c_str());
   }
}


void EditorUserInterface::renderSaveMessage() const
{
   if(mSaveMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if(mSaveMsgTimer.getCurrent() < (U32)ONE_SECOND)
         alpha = (F32) mSaveMsgTimer.getCurrent() / 1000;

      const S32 textsize = 25;
      const S32 len = getStringWidth(textsize, mSaveMsg.c_str()) + 20;
      const S32 inset = min((DisplayManager::getScreenInfo()->getGameCanvasWidth() - len)  / 2, 200);
      const S32 boxTop = 515;
      const S32 boxBottom = 555;
      const S32 cornerInset = 10;

      // Fill
      Renderer::get().setColor(Colors::black, alpha * 0.80f);
      drawFancyBox(inset, boxTop, DisplayManager::getScreenInfo()->getGameCanvasWidth() - inset, boxBottom, cornerInset, RenderType::TriangleFan);

      // Border
      Renderer::get().setColor(Colors::blue, alpha);
      drawFancyBox(inset, boxTop, DisplayManager::getScreenInfo()->getGameCanvasWidth() - inset, boxBottom, cornerInset, RenderType::LineLoop);

      Renderer::get().setColor(mSaveMsgColor, alpha);
      drawCenteredString(520, textsize, mSaveMsg.c_str());
   }
}


void EditorUserInterface::renderWarnings() const
{
   Renderer& r = Renderer::get();
   if(mWarnMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if (mWarnMsgTimer.getCurrent() < 1000)
         alpha = (F32) mWarnMsgTimer.getCurrent() / 1000;

      r.setColor(mWarnMsgColor, alpha);
      drawCenteredString(DisplayManager::getScreenInfo()->getGameCanvasHeight() / 4, 25, mWarnMsg1.c_str());
      drawCenteredString(DisplayManager::getScreenInfo()->getGameCanvasHeight() / 4 + 30, 25, mWarnMsg2.c_str());
   }

   if(mLevelErrorMsgs.size() || mLevelWarnings.size())
   {
      S32 ypos = vertMargin + 50;

      r.setColor(Colors::ErrorMessageTextColor);

      for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
      {
         drawCenteredString(ypos, 20, mLevelErrorMsgs[i].c_str());
         ypos += 25;
      }

      r.setColor(Colors::yellow);

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
      {
         drawCenteredString(ypos, 20, mLevelWarnings[i].c_str());
         ypos += 25;
      }
   }
}


void EditorUserInterface::renderLingeringMessage() const
{
   mLingeringMessage.render(horizMargin, vertMargin + mLingeringMessage.getHeight(), AlignmentLeft);
}


////////////////////////////////////////
////////////////////////////////////////
/*
1. User creates wall by drawing line
2. Each line segment is converted to a series of endpoints, who's location is adjusted to improve rendering (extended)
3. Those endpoints are used to generate a series of WallSegment objects, each with 4 corners
4. Those corners are used to generate a series of edges on each WallSegment.  Initially, each segment has 4 corners
   and 4 edges.
5. Segments are intsersected with one another, punching "holes", and creating a series of shorter edges that represent
   the dark blue outlines seen in the game and in the editor.

If wall shape or location is changed steps 1-5 need to be repeated
If intersecting wall is changed, only steps 4 and 5 need to be repeated
If wall thickness is changed, steps 3-5 need to be repeated
*/


// Mark all objects in database as unselected
void EditorUserInterface::clearSelection(GridDatabase *database)
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->unselect();
   }
}


// Mark everything as selected
void EditorUserInterface::selectAll(GridDatabase *database)
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->setSelected(true);
   }
}


bool EditorUserInterface::anyItemsSelected(const GridDatabase *database) const
{
   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->isSelected())
         return true;
   }

   return false;
}


// Copy selection to the clipboard
void EditorUserInterface::copySelection()
{
   GridDatabase *database = getDatabase();

   if(!anyItemsSelected(database))
      return;

   mClipboard.clear();     

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         BfObject *objcopy = obj->copy();
         mClipboard.push_back(shared_ptr<BfObject>(objcopy));
      }
   }
}


// Paste items on the clipboard
void EditorUserInterface::pasteSelection()
{
   if(mDraggingObjects)    // Pasting while dragging can cause crashes!!
      return;

   S32 objCount = mClipboard.size();

    if(objCount == 0)         // Nothing on clipboard, nothing to do
      return;

   saveUndoState();           // So we can undo the paste

   GridDatabase *database = getDatabase();
   clearSelection(database);  // Only the pasted items should be selected when we're done

   Point pastePos = snapPoint(database, convertCanvasToLevelCoord(mMousePos));

   Point firstPoint = mClipboard[0]->getVert(0);

   Point offsetFromFirstPoint;
   Vector<DatabaseObject *> copiedObjects;

   for(S32 i = 0; i < objCount; i++)
   {
      offsetFromFirstPoint = firstPoint - mClipboard[i]->getVert(0);

      BfObject *newObject = mClipboard[i]->newCopy();
      newObject->setSelected(true);
      newObject->moveTo(pastePos - offsetFromFirstPoint);

      // addToGame is first so setSelected and onGeomChanged have mGame (at least barriers need it)
      newObject->addToGame(getGame(), NULL);    // Passing NULL keeps item out of any databases... will add in bulk below  

      copiedObjects.push_back(newObject);
   }

   getDatabase()->addToDatabase(copiedObjects);

   // TODO: Need to do something here to snap pasted turrets that are not already snapped to something else

   for(S32 i = 0; i < copiedObjects.size(); i++)   
      copiedObjects[i]->onGeomChanged();

   onSelectionChanged();

   resnapAllEngineeredItems(getDatabase(), false);  // True would work?

   validateLevel();
   setNeedToSave(true);
   autoSave();
}


// Expand or contract selection by scale (i.e. resize)
void EditorUserInterface::scaleSelection(F32 scale)
{
   GridDatabase *database = getDatabase();

   if(!anyItemsSelected(database) || scale < .01 || scale == 1)    // Apply some sanity checks; limits here are arbitrary
      return;

   saveUndoState();

   // Find center of selection
   Point min, max;                        
   database->computeSelectionMinMax(min, max);
   Point ctr = (min + max) * 0.5;

   bool modifiedWalls = false;
   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   wallSegmentManager->beginBatchGeomUpdate();

   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         obj->scale(ctr, scale);
         obj->onGeomChanged();

         if(isWallType(obj->getObjectTypeNumber()))
            modifiedWalls = true;
      }
   }

   wallSegmentManager->endBatchGeomUpdate(database, modifiedWalls);

   setNeedToSave(true);
   autoSave();
}


bool EditorUserInterface::canRotate() const
{
   return !mDraggingObjects && anyItemsSelected(getDatabase());
}


struct PointCompare
{
   bool operator()( const Point& lhs, const Point& rhs ) const
   {
      return lhs.x != rhs.x || lhs.y != rhs.y;
   }
};


// Rotate selected objects around their center point by angle
void EditorUserInterface::rotateSelection(F32 angle, bool useOrigin)
{
   static const F32 NormalizeMultiplier = 64;
   static const F32 NormalizeFraction = 1.0 / NormalizeMultiplier;

   if(!canRotate())
      return;

   saveUndoState();

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   Point center(0,0);

   // If we're not going to use the origin, we're going to use the 'center of mass' of the total
   if(!useOrigin)
   {

      // Add all object centroids to an unordered set for de-duplication.
      // We'll then get the centroid of the set
      set<Point, PointCompare> centroidSet;

      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->isSelected())
         {
            Point thisCentroid = obj->getCentroid();

            // Perform a rounding to some fraction of a grid point. This will
            // help with de-duplication, centroid finding, and float comparison
            // in the set
            thisCentroid.scaleFloorDiv(NormalizeMultiplier, NormalizeFraction);

            centroidSet.insert(thisCentroid);
         }
      }

      // Convert to Vector for centroid finding
      Vector<Point> centroidList(vector<Point>(centroidSet.begin(), centroidSet.end()));

      // If we have only 1 or 2 selected objects, the centroid is calculated differently
      if(centroidList.size() == 1)
         center = centroidList[0];
      else if(centroidList.size() == 2)
         center = (centroidList[0] + centroidList[1]) * 0.5f;  // midpoint
      else
         center = findCentroid(centroidList);
   }

   // Now do the actual rotation
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         obj->rotateAboutPoint(center, angle);
         obj->onGeomChanged();
      }
   }

   setNeedToSave(true);
   autoSave();
}


void EditorUserInterface::setSelectionId(S32 id)
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())               // Should only be one
      {
         if(obj->getUserAssignedId() != id)     // Did the id actually change?
         {
            obj->setUserAssignedId(id, true);
            mAllUndoneUndoLevel = -1;     // If so, it can't be undone
         }
         break;
      }
   }
}


// Set the team affiliation of any selected items
void EditorUserInterface::setCurrentTeam(S32 currentTeam)
{
   mCurrentTeam = currentTeam;
   bool anyChanged = false;

   if(anythingSelected())
      saveUndoState();

   if(currentTeam >= getTeamCount())
   {
      char msg[255];

      if(getTeamCount() == 1)
         dSprintf(msg, sizeof(msg), "Only 1 team has been configured.");
      else
         dSprintf(msg, sizeof(msg), "Only %d teams have been configured.", getTeamCount());

      setWarnMessage(msg, "Hit [F2] to configure teams.");

      return;
   }

   // Update all dock items to reflect new current team
   for(S32 i = 0; i < mDockItems.size(); i++)
   {
      if(!mDockItems[i]->hasTeam())
         continue;

      if(currentTeam == TEAM_NEUTRAL && !mDockItems[i]->canBeNeutral())
         continue;

      if(currentTeam == TEAM_HOSTILE && !mDockItems[i]->canBeHostile())
         continue;

      mDockItems[i]->setTeam(currentTeam);
   }


   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         if(!obj->hasTeam())
            continue;

         if(currentTeam == TEAM_NEUTRAL && !obj->canBeNeutral())
            continue;

         if(currentTeam == TEAM_HOSTILE && !obj->canBeHostile())
            continue;

         if(!anyChanged)
            saveUndoState();

         obj->setTeam(currentTeam);
         anyChanged = true;
      }
   }

   // Overwrite any warnings set above.  If we have a group of items selected, it makes no sense to show a
   // warning if one of those items has the team set improperly.  The warnings are more appropriate if only
   // one item is selected, or none of the items are given a valid team setting.

   if(anyChanged)
   {
      setWarnMessage("", "");
      validateLevel();
      setNeedToSave(true);
      autoSave();
   }
}


void EditorUserInterface::flipSelectionHorizontal()
{
   Point min, max;
   getDatabase()->computeSelectionMinMax(min, max);
   F32 centerX = (min.x + max.x) / 2;

   flipSelection(centerX, true);
}


void EditorUserInterface::flipSelectionVertical()
{
   Point min, max;
   getDatabase()->computeSelectionMinMax(min, max);
   F32 centerY = (min.y + max.y) / 2;

   flipSelection(centerY, false);
}


void EditorUserInterface::flipSelection(F32 center, bool isHoriz)
{
   if(!canRotate())
      return;

   GridDatabase *database = getDatabase();

   saveUndoState();

   Point min, max;
   database->computeSelectionMinMax(min, max);
//   F32 centerX = (min.x + max.x) / 2;

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   bool modifiedWalls = false;
   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   wallSegmentManager->beginBatchGeomUpdate();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         obj->flip(center, isHoriz);
         obj->onGeomChanged();

         if(isWallType(obj->getObjectTypeNumber()))
            modifiedWalls = true;
      }
   }

   wallSegmentManager->endBatchGeomUpdate(database, modifiedWalls);

   setNeedToSave(true);
   autoSave();
}


static const S32 POINT_HIT_RADIUS = 9;
static const S32 EDGE_HIT_RADIUS = 6;

void EditorUserInterface::findHitItemAndEdge()
{
   mHitItem = NULL;
   mEdgeHit = NONE;
   mHitVertex = NONE;

   // Make hit rectangle larger than 1x1 -- when we consider point items, we need to make sure that we grab the item even when we're not right
   // on top of it, as the point item's hit target is much larger than the item itself.  50 is a guess that seems to work well.
   // Note that this is only used for requesting a candidate list from the database, actual hit detection is more precise.
   const Rect cursorRect((mMousePos - mCurrentOffset) / mCurrentScale, 50); 

   fillVector.clear();
   GridDatabase *editorDb = getDatabase();
   editorDb->findObjects((TestFunc)isAnyObjectType, fillVector, cursorRect);

   Point mouse = convertCanvasToLevelCoord(mMousePos);      // Figure out where the mouse is in level coords

   // Do this in two passes -- the first we only consider selected items, the second pass will consider all targets.
   // This will give priority to hitting vertices of selected items.
   for(S32 firstPass = 1; firstPass >= 0; firstPass--)     // firstPass will be true the first time through, false the second time
      for(S32 i = fillVector.size() - 1; i >= 0; i--)      // Go in reverse order to prioritize items drawn on top
      {
         BfObject *obj = dynamic_cast<BfObject *>(fillVector[i]);

         TNLAssert(obj, "Expected a BfObject!");

         if(firstPass == (!obj->isSelected() && !obj->anyVertsSelected()))  // First pass is for selected items only
            continue;                                                       // Second pass only for unselected items
         
         if(checkForVertexHit(obj) || checkForEdgeHit(mouse, obj)) 
            return;                 
      }

   // We've already checked for wall vertices; now we'll check for hits in the interior of walls
   GridDatabase *wallDb = editorDb->getWallSegmentManager()->getWallSegmentDatabase();
   fillVector2.clear();

   wallDb->findObjects((TestFunc)isAnyObjectType, fillVector2, cursorRect);

   for(S32 i = 0; i < fillVector2.size(); i++)
      if(checkForWallHit(mouse, fillVector2[i]))
         return;

   // If we're still here, it means we didn't find anything yet.  Make one more pass, and see if we're in any polys.
   // This time we'll loop forward, though I don't think it really matters.
   for(S32 i = 0; i < fillVector.size(); i++)
     if(checkForPolygonHit(mouse, dynamic_cast<BfObject *>(fillVector[i])))
        return;
}


// Vertex is weird because we don't always do thing in level coordinates -- some of our hit computation is based on
// absolute screen coordinates; some things, like wall vertices, are the same size at every zoom scale.  
bool EditorUserInterface::checkForVertexHit(BfObject *object)
{
   F32 radius = object->getEditorRadius(mCurrentScale);

   for(S32 i = object->getVertCount() - 1; i >= 0; i--)
   {
      // p represents pixels from mouse to obj->getVert(j), at any zoom
      Point p = mMousePos - mCurrentOffset - (object->getVert(i) + object->getEditorSelectionOffset(mCurrentScale)) * mCurrentScale;    

      if(fabs(p.x) < radius && fabs(p.y) < radius)
      {
         mHitItem = object;
         mHitVertex = i;
         return true;
      }
   }

   return false;
}


bool EditorUserInterface::checkForEdgeHit(const Point &point, BfObject *object)
{
   // Points have no edges, and walls are checked via another mechanism
   if(object->getGeomType() == geomPoint) 
      return false;

   const Vector<Point> &verts = *object->getEditorHitPoly(); 
   TNLAssert(verts.size() > 0, "Empty vertex problem -- if debugging, check what kind of object 'object' is, and see "
                               "if you can figure out why it has no verts"); 
   if(verts.size() == 0)
      return false;

   bool loop = (object->getGeomType() == geomPolygon);

   Point closest;

   S32 j_prev  = loop ? (verts.size() - 1) : 0;
         
   for(S32 j = loop ? 0 : 1; j < verts.size(); j++)
   {
      if(findNormalPoint(point, verts[j_prev], verts[j], closest))
      {
         F32 distance = (point - closest).len();
         if(distance < EDGE_HIT_RADIUS / mCurrentScale) 
         {
            mHitItem = object;
            mEdgeHit = j_prev;

            return true;
         }
      }
      j_prev = j;
   }

   return false;
}


bool EditorUserInterface::checkForWallHit(const Point &point, DatabaseObject *object)
{
   TNLAssert(dynamic_cast<WallSegment *>(object), "Expected a WallSegment!");
   WallSegment *wallSegment = static_cast<WallSegment *>(object);

   if(triangulatedFillContains(wallSegment->getTriangulatedFillPoints(), point))
   {
      // Now that we've found a segment that our mouse is over, we need to find the wall object that it belongs to.  Chances are good
      // that it will be one of the objects sitting in fillVector.
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         if(isWallType(fillVector[i]->getObjectTypeNumber()))
         {
            BfObject *eobj = dynamic_cast<BfObject *>(fillVector[i]);

            if(eobj->getSerialNumber() == wallSegment->getOwner())
            {
               mHitItem = eobj;
               return true;
            }
         }
      }

      // Note, if we get to here, we have a problem.

      // This code does a less efficient but more thorough job finding a wall that matches the segment we hit... if the above assert
      // keeps going off, and we can't fix it, this code here should take care of the problem.  But using it is an admission of failure.

      const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(isWallType(obj->getObjectTypeNumber()))
         {
            if(obj->getSerialNumber() == wallSegment->getOwner())
            {
               mHitItem = obj;
               return true;
            }
         }
      }
   }

   return false;
}


bool EditorUserInterface::checkForPolygonHit(const Point &point, BfObject *object)
{
   if(object->getGeomType() == geomPolygon && triangulatedFillContains(object->getFill(), point))
   {
      mHitItem = object;
      return true;
   }

   return false;
}


// Sets mDockItemHit
void EditorUserInterface::findHitItemOnDock()
{
   mDockItemHit = NULL;

   for(S32 i = mDockItems.size() - 1; i >= 0; i--)     // Go in reverse order because the code we copied did ;-)
   {
      Point pos = mDockItems[i]->getPos();

      if(fabs(mMousePos.x - pos.x) < POINT_HIT_RADIUS && fabs(mMousePos.y - pos.y) < POINT_HIT_RADIUS)
      {
         mDockItemHit = mDockItems[i].get();
         return;
      }
   }

   // Now check for polygon interior hits
   for(S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getGeomType() == geomPolygon)
      {
         Vector<Point> verts;
         for(S32 j = 0; j < mDockItems[i]->getVertCount(); j++)
            verts.push_back(mDockItems[i]->getVert(j));

         if(polygonContainsPoint(verts.address(),verts.size(), mMousePos))
         {
            mDockItemHit = mDockItems[i].get();
            return;
         }
      }

   return;
}


S32 EditorUserInterface::findHitPlugin()
{
   S32 i;
   for(i = 0; i < mPluginInfos.size(); i++)
   {
      if(mMousePos.y > 1.5 * vertMargin + PLUGIN_LINE_SPACING * i &&
         mMousePos.y < 1.5 * vertMargin + PLUGIN_LINE_SPACING * (i + 1)
      )
      {
         return i + mDockPluginScrollOffset;
      }
   }
   return -1;
}


void EditorUserInterface::onMouseMoved()
{
   Parent::onMouseMoved();

   if(mouseIgnore)  // Needed to avoid freezing effect from too many mouseMoved events without a render in between
      return;

   mouseIgnore = true;

   mMousePos.set(DisplayManager::getScreenInfo()->getMousePos());

   // Doing this with MOUSE_RIGHT allows you to drag a vertex you just placed by holding the right-mouse button
   if(InputCodeManager::getState(MOUSE_LEFT) || InputCodeManager::getState(MOUSE_RIGHT) || InputCodeManager::getState(MOUSE_MIDDLE))
   {
      onMouseDragged();
      return;
   }

   if(mCreatingPoly || mCreatingPolyline)
      return;

   // Turn off highlight on selected item -- will be turned back on for this object or another below
   if(mHitItem)
      mHitItem->setLitUp(false);

   findHitItemAndEdge();      //  Sets mHitItem, mHitVertex, and mEdgeHit
   findHitItemOnDock();
   
   bool spaceDown = InputCodeManager::getState(KEY_SPACE);

   // Highlight currently selected item
   if(mHitItem)
      mHitItem->setLitUp(true);

   if(mVertexEditMode) {
      // We hit a vertex that wasn't already selected
      if(!spaceDown && mHitItem && mHitVertex != NONE && !mHitItem->vertSelected(mHitVertex))   
         mHitItem->setVertexLitUp(mHitVertex);

      findSnapVertex();
   }

   Cursor::enableCursor();
}


// onDrag
void EditorUserInterface::onMouseDragged()
{
   if(InputCodeManager::getState(MOUSE_MIDDLE) && mMousePos != mScrollWithMouseLocation)
   {
      mCurrentOffset += mMousePos - mScrollWithMouseLocation;
      mScrollWithMouseLocation = mMousePos;
      mAutoScrollWithMouseReady = false;

      return;
   }

   if(mCreatingPoly || mCreatingPolyline || mDragSelecting)
      return;


   bool needToSaveUndoState = true;

   // I assert we never need to save an input state here if we are right-mouse dragging
   if(InputCodeManager::getState(MOUSE_RIGHT))
      needToSaveUndoState = false;

   if(mDraggingDockItem.isValid())    // We just started dragging an item off the dock
   {
       startDraggingDockItem();  
       needToSaveUndoState = false;
   }

   findSnapVertex();                               // Sets mSnapObject and mSnapVertexIndex
   if(!mSnapObject || mSnapVertexIndex == NONE)    // If we've just started dragging a dock item, this will be it
      return;

   mDelayedUnselectObject = NULL;

   if(!mDraggingObjects) 
      onMouseDragged_StartDragging(needToSaveUndoState);

   SDL_SetCursor(Cursor::getSpray());

   Point lastSnapDelta = mSnapDelta;
   // The thinking here is that for large items -- walls, polygons, etc., we may grab an item far from its snap vertex, and we
   // want to factor that offset into our calculations.  For point items (and vertices), we don't really care about any slop
   // in the selection, and we just want the damn thing where we put it.
   if(mSnapObject->getGeomType() == geomPoint || (mHitItem && mHitItem->anyVertsSelected()))
      mSnapDelta = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)) - mMoveOrigin;
   else  // larger items
      mSnapDelta = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos) + mMoveOrigin - mMouseDownPos) - mMoveOrigin;

   translateSelectedItems(mMoveOrigins, mSnapDelta, lastSnapDelta);  // Nudge all selected objects by incremental move amount
   snapSelectedEngineeredItems(mSnapDelta);                          // Snap all selected engr. objects if possible
}


// onStartDragging
void EditorUserInterface::onMouseDragged_StartDragging(const bool needToSaveUndoState)
{
   if(needToSaveUndoState)
      saveUndoState(true);       // Save undo state before we clear the selection

   mMoveOrigin = mSnapObject->getVert(mSnapVertexIndex);
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

#ifdef TNL_OS_MAC_OSX 
   bool ctrlDown = InputCodeManager::getState(KEY_META);
#else
   bool ctrlDown = InputCodeManager::getState(KEY_CTRL);
#endif

   if(ctrlDown)     // Ctrl+Drag ==> copy and drag (except for Mac)
      onMouseDragged_CopyAndDrag(objList);

   onSelectionChanged();
   mDraggingObjects = true; 
   mSnapDelta.set(0,0);

   mMoveOrigins.resize(objList->size());

   // Save the original location of each item pre-move, only used for snapping engineered items to walls
   // Saves location of every item, selected or not
   for(S32 i = 0; i < objList->size(); i++)
      mMoveOrigins[i].set(objList->get(i)->getVert(0));

   markSelectedObjectsAsUnsnapped(objList);
}


// Copy objects and start dragging the copies
void EditorUserInterface::onMouseDragged_CopyAndDrag(const Vector<DatabaseObject *> *objList)
{
   Vector<DatabaseObject *> copiedObjects;

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         BfObject *newObject = obj->newCopy();
         newObject->setSelected(true);
         newObject->addToGame(getGame(), NULL);    // NULL keeps object out of database... will be added in bulk below 

         copiedObjects.push_back(newObject);

         // Make mHitItem be the new copy of the old mHitItem
         if(mHitItem == obj)
            mHitItem = newObject;

         if(mSnapObject == obj)
            mSnapObject = newObject;
      }
   }

   mDragCopying = true;

   // Now mark source objects as unselected
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      obj->setSelected(false);
      obj->setLitUp(false);
   }

   // Now add copied objects to our database; these were marked as selected when they were created
   getDatabase()->addToDatabase(copiedObjects);

   // Running onGeomChanged causes any copied walls to have a full body while we're dragging them 
   for(S32 i = 0; i < copiedObjects.size(); i++)
      copiedObjects[i]->onGeomChanged();
}


void EditorUserInterface::translateSelectedItems(const Vector<Point> &origins, const Point &offset, const Point &lastOffset)
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   //mMoveOrigins[i].set(objList->get(i)->getPos());
   TNLAssert(mMoveOrigins.size() == objList->size(), "Expected these to be the same size!");

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      Point newVert;    // Reusable container

      if(obj->isSelected())            // ==> Dragging whole object
      {
         for(S32 j = obj->getVertCount() - 1; j >= 0;  j--)
         {
            newVert = (obj->getVert(j) - obj->getVert(0)) + (mMoveOrigins[i] + offset);
            obj->setVert(newVert, j);
         }
         obj->onItemDragging();        // Let the item know it's being dragged
      }

      else if(obj->anyVertsSelected()) // ==> Dragging individual vertex
      {
         for(S32 j = obj->getVertCount() - 1; j >= 0;  j--)
            if(obj->vertSelected(j))
            { 
               // Pos of vert at last tick + Offset from last tick
               newVert = obj->getVert(j) + (offset - lastOffset);
               obj->setVert(newVert, j);
               obj->onGeomChanging();  // Because, well, the geom is changing
            }
      }
   }
}


void EditorUserInterface::snapSelectedEngineeredItems(const Point &cumulativeOffset)
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   WallSegmentManager *wallSegmentManager = getDatabase()->getWallSegmentManager();

   for(S32 i = 0; i < objList->size(); i++)
   {
      if(isEngineeredType(objList->get(i)->getObjectTypeNumber()))
      {
         // Only try to mount any items that are both 1) selected and 2) marked as wanting to snap
         EngineeredItem *engrObj = static_cast<EngineeredItem *>(objList->get(i));
         if(engrObj->isSelected() && promiscuousSnapper[i])
            engrObj->mountToWall(snapPointToLevelGrid(mMoveOrigins[i] + cumulativeOffset), wallSegmentManager, &selectedWalls);
      }
   }
}


BfObject *EditorUserInterface::copyDockItem(BfObject *source)
{
   // Instantiate object so we are essentially dragging a non-dock item
   BfObject *newObject = source->newCopy();
   newObject->newObjectFromDock(F32(mGridSize));     // Do things particular to creating an object that came from dock

   return newObject;
}


// User just dragged an item off the dock
void EditorUserInterface::startDraggingDockItem()
{
   saveUndoState();     // Save our undo state before we create a new item

   BfObject *item = copyDockItem(mDraggingDockItem);

   // Offset lets us drag an item out from the dock by an amount offset from the 0th vertex.  This makes placement seem more natural.
   Point pos = convertCanvasToLevelCoord(mMousePos) - item->getInitialPlacementOffset(mGridSize);
   item->moveTo(pos);
      
   GridDatabase *database = getDatabase();

   addToEditor(item); 
//   database->dumpObjects();
   
   clearSelection(database);    // No items are selected...
   item->setSelected(true);     // ...except for the new one
   onSelectionChanged();
   mDraggingDockItem = NULL;    // Because now we're dragging a real item
   validateLevel();             // Check level for errors


   // Because we sometimes have trouble finding an item when we drag it off the dock, after it's been sorted,
   // we'll manually set mHitItem based on the selected item, which will always be the one we just added.
   // TODO: Still needed?

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   mEdgeHit = NONE;
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         mHitItem = obj;
         break;
      }
   }
}


// Sets mSnapObject and mSnapVertexIndex based on the vertex closest to the cursor that is part of the selected set
// What we really want is the closest vertex in the closest feature
void EditorUserInterface::findSnapVertex()
{
   F32 closestDist = F32_MAX;

   if(mDraggingObjects)    // Don't change snap vertex once we're dragging
      return;

   clearSnapEnvironment();

   Point mouseLevelCoord = convertCanvasToLevelCoord(mMousePos);

   // If we have a hit item, and it's selected, find the closest vertex in the item
   if(mHitItem.isValid() && mHitItem->isSelected())   
   {
      // If we've hit an edge, restrict our search to the two verts that make up that edge
      if(mEdgeHit != NONE)
      {
         mSnapObject = mHitItem;     // Regardless of vertex, this is our hit item
         S32 v1 = mEdgeHit;
         S32 v2 = mEdgeHit + 1;

         // Handle special case of looping item
         if(mEdgeHit == mHitItem->getVertCount() - 1)
            v2 = 0;

         // Find closer vertex: v1 or v2
         mSnapVertexIndex = (mHitItem->getVert(v1).distSquared(mouseLevelCoord) < 
                             mHitItem->getVert(v2).distSquared(mouseLevelCoord)) ? v1 : v2;
         return;
      }

      // Didn't hit an edge... find the closest vertex anywhere in the item
      for(S32 j = 0; j < mHitItem->getVertCount(); j++)
      {
         F32 dist = mHitItem->getVert(j).distSquared(mouseLevelCoord);

         if(dist < closestDist)
         {
            closestDist = dist;
            mSnapObject = mHitItem;
            mSnapVertexIndex = j;
         }
      }
      
      return;
   } 

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   // Otherwise, we don't have a selected hitItem -- look for a selected vertex
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      for(S32 j = 0; j < obj->getVertCount(); j++)
      {
         F32 dist = obj->getVert(j).distSquared(mouseLevelCoord);

         if(obj->vertSelected(j) && dist < closestDist)
         {
            closestDist = dist;
            mSnapObject = obj;
            mSnapVertexIndex = j;
         }
      }
   }
}


// Delete selected items (true = items only, false = items & vertices)
void EditorUserInterface::deleteSelection(bool objectsOnly)
{
   if(mDraggingObjects)     // No deleting while we're dragging, please...
      return;

   if(!anythingSelected())  // Nothing to delete
      return;

   bool deleted = false, deletedWall = false;

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = objList->size() - 1; i >= 0; i--)  // Reverse to avoid having to have i-- in middle of loop
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {  
         // Since indices change as items are deleted, this will keep incorrect items from being deleted
         if(obj->isLitUp())
            mHitItem = NULL;

         if(!deleted)
            saveUndoState();

         if(isWallType(obj->getObjectTypeNumber()))
            deletedWall = true;

         deleteItem(i, true);
         deleted = true;
      }
      else if(!objectsOnly)      // Deleted only selected vertices
      {
         bool geomChanged = false;

         // Backwards!  Since we could be deleting multiple at once
         for(S32 j = obj->getVertCount() - 1; j > -1 ; j--)
         {
            if(obj->vertSelected(j))
            {
               if(!deleted)
                  saveUndoState();
              
               obj->deleteVert(j);
               deleted = true;

               geomChanged = true;
               clearSnapEnvironment();
            }
         }

         // Check if item has too few vertices left to be viable
         if(obj->getVertCount() < obj->getMinVertCount())
         {
            if(isWallType(obj->getObjectTypeNumber()))
               deletedWall = true;

            deleteItem(i, true);
            deleted = true;
         }
         else if(geomChanged)
            obj->onGeomChanged();

      }  // else if(!objectsOnly) 
   }  // for


   if(deletedWall)
      doneDeleteingWalls();

   if(deleted)
   {
      setNeedToSave(true);
      autoSave();

      doneDeleting();
   }
}


// Increase selected wall thickness by amt
void EditorUserInterface::changeBarrierWidth(S32 amt)
{
   if(!mLastUndoStateWasBarrierWidthChange)
      saveUndoState(); 

   fillVector2.clear();    // fillVector gets modified in some child function, so use our secondary reusable container
   getDatabase()->findObjects((TestFunc)isWallItemType, fillVector2);

   for(S32 i = 0; i < fillVector2.size(); i++)
   {
      WallItem *obj = static_cast<WallItem *>(fillVector2[i]);
      if(obj->isSelected())
         obj->changeWidth(amt);     
   }

   mLastUndoStateWasBarrierWidthChange = true;
}


// Split wall/barrier on currently selected vertex/vertices
// Or, if entire wall is selected, split on snapping vertex -- this seems a natural way to do it
void EditorUserInterface::splitBarrier()
{
   bool split = false;

   GridDatabase *database = getDatabase();

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->getGeomType() == geomPolyLine)
          for(S32 j = 1; j < obj->getVertCount() - 1; j++)     // Can't split on end vertices!
            if(obj->vertSelected(j))
            {
               saveUndoState();
               
               doSplit(obj, j);

               split = true;
               goto done2;                         // Yes, gotos are naughty, but they just work so well sometimes...
            }
   }

   // If we didn't find a suitable selected vertex to split on, look for a selected line with a magenta vertex
   if(!split && mSnapObject && mSnapObject->getGeomType() == geomPolyLine && mSnapObject->isSelected() && 
         mSnapVertexIndex != NONE && mSnapVertexIndex != 0 && mSnapVertexIndex != mSnapObject->getVertCount() - 1)
   {         
      saveUndoState();

      doSplit(mSnapObject, mSnapVertexIndex);

      split = true;
   }

done2:
   if(split)
   {
      clearSelection(database);
      setNeedToSave(true);
      autoSave();
   }
}


// Split wall or line -- will probably crash on other geom types
void EditorUserInterface::doSplit(BfObject *object, S32 vertex)
{
   BfObject *newObj = object->newCopy();    // Copy the attributes
   newObj->clearVerts();                        // Wipe out the geometry

   // Note that it would be more efficient to start at the end and work backwards, but the direction of our numbering would be
   // reversed in the new object compared to what it was.  This isn't important at the moment, but it just seems wrong from a
   // geographic POV.  Which I have.
   for(S32 i = vertex; i < object->getVertCount(); i++) 
   {
      newObj->addVert(object->getVert(i), true);      // true: If wall already has more than max number of points, let children have more as well
      if(i != vertex)               // i.e. if this isn't the first iteration
      {
         object->deleteVert(i);     // Don't delete first vertex -- we need it to remain as final vertex of old feature
         i--;
      }
   }

   addToEditor(newObj);     // Needs to happen before onGeomChanged, so mGame will not be NULL

   // Tell the new segments that they have new geometry
   object->onGeomChanged();
   newObj->onGeomChanged();            
}


// Join two or more sections of wall that have coincident end points.  Will ignore invalid join attempts.
// Will also merge two or more overlapping polygons.
void EditorUserInterface::joinBarrier()
{
   BfObject *joinedObj = NULL;

   GridDatabase *database = getDatabase();

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size()-1; i++)
   {
      BfObject *obj_i = static_cast<BfObject *>(objList->get(i));

      // Will work for both lines and walls, or any future polylines
      if((obj_i->getGeomType() == geomPolyLine) && obj_i->isSelected())  
      {
         joinedObj = doMergeLines(obj_i, i);
         break;
      }
      else if(obj_i->getGeomType() == geomPolygon && obj_i->isSelected())
      {
         joinedObj = doMergePolygons(obj_i, i);
         break;
      }
   }

   if(joinedObj)     // We had a successful merger
   {
      clearSelection(database);
      setNeedToSave(true);
      autoSave();
      joinedObj->onGeomChanged();
      joinedObj->setSelected(true);

      onSelectionChanged();
   }
}


BfObject *EditorUserInterface::doMergePolygons(BfObject *firstItem, S32 firstItemIndex)
{
   Vector<const Vector<Point> *> inputPolygons;
   Vector<Vector<Point> > outputPolygons;
   Vector<S32> deleteList;

   saveUndoState();

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   inputPolygons.push_back(firstItem->getOutline());

   bool cw = isWoundClockwise(*firstItem->getOutline());       // Make sure all our polys are wound the same direction as the first
                                                               
   for(S32 i = firstItemIndex + 1; i < objList->size(); i++)   // Compare against remaining objects
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->getObjectTypeNumber() == firstItem->getObjectTypeNumber() && obj->isSelected())
      {
         // We can just reverse the winding in place -- if merge succeeds, the poly will be deleted,
         // and if it fails we'll just undo and revert everything to the way it was
         if(isWoundClockwise(*obj->getOutline()) != cw)
            obj->reverseWinding();

         inputPolygons.push_back(obj->getOutline());
         deleteList.push_back(i);
      }
   }

   bool ok = mergePolys(inputPolygons, outputPolygons);

   if(ok && outputPolygons.size() == 1)
   {
      // Clear out the polygon
      while(firstItem->getVertCount())
         firstItem->deleteVert(firstItem->getVertCount() - 1);

      // Add the new points
      for(S32 i = 0; i < outputPolygons[0].size(); i++)
         ok &= firstItem->addVert(outputPolygons[0][i], true);

      if(ok)
      {
         // Delete the constituent parts; work backwards to avoid queering the deleteList indices
         for(S32 i = deleteList.size() - 1; i >= 0; i--)
            deleteItem(deleteList[i]);

         return firstItem;
      }
   }

   undo(false);   // Merge failed for some reason.  Revert everything to undo state saved at the top of method.
   return NULL;
}


BfObject *EditorUserInterface::doMergeLines(BfObject *firstItem, S32 firstItemIndex)
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();
   BfObject *joinedObj = NULL;

   for(S32 i = firstItemIndex + 1; i < objList->size(); i++)              // Compare against remaining objects
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->getObjectTypeNumber() == firstItem->getObjectTypeNumber() && obj->isSelected())
      {
         // Don't join if resulting object would be too big!
         if(firstItem->getVertCount() + obj->getVertCount() > Geometry::MAX_POLY_POINTS)
            continue;

         if(firstItem->getVert(0).distSquared(obj->getVert(0)) < .0001)   // First vertices are the same  1 2 3 | 1 4 5
         {
            if(!joinedObj)          // This is first join candidate found; something's going to merge, so save an undo state
               saveUndoState();
               
            joinedObj = firstItem;

            for(S32 a = 1; a < obj->getVertCount(); a++)           // Skip first vertex, because it would be a dupe
               firstItem->addVertFront(obj->getVert(a));

            deleteItem(i);
            i--;
         }

         // First vertex conincides with final vertex 3 2 1 | 5 4 3
         else if(firstItem->getVert(0).distSquared(obj->getVert(obj->getVertCount() - 1)) < .0001)     
         {
            if(!joinedObj)
               saveUndoState();

            joinedObj = firstItem;
                  
            for(S32 a = obj->getVertCount() - 2; a >= 0; a--)
               firstItem->addVertFront(obj->getVert(a));

            deleteItem(i);    // i has been merged into firstItem; don't need i anymore!
            i--;
         }

         // Last vertex conincides with first 1 2 3 | 3 4 5
         else if(firstItem->getVert(firstItem->getVertCount() - 1).distSquared(obj->getVert(0)) < .0001)     
         {
            if(!joinedObj)
               saveUndoState();

            joinedObj = firstItem;

            for(S32 a = 1; a < obj->getVertCount(); a++)  // Skip first vertex, because it would be a dupe         
               firstItem->addVert(obj->getVert(a));

            deleteItem(i);
            i--;
         }

         // Last vertices coincide  1 2 3 | 5 4 3
         else if(firstItem->getVert(firstItem->getVertCount() - 1).distSquared(obj->getVert(obj->getVertCount() - 1)) < .0001)     
         {
            if(!joinedObj)
               saveUndoState();

            joinedObj = firstItem;

            for(S32 j = obj->getVertCount() - 2; j >= 0; j--)
               firstItem->addVert(obj->getVert(j));

            deleteItem(i);
            i--;
         }
      }
   }

   return joinedObj;
}


void EditorUserInterface::deleteItem(S32 itemIndex, bool batchMode)
{
   GridDatabase *database = getDatabase();
   WallSegmentManager *wallSegmentManager = database->getWallSegmentManager();

   BfObject *obj = static_cast<BfObject *>(getDatabase()->findObjects_fast()->get(itemIndex));

   if(isWallType(obj->getObjectTypeNumber()))
   {
      // Need to recompute boundaries of any intersecting walls
      wallSegmentManager->deleteSegments(obj->getSerialNumber());          // Delete the segments associated with the wall
      database->removeFromDatabase(obj, true);

      if(!batchMode)
         doneDeleteingWalls();
   }
   else
      database->removeFromDatabase(obj, true);

   if(!batchMode)
      doneDeleting();
}


// After deleting a bunch of items, clean up
void EditorUserInterface::doneDeleteingWalls()
{
   WallSegmentManager *wallSegmentManager = mLoadTarget->getWallSegmentManager();

   wallSegmentManager->recomputeAllWallGeometry(mLoadTarget);   // Recompute wall edges
   resnapAllEngineeredItems(mLoadTarget, false);
}


void EditorUserInterface::doneDeleting()
{
   // Reset a bunch of things
   clearSnapEnvironment();
   validateLevel();
   onMouseMoved();   // Reset cursor  
}


// Only called when user presses a hotkey to insert an item -- may crash if item to be inserted is not currently on the dock!
void EditorUserInterface::insertNewItem(U8 itemTypeNumber)
{
   if(mDraggingObjects)     // No inserting when items are being dragged!
      return;

   GridDatabase *database = getDatabase();

   clearSelection(database);
   saveUndoState();

   BfObject *newObject = NULL;

   // Find a dockItem to copy
   for(S32 i = 0; i < mDockItems.size(); i++)
      if(mDockItems[i]->getObjectTypeNumber() == itemTypeNumber)
      {
         newObject = copyDockItem(mDockItems[i].get());
         break;
      }

   // May occur if requested item is not currently on the dock
   TNLAssert(newObject, "Couldn't create object in insertNewItem()");
   if(!newObject)
      return;


   newObject->moveTo(snapPoint(database, convertCanvasToLevelCoord(mMousePos)));
   addToEditor(newObject);    
   newObject->onGeomChanged();

   validateLevel();
   setNeedToSave(true);
   autoSave();
}


void EditorUserInterface::centerView(bool isScreenshot)
{
   Rect extents = getDatabase()->getExtents();
   Rect levelgenDbExtents = mLevelGenDatabase.getExtents();

   if(levelgenDbExtents.getWidth() > 0 || levelgenDbExtents.getHeight() > 0)
      extents.unionRect(levelgenDbExtents);

   // If we have nothing, or maybe only one point object in our level
   if(extents.getWidth() < 1 && extents.getHeight() < 1)    // e.g. a single point item
   {
      mCurrentScale = STARTING_SCALE;
      setDisplayCenter(extents.getCenter());
   }
   else
   {
      if(isScreenshot)
      {
         // Expand just slightly so we don't clip edges
         extents.expand(Point(2,2));
         setDisplayExtents(extents, 1.0f);
      }
      else
         setDisplayExtents(extents, 1.3f);
   }
}


F32 EditorUserInterface::getCurrentScale()
{
   return mCurrentScale;
}


Point EditorUserInterface::getCurrentOffset()
{
   return mCurrentOffset;
}


// Positive amounts are zooming in, negative are zooming out
void EditorUserInterface::zoom(F32 zoomAmount)
{
   Point mouseLevelPoint = convertCanvasToLevelCoord(mMousePos);

   setDisplayScale(mCurrentScale * (1 + zoomAmount));

   Point newMousePoint = convertLevelToCanvasCoord(mouseLevelPoint);

   mCurrentOffset += mMousePos - newMousePoint;
}


void EditorUserInterface::setDisplayExtents(const Rect &extents, F32 backoffFact)
{
   F32 scale = min(DisplayManager::getScreenInfo()->getGameCanvasWidth()  / extents.getWidth(), 
                   DisplayManager::getScreenInfo()->getGameCanvasHeight() / extents.getHeight());

   scale /= backoffFact;

   setDisplayScale(scale);
   setDisplayCenter(extents.getCenter());
}


Rect EditorUserInterface::getDisplayExtents() const
{
   // mCurrentOffset is the UL corner of our screen... just what we need for the bounding box
   Point lr = Point(DisplayManager::getScreenInfo()->getGameCanvasWidth(),
                    DisplayManager::getScreenInfo()->getGameCanvasHeight()) - mCurrentOffset;

   F32 mult = 1 / mCurrentScale;

   return Rect(-mCurrentOffset * mult, lr * mult);
}


// cenx and ceny are the desired center of the display; mCurrentOffset is the UL corner
void EditorUserInterface::setDisplayCenter(const Point &center)
{
   mCurrentOffset.set(DisplayManager::getScreenInfo()->getGameCanvasWidth()  / 2 - mCurrentScale * center.x, 
                      DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2 - mCurrentScale * center.y);
}                             


// We will need to recenter the display after changing the scale.  Higher scales are more zoomed in.
void EditorUserInterface::setDisplayScale(F32 scale)
{
   Point center = getDisplayCenter();

   mCurrentScale = scale;

   if(mCurrentScale < MIN_SCALE)
      mCurrentScale = MIN_SCALE;
   else if(mCurrentScale > MAX_SCALE)
      mCurrentScale = MAX_SCALE;

   setDisplayCenter(center);
}


Point EditorUserInterface::getDisplayCenter() const
{
   F32 mult = 1 / mCurrentScale;

   return Point(((DisplayManager::getScreenInfo()->getGameCanvasWidth()  / 2) - mCurrentOffset.x), 
                ((DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2) - mCurrentOffset.y)) * mult;
}


void EditorUserInterface::onTextInput(char ascii)
{
   // Pass the key on to the console for processing
   if(gConsole.onKeyDown(ascii))
       return;
}


// Handle key presses
bool EditorUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;

   if(gConsole.onKeyDown(inputCode))      // Pass the key on to the console for processing
      return true;

   // If console is open, then we want to capture text, so return false
   if(gConsole.isVisible())
      return false;

   string inputString = InputCodeManager::getCurrentInputString(inputCode);

   GameSettings *settings = getGame()->getSettings();

   if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER)       // Enter - Edit props
      startAttributeEditor();

   // Mouse wheel scrolls the plugin list, or zooms in and out

   else if(inputCode == MOUSE_WHEEL_UP)
   {
      if(mDockMode == DOCKMODE_PLUGINS && mouseOnDock())
      {
         if(mDockPluginScrollOffset > 0)
            mDockPluginScrollOffset -= 1;
      }
      else
         zoom(0.2f);
   }
   else if(inputCode == MOUSE_WHEEL_DOWN)
   {
      if(mDockMode == DOCKMODE_PLUGINS && mouseOnDock())
      {
         if(mDockPluginScrollOffset < (S32) (mPluginInfos.size() - getDockHeight() / PLUGIN_LINE_SPACING))
            mDockPluginScrollOffset += 1;
      }
      else
         zoom(-0.2f);
   }
   else if(inputCode == MOUSE_MIDDLE)     // Click wheel to drag
   {
      mScrollWithMouseLocation = mMousePos;
      mAutoScrollWithMouseReady = !mAutoScrollWithMouse; // Ready to scroll when button is released
      mAutoScrollWithMouse = false;  // turn off in case we were already auto scrolling.
   }

   // Regular key handling from here on down
   else if(InputCodeManager::checkModifier(KEY_SHIFT) && inputCode == KEY_0)  // Shift-0 -> Set team to hostile
      setCurrentTeam(-2);
   else if(inputCode >= KEY_0 && inputCode <= KEY_9 && InputCodeManager::checkModifier(KEY_NONE))  // Change team affiliation of selection with 0-9 keys
   {
      setCurrentTeam((inputCode - KEY_0) - 1);
      return true;
   }

#ifdef TNL_OS_MAC_OSX 
   // Ctrl-left click is same as right click for Mac users
   else if(inputCode == MOUSE_RIGHT || (inputCode == MOUSE_LEFT && InputCodeManager::checkModifier(KEY_CTRL)))
#else
   else if(inputCode == MOUSE_RIGHT)
#endif
      onMouseClicked_right();

   else if(inputCode == MOUSE_LEFT)
      onMouseClicked_left();

   // Neither mouse button, let's try some keys
   else if(inputString == "D" || inputString == "Shift+D")                                  // Pan right
      mRight = true;
   else if(inputString == "Right Arrow")  // Pan right
      mRight = true;
	   else if(inputString == getEditorBindingString(settings, BINDING_FLIP_HORIZ))          // Flip horizontal
      flipSelectionHorizontal();
	   else if(inputString == getEditorBindingString(settings, BINDING_PASTE_SELECTION))     // Paste selection
      pasteSelection();
	   else if(inputString == getEditorBindingString(settings, BINDING_FLIP_VERTICAL))       // Flip vertical
      flipSelectionVertical();
   else if(inputString == "/" || inputString == "Keypad /")
      openConsole(NULL);
	   else if(inputString == getEditorBindingString(settings, BINDING_RELOAD_LEVEL))        // Reload level
   {
      saveUndoState();
      loadLevel(true);                        

      string undoBinding = getEditorBindingString(settings, BINDING_UNDO_ACTION);
      setSaveMessage("Reloaded " + getLevelFileName() + "        [" + undoBinding + "] to undo)", true);
   }
	   else if(inputString == getEditorBindingString(settings, BINDING_REDO_ACTION))         // Redo
   {
      if(!mCreatingPolyline && !mCreatingPoly && !mDraggingObjects && !mDraggingDockItem)
         redo();
   }
	   else if(inputString == getEditorBindingString(settings, BINDING_UNDO_ACTION))         // Undo
   {
      if(!mCreatingPolyline && !mCreatingPoly && !mDraggingObjects && !mDraggingDockItem)
         undo(true);
   }
	   else if(inputString == getEditorBindingString(settings, BINDING_RESET_VIEW))          // Reset veiw
      centerView();
	   else if(inputString == getEditorBindingString(settings, BINDING_LVLGEN_SCRIPT))       // Run levelgen script, or clear last results
   {
      // Ctrl+R is a toggle -- we either add items or clear them
      if(mLevelGenDatabase.getObjectCount() == 0)
         runLevelGenScript();
      else
         clearLevelGenItems();
   }
   else if(inputString == "Shift+1" || inputString == "Shift+3")  // '!' or '#'
      startSimpleTextEntryMenu(SimpleTextEntryID);
   else if(inputString == getEditorBindingString(settings, BINDING_ROTATE_CENTROID))     // Spin by arbitrary amount
   {
      if(canRotate())
         startSimpleTextEntryMenu(SimpleTextEntryRotateCentroid);
   }
	else if(inputString == getEditorBindingString(settings, BINDING_ROTATE_ORIGIN))      // Rotate by arbitrary amount
      startSimpleTextEntryMenu(SimpleTextEntryRotateOrigin);
	else if(inputString == getEditorBindingString(settings, BINDING_SPIN_CCW))            // Spin CCW
      rotateSelection(-15.f, false);
	else if(inputString == getEditorBindingString(settings, BINDING_SPIN_CW))             // Spin CW
      rotateSelection(15.f, false);
	else if(inputString == getEditorBindingString(settings, BINDING_ROTATE_CCW_ORIGIN))   // Rotate CCW about origin
      rotateSelection(-15.f, true);
	else if(inputString == getEditorBindingString(settings, BINDING_ROTATE_CW_ORIGIN))    // Rotate CW about origin
      rotateSelection(15.f, true);

	else if(inputString == getEditorBindingString(settings, BINDING_INSERT_GEN_ITEMS))    // Insert items generated with script into editor
      copyScriptItemsToEditor();

   else if(inputString == "Up Arrow" || inputString == "W" || inputString == "Shift+W")     // W or Up - Pan up
      mUp = true;
   else if(inputString == "Ctrl+Up Arrow")      // Zoom in
      mIn = true;
   else if(inputString == "Ctrl+Down Arrow")    // Zoom out
      mOut = true;
   else if(inputString == "Down Arrow")         // Pan down
      mDown = true;
	   else if(inputString == getEditorBindingString(settings, BINDING_SAVE_LEVEL))           // Save
      saveLevel(true, true);
   else if(inputString == "S"|| inputString == "Shift+S")                                    // Pan down
      mDown = true;
   else if(inputString == "Left Arrow" || inputString == "A" || inputString == "Shift+A")    // Left or A - Pan left
      mLeft = true;
   else if(inputString == "Shift+=" || inputString == "Shift+Keypad +")                      // Shifted - Increase barrier width by 1
      changeBarrierWidth(1);
   else if(inputString == "=" || inputString == "Keypad +")                                  // Unshifted + --> by 5
      changeBarrierWidth(5);
   else if(inputString == "Shift+-" || inputString == "Shift+Keypad -")                      // Shifted - Decrease barrier width by 1
      changeBarrierWidth(-1);
   else if(inputString == "-" || inputString == "Keypad -")                                  // Unshifted --> by 5
      changeBarrierWidth(-5);
	else if(inputString == getEditorBindingString(settings, BINDING_ZOOM_IN))                 // Zoom In
      mIn = true;                                                                           
   else if(inputString == "\\")                                                              // Split barrier on selected vertex               
      splitBarrier();                                                                       
	else if(inputString == getEditorBindingString(settings, BINDING_JOIN_SELECTION))          // Join selected barrier segments or polygons
      joinBarrier();                                                                        
	else if(inputString == getEditorBindingString(settings, BINDING_SELECT_EVERYTHING))       // Select everything
      selectAll(getDatabase());                                                             
	else if(inputString == getEditorBindingString(settings, BINDING_RESIZE_SELECTION))        // Resize selection
      startSimpleTextEntryMenu(SimpleTextEntryScale);                                       
	else if(inputString == getEditorBindingString(settings, BINDING_CUT_SELECTION))           // Cut selection
   {
      copySelection();
      deleteSelection(true);
   }
	else if(inputString == getEditorBindingString(settings, BINDING_COPY_SELECTION))          // Copy selection to clipboard
      copySelection();                                                                      
	else if(inputString == getEditorBindingString(settings, BINDING_ZOOM_OUT))                // Zoom out
      mOut = true;                                                                          
	else if(inputString == getEditorBindingString(settings, BINDING_LEVEL_PARAM_EDITOR))      // Level Parameter Editor
   {                                                                                        
      getUIManager()->activate<GameParamUserInterface>();                                   
      playBoop();                                                                           
   }                                                                                        
	else if(inputString == getEditorBindingString(settings, BINDING_TEAM_EDITOR))             // Team Editor Menu
   {                                                                                        
      getUIManager()->activate<TeamDefUserInterface>();                                     
      playBoop();                                                                           
   }                                                                                        
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_TELEPORTER))        // Teleporter
      insertNewItem(TeleporterTypeNumber);                                                  
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_SPEEDZONE))         // SpeedZone
      insertNewItem(SpeedZoneTypeNumber);                                                   
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_SPAWN))             // Spawn
      insertNewItem(ShipSpawnTypeNumber);                                                   
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_SPYBUG))            // Spybug
      insertNewItem(SpyBugTypeNumber);                                                      
   else if(inputString == getEditorBindingString(settings, BINDING_PLACE_REPAIR))            // Repair
      insertNewItem(RepairItemTypeNumber);
   else if(inputString == getEditorBindingString(settings, BINDING_PLACE_ENERGY))            // Energy
      insertNewItem(EnergyItemTypeNumber);
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_TURRET))            // Turret
      insertNewItem(TurretTypeNumber);                                                      
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_MINE))              // Mine
      insertNewItem(MineTypeNumber);                                                        
	else if(inputString == getEditorBindingString(settings, BINDING_PLACE_FORCEFIELD))        // Forcefield
      insertNewItem(ForceFieldProjectorTypeNumber);
   else if(inputString == "Backspace" || inputString == "Del" || inputString == "Keypad .")  // Keypad . is the keypad's del key
      deleteSelection(false);
	else if(checkInputCode(BINDING_HELP, inputCode))                                          // Turn on help screen
   {
      getGame()->getUIManager()->activate<EditorInstructionsUserInterface>();
      playBoop();
   }
   else if(inputCode == KEY_ESCAPE)          // Activate the menu
   {
      playBoop();
      getGame()->getUIManager()->activate<EditorMenuUserInterface>();
   }
	else if(inputString == getEditorBindingString(settings, BINDING_NO_SNAPPING))             // No snapping to grid, but still to other things
      mSnapContext = NO_GRID_SNAPPING;                                                     
	else if(inputString == getEditorBindingString(settings, BINDING_NO_GRID_SNAPPING))        // Completely disable snapping
      mSnapContext = NO_SNAPPING;                                                          
	else if(inputString == getEditorBindingString(settings, BINDING_PREVIEW_MODE))            // Turn on preview mode
      mPreviewMode = true;                                                                 
	else if(inputString == getEditorBindingString(settings, BINDING_DOCKMODE_ITEMS))          //  Toggle dockmode Items
   {
	  if(mDockMode == DOCKMODE_ITEMS)
	  {
	      mDockMode = DOCKMODE_PLUGINS;
	      mDockWidth = findPluginDockWidth();
	  }
	  else
	  {
	      mDockMode = DOCKMODE_ITEMS;
	      mDockWidth = ITEMS_DOCK_WIDTH;
	  }
   }
   else if(checkPluginKeyBindings(inputString))
   {
      // Do nothing
   }
   else if(inputString == getEditorBindingString(settings, BINDING_TOGGLE_EDIT_MODE))
   {
      mVertexEditMode = !mVertexEditMode;
   }
   else
      return false;

   // A key was handled
   return true;
}


void EditorUserInterface::onMouseClicked_left()
{
   if(InputCodeManager::getState(MOUSE_RIGHT))  // Prevent weirdness
      return;

   bool spaceDown = InputCodeManager::getState(KEY_SPACE);

   mDraggingDockItem = NULL;
   mMousePos.set(DisplayManager::getScreenInfo()->getMousePos());
   mJustInsertedVertex = false;

   if(mCreatingPoly || mCreatingPolyline)       // Save any polygon/polyline we might be creating
   {
      if(mNewItem->getVertCount() < 2)
      {
         delete mNewItem.getPointer();
         removeUndoState();
      }
      else
         addToEditor(mNewItem);

      mNewItem = NULL;

      mCreatingPoly = false;
      mCreatingPolyline = false;
   }

   mMouseDownPos = convertCanvasToLevelCoord(mMousePos);

   if(mouseOnDock())    // On the dock?  Did we hit something to start dragging off the dock?
   {
      switch(mDockMode)
      {
         case DOCKMODE_ITEMS:
      clearSelection(getDatabase());
      mDraggingDockItem = mDockItemHit;      // Could be NULL

      if(mDraggingDockItem)
         SDL_SetCursor(Cursor::getSpray());
            break;

         case DOCKMODE_PLUGINS:
            S32 hitPlugin = findHitPlugin();

            if(hitPlugin >= 0 && hitPlugin < mPluginInfos.size())
               runPlugin(getGame()->getSettings()->getFolderManager(), mPluginInfos[hitPlugin].fileName, Vector<string>());

            break;
      }
   }
   else                 // Mouse is not on dock
   {
      mDraggingDockItem = NULL;
      SDL_SetCursor(Cursor::getDefault());

      // rules for mouse down:
      // if the click has no shift- modifier, then
      //   if the click was on something that was selected
      //     do nothing
      //   else
      //     clear the selection
      //     add what was clicked to the selection
      //  else
      //    toggle the selection of what was clicked
      //
      // Also... if we are unselecting something, don't make that unselection take effect until mouse-up, in case
      // it is the beginning of a drag; that way we don't unselect when we mean to drag when shift is down
      if(InputCodeManager::checkModifier(KEY_SHIFT))  // ==> Shift key is down
      {
         // Check for vertices
         if(mVertexEditMode && !spaceDown && mHitItem && mHitVertex != NONE && mHitItem->getGeomType() != geomPoint)
         {
            if(mHitItem->vertSelected(mHitVertex))
            {
               // These will be unselected when the mouse is released, unless we are initiating a drag event
               mDelayedUnselectObject = mHitItem;
               mDelayedUnselectVertex = mHitVertex;
            }
            else
               mHitItem->aselectVert(mHitVertex);
         }
         else if(mHitItem)    // Item level
         {
            // Unselecting an item
            if(mHitItem->isSelected())
            {
               mDelayedUnselectObject = mHitItem;
               mDelayedUnselectVertex = NONE;
            }
            else
               mHitItem->setSelected(true);

            onSelectionChanged();
         }
         else
            mDragSelecting = true;
      }
      else                                            // ==> Shift key is NOT down
      {

         // If we hit a vertex of an already selected item --> now we can move that vertex w/o losing our selection.
         // Note that in the case of a point item, we want to skip this step, as we don't select individual vertices.
         if(mVertexEditMode && !spaceDown && mHitVertex != NONE && mHitItem && mHitItem->isSelected() && mHitItem->getGeomType() != geomPoint)
         {
            clearSelection(getDatabase());
            mHitItem->selectVert(mHitVertex);
            onSelectionChanged();
         }

         if(mHitItem && mHitItem->isSelected())    // Hit an already selected item
         {
            // Do nothing so user can drag a group of items that's already been selected
         }
         else if(mHitItem && mHitItem->getGeomType() == geomPoint)  // Hit a point item
         {
            clearSelection(getDatabase());
            mHitItem->setSelected(true);
            onSelectionChanged();
         }
         else if(mVertexEditMode && !spaceDown && mHitVertex != NONE && (mHitItem && !mHitItem->isSelected()))      // Hit a vertex of an unselected item
         {        // (braces required)
            if(!(mHitItem->vertSelected(mHitVertex)))
            {
               clearSelection(getDatabase());
               mHitItem->selectVert(mHitVertex);
               onSelectionChanged();
            }
         }
         else if(mHitItem)                                                          // Hit a non-point item, but not a vertex
         {
            clearSelection(getDatabase());
            mHitItem->setSelected(true);
            onSelectionChanged();
         }
         else     // Clicked off in space.  Starting to draw a bounding rectangle?
         {
            mDragSelecting = true;
            clearSelection(getDatabase());
            onSelectionChanged();
         }
      }
   }     // end mouse not on dock block, doc

   findSnapVertex();     // Update snap vertex in the event an item was selected
}


void EditorUserInterface::onMouseClicked_right()
{
   if(InputCodeManager::getState(MOUSE_LEFT) && !InputCodeManager::checkModifier(KEY_CTRL))  // Prevent weirdness
      return;  

   mMousePos.set(DisplayManager::getScreenInfo()->getMousePos());

   if(mCreatingPoly || mCreatingPolyline)
   {
      if(mNewItem->getVertCount() < Geometry::MAX_POLY_POINTS)    // Limit number of points in a polygon/polyline
      {
         mNewItem->addVert(snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)));
         mNewItem->onGeomChanging();
      }
         
      return;
   }

   saveUndoState(true);             // Save undo state before we clear the selection

   clearSelection(getDatabase());   // Unselect anything currently selected
   onSelectionChanged();

   // Can only add new vertices by clicking on item's edge, not it's interior (for polygons, that is)
   if(mEdgeHit != NONE && mHitItem && (mHitItem->getGeomType() == geomPolyLine || mHitItem->getGeomType() >= geomPolygon))
   {
      if(mHitItem->getVertCount() >= Geometry::MAX_POLY_POINTS)     // Polygon full -- can't add more
         return;

      Point newVertex = snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos));   // adding vertex w/ right-mouse

      mAddingVertex = true;

      // Insert an extra vertex at the mouse clicked point, and then select it
      mHitItem->insertVert(newVertex, mEdgeHit + 1);
      mHitItem->selectVert(mEdgeHit + 1);
      mJustInsertedVertex = true;

      // Alert the item that its geometry is changing -- needed by polygons so they can recompute their fill
      mHitItem->onGeomChanging();

      // The user might just insert a vertex and be done; in that case we'll need to rebuild the wall outlines to account
      // for the new vertex.  If the user continues to drag the vertex to a new location, this will be wasted effort...
      mHitItem->onGeomChanged();


      mMouseDownPos = newVertex;
   }
   else     // Start creating a new poly or new polyline (tilde key + right-click ==> start polyline)
   { 
      if(InputCodeManager::getState(KEY_BACKQUOTE))   // Tilde  
      {
         mCreatingPolyline = true;
         mNewItem = new LineItem();
      }
      else
      {
         mCreatingPoly = true;
         mNewItem = new WallItem();
      }

      mNewItem->initializeEditor();
      mNewItem->setTeam(mCurrentTeam);
      mNewItem->addVert(snapPoint(getDatabase(), convertCanvasToLevelCoord(mMousePos)));
   }
}


// Returns true if key was handled, false if not
bool EditorUserInterface::checkPluginKeyBindings(string inputString)
{
   GameSettings *settings = getGame()->getSettings();

   for(S32 i = 0; i < mPluginInfos.size(); i++)
   {
      if(mPluginInfos[i].binding != "" && inputString == mPluginInfos[i].binding)
      {
         runPlugin(settings->getFolderManager(), mPluginInfos[i].fileName, Vector<string>());
         return true;
      }
   }

   return false;
}


static void simpleTextEntryMenuCallback(ClientGame *game, U32 unused)
{
   SimpleTextEntryMenuUI *ui = dynamic_cast<SimpleTextEntryMenuUI *>(game->getUIManager()->getCurrentUI());
   TNLAssert(ui, "Unexpected UI here -- expected a SimpleTextEntryMenuUI!");

   ui->doneEditing();
   ui->getUIManager()->reactivatePrevUI();
}


void idEntryCallback(string text, BfObject *object)
{
   TNLAssert(object, "Expected an object here!");

   // Grab ClientGame from our object
   ClientGame *clientGame = static_cast<ClientGame *>(object->getGame());

   // Check for duplicate IDs
   S32 id = atoi(text.c_str());
   bool duplicateFound = false;

   if(id != 0)
   {
      const Vector<DatabaseObject *> *objList = clientGame->getUIManager()->getUI<EditorUserInterface>()->getDatabase()->findObjects_fast();

      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->getUserAssignedId() == id && !obj->isSelected())
         {
            duplicateFound = true;
            break;
         }
      }
   }


   SimpleTextEntryMenuUI *ui = dynamic_cast<SimpleTextEntryMenuUI *>(clientGame->getUIManager()->getCurrentUI());
   TNLAssert(ui, "Should be in SimpleTextEntryMenuUI!");

   SimpleTextEntryMenuItem *menuItem = static_cast<SimpleTextEntryMenuItem*>(ui->getMenuItem(0));

   // Show a message if we've detected a duplicate ID is being entered
   if(duplicateFound)
   {
      menuItem->setHasError(true);
      menuItem->setHelp("ERROR: Duplicate ID detected!");
   }
   else
   {
      menuItem->setHasError(false);
      menuItem->setHelp("");
   }
}


void EditorUserInterface::startSimpleTextEntryMenu(SimpleTextEntryType entryType)
{
   // No items selected?  Abort!
   if(!anyItemsSelected(getDatabase()))
      return;

   // Are items being dragged?  If so, abort!
   if(mDraggingObjects)
      return;

   string menuTitle = "Some Interesting Title";
   string menuItemTitle = "Another Interesting Title";
   string lineValue = "";

   LineEditorFilter filter = numericFilter;
   void(*callback)(string, BfObject *) = NULL;  // Our input callback; triggers on input change

   static U32 inputLength = 9;   // Less than S32_MAX


   // Find first selected item, and work with that
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   S32 selectedIndex = NONE;
   BfObject *selectedObject = NULL;
   BfObject *obj = NULL;
   for(S32 i = 0; i < objList->size(); i++)
   {
      obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         selectedIndex = i;
         selectedObject = obj;
         break;
      }
   }


   // Adjust our UI depending on which type was requested
   switch(entryType)
   {
      case SimpleTextEntryID:
      {
         menuTitle = "Add Item ID";
         menuItemTitle = "ID:";
         filter = digitsOnlyFilter;
         callback = idEntryCallback;

         // We need to assure that we only assign an ID to ONE object
         const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

         // Unselect all objects but our first selected one
         for(S32 i = 0; i < objList->size(); i++)
         {
            if(i != selectedIndex)
               static_cast<BfObject *>(objList->get(i))->setSelected(false);
         }

         onSelectionChanged();

         S32 currentId = selectedObject->getUserAssignedId();  // selectedObject should never be NULL here

         lineValue = currentId <= 0 ? "" : itos(currentId);

         break;
      }
      case SimpleTextEntryRotateOrigin:
         menuTitle = "Rotate object(s) about (0,0)";
         menuItemTitle = "Angle:";
         break;
      case SimpleTextEntryRotateCentroid:
         menuTitle = "Spin object(s)";
         menuItemTitle = "Angle:";
         break;
      case SimpleTextEntryScale:
         menuTitle = "Resize";
         menuItemTitle = "Resize Factor:";
         break;
      default:
         break;
   }

   // Create our menu item
   SimpleTextEntryMenuItem *menuItem = new SimpleTextEntryMenuItem(menuItemTitle, inputLength, simpleTextEntryMenuCallback);
   menuItem->getLineEditor()->setFilter(filter);

   if(lineValue != "")   // This object has an ID already
      menuItem->getLineEditor()->setString(lineValue);

   if(callback != NULL)  // Add a callback for IDs to check for duplicates
      menuItem->setTextEditedCallback(callback);

   // Create our menu, use unique_ptr since we only need once instance of this menu
   mSimpleTextEntryMenu.reset(new SimpleTextEntryMenuUI(getGame(), menuTitle, entryType));
   mSimpleTextEntryMenu->addMenuItem(menuItem);                // addMenuItem wraps the menu item in a smart pointer
   mSimpleTextEntryMenu->setAssociatedObject(selectedObject);  // Add our object for usage in the menu item callback

   getUIManager()->activate(mSimpleTextEntryMenu.get());
}


void EditorUserInterface::doneWithSimpleTextEntryMenu(SimpleTextEntryMenuUI *menu, S32 data)
{
   SimpleTextEntryType entryType = (SimpleTextEntryType) data;

   string value = menu->getMenuItem(0)->getValue();

   switch(entryType)
   {
      case SimpleTextEntryID:
         setSelectionId(atoi(value.c_str()));
         break;

      case SimpleTextEntryRotateOrigin:
      {
         F32 angle = (F32)Zap::stof(value);
         rotateSelection(-angle, true);       // Positive angle should rotate CW, negative makes that happen
         break;
      }

      case SimpleTextEntryRotateCentroid:
      {
         F32 angle = (F32)Zap::stof(value);
         rotateSelection(-angle, false);      // Positive angle should rotate CW, negative makes that happen
         break;
      }

      case SimpleTextEntryScale:
         scaleSelection((F32)Zap::stof(value));
         break;

      default:
         break;
   }
}


void EditorUserInterface::startAttributeEditor()
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj_i = static_cast<BfObject *>(objList->get(i));

      if(obj_i->isSelected())
      {
         // Force item i to be the one and only selected item type.  This will clear up some problems that might otherwise
         // occur if you had different item types selected while you were editing attributes.   If you have multiple
         // items selected, all will end up with the same values, which only make sense if they are the same kind
         // of object.  So after this runs, there may be multiple items selected, but they'll all  be the same type.
         for(S32 j = 0; j < objList->size(); j++)
         {
            BfObject *obj_j = static_cast<BfObject *>(objList->get(j));

            if(obj_j->isSelected() && obj_j->getObjectTypeNumber() != obj_i->getObjectTypeNumber())
               obj_j->unselect();
         }

         // Activate the attribute editor if there is one
         EditorAttributeMenuUI *menu = mEditorAttributeMenuItemBuilder.getAttributeMenu(obj_i);
         if(menu)
         {
            menu->startEditingAttrs(obj_i);
            getUIManager()->activate(menu);

            saveUndoState();
         }

         break;
      }
   }
}


// Gets run when user exits special-item editing mode, called from attribute editors
void EditorUserInterface::doneEditingAttributes(EditorAttributeMenuUI *editor, BfObject *object)
{
   object->onAttrsChanged();

   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   // Find any other selected items of the same type of the item we just edited, and update their attributes too
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj != object && obj->isSelected() && obj->getObjectTypeNumber() == object->getObjectTypeNumber())
      {
         editor->doneEditingAttrs(obj);  // Transfer attributes from editor to object
         obj->onAttrsChanged();          // And notify the object that its attributes have changed
      }
   }
}


void EditorUserInterface::onKeyUp(InputCode inputCode)
{
   switch(inputCode)
   {
      case KEY_UP:
         mIn = false;
         mUp = false;
         break;
      case KEY_W:
         mUp = false;
         break;
      case KEY_DOWN:
         mOut = false;
         mDown = false;
         break;
      case KEY_S:
         mDown = false;
         break;
      case KEY_LEFT:
      case KEY_A:
         mLeft = false;
         break;
      case KEY_RIGHT:
      case KEY_D:
         mRight = false;
         break;
      case KEY_E:
         mIn = false;
         break;
      case KEY_C:
         mOut = false;
         break;
      case KEY_SPACE:
         mSnapContext = FULL_SNAPPING;
         break;
      case KEY_SHIFT:
         // Check if space is down... if so, modify snapping accordingly
         // This is a little special-casey, but it is, after all, a special case.
         if(InputCodeManager::getState(KEY_SPACE) && mSnapContext == NO_SNAPPING)
            mSnapContext = NO_GRID_SNAPPING;
         break;
      case KEY_TAB:
         mPreviewMode = false;
         break;
      case MOUSE_MIDDLE:
         mAutoScrollWithMouse = mAutoScrollWithMouseReady;
         break;
      case MOUSE_LEFT:
      case MOUSE_RIGHT:  
         if(mDelayedUnselectObject != NULL)
         {
            if(mDelayedUnselectVertex != NONE)
               mDelayedUnselectObject->unselectVert(mDelayedUnselectVertex);
            else
            {
               mDelayedUnselectObject->setSelected(false);
               onSelectionChanged();
            }

            mDelayedUnselectObject = NULL;
         }

         mMousePos.set(DisplayManager::getScreenInfo()->getMousePos());

         if(mDragSelecting)      // We were drawing a rubberband selection box
         {
            Rect r(convertCanvasToLevelCoord(mMousePos), mMouseDownPos);

            fillVector.clear();

            getDatabase()->findObjects(fillVector);


            for(S32 i = 0; i < fillVector.size(); i++)
            {
               BfObject *obj = dynamic_cast<BfObject *>(fillVector[i]);

               // Make sure that all vertices of an item are inside the selection box; basically means that the entire 
               // item needs to be surrounded to be included in the selection
               S32 j;

               for(j = 0; j < obj->getVertCount(); j++)
                  if(!r.contains(obj->getVert(j)))
                     break;
               if(j == obj->getVertCount())
                  obj->setSelected(true);
            }
            mDragSelecting = false;
            onSelectionChanged();
         }

         else if(mDraggingObjects || mAddingVertex)     // We were dragging and dropping.  Could have been a move or a delete (by dragging to dock).
         {
            if(mAddingVertex)
            {
               //deleteUndoState();
               mAddingVertex = false;
            }
            
            onFinishedDragging();
         }

         break;

      default:
         break;
   }     // case
}


// Called when user has been dragging an object and then releases it
void EditorUserInterface::onFinishedDragging()
{
   mDraggingObjects = false;
   SDL_SetCursor(Cursor::getDefault());

   // Dragged item off the dock, then back on  ==> nothing changed; restore to unmoved state, which was stored on undo stack
   if(mouseOnDock() && mDraggingDockItem != NULL)
   {
      undo(false);
      return;
   }

   // Mouse is over the dock and we dragged something to the dock (probably a delete)
   if(mouseOnDock() && !mDraggingDockItem)
   {
      // Only delete items in normal dock mode
      if(mDockMode == DOCKMODE_ITEMS)
      {
         const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();
         bool deletedSomething = false, deletedWall = false;

         for(S32 i = 0; i < objList->size(); i++)    //  Delete all selected items
         {
            BfObject *obj = static_cast<BfObject *>(objList->get(i));

            if(obj->isSelected())
            {
               if(isWallType(obj->getObjectTypeNumber()))
                  deletedWall = true;

               deleteItem(i, true);
               i--;
               deletedSomething = true;
            }
         }

         // We deleted something, do some clean up and our job is done
         if(deletedSomething)
         {
            if(deletedWall)
               doneDeleteingWalls();

            doneDeleting();

            return;
         }
      }
   }

   // Mouse not on dock, we are either:
   // 1. dragging from the dock,
   // 2. moving something,
   // 3. or we moved something to the dock and nothing was deleted, e.g. when dragging a vertex
   // need to save an undo state if anything changed
   if(mDraggingDockItem == NULL)    // Not dragging from dock - user is moving object around screen, or dragging vertex to dock
   {
      // If our snap vertex has moved then all selected items have moved
      bool itemsMoved = mDragCopying || (mSnapObject && mSnapObject->getVert(mSnapVertexIndex) != mMoveOrigin);

      if(itemsMoved)    // Move consumated... update any moved items, and save our autosave
      {
         bool wallMoved = false;

         const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

         for(S32 i = 0; i < objList->size(); i++)
         {
            BfObject *obj = static_cast<BfObject *>(objList->get(i));

            if(obj->isSelected() || objList->get(i)->anyVertsSelected())
               obj->onGeomChanged();

            if(obj->isSelected() && isWallType(obj->getObjectTypeNumber()))      // Wall or polywall
               wallMoved = true;
         }

         if(wallMoved)
            resnapAllEngineeredItems(getDatabase(), true);

         setNeedToSave(true);
         autoSave();

         mDragCopying = false;

         return;
      }
      else if(!mJustInsertedVertex)    // We started our move, then didn't end up moving anything... remove associated undo state
         deleteUndoState();
      else
         mJustInsertedVertex = false;
   }
}


bool EditorUserInterface::mouseOnDock()
{
   return (mMousePos.x >= DisplayManager::getScreenInfo()->getGameCanvasWidth() - mDockWidth - horizMargin &&
           mMousePos.x <= DisplayManager::getScreenInfo()->getGameCanvasWidth() - horizMargin &&
           mMousePos.y >= DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - getDockHeight() &&
           mMousePos.y <= DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin);
}


S32 EditorUserInterface::getItemSelectedCount()
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   S32 count = 0;

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
         count++;
   }

   return count;
}


bool EditorUserInterface::anythingSelected() const
{
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->isSelected() || obj->anyVertsSelected() )
         return true;
   }

   return false;
}


void EditorUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   F32 pixelsToScroll = timeDelta * (InputCodeManager::getState(KEY_SHIFT) ? 1.0f : 0.5f);    // Double speed when shift held down

   if(mLeft && !mRight)
      mCurrentOffset.x += pixelsToScroll;
   else if(mRight && !mLeft)
      mCurrentOffset.x -= pixelsToScroll;
   if(mUp && !mDown)
      mCurrentOffset.y += pixelsToScroll;
   else if(mDown && !mUp)
      mCurrentOffset.y -= pixelsToScroll;

   if(mAutoScrollWithMouse)
   {
      mCurrentOffset += (mScrollWithMouseLocation - mMousePos) * pixelsToScroll / 128.f;
      onMouseMoved();  // Prevents skippy problem while dragging something
   }

   if(mIn && !mOut)
      zoom(timeDelta * 0.002f);
   else if(mOut && !mIn)
      zoom(timeDelta * -0.002f);

   mSaveMsgTimer.update(timeDelta);
   mWarnMsgTimer.update(timeDelta);

   // Process the messageBoxQueue
   if(mMessageBoxQueue.size() > 0)
   {
      ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

      ui->reset();
      ui->setTitle(mMessageBoxQueue[0][0]);
      ui->setInstr(mMessageBoxQueue[0][1]);
      ui->setMessage(mMessageBoxQueue[0][2]);  
      getUIManager()->activate(ui);

      mMessageBoxQueue.erase(0);
   }

   if(mLingeringMessageQueue != "")
   {
      setLingeringMessage(mLingeringMessageQueue);
      mLingeringMessageQueue = "";
   }
}


// This may seem redundant, but... this gets around errors stemming from trying to run setLingeringMessage() from
// the LevelDatabaseUploadThread::run() method.  It seems there are some concurrency issues... blech.
void EditorUserInterface::queueSetLingeringMessage(const string &msg)
{
   mLingeringMessageQueue = msg;
}


void EditorUserInterface::setLingeringMessage(const string &msg)
{
   mLingeringMessage.setSymbolsFromString(msg, NULL, HelpContext, 12, &Colors::red);
}


void EditorUserInterface::clearLingeringMessage()
{
   mLingeringMessage.clear();
}


void EditorUserInterface::setSaveMessage(const string &msg, bool savedOK)
{
   mSaveMsg = msg;
   mSaveMsgTimer.reset();
   mSaveMsgColor = (savedOK ? Colors::green : Colors::red);
}


void EditorUserInterface::clearSaveMessage()
{
   mSaveMsgTimer.clear();
}


void EditorUserInterface::setWarnMessage(const string &msg1, const string &msg2)
{
   mWarnMsg1 = msg1;
   mWarnMsg2 = msg2;
   mWarnMsgTimer.reset(FOUR_SECONDS);    // Display for 4 seconds
   mWarnMsgColor = Colors::ErrorMessageTextColor;
}


void EditorUserInterface::autoSave()
{
   doSaveLevel("auto.save", false);
}


bool EditorUserInterface::saveLevel(bool showFailMessages, bool showSuccessMessages)
{
   string filename = getLevelFileName();
   TNLAssert(filename != "", "Need file name here!");

   if(!doSaveLevel(filename, showFailMessages))
      return false;

   mNeedToSave = false;
   mAllUndoneUndoLevel = mLastUndoIndex;     // If we undo to this point, we won't need to save

   if(showSuccessMessages)
      setSaveMessage("Saved " + getLevelFileName(), true);

   return true;
}


void EditorUserInterface::lockQuit(const string &message)
{
   mQuitLocked = true;
   mQuitLockedMessage = message;
}


void EditorUserInterface::unlockQuit()
{
   mQuitLocked = false;
   getUIManager()->getUI<EditorMenuUserInterface>()->unlockQuit();
}


bool EditorUserInterface::isQuitLocked()
{
   return mQuitLocked;
}


string EditorUserInterface::getQuitLockedMessage()
{
   return mQuitLockedMessage;
}


string EditorUserInterface::getLevelText() 
{
   string result;

   // Write out basic game parameters, including gameType info
   result += getGame()->toLevelCode();    // Note that this toLevelCode appends a newline char; most don't

   // Next come the robots
   for(S32 i = 0; i < mRobotLines.size(); i++)
      result += mRobotLines[i] + "\n";

   // Write out all level items (do two passes; walls first, non-walls next, so turrets & forcefields have something to grab onto)
   const Vector<DatabaseObject *> *objList = getDatabase()->findObjects_fast();

   for(S32 j = 0; j < 2; j++)
   {
      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         // Writing wall items on first pass, non-wall items next -- that will make sure mountable items have something to grab onto
         if((j == 0 && isWallType(obj->getObjectTypeNumber())) || (j == 1 && ! isWallType(obj->getObjectTypeNumber())) )
            result += obj->toLevelCode() + "\n";
      }
   }

   return result;
}


const Vector<PluginInfo> *EditorUserInterface::getPluginInfos() const
{
   return &mPluginInfos;
}


void EditorUserInterface::clearRobotLines()
{
   mRobotLines.clear();
}


void EditorUserInterface::addRobotLine(const string &robotLine)
{
   mRobotLines.push_back(robotLine);
}


// Returns true if successful, false otherwise
bool EditorUserInterface::doSaveLevel(const string &saveName, bool showFailMessages)
{
   try
   {
      FolderManager *folderManager = getGame()->getSettings()->getFolderManager();

      string fileName = joindir(folderManager->levelDir, saveName);
      if(!writeFile(fileName, getLevelText()))
         throw(SaveException("Could not open file for writing"));
   }
   catch (SaveException &e)
   {
      if(showFailMessages)
         setSaveMessage("Error Saving: " + string(e.what()), false);
      return false;
   }

   return true;      // Saved OK
}


// We need some local hook into the testLevelStart() below.  Ugly but apparently necessary.
void testLevelStart_local(ClientGame *game)
{
   game->getUIManager()->getUI<EditorUserInterface>()->testLevelStart();
}


void EditorUserInterface::testLevel()
{
   bool gameTypeError = false;
   if(!getGame()->getGameType())     // Not sure this could really happen anymore...  TODO: Make sure we always have a valid gametype
      gameTypeError = true;

   // With all the map loading error fixes, game should never crash!
   validateLevel();
   if(mLevelErrorMsgs.size() || mLevelWarnings.size() || gameTypeError)
   {
      ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

      ui->reset();
      ui->setTitle("LEVEL HAS PROBLEMS");
      ui->setRenderUnderlyingUi(false);      // Use black background... it's comforting

      string msg = "";

      for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
         msg += mLevelErrorMsgs[i] + "\n";

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
         msg += mLevelWarnings[i] + "\n";

      if(gameTypeError)
      {
         msg += "ERROR: GameType is invalid.\n";
         msg += "(Fix in Level Parameters screen [[GameParameterEditor]])";
      }

      ui->setMessage(msg);
      ui->setInstr("Press [[Y]] to start,  [[Esc]] to cancel");
      ui->registerKey(KEY_Y, testLevelStart_local);      // testLevelStart_local() just calls testLevelStart() below
      getUIManager()->activate(ui);

      return;
   }

   testLevelStart();
}


void EditorUserInterface::testLevelStart()
{
   Cursor::disableCursor();                           // Turn off cursor

   mEditorGameType = getGame()->getGameType();        // Sock our current gametype away, will use it when we reenter the editor

   if(!doSaveLevel(LevelSource::TestFileName, true))
      getGame()->getUIManager()->reactivatePrevUI();  // Saving failed, can't test, reactivate editor
   else
   {
      mWasTesting = true;

      Vector<string> levelList;
      levelList.push_back(LevelSource::TestFileName);

      LevelSourcePtr levelSource = LevelSourcePtr(new FolderLevelSource(levelList, getGame()->getSettings()->getFolderManager()->levelDir));

      getGame()->setGameType(NULL); // Prevents losing seconds on game timer (test level from editor, save, and reload level)

      initHosting(getGame()->getSettingsPtr(), levelSource, true, false);
   }
}


void EditorUserInterface::createNormalizedScreenshot(ClientGame* game)
{
   mPreviewMode = true;
   mNormalizedScreenshotMode = true;

   Renderer::get().clear();
   centerView(true);

   render();
#ifndef BF_NO_SCREENSHOTS
   ScreenShooter::saveScreenshot(game->getUIManager(), game->getSettings(), true,
                                 LevelDatabaseUploadThread::UploadScreenshotFilename);
#endif
   mPreviewMode = false;
   mNormalizedScreenshotMode = false;
   centerView(false);
}



////////////////////////////////////////
////////////////////////////////////////


// Constructor
EditorMenuUserInterface::EditorMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "EDITOR MENU";
}


// Destructor
EditorMenuUserInterface::~EditorMenuUserInterface()
{
   // Do nothing
}


void EditorMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


void EditorUserInterface::findPlugins()
{
   mPluginInfos.clear();
   string dirName = getGame()->getSettings()->getFolderManager()->pluginDir;
   Vector<string> plugins;
   string extension = ".lua";
   getFilesFromFolder(dirName, plugins, &extension, 1);

   // Reference to original
   Vector<PluginBinding> &bindings = getGame()->getSettings()->getIniSettings()->pluginBindings;

   // Check for binding collision in INI.  If one is detected, set its key to empty
   for(S32 i = 0; i < bindings.size(); i++)
   {
      for(S32 j = 0; j < i; j++)  // Efficiency!
      {
         if(bindings[i].key == bindings[j].key)
         {
            bindings[i].key = "";
            break;
         }
      }
   }

   // Loop through all of our detected plugins
   for(S32 i = 0; i < plugins.size(); i++)
   {
      // Try to find the title
      string title;
      Vector<shared_ptr<MenuItem> > menuItems;  // Unused

      EditorPlugin plugin(dirName + "/" + plugins[i], Vector<string>(), mLoadTarget, getGame());

      if(plugin.prepareEnvironment() && plugin.loadScript(false))
         plugin.runGetArgsMenu(title, menuItems);

      // If the title is blank or couldn't be found, use the file name
      if(title == "")
         title = plugins[i];

      PluginInfo info(title, plugins[i], plugin.getDescription(), plugin.getRequestedBinding());

      // Check for a binding from the INI, if it exists set it for this plugin
      for(S32 j = 0; j < bindings.size(); j++)
      {
         if(bindings[j].script == plugins[i])
         {
            info.binding = bindings[j].key;
            break;
         }
      }

      // If no binding is configured, and the plugin specifies a requested binding
      // Use the requested binding if it is not currently in use
      if(info.binding == "" && info.requestedBinding != "")
      {
         bool bindingCollision = false;

         // Determine if this requested binding is already in use by a binding
         // in the INI
         for(S32 j = 0; j < bindings.size(); j++)
         {
            if(bindings[j].key == info.requestedBinding)
            {
               bindingCollision = true;
               break;
            }
         }

         // Determine if this requested binding is already in use by a previously
         // loaded plugin
         for(S32 j = 0; j < mPluginInfos.size(); j++)
         {
            if(mPluginInfos[j].binding == info.requestedBinding)
            {
               bindingCollision = true;
               break;
            }
         }

         info.bindingCollision = bindingCollision;

         // Available!  Set our binding to the requested one
         if(!bindingCollision)
            info.binding = info.requestedBinding;
      }

      mPluginInfos.push_back(info);
   }

   mPluginInfos.sort([](const PluginInfo &a, const PluginInfo &b)
         {
            return alphaSort(a.prettyName, b.prettyName);
         }
   );

   // Now update all the bindings in the INI
   bindings.clear();

   for(S32 i = 0; i < mPluginInfos.size(); i++)
   {
      PluginInfo info = mPluginInfos[i];

      // Only write out valid ones
      if(info.binding == "" || info.bindingCollision)
         continue;

      PluginBinding binding;
      binding.key = info.binding;
      binding.script = info.fileName;
      binding.help = info.description;

      bindings.push_back(binding);
   }
}


U32 EditorUserInterface::findPluginDockWidth()
{
   U32 maxNameWidth = 0;
   U32 maxBindingWidth = 0;
   for(S32 i = 0; i < mPluginInfos.size(); i++)
   {
      U32 nameWidth = getStringWidth(DOCK_LABEL_SIZE, mPluginInfos[i].prettyName.c_str());
      U32 bindingWidth = getStringWidth(DOCK_LABEL_SIZE, mPluginInfos[i].binding.c_str());
      maxNameWidth = max(maxNameWidth, nameWidth);
      maxBindingWidth = max(maxBindingWidth, bindingWidth);
   }
   return maxNameWidth + maxBindingWidth + 2 * horizMargin;
}


extern MenuItem *getWindowModeMenuItem(U32 displayMode);

//////////
// Editor menu callbacks
//////////

void reactivatePrevUICallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->reactivatePrevUI();
}


static void testLevelCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getUI<EditorUserInterface>()->testLevel();
}


void returnToEditorCallback(ClientGame *game, U32 unused)
{
   EditorUserInterface *ui = game->getUIManager()->getUI<EditorUserInterface>();

   ui->saveLevel(true, true);                                     // Save level
   ui->setSaveMessage("Saved " + ui->getLevelFileName(), true);   // Setup a message for the user
   game->getUIManager()->reactivatePrevUI();                      // Return to editor
}


static void activateHelpCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<EditorInstructionsUserInterface>();
}


static void activateLevelParamsCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<GameParamUserInterface>();
}


static void activateTeamDefCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<TeamDefUserInterface>();
}

void uploadToDbCallback(ClientGame *game)
{
   EditorUserInterface* editor = game->getUIManager()->getUI<EditorUserInterface>();
   // Back to the editor
   game->getUIManager()->reactivate(editor);

   editor->createNormalizedScreenshot(game);

   if(game->getGameType()->getLevelName() == "")    
   {
      editor->setSaveMessage("Failed: Level name required", false);
      return;
   }


   if(strcmp(game->getClientInfo()->getName().getString(),
         game->getGameType()->getLevelCredits()->getString()) != 0)
   {
      editor->setSaveMessage("Failed: Level author must match your username", false);
      return;
   }

   RefPtr<LevelDatabaseUploadThread> uploadThread;
   uploadThread = new LevelDatabaseUploadThread(game);
   game->getSecondaryThread()->addEntry(uploadThread);
}


void uploadToDbPromptCallback(ClientGame *game, U32 unused)
{
   EditorUserInterface *editorUI = game->getUIManager()->getUI<EditorUserInterface>();

   if(editorUI->getNeedToSave())
   {
      game->getUIManager()->displayMessageBox("Error", "Press [[Esc]] to continue", "Level must be saved before uploading");
      return;
   }

   ErrorMessageUserInterface *ui = game->getUIManager()->getUI<ErrorMessageUserInterface>();

   ui->reset();
   ui->setTitle("UPLOAD LEVEL?");
   ui->setMessage("Do you want to upload your level to the online\n\n"
                  "level database?");
   ui->setInstr("Press [[Y]] to upload,  [[Esc]] to cancel");

   ui->registerKey(KEY_Y, uploadToDbCallback);
   ui->setRenderUnderlyingUi(false);

   game->getUIManager()->activate(ui);
}


void quitEditorCallback(ClientGame *game, U32 unused)
{
   EditorUserInterface *editorUI = game->getUIManager()->getUI<EditorUserInterface>();

   if(editorUI->getNeedToSave())
   {
      ErrorMessageUserInterface *ui = game->getUIManager()->getUI<ErrorMessageUserInterface>();

      ui->reset();
      ui->setTitle("SAVE YOUR EDITS?");
      ui->setMessage("You have not saved your changes to this level.\n\n"
                     "Do you want to?");
      ui->setInstr("Press [[Y]] to save,  [[N]] to quit,  [[Esc]] to cancel");

      ui->registerKey(KEY_Y, saveLevelCallback);
      ui->registerKey(KEY_N, backToMainMenuCallback);
      ui->setRenderUnderlyingUi(false);

      game->getUIManager()->activate(ui);
   }
   else
     backToMainMenuCallback(game);
}

//////////

void EditorMenuUserInterface::setupMenus()
{
   GameSettings *settings = getGame()->getSettings();
   InputCode keyHelp = getInputCode(settings, BINDING_HELP);

   clearMenuItems();
   addMenuItem(new MenuItem("RETURN TO EDITOR", reactivatePrevUICallback,    "", KEY_R));
   addMenuItem(getWindowModeMenuItem((U32)settings->getIniSettings()->mSettings.getVal<DisplayMode>("WindowMode")));
   addMenuItem(new MenuItem("TEST LEVEL",       testLevelCallback,           "", KEY_T));
   addMenuItem(new MenuItem("SAVE LEVEL",       returnToEditorCallback,      "", KEY_S));
   addMenuItem(new MenuItem("HOW TO EDIT",      activateHelpCallback,        "", KEY_E, keyHelp));
   addMenuItem(new MenuItem("LEVEL PARAMETERS", activateLevelParamsCallback, "", KEY_L, KEY_F3));
   addMenuItem(new MenuItem("MANAGE TEAMS",     activateTeamDefCallback,     "", KEY_M, KEY_F2));

   // Only show the upload to database option if authenticated
   if(getGame()->getClientInfo()->isAuthenticated())
   {
      string title = LevelDatabase::isLevelInDatabase(getGame()->getLevelDatabaseId()) ?
         "UPDATE LEVEL IN DB" :
         "UPLOAD LEVEL TO DB";

      addMenuItem(new MenuItem(title, uploadToDbPromptCallback, "Levels posted at " + HttpRequest::LevelDatabaseBaseUrl, KEY_U));
   }
   else
      addMenuItem(new MessageMenuItem("MUST BE LOGGED IN TO UPLOAD LEVELS TO DB", Colors::gray40));

   if(getUIManager()->getUI<EditorUserInterface>()->isQuitLocked())
      addMenuItem(new MessageMenuItem(getUIManager()->getUI<EditorUserInterface>()->getQuitLockedMessage(), Colors::red));
   else
      addStandardQuitItem();
}


void EditorMenuUserInterface::addStandardQuitItem()
{
   addMenuItem(new MenuItem("QUIT", quitEditorCallback, "", KEY_Q, KEY_UNKNOWN));
}


void EditorMenuUserInterface::unlockQuit()
{
   if(mMenuItems.size() > 0)
   {
      mMenuItems.erase(mMenuItems.size() - 1);     // Remove last item
      addStandardQuitItem();                       // Replace it with QUIT
   }
}


void EditorMenuUserInterface::onEscape()
{
   Cursor::disableCursor();
   getUIManager()->reactivatePrevUI();
}


};
