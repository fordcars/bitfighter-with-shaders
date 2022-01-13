//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "ClientInfo.h"

#include "gameConnection.h"
#include "playerInfo.h"
#include "EngineeredItem.h"   // For EngineerModuleDeployer def

#include "voiceCodec.h"       // This should be removed


class Game;

namespace Zap
{

// Constructor
ClientInfo::ClientInfo()
{
   mPlayerInfo = NULL;
   mGame = NULL;

   // All of these *can* be available to both client and server, but if
   // generated on only one of the client or server they will need to be sent
   // to the other
   mScore = 0;
   mRating = 0;
   mGamesPlayed = 0;
   mTotalScore = 0;
   mTeamIndex = (NO_TEAM + 0);
   mPing = 0;
   mCurrentKillStreak = 0;
   mKills = 0;
   mDeaths = 0;
   mRole = RoleNone;
   mIsAuthenticated = false;
   mBadges = NO_BADGES;
   mNeedToCheckAuthenticationWithMaster = false;     // Does client report that they are verified
   mSpawnDelayed = false;
   mIsBusy = false;
   mIsEngineeringTeleporter = false;
   mShipSystemsDisabled = false;

   mReturnToGameTimer.setPeriod(SPAWN_UNDELAY_TIMER_DELAY);
   mNeedReturnToGameTimer = false;
}


// Destructor
ClientInfo::~ClientInfo()
{
   delete mPlayerInfo;
}


void ClientInfo::setAuthenticated(bool isAuthenticated, Int<BADGE_COUNT> badges, U16 gamesPlayed)
{
   mNeedToCheckAuthenticationWithMaster = false;     // Once we get here, we'll treat the ruling as definitive
   mIsAuthenticated = isAuthenticated; 
   mBadges = badges;
   mGamesPlayed = gamesPlayed;
}


Int<BADGE_COUNT> ClientInfo::getBadges()
{
   return mBadges;
}


bool ClientInfo::hasBadge(MeritBadges badge)
{
   return mBadges & BIT(badge);
}


const StringTableEntry ClientInfo::getName() const
{
   return mName;
}


// An 8 bit bitmask to send to master on connection
const U8 ClientInfo::getPlayerFlagstoSendToMaster() const
{
   U8 bitmask = 0;

#ifdef TNL_DEBUG
   bitmask |= ClientDebugModeFlag;
#endif

   return bitmask;
}


void ClientInfo::setName(const StringTableEntry &name)
{
   mName = name;
}


S32 ClientInfo::getScore()
{
   return mScore;
}


void ClientInfo::setScore(S32 score)
{
   mScore = score;
}


void ClientInfo::addScore(S32 score)
{
   mScore += score;
}


void ClientInfo::setRating(F32 rating)
{
   mRating = rating;
}


F32 ClientInfo::getRating()
{
   return mRating;
}


U16 ClientInfo::getGamesPlayed() const
{
   return mGamesPlayed;
}


void ClientInfo::setShip(Ship *ship)
{
   mShip = ship;
}


// Could return NULL on client if ship is out of scope
Ship *ClientInfo::getShip()
{
   return mShip;
}


void ClientInfo::setNeedToCheckAuthenticationWithMaster(bool needToCheck)
{
   mNeedToCheckAuthenticationWithMaster = needToCheck;
}


bool ClientInfo::getNeedToCheckAuthenticationWithMaster()
{
   return mNeedToCheckAuthenticationWithMaster;
}


// Returns true if spawn has actually been delayed 
bool ClientInfo::isSpawnDelayed()
{
   return mSpawnDelayed;
}


void ClientInfo::setShipSystemsDisabled(bool disabled)
{
   mShipSystemsDisabled = disabled;
}


bool ClientInfo::isShipSystemsDisabled()
{
   return mShipSystemsDisabled;
}


bool ClientInfo::isPlayerInactive()
{
   TNLAssert(false, "Not implemented for this class!");
   return false;
}


ClientInfo::ClientClass ClientInfo::getClientClass() const
{
   TNLAssert(false, "Not implemented for this class!");
   return ClassUnknown;
}


// Returns true if spawn has actually been delayed 
bool ClientInfo::isBusy()
{
   return mIsBusy;
}


void ClientInfo::setIsBusy(bool isBusy)
{
   mIsBusy = isBusy;
}


bool ClientInfo::isLoadoutValid(const LoadoutTracker &loadout, bool engineerAllowed)
{
   if(!loadout.isValid())
      return false;

   // Reject if module contains engineer but it is not enabled on this level
   if(!engineerAllowed && loadout.hasModule(ModuleEngineer))
      return false;

   // Check for illegal weapons
   if(loadout.hasWeapon(WeaponTurret))
      return false;

   return true;     // Passed validation
}


// Server only -- to trigger this on client, use GameConnection::c2sRequestLoadout()
// Updates the ship's loadout to the current or on-deck loadout
void::ClientInfo::updateLoadout(bool useOnDeck, bool engineerAllowed, bool silent)
{
   LoadoutTracker loadout = useOnDeck ? getOnDeckLoadout() : getOldLoadout();

   // This could be triggered if on-deck loadout were set on a level where engineer were allowed,
   // but not actualized until after a level change where engineer was banned.
   if(!isLoadoutValid(loadout, engineerAllowed))   
      return;

   Ship *ship = getShip();

   bool loadoutChanged = false;
   if(ship)
      loadoutChanged = ship->setLoadout(loadout.toU8Vector(), silent);

   // Write some stats
   if(loadoutChanged)
   {
      // This builds a loadout 'hash' by devoting the first 16 bits to modules, the
      // second 16 bits to weapons.  The integer created might look like so:
      //    00000000000001110000000000000011
      U32 loadoutHash = 0;
      for(S32 i = 0; i < ShipModuleCount; i++)
         loadoutHash |= BIT(loadout.hasModule(ShipModule(i)) ? 1 : 0);

      for(S32 i = 0; i < ShipWeaponCount; i++)
         loadoutHash |= BIT(loadout.hasWeapon(WeaponType(i)) ? 1 : 0) << 16;

      getStatistics()->addLoadout(loadoutHash);
   }
}


void ClientInfo::resetLoadout(bool levelHasLoadoutZone)
{
   // Save current loadout to put on-deck
   LoadoutTracker loadout = getOnDeckLoadout();

   resetLoadout();
   mActiveLoadout.resetLoadout();

   // If the current level has a loadout zone, put last level's load-out on-deck
   if(levelHasLoadoutZone)
      requestLoadout(loadout);
}


void ClientInfo::resetLoadout()
{
   mOnDeckLoadout.setLoadout(DefaultLoadout);
}


const LoadoutTracker &ClientInfo::getOnDeckLoadout() const
{
   return mOnDeckLoadout;
}


// Resets this mOldLoadout to its factory settings
void ClientInfo::resetActiveLoadout()
{
   mActiveLoadout.resetLoadout();
}


// This is only called when a ship/bot dies
void ClientInfo::saveActiveLoadout(const LoadoutTracker &loadout)
{
   mActiveLoadout = loadout;
}


S32 ClientInfo::getPing()
{
   return mPing;
}


void ClientInfo::setPing(S32 ping)
{
   mPing = ping;
}


S32 ClientInfo::getTeamIndex()
{
   return mTeamIndex;
}


void ClientInfo::setTeamIndex(S32 teamIndex)
{
   mTeamIndex = teamIndex;
}


void ClientInfo::setShowLevelUpMessage(S32 level)
{
   TNLAssert(false, "Not implemented for this class!");
}


S32 ClientInfo::getShowLevelUpMessage() const
{
   TNLAssert(false, "Not implemented for this class!");
   return NONE;
}


bool ClientInfo::isAuthenticated()
{
   return mIsAuthenticated;
}


ClientInfo::ClientRole ClientInfo::getRole()
{
   return mRole;
}


void ClientInfo::setRole(ClientRole role)
{
   mRole = role;
}


bool ClientInfo::isLevelChanger()
{
   return mRole >= RoleLevelChanger;
}


bool ClientInfo::isAdmin()
{
   return mRole >= RoleAdmin;
}


bool ClientInfo::isOwner()
{
   return mRole >= RoleOwner;
}


F32 ClientInfo::getCalculatedRating()
{
   return mStatistics.getCalculatedRating();
}


// Resets stats and the like
void ClientInfo::endOfGameScoringHandler()
{
   mStatistics.addGamePlayed();
   mStatistics.resetStatistics();
}


void ClientInfo::incrementKillStreak() { mCurrentKillStreak++;   }
void ClientInfo::clearKillStreak()     { mCurrentKillStreak = 0; }


U32 ClientInfo::getKillStreak() const
{
   return mCurrentKillStreak;
}


U32  ClientInfo::getKills()          { return mKills; }
void ClientInfo::setKills(U32 kills) { mKills = kills; }

U32  ClientInfo::getDeaths()           { return mDeaths; }
void ClientInfo::setDeaths(U32 deaths) { mDeaths = deaths; }


LuaPlayerInfo *ClientInfo::getPlayerInfo()
{
   // Lazily initialize
   if(!mPlayerInfo)
      mPlayerInfo = new PlayerInfo(this);   // Deleted in destructor

   return mPlayerInfo;
}


// Server only, robots can run this, bypassing the net interface. Return true if successfuly deployed.
// TODO: Move this elsewhere   <--  where?...  anybody?
bool ClientInfo::sEngineerDeployObject(U32 objectType)
{
   Ship *ship = getShip();
   if(!ship)                                    // Not a good sign...
      return false;                             // ...bail

   GameType *gameType = ship->getGame()->getGameType();

   if(!gameType->isEngineerEnabled())          // Something fishy going on here...
      return false;                            // ...bail

   EngineerModuleDeployer deployer;

   // Check if we can create the engineer object; if not, return false
   if(!deployer.canCreateObjectAtLocation(ship->getGame()->getGameObjDatabase(), ship, objectType))
   {
      if(!isRobot())
         getConnection()->s2cDisplayErrorMessage(deployer.getErrorMessage().c_str());

      return false;
   }

   // Now deploy the object
   if(deployer.deployEngineeredItem(ship->getClientInfo(), objectType))
   {
      // Announce the build
      StringTableEntry msg("%e0 has engineered a %e1.");

      U32 responseEvent;

      Vector<StringTableEntry> e;
      e.push_back(getName());

      Statistics *stats = getStatistics();

      switch(objectType)
      {
         case EngineeredTurret:
            e.push_back("turret");
            responseEvent = EngineerEventTurretBuilt;
            stats->mTurretsEngineered++;
            break;

         case EngineeredForceField:
            e.push_back("force field");
            responseEvent = EngineerEventForceFieldBuilt;
            stats->mFFsEngineered++;
            break;

         case EngineeredTeleporterEntrance:
            e.push_back("teleport entrance");
            responseEvent = EngineerEventTeleporterEntranceBuilt;
            break;

         case EngineeredTeleporterExit:
            e.push_back("teleport exit");
            responseEvent = EngineerEventTeleporterExitBuilt;
            stats->mTeleportersEngineered++;
            break;

         default:
            TNLAssert(false, "Should never get here");
            return false;
      }

      // Send response to client that is doing the engineering
      if(!isRobot())
         getConnection()->s2cEngineerResponseEvent(responseEvent);

      gameType->broadcastMessage(GameConnection::ColorInfo, SFXNone, msg, e);

      // Finally, deduct energy cost
      S32 energyCost = ModuleInfo::getModuleInfo(ModuleEngineer)->getPrimaryPerUseCost();
      if(objectType != EngineeredTeleporterExit)
         ship->creditEnergy(-energyCost);    // Deduct energy from engineer

      return true;
   }

   // Else deployment failed and we need to credit some energy back to the client
   S32 energyCost = ModuleInfo::getModuleInfo(ModuleEngineer)->getPrimaryPerUseCost();
   getConnection()->s2cCreditEnergy(energyCost);
   getConnection()->s2cSetFastRechargeTime(/*gameType->getTotalGamePlayedInMs() -*/ ship->mFastRechargeTimer.getCurrent());

   // Depart quietly
   return false;
}


void ClientInfo::setEngineeringTeleporter(bool engineeringTeleporter)
{
   if(isEngineeringTeleporter() == engineeringTeleporter)
      return;

   setIsEngineeringTeleporter(engineeringTeleporter);

   // Tell everyone that a particular client is engineering a teleport
   for(S32 i = 0; i < mGame->getClientCount(); i++)
   {
      GameType *gameType = mGame->getGameType();

      if(gameType)
         gameType->s2cSetPlayerEngineeringTeleporter(mName, engineeringTeleporter);
   }
}


void ClientInfo::sDisableShipSystems(bool disable)
{
   // We only need to tell the one client
   if(!isRobot() && isShipSystemsDisabled() != disable)  // only send if different
      getConnection()->s2cDisableWeaponsAndModules(disable);

   // Server's ClientInfo
   setShipSystemsDisabled(disable);
}


void ClientInfo::sEngineerDeploymentInterrupted(U32 objectType)
{
   if(objectType == EngineeredTeleporterExit)
   {
      Ship *ship = getShip();

      TNLAssert(ship, "Expected a ship here!");

      if(ship)
         ship->destroyPartiallyDeployedTeleporter();
      sTeleporterCleanup();
   }
}


void ClientInfo::sTeleporterCleanup()
{
   getShip()->setEngineeredTeleporter(NULL);   // Clear out the attached teleporter
   sDisableShipSystems(false);
   setEngineeringTeleporter(false);
}


// Client has requested a new loadout
void ClientInfo::requestLoadout(const LoadoutTracker &loadout)
{
   if(!loadout.isValid())
      return;

   mOnDeckLoadout = loadout;

   GameType *gt = mGame->getGameType();

   if(gt)
      gt->makeRequestedLoadoutActiveIfShipIsInLoadoutZone(this, loadout);
}


const LoadoutTracker &ClientInfo::getOldLoadout() const
{
   return mActiveLoadout;
}


// Return pointer to statistics tracker 
Statistics *ClientInfo::getStatistics()
{
   return &mStatistics;
}


Nonce *ClientInfo::getId()
{
   return &mId;
}


// Server only
void ClientInfo::addKill()
{
   mCurrentKillStreak++;
   mStatistics.addKill(mCurrentKillStreak);
}


// Server only
void ClientInfo::addDeath()
{
   mCurrentKillStreak = 0;
   mStatistics.addDeath();
}


// Methods to provide access to mReturnToGameTimer -- this is used on the server to enforce a post /idle delay
// and used on the client to display the (approximate) time left in that delay.
U32  ClientInfo::getReturnToGameTime()                   { return mReturnToGameTimer.getCurrent();      }
void ClientInfo::setReturnToGameTimer(U32 time)          {        mReturnToGameTimer.reset(time, mReturnToGameTimer.getPeriod()); }
bool ClientInfo::updateReturnToGameTimer(U32 timeDelta)  { return mReturnToGameTimer.update(timeDelta); }
void ClientInfo::requireReturnToGameTimer(bool required) {        mNeedReturnToGameTimer = required;    }


////////////////////////////////////////
////////////////////////////////////////

// Constructor
FullClientInfo::FullClientInfo(Game *game, GameConnection *gameConnection, const string &name, ClientClass clientClass) : ClientInfo()
{
   mGame = game;
   mName = name;

   mClientConnection = gameConnection;
   mClientClass = clientClass;

   mShowLevelUpMessage = NONE;
}


// Destructor
FullClientInfo::~FullClientInfo()
{
   // Do nothing
}

// Seems to run on both client and server, or at least with mGame as a ClientGame and a ServerGame
void FullClientInfo::setAuthenticated(bool isAuthenticated, Int<BADGE_COUNT> badges, U16 gamesPlayed)
{
   TNLAssert(isAuthenticated || badges == NO_BADGES || gamesPlayed == 0, 
                  "Unauthenticated players should never have badges or gamesPlayed!");
   Parent::setAuthenticated(isAuthenticated, badges, gamesPlayed);

   // Broadcast new connection status to all clients, except the client that is authenticated.  Presumably they already know.  
   if(mGame->isServer())
      for(S32 i = 0; i < mGame->getClientCount(); i++)
         if(mGame->getClientInfo(i)->getName() != mName && mGame->getClientInfo(i)->getConnection())
            mGame->getClientInfo(i)->getConnection()->s2cSetAuthenticated(mName, isAuthenticated, badges, gamesPlayed);
}


bool FullClientInfo::isRobot() const
{
   return mClientClass == ClassRobotAddedByAddbots     || 
          mClientClass == ClassRobotAddedByAutoleveler || 
          mClientClass == ClassRobotAddedByLevel       ||
          mClientClass == ClassRobotAddedByLevelNoTeam ||
          mClientClass == ClassRobotWithUnknownSource;
}


void FullClientInfo::setClientClass(ClientClass clientClass)
{
   mClientClass = clientClass;
}


ClientInfo::ClientClass FullClientInfo::getClientClass() const
{
   return mClientClass;
}


// Check if player is "on hold" due to inactivity; bots are never on hold.  Server only!
bool FullClientInfo::isPlayerInactive()
{
   if(isRobot())         // Robots are never spawn-delayed
      return false;

   return getConnection()->getTimeSinceLastMove() > GameConnection::SPAWN_DELAY_TIME;    // 20 secs -- includes time between games
}


bool FullClientInfo::hasReturnToGamePenalty()
{
   return mNeedReturnToGameTimer;
}


// Runs on server and client -- note that RemoteClientInfo has a client-side override for remote players
void FullClientInfo::setSpawnDelayed(bool spawnDelayed)
{
   if(spawnDelayed == mSpawnDelayed)                     // Already in requested state -- nothing to do
      return;

   mSpawnDelayed = spawnDelayed;

   // Notify client if we are the server
   if(mGame->isServer())
   {
      if(spawnDelayed)                                   // Tell client their spawn has been delayed
         getConnection()->s2cPlayerSpawnDelayed(0);      // Any penalty will be sent later
		else
			getConnection()->s2cPlayerSpawnUndelayed();

      mGame->getGameType()->s2cSetIsSpawnDelayed(mName, spawnDelayed);  // Notify other clients
   }
}


GameConnection *FullClientInfo::getConnection()
{
   return mClientConnection;
}


void FullClientInfo::setConnection(GameConnection *conn)
{
   mClientConnection = conn;
}


SoundEffect *FullClientInfo::getVoiceSFX()
{
   TNLAssert(false, "Can't access VoiceSFX from this class!");
   return NULL;
}


VoiceDecoder *FullClientInfo::getVoiceDecoder()
{
   TNLAssert(false, "Can't access VoiceDecoder from this class!");
   return NULL;
}


void FullClientInfo::playVoiceChat(const ByteBufferPtr &voiceBuffer)
{
   TNLAssert(false, "Can't play voice from this class!");
}


bool FullClientInfo::isEngineeringTeleporter()
{
   Ship *ship = getShip();
   TNLAssert(ship, "Expected ship here!");

   if(!ship)
      return false;

   return getShip()->getEngineeredTeleporter() != NULL;
}


void FullClientInfo::setIsEngineeringTeleporter(bool isEngineeringTeleporter)
{
   TNLAssert(false, "isEngineeringTeleporter shouldn't be set for this class!");
}


void FullClientInfo::setShowLevelUpMessage(S32 level)
{
   mShowLevelUpMessage = level;
}


S32 FullClientInfo::getShowLevelUpMessage() const
{
   return mShowLevelUpMessage;
}


////////////////////////////////////////
////////////////////////////////////////


#ifndef ZAP_DEDICATED
// Constructor
RemoteClientInfo::RemoteClientInfo(Game *game, const StringTableEntry &name, bool isAuthenticated, Int<BADGE_COUNT> badges, 
                                   U16 gamesPlayed, RangedU32<0, MaxKillStreakLength> killStreak, bool isRobot, ClientRole role, 
                                   bool isSpawnDelayed, bool isBusy) : ClientInfo()
{
   mGame = game;
   mName = name;
   mIsAuthenticated = isAuthenticated;
   mIsRobot = isRobot;
   mRole = role;
   mTeamIndex = NO_TEAM;
   mBadges = badges;
   mGamesPlayed = gamesPlayed;
   mSpawnDelayed = isSpawnDelayed;
   mIsBusy = isBusy;
   mCurrentKillStreak = killStreak;

   // Initialize speech stuff
   mDecoder = new SpeexVoiceDecoder();                                  // Deleted in destructor
   mVoiceSFX = new SoundEffect(SFXVoice, NULL, 1, Point(), Point());    // RefPtr, will self-delete
}


// Destructor
RemoteClientInfo::~RemoteClientInfo()
{
   delete mDecoder;
}


GameConnection *RemoteClientInfo::getConnection()
{
   TNLAssert(false, "Can't get a GameConnection from a RemoteClientInfo!");
   return NULL;
}


void RemoteClientInfo::setConnection(GameConnection *conn)
{
   TNLAssert(false, "Can't set a GameConnection on a RemoteClientInfo!");
}


void RemoteClientInfo::setSpawnDelayed(bool spawnDelayed)
{
   mSpawnDelayed = spawnDelayed;
}


bool RemoteClientInfo::isRobot() const
{
   return mIsRobot;
}


// Voice chat stuff -- these will be invalid on the server side
SoundEffect *RemoteClientInfo::getVoiceSFX()
{
   return mVoiceSFX;
}


VoiceDecoder *RemoteClientInfo::getVoiceDecoder()
{
   return mDecoder;
}


void RemoteClientInfo::playVoiceChat(const ByteBufferPtr &voiceBuffer)
{
   ByteBufferPtr playBuffer = getVoiceDecoder()->decompressBuffer(voiceBuffer);
   mGame->queueVoiceChatBuffer(getVoiceSFX(), playBuffer);
}


bool RemoteClientInfo::isEngineeringTeleporter()
{
   return mIsEngineeringTeleporter;
}


void RemoteClientInfo::setIsEngineeringTeleporter(bool isEngineeringTeleporter)
{
   mIsEngineeringTeleporter = isEngineeringTeleporter;
}

#endif


};
