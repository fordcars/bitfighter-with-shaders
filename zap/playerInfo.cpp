//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "playerInfo.h"
#include "robot.h"         // Used in RobotPlayerInfo below...

namespace Zap
{

// Constructor
LuaPlayerInfo::LuaPlayerInfo()
{
   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
LuaPlayerInfo::~LuaPlayerInfo()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


/**
 * @luaclass PlayerInfo
 *
 * @brief Contains information about a specific player.
 *
 * @descr The PlayerInfo object contains data about each player, including both
 * humans and robots.
 * 
 * @code
 *   -- Monitor all spawning ships, and if they're a bot, relocate them to 200,200.
 *   function onShipSpawned(ship)
 *       playerInfo = ship:getPlayerInfo()
 *       if playerInfo:isRobot() then
 *          ship.setPos(200, 200)
 *    end
 * @endcode
 */
//                Fn name                  Param profiles            Profile count
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getName,             ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getShip,             ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getTeamIndex,        ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getRating,               ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getScore,                ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, isRobot,                 ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getScriptName,           ARRAYDEF({{ END }}), 1 ) \

GENERATE_LUA_FUNARGS_TABLE(LuaPlayerInfo, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(LuaPlayerInfo, LUA_METHODS);

#undef LUA_METHODS


const char *LuaPlayerInfo::luaClassName = "PlayerInfo";  // Class name as it appears to Lua scripts
REGISTER_LUA_CLASS(LuaPlayerInfo);


/**
 * @luafunc string PlayerInfo::getName()
 *
 * @return The player's game-unique username (e.g. ChumpChange or S_bot.0).
 */
S32 LuaPlayerInfo::lua_getName(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


/**
 * @luafunc Ship PlayerInfo::getShip()
 *
 * @return The player's ship, or nil if the player is "dead".
 */
S32 LuaPlayerInfo::lua_getShip(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


/**
 * @luafunc int PlayerInfo::getTeamIndex()
 *
 * @return The index of the player's team.
 */
S32 LuaPlayerInfo::lua_getTeamIndex(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


/**
 * @luafunc num PlayerInfo::getRating()
 *
 * @return The player's rating a shown on the scoreboard.
 */
S32 LuaPlayerInfo::lua_getRating(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


/**
 * @luafunc int PlayerInfo::getScore()
 *
 * @return The number of points this player has scored this game.
 */
S32 LuaPlayerInfo::lua_getScore(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


/**
 * @luafunc bool PlayerInfo::isRobot()
 *
 * @return `true` if the player is a Robot, false otherwise
 */
S32 LuaPlayerInfo::lua_isRobot(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


/**
 * @luafunc string PlayerInfo::getScriptName()
 *
 * @return The filename of the script (e.g. `s_bot.bot`)
 */
S32 LuaPlayerInfo::lua_getScriptName(lua_State *L)
{
   TNLAssert(false, "Unimplemented method!");
   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

// C++ Constructor
PlayerInfo::PlayerInfo(ClientInfo *clientInfo)
{
   mClientInfo = clientInfo;
}


// Destuctor
PlayerInfo::~PlayerInfo()
{
   // Do nothing
}


S32 PlayerInfo::lua_getName(lua_State *L)
{
   return returnString(L, mClientInfo->getName().getString());
}


S32 PlayerInfo::lua_getScriptName(lua_State *L)
{
   return returnNil(L);
}


S32 PlayerInfo::lua_getTeamIndex(lua_State *L)
{
   return returnTeamIndex(L, mClientInfo->getTeamIndex());
}


S32 PlayerInfo::lua_getRating(lua_State *L)
{
   return returnFloat(L, mClientInfo->getRating());
}


S32 PlayerInfo::lua_getScore(lua_State *L)
{
   return returnInt(L, mClientInfo->getScore());
}


S32 PlayerInfo::lua_isRobot(lua_State *L)
{
   return returnBool(L, mClientInfo->getShip()->isRobot());
}


S32 PlayerInfo::lua_getShip(lua_State *L)
{
	return returnShip(L, mClientInfo->getShip());      // Handles NULL ship
}


////////////////////////////////////////
////////////////////////////////////////

// C++ Constructor
RobotPlayerInfo::RobotPlayerInfo(Robot *robot)
{
   mRobot = robot;
}


RobotPlayerInfo::~RobotPlayerInfo()
{
   // Do nothing
}


S32 RobotPlayerInfo::lua_getName(lua_State *L)
{
   return returnString(L, mRobot->getClientInfo()->getName().getString());
}


S32 RobotPlayerInfo::lua_getShip(lua_State *L)
{
   return returnShip(L, mRobot);
}


S32 RobotPlayerInfo::lua_getScriptName(lua_State *L)
{
   return returnString(L, mRobot->getScriptName());
}


S32 RobotPlayerInfo::lua_getTeamIndex(lua_State *L)
{
   return returnTeamIndex(L, mRobot->getTeam());
}


S32 RobotPlayerInfo::lua_getRating(lua_State *L)
{
   return returnFloat(L, mRobot->getClientInfo()->getRating());
}


S32 RobotPlayerInfo::lua_getScore(lua_State *L)
{
   return returnInt(L, mRobot->getClientInfo()->getScore());
}


S32 RobotPlayerInfo::lua_isRobot(lua_State *L)
{
   return returnBool(L, true);
}


};
