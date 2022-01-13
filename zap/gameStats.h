//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GAMESTATS_H_
#define _GAMESTATS_H_

#include "gameWeapons.h"
#include "shipItems.h"

#include "tnlTypes.h"
#include "tnlVector.h"
#include "tnlBitStream.h"
#include "tnlNetStringTable.h"
#include "tnlNonce.h"

using namespace std;
using namespace TNL;

namespace Zap {

struct WeaponStats 
{
   WeaponType weaponType;
   U32 shots;
   U32 hits;
   U32 hitBy;
};


struct ModuleStats 
{
   ShipModule shipModule;
   U32 seconds;
};


struct LoadoutStats
{
   U32 loadoutHash;
};


// Embedded within TeamStats below
struct PlayerStats
{
   string name;
   bool isAuthenticated;
   Nonce nonce;         // Used for authentication, will only send if isAuthenticated == true
   bool isRobot;
   char gameResult;     // 'W', 'L', 'T', not sent, calculated on master
   S32 points;
   U32 kills;
   U32 turretKills;     // Turrets killed
   U32 ffKills;         // FFs killed 
   U32 astKills;        // Asteroids killed 
   U32 turretsEngr;     // Turrets constructed with engineer
   U32 ffEngr;          // Forcefields engineered
   U32 telEngr;         // Teleporters engineered
   U32 deaths;
   U32 suicides;
   U32 switchedTeamCount;
   Vector<WeaponStats> weaponStats;
   Vector<ModuleStats> moduleStats;
   Vector<LoadoutStats> loadoutStats;

   U32 flagPickup;
   U32 flagDrop;
   U32 flagReturn;
   U32 flagScore;
   U32 crashedIntoAsteroid;
   U32 changedLoadout;
   U32 teleport;
   U32 distTraveled;
	U32 playTime;

   bool isAdmin;
   bool isLevelChanger;
   bool isHosting;

   U32 fratricides;  // Count of killing your team

   PlayerStats();    // Constructor
};


// Embedded within GameStats below
struct TeamStats 
{
   U32 intColor;     // To send as number, not string
   string hexColor;  // Not sent, calculated on receiving based on intColor
   string name;
   S32 score;
   char gameResult;  // 'W', 'L', 'T'  // not sent, calculated in master
   Vector<PlayerStats> playerStats;    // Info about all players on this team

   TeamStats();      // Constructor
};


// Embedded with VersionedGameStats below
struct GameStats
{
   string serverName;       // Not sent, master fills this in
   string serverIP;         // Not sent, master fills this in
   S32 cs_protocol_version; // Not sent, master fills this in
   S32 build_version;

   string gameType;
   string levelName;
   bool isOfficial;
   bool isTesting;
   U32 playerCount;  // Not sent, this is calculated while receiving
   U32 duration;     // Game length in seconds
   bool isTeamGame;
   Vector<TeamStats> teamStats;     // For team games

   GameStats();      // Constructor
};


// CURRENT_VERSION = 0 (older then 016, unsupported by master)
// CURRENT_VERSION = 1 (016)
// CURRENT_VERSION = 2 (017)
// CURRENT_VERSION = 3 (018a)
// This contains the whole kilbasa
struct VersionedGameStats
{
   static const U8 CURRENT_VERSION = 3;

   U8 version;
   bool valid;
   GameStats gameStats;
};


extern char getResult(S32 scores, S32 score1, S32 score2, S32 currScore, bool isFirst);
//extern S32 QSORT_CALLBACK playerScoreSort(PlayerStats *a, PlayerStats *b);
extern S32 QSORT_CALLBACK teamScoreSort(TeamStats *a, TeamStats *b);
extern void processStatsResults(GameStats *gameStats);
extern void logGameStats(VersionedGameStats *stats);

};    // end namespace Zap



namespace Types
{
   extern void read (TNL::BitStream &s, Zap::LoadoutStats *val, U8 version);
   extern void write(TNL::BitStream &s, Zap::LoadoutStats &val, U8 version);
   extern void read (TNL::BitStream &s, Zap::WeaponStats  *val, U8 version);
   extern void write(TNL::BitStream &s, Zap::WeaponStats  &val, U8 version);
   extern void read (TNL::BitStream &s, Zap::ModuleStats  *val);
   extern void write(TNL::BitStream &s, Zap::ModuleStats  &val);
   extern void read (TNL::BitStream &s, Zap::PlayerStats  *val, U8 version);
   extern void write(TNL::BitStream &s, Zap::PlayerStats  &val, U8 version);
   extern void read (TNL::BitStream &s, Zap::TeamStats    *val, U8 version);
   extern void write(TNL::BitStream &s, Zap::TeamStats    &val, U8 version);
   extern void read (TNL::BitStream &s, Zap::GameStats    *val, U8 version);
   extern void write(TNL::BitStream &s, Zap::GameStats    &val, U8 version);
   extern void read (TNL::BitStream &s, Zap::VersionedGameStats *val);
   extern void write(TNL::BitStream &s, Zap::VersionedGameStats &val);
};




#endif

