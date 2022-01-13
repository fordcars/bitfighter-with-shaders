//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "luaLevelGenerator.h"
#include "gameLoader.h"
#include "game.h"
#include "gameType.h"
#include "barrier.h"             // For PolyWall def

#include "stringUtils.h"         // fileExists

#include "tnlLog.h"

namespace Zap
{

// Default constructor
LuaLevelGenerator::LuaLevelGenerator() { TNLAssert(false, "Don't use this constructor!"); }

// Standard constructor
LuaLevelGenerator::LuaLevelGenerator(Game *game, const string &scriptName, const Vector<string> &scriptArgs, GridDatabase *gridDatabase)
{
   TNLAssert(scriptName == "" || fileExists(scriptName), "Files should be checked before we get here -- something has gone wrong!");

   if(gridDatabase == NULL)
   {
      gridDatabase = game->getGameObjDatabase();
   }

   mScriptName = scriptName;
   mScriptArgs = scriptArgs;
   mScriptType = ScriptTypeLevelgen;

   mGridDatabase = gridDatabase;
   mLuaGridDatabase = gridDatabase;
   mGame = game;
   mLuaGame = game;  // Set our parent member, too

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
LuaLevelGenerator::~LuaLevelGenerator()
{
   logprintf(LogConsumer::LogLuaObjectLifecycle, "deleted LuaLevelGenerator (%p)\n", this);
   LUAW_DESTRUCTOR_CLEANUP;
}


const char *LuaLevelGenerator::getErrorMessagePrefix() 
{ 
   return "***LEVELGEN ERROR***"; 
}


string LuaLevelGenerator::getScriptName()
{
   return mScriptName;
}



// TODO: Provide mechanism to modify basic level parameters like game length and teams.



//// Let someone else do the work!
//void LuaLevelGenerator::processLevelLoadLine(S32 argc, S32 id, const char **argv, GridDatabase *database, const string &levelFileName)
//{
//   mGame->processLevelLoadLine(argc, id, argv, database, levelFileName);
//}


///// Initialize levelgen specific stuff
bool LuaLevelGenerator::prepareEnvironment()
{
   if(!LuaScriptRunner::prepareEnvironment())
      return false;

   // Set this first so we have this object available in the helper functions in case we need overrides
   setSelf(L, this, "levelgen");

   if(!loadCompileRunEnvironmentScript("timer.lua") || !loadAndRunGlobalFunction(L, LEVELGEN_HELPER_FUNCTIONS_KEY, LevelgenContext))
      return false;

   return true;
}


// This will need to run on both client (from editor) and server (in game)
void LuaLevelGenerator::killScript()
{
   mGame->deleteLevelGen(this);
}


/////
// Lua interface
/**
 * @luaclass LevelGenerator
 *
 * @brief Supervisor class of a levelgen with various utilities.
 */
//               Fn name    Param profiles         Profile count
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, setGameTime,       ARRAYDEF({{ NUM, END }}), 1 )            \
   METHOD(CLASS, globalMsg,         ARRAYDEF({{ STR, END }}), 1 )            \
   METHOD(CLASS, teamMsg,           ARRAYDEF({{ STR, TEAM_INDX, END }}), 1 ) \
   METHOD(CLASS, privateMsg,        ARRAYDEF({{ STR, STR, END }}), 1 )       \
   METHOD(CLASS, announce,          ARRAYDEF({{ STR, END }}), 1 )            \

GENERATE_LUA_METHODS_TABLE(LuaLevelGenerator, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(LuaLevelGenerator, LUA_METHODS);

#undef LUA_METHODS


const char *LuaLevelGenerator::luaClassName = "LuaLevelGenerator";
REGISTER_LUA_CLASS(LuaLevelGenerator);


/**
 * @luafunc void LevelGenerator::setGameTime(num timeInMinutes)
 *
 * @brief Sets the time remaining in the current game to the specified value
 *
 * @descr Decimals can be used to specify fractions of minutes.  A value of 0
 * will set the game time to unlimited
 *
 * @param timeInMinutes Time, in minutes, that the game should continue. Can be
 * fractional.
 */
S32 LuaLevelGenerator::lua_setGameTime(lua_State *L)
{
   checkArgList(L, functionArgs, luaClassName, "setGameTime");

   mGame->setGameTime(getFloat(L, 1));

   // Update clients
   mGame->getGameType()->broadcastNewRemainingTime();

   return 0;
}


static const StringTableEntry levelControllerName = "LevelController";

// The following two functions are implemented in Lua, but we'll document them here for simplicity.

/**
 * @luafunc void LuaLevelGenerator::subscribe(Event event)
 *
 * @brief Let the level listen for and respond to the specified event.
 * 
 * @descr When the specified event is fired, the level can respond by implementing a specially
 * named listener function. See @ref events "Subscribing to Events" for more information.
 * See also the Event page for a list of events you can subscribe to and their
 * corresponding listener funtcitons.  
 *
 * @param event Event to subscribe to.
 */


 /**
  * @luafunc void LuaLevelGenerator::unsubscribe(Event event)
  *
  * @brief Stop listening for the specified event.
  *
  * @descr After unsubscribing from the event, the corresponding listener function will no longer be 
  * called when the event is fired.
  *
  * @param event Event to unsubscribe from.
  */


/**
 * @luafunc void LevelGenerator::globalMsg(string message)
 *
 * @brief Broadcast a message to all players.
 *
 * @param message Message to broadcast.
 */
S32 LuaLevelGenerator::lua_globalMsg(lua_State *L)
{
   checkArgList(L, functionArgs, luaClassName, "globalMsg");

   const char *message = getString(L, 1);

   mGame->sendChat(levelControllerName, NULL, message, true, NO_TEAM);

   // Clean up before firing event
   lua_pop(L, 1);

   // Fire our event handler
   EventManager::get()->fireEvent(this, EventManager::MsgReceivedEvent, message, NULL, true);

   return 0;
}


/**
 * @luafunc void LevelGenerator::teamMsg(string message, num teamIndex)
 *
 * @brief Broadcast a message to players of a team.
 *
 * @param message Message to broadcast.
 * @param teamIndex Index of team to which to send a message.
 */
S32 LuaLevelGenerator::lua_teamMsg(lua_State *L)
{
   checkArgList(L, functionArgs, luaClassName, "teamMsg");

   const char *message = getString(L, 1);
   S32 teamIndex = getTeamIndex(L, 2);

   mGame->sendChat(levelControllerName, NULL, message, false, teamIndex);

   // Clean up before firing event
   lua_pop(L, 2);

   // Fire our event handler
   EventManager::get()->fireEvent(this, EventManager::MsgReceivedEvent, message, NULL, true);

   return 0;
}


/**
 * @luafunc LevelGenerator::privateMsg(string message, string playerName)
 * @brief Broadcast a private message to a player.
 *
 * @param message Message to broadcast.
 * @param playerName Name of player to which to send a message.
 */
// Note that identical code is found in Robot::lua_privateMsg()
S32 LuaLevelGenerator::lua_privateMsg(lua_State *L)
{
   checkArgList(L, functionArgs, luaClassName, "privateMsg");

   const char *message = getString(L, 1);
   const char *playerName = getString(L, 2);

   mGame->sendPrivateChat(levelControllerName, playerName, message);

   // No event fired for private message

   return 0;
}


/**
 * @luafunc LevelGenerator::announce(string message)
 *
 * @brief Broadcast an announcement.
 *
 * @param message Message to broadcast.
 */
S32 LuaLevelGenerator::lua_announce(lua_State *L)
{
   checkArgList(L, functionArgs, luaClassName, "announce");

   const char *message = getString(L, 1);

   mGame->sendAnnouncementFromController(message);

   return 0;
}


};

