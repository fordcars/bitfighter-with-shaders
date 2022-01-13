//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _TEAM_INFO_H_
#define _TEAM_INFO_H_

#include "LuaBase.h"          // Parent class

#include "TeamPreset.h"       // For TeamPreset def
#include "LuaWrapper.h"
#include "TeamConstants.h"    // For TEAM_NEUTRAL et. al.
#include "Color.h"

#include "tnlNetStringTable.h"
#include "tnlNetBase.h"

#include <string>

namespace Zap
{

static const S32 MAX_NAME_LEN = 256;

class AbstractTeam : public RefPtrData
{
private:
   Color mColor;
   Color mHealthBarColor;

protected:
   
   S32 mTeamIndex;           // Team index of this team according to the level file and game

public:
   AbstractTeam();           // Constructor
   virtual ~AbstractTeam();  // Destructor

   static const S32 MAX_TEAM_NAME_LENGTH = 32;

   virtual void setColor(F32 r, F32 g, F32 b);
   virtual void setColor(const Color &color);

   const Color *getColor() const;
   const Color &getHealthBarColor() const;

   virtual void setName(const char *name) = 0;
   virtual StringTableEntry getName() const = 0;

   void setTeamIndex(S32 index);
   const S32 getTeamIndex() const;

   bool processArguments(S32 argc, const char **argv);          // Read team info from level line
   string toLevelCode() const;

   void alterRed(F32 amt);
   void alterGreen(F32 amt);
   void alterBlue(F32 amt);

   virtual S32 getPlayerBotCount() const = 0; 
   virtual S32 getPlayerCount() const = 0;      
   virtual S32 getBotCount() const = 0;
};


////////////////////////////////////////
////////////////////////////////////////

struct TeamInfo
{
   Color color;
   string name;
};


////////////////////////////////////////
////////////////////////////////////////

class FlagSpawn;

// Class for managing teams in the game
class Team : public AbstractTeam
{  
private:
   StringTableEntry mName;

   S32 mPlayerCount;      // Number of human players --> Needs to be computed before use, not dynamically tracked (see countTeamPlayers())
   S32 mBotCount;         // Number of robot players --> Needs to be computed before use, not dynamically tracked

   S32 mScore;
   F32 mRatingSum; 

   Vector<Point> mItemSpawnPoints;
   Vector<FlagSpawn *> mFlagSpawns;    // List of places for team flags to spawn

public:
   Team();              // Constructor
   virtual ~Team();     // Destructor

   void setName(const char *name);
   void setName(StringTableEntry name);
  
   StringTableEntry getName() const;

   S32 getScore();
   void setScore(S32 score);
   void addScore(S32 score);

   F32 getRatingSum();
   void addToRatingSum(F32 rating);     // For summing ratings of all players on a team

   void clearStats();

   // Players & bots on each team:
   // Note that these values need to be precalulated before they are ready for use;
   // they are not dynamically updated!
   S32 getPlayerCount() const;      // Get number of human players on team
   S32 getBotCount() const;         // Get number of bots on team
   S32 getPlayerBotCount() const;   // Get total number of players/bots on team

   void incrementPlayerCount();
   void incrementBotCount();

   ///// Lua interface
   LUAW_DECLARE_CLASS(Team);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getName(lua_State *L);
   S32 lua_getIndex(lua_State *L);
   S32 lua_getPlayerCount(lua_State *L);
   S32 lua_getScore(lua_State *L);
   S32 lua_getPlayers(lua_State *L);
   S32 lua_getColor(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class TeamManager
{
private:
   Vector<RefPtr<AbstractTeam> > mTeams;
   Vector<S32> mTeamHasFlagList;      // Track which team (or teams) have the flag

public:
   virtual ~TeamManager();      // Destructor

   const Color* getTeamHealthBarColor(S32 index) const;
   const Color *getTeamColor(S32 index) const;
   S32 getTeamCount();

   AbstractTeam *getTeam(S32 teamIndex);

   void removeTeam(S32 teamIndex);
   void addTeam(AbstractTeam *team);
   void addTeam(AbstractTeam *team, S32 index);
   void replaceTeam(AbstractTeam *team, S32 index);
   void clearTeams();

   S32 getBotCount() const;

   // Access to mTeamHasFlagList
   bool getTeamHasFlag(S32 teamIndex) const;
   void setTeamHasFlag(S32 teamIndex, bool hasFlag);
   void clearTeamHasFlagList();
};


};

#endif


