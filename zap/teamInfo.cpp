//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "teamInfo.h"

#include "GameManager.h"
#include "playerInfo.h"
#include "ServerGame.h"
#include "robot.h"
#include "Colors.h"

#include "stringUtils.h"

namespace Zap
{

// Constructor
AbstractTeam::AbstractTeam()
{
   mTeamIndex = -1;
}


// Destructor
AbstractTeam::~AbstractTeam()
{
   // Do nothing
}


const Color &AbstractTeam::getHealthBarColor() const
{
   return mHealthBarColor;
}


// Overridden in EditorTeam, but that calls this
void AbstractTeam::setColor(F32 r, F32 g, F32 b)
{
   mColor.set(r,g,b);

   mHealthBarColor = mColor;
   mHealthBarColor.ensureMinimumBrightness();
}


void AbstractTeam::setColor(const Color &color)
{
   setColor(color.r, color.g, color.b);
}


const Color *AbstractTeam::getColor() const
{
   return &mColor;
}


void AbstractTeam::setTeamIndex(S32 index)
{
   mTeamIndex = index;
}


const S32 AbstractTeam::getTeamIndex() const
{
   return mTeamIndex;
}


// Read team from level file params
bool AbstractTeam::processArguments(S32 argc, const char **argv)
{
   if(argc < 5)               // Not enough arguments!
      return false;

   setName(argv[1]);
   
   Color color;
   color.read(argv + 2);

   setColor(color);

   return true;
}


string AbstractTeam::toLevelCode() const
{
   return string("Team ") + writeLevelString(getName().getString()) + " " + mColor.toRGBString();
}


void AbstractTeam::alterRed(F32 amt) 
{
   Color color(getColor());

   color.r += amt;

   if(color.r < 0)
      color.r = 0;
   else if(color.r > 1)
      color.r = 1;

   setColor(color);     // Do not set mColor directly, or overrides won't fire
}


void AbstractTeam::alterGreen(F32 amt) 
{
   Color color(getColor());

   color.g += amt;

   if(color.g < 0)
      color.g = 0;
   else if(color.g > 1)
      color.g = 1;

   setColor(color);     // Do not set mColor directly, or overrides won't fire
}


void AbstractTeam::alterBlue(F32 amt) 
{
   Color color(getColor());

   color.b += amt;

   if(color.b < 0)
      color.b = 0;
   else if(color.b > 1)
      color.b = 1;

   setColor(color);     // Do not set mColor directly, or overrides won't fire
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
Team::Team()
{
   mPlayerCount = 0;
   mBotCount = 0;
   mScore = 0;
   mRatingSum = 0;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
Team::~Team()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void Team::clearStats()
{
   mPlayerCount = 0;
   mBotCount = 0;
   mRatingSum = 0;
}


void Team::setName(const char *name)
{
   mName.set(name);
}


void Team::setName(StringTableEntry name)
{
   mName = name;
}


StringTableEntry Team::getName() const
{
   return mName;
}


S32 Team::getScore()
{
   return mScore;
}


void Team::setScore(S32 score)
{
   mScore = score;
}


void Team::addScore(S32 score)
{
   mScore += score;
}


F32 Team::getRatingSum()
{
   return mRatingSum;
}


void Team::addToRatingSum(F32 rating)
{
   mRatingSum += rating;
}


S32 Team::getPlayerCount() const
{
   return mPlayerCount;
}


S32 Team::getBotCount() const
{
   TNLAssert(mBotCount >= 0, "No on expects the Spanish Inquisition!");
   return mBotCount;
}


S32 Team::getPlayerBotCount() const
{
   return mPlayerCount + mBotCount;
}


void Team::incrementPlayerCount()
{
   mPlayerCount++;
}


// This should definitely NOT be a public method... 
void Team::incrementBotCount()
{
   mBotCount++;
}


//                Fn name                  Param profiles            Profile count
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getIndex,          ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getName,           ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getScore,          ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getPlayerCount,    ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getPlayers,        ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getColor,          ARRAYDEF({{ END }}), 1 ) \

GENERATE_LUA_FUNARGS_TABLE(Team, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(Team, LUA_METHODS);

#undef LUA_METHODS


const char *Team::luaClassName = "Team";  // Class name as it appears to Lua scripts
REGISTER_LUA_CLASS(Team);

/**
 * @luaclass Team
 *
 * @brief Get information about a team in the current game.
 *
 * @descr The Team object contains data about each team in a game.  You can get a team object from several places, including the GameInfo class.
 *
 * @code
 * gameInfo = bf:getGameInfo()      -- for a bot; use levelgen:getGameInfo() in a levelgen script
 * team = gameInfo:getLeadingTeam()
 * @endcode
 *
 * 
 */


/**
 * @luafunc int Team::getIndex()
 *
 * @brief Get the numerical index of this Team.
 *
 * @return The numerical index of this Team.  Note that the first team has an index of 1, and neutral and hostile "teams" 
 * will have the index of `Team.Neutral` and `Team.Hostile`, respectively.
 */
S32 Team::lua_getIndex(lua_State *L)
{
   return returnInt(L, mTeamIndex + 1);
}


/**
 * @luafunc string Team::getName()
 *
 * @brief Get the name of the Team
 *
 * @return The name of the Team
 */
S32 Team::lua_getName(lua_State *L)
{
   return returnString(L, mName.getString());
}


/**
 * @luafunc int Team::getScore()
 *
 * @brief Get the team's current score.
 *
 * @return The team's current score.
 */
S32 Team::lua_getScore(lua_State *L)
{
   return returnInt(L, mScore);
}


/**
 * @luafunc int Team::getPlayerCount()
 *
 * @brief Get the number of players currently on this team.
 *
 * @return The number of players currently on this team.
 */
S32 Team::lua_getPlayerCount(lua_State *L)
{
   GameManager::getServerGame()->countTeamPlayers();    // Make sure player counts are up-to-date
   return returnInt(L, mPlayerCount);
}


/**
 * @luafunc table Team::getPlayers()
 *
 * @brief Get a table containing all players on a team.
 *
 * @code
 *   local players = team:getPlayers()
 *   for i, v in ipairs(players) do
 *     print(v:getName())
 *   end
 * @endcode
 *
 * @return A table of \link PlayerInfo PlayerInfos\endlink currently on this
 * team. 
 */
S32 Team::lua_getPlayers(lua_State *L)
{
   ServerGame *game = GameManager::getServerGame();

   TNLAssert(game->getPlayerCount() == game->getClientCount(), "Mismatched player counts!");

   S32 pushed = 0;

   lua_newtable(L);    // Create a table, with no slots pre-allocated for our data

   for(S32 i = 0; i < game->getClientCount(); i++)
   {
      ClientInfo *clientInfo = game->getClientInfo(i);

      if(clientInfo->getTeamIndex() == mTeamIndex)
      {
         clientInfo->getPlayerInfo()->push(L);
         pushed++;      // Increment pushed before using it because Lua uses 1-based arrays
         lua_rawseti(L, 1, pushed);
      }
   }

   for(S32 i = 0; i < game->getRobotCount(); i ++)
   {
      if(game->getBot(i)->getTeam() == mTeamIndex)
      {
         game->getBot(i)->getPlayerInfo()->push(L);
         pushed++;      // Increment pushed before using it because Lua uses 1-based arrays
         lua_rawseti(L, 1, pushed);
      }
   }

   return 1;
}


/**
 * @luafunc table Team::getColor()
 *
 * @brief Get the team color
 *
 * @descr
 * The team color is a table with 3 values: red, green, blue.  These are returned
 * as integers in the range of 0-255
 *
 * @code
 *   local color = team:getColor()
 *
 *   local red   = color[0]
 *   local green = color[1]
 *   local blue  = color[2]
 * @endcode
 *
 * @return A table of RGB values for this team's color.
 */
S32 Team::lua_getColor(lua_State *L)
{
   const Color *color = getColor();

   S32 r = S32(color->r * 255);
   S32 g = S32(color->g * 255);
   S32 b = S32(color->b * 255);

   lua_newtable(L);    // Create a table, with no slots pre-allocated for our data

   lua_pushinteger(L, r);
   lua_rawseti(L, 1, 0);
   lua_pushinteger(L, g);
   lua_rawseti(L, 1, 1);
   lua_pushinteger(L, b);
   lua_rawseti(L, 1, 2);

   return 1;
}


////////////////////////////////////////
////////////////////////////////////////

// Destructor
TeamManager::~TeamManager()
{
   clearTeams();
}


S32 TeamManager::getTeamCount()
{
   return mTeams.size();
}


const Color *TeamManager::getTeamColor(S32 index) const
{
   if(index == TEAM_NEUTRAL)
      return &Colors::NeutralTeamColor;
   if(index == TEAM_HOSTILE)
      return &Colors::HostileTeamColor;
   if((U32)index < (U32)mTeams.size())       // Using U32 lets us handle goofball negative team numbers without explicitly checking for them
      return mTeams[index]->getColor();

   return &Colors::magenta;                  // Use a rare color to let user know an object has an out of range team number
}

   
const Color *TeamManager::getTeamHealthBarColor(S32 index) const
{
   if(index < 0 || index >= mTeams.size())
      return getTeamColor(index);
   
   return &(mTeams[index]->getHealthBarColor());
}


AbstractTeam *TeamManager::getTeam(S32 teamIndex)
{
   return mTeams[teamIndex];
}


void TeamManager::removeTeam(S32 teamIndex)
{
   mTeams.erase(teamIndex);
   mTeamHasFlagList.resize(mTeams.size());
}


void TeamManager::addTeam(AbstractTeam *team)
{
   mTeams.push_back(team);
   mTeamHasFlagList.resize(mTeams.size());
   mTeamHasFlagList[mTeamHasFlagList.size() - 1] = false;

   team->setTeamIndex(mTeams.size() - 1);  // Size of mTeams - 1 should be the index
}


void TeamManager::addTeam(AbstractTeam *team, S32 index)
{
   mTeams.insert(index);
   mTeams[index] = team;
   mTeamHasFlagList.resize(mTeams.size());
   mTeamHasFlagList[index] = false;

   team->setTeamIndex(index);
}


bool TeamManager::getTeamHasFlag(S32 teamIndex) const
{
   return mTeamHasFlagList[teamIndex] != 0;
}


void TeamManager::setTeamHasFlag(S32 teamIndex, bool hasFlag)
{
   mTeamHasFlagList[teamIndex] = hasFlag ? 1 : 0;
}


void TeamManager::clearTeamHasFlagList()
{
   for(S32 i = 0; i < mTeamHasFlagList.size(); i++)
      mTeamHasFlagList[i] = 0;
}


void TeamManager::replaceTeam(AbstractTeam *team, S32 index)
{
   mTeams[index] = team;

   team->setTeamIndex(index);
}


// Because Teams are RefPtrs, clearing them here will trigger their deletion
void TeamManager::clearTeams()
{
   mTeams.clear();
   mTeamHasFlagList.clear();
}


S32 TeamManager::getBotCount() const
{
   S32 bots = 0;

   for(S32 i = 0; i < mTeams.size(); i++)
      bots += mTeams[i]->getBotCount();

   return bots;
}



};


