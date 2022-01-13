//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "CoreGame.h"

#include "SoundSystem.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#  include "gameObjectRender.h"
#  include "UIMenuItems.h"
#endif

#include "Colors.h"

#include "stringUtils.h"
#include "GeomUtils.h"

#include <cmath>
#include <map>

namespace Zap {

using namespace LuaArgs;

CoreGameType::CoreGameType() : GameType(0)  // Winning score hard-coded to 0
{
   mRedistMethod = RedistNone;
}

CoreGameType::~CoreGameType()
{
   // Do nothing
}


// Build up key/value map
// One with keyString as key
const map<string, CoreGameType::RedistMethod> CoreGameRedistKeyMap = {
#  define COREGAME_REDIST_ITEM(enumValue, keyString, c, d) {keyString, CoreGameType::enumValue},
      COREGAME_REDIST_TABLE
#  undef COREGAME_REDIST_ITEM
};

// One with enumValue as key
const map<CoreGameType::RedistMethod, string> CoreGameRedistEnumMap = {
#  define COREGAME_REDIST_ITEM(enumValue, keyString, c, d) {CoreGameType::enumValue, keyString},
      COREGAME_REDIST_TABLE
#  undef COREGAME_REDIST_ITEM
};

bool CoreGameType::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc > 0)
      setGameTime(F32(atof(argv[0]) * 60.0 * 1000));      // Game time, stored in minutes in level file

   // Added in 019g
   mRedistMethod = RedistNone;  // Default for all legacy maps
   if(argc > 1)
   {
      // This will default to a return value of '0' if key is not found.  This
      // means it'll default to RedistNone, which I think is OK
      string key = argv[1];
      try
      {
         mRedistMethod = CoreGameRedistKeyMap.at(key);
      }
      catch (...)
      {
         return false;
      }
   }

   return true;
}


string CoreGameType::toLevelCode() const
{
   return string(getClassName()) + " " + getRemainingGameTimeInMinutesString() +
         " " + CoreGameRedistEnumMap.at(mRedistMethod);
}


// Runs on client
void CoreGameType::renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const
{
#ifndef ZAP_DEDICATED

   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship) 
   {
      Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);
      return;
   }

   for(S32 i = mCores.size() - 1; i >= 0; i--)
   {
      CoreItem *coreItem = mCores[i];
      if(coreItem)  // Core may have been destroyed
         if(coreItem->getTeam() != ship->getTeam())
            renderObjectiveArrow(coreItem, canvasWidth, canvasHeight);
   }

   Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);
#endif
}


bool CoreGameType::isTeamCoreBeingAttacked(S32 teamIndex) const
{
   for(S32 i = mCores.size() - 1; i >= 0; i--)
   {
      CoreItem *coreItem = mCores[i];

      if(coreItem && coreItem->getTeam() == teamIndex)
         if(coreItem->isBeingAttacked())
            return true;
   }

   return false;
}


#ifndef ZAP_DEDICATED
const string CoreGameTeamRedistKey = "Team Redistribution";

const char *CoreGameRedistNames[] = {
#  define COREGAME_REDIST_ITEM(a, b, name, d) name,
      COREGAME_REDIST_TABLE
#  undef COREGAME_REDIST_ITEM
};

const char *CoreGameRedistInstructions[] = {
#  define COREGAME_REDIST_ITEM(a, b, c, inst) inst,
      COREGAME_REDIST_TABLE
#  undef COREGAME_REDIST_ITEM
};

void teamRedistCallback(ClientGame *game, U32 val)
{
   // Update the help text for team redistribution
   CoreGameType *coreGame = static_cast<CoreGameType*>(game->getGameType());

   coreGame->getMenuItem(CoreGameTeamRedistKey)->setHelp(CoreGameRedistInstructions[val]);
}


// Any unique items defined here must be handled in both getMenuItem() and saveMenuItem() below!
Vector<string> CoreGameType::getGameParameterMenuKeys()
{
   Vector<string> items = Parent::getGameParameterMenuKeys();

   // Replace "Win Score" as that's not needed here -- win score is determined by the number of cores
   for(S32 i = 0; i < items.size(); i++)
      if(items[i] == "Win Score")
      {
         items[i] = CoreGameTeamRedistKey;
         break;
      }
 
   return items;
}

// Definitions for those items
shared_ptr<MenuItem> CoreGameType::getMenuItem(const string &key)
{
   Vector<string> opts;

   for(S32 i = 0; i < RedistCount; i++)
      opts.push_back(CoreGameRedistNames[i]);

   if(key == CoreGameTeamRedistKey)
      return shared_ptr<MenuItem>(new ToggleMenuItem("Losing Team Redistribution:", opts, mRedistMethod, false,
            teamRedistCallback, "Method of moving players of a losing team to the remaining teams", KEY_T));
   else
      return Parent::getMenuItem(key);
}


bool CoreGameType::saveMenuItem(const MenuItem *menuItem, const string &key)
{
   if(key == CoreGameTeamRedistKey)
      mRedistMethod = RedistMethod(menuItem->getIntValue());
   else
      return Parent::saveMenuItem(menuItem, key);

   return true;
}

#endif


void CoreGameType::addCore(CoreItem *core, S32 teamIndex)
{
   mCores.push_back(core);

   if(!core->isGhost() && U32(teamIndex) < U32(getGame()->getTeamCount()) && getGame()->isServer()) // No EditorTeam
   {
      Team * team = dynamic_cast<Team *>(getGame()->getTeam(teamIndex));
      TNLAssert(team, "Bad team pointer or bad type");
      team->addScore(1);
      s2cSetTeamScore(teamIndex, team->getScore());
   }
}


// Dont't need to handle scores here; will be handled elsewhere
void CoreGameType::removeCore(CoreItem *core)
{
   S32 index = mCores.getIndex(core);
   if(index > -1)
      mCores.erase_fast(index);
}


void CoreGameType::updateScore(ClientInfo *player, S32 team, ScoringEvent event, S32 data)
{
   if(isGameOver()) // Game play ended, no changing score
      return;

   if(player != NULL)  // Individual scores is only for game reports statistics not seen during game play
   {
      S32 points = getEventScore(IndividualScore, event, data);
      TNLAssert(points != naScore, "Bad score value");
      player->addScore(points);
   }

   // If any Core on an active team was destroyed
   if((event == OwnCoreDestroyed || event == EnemyCoreDestroyed) && U32(team) < U32(getGame()->getTeamCount()))
   {
      Team *thisTeam = (Team *)getGame()->getTeam(team);
      thisTeam->addScore(-1); // Count down when a core is destoryed
      s2cSetTeamScore(team, thisTeam->getScore());     // Broadcast result


      S32 numberOfTeamsHaveSomeCores = 0;
      // Count up the teams that still have Cores
      for(S32 i = 0; i < getGame()->getTeamCount(); i++)
      {
         if(((Team *)getGame()->getTeam(i))->getScore() != 0)
            numberOfTeamsHaveSomeCores++;
      }

      // Handle losing team redistribution
      // Happens when this team loses last core and there are at least 2 teams
      // left in play
      if(thisTeam->getScore() == 0 && numberOfTeamsHaveSomeCores >= 2)
      {
         // Get players on this (losing) team
         Vector<ClientInfo*> players;
         for(S32 i = 0; i < getGame()->getClientCount(); i++)
         {
            ClientInfo *info = getGame()->getClientInfo(i);

            if(info->getTeamIndex() == team)
               players.push_back(info);
         }

         // Redistribute them
         handleRedistribution(players);
      }

      // One team left, they win!
      if(numberOfTeamsHaveSomeCores <= 1)
         gameOverManGameOver();
   }
}


S32 CoreGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
{
   if(scoreGroup == TeamScore)
   {
      return naScore;  // We never use TeamScore in CoreGameType
   }
   else  // scoreGroup == IndividualScore
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 1;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return -1;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 1;
         case KillOwnTurret:
            return -1;
         case OwnCoreDestroyed:
            return -5 * data;
         case EnemyCoreDestroyed:
            return 5 * data;
         default:
            return naScore;
      }
   }
}


void CoreGameType::score(ClientInfo *destroyer, S32 coreOwningTeam, S32 score)
{
   Vector<StringTableEntry> e;

   if(destroyer)
   {
      e.push_back(destroyer->getName());
      e.push_back(getGame()->getTeamName(coreOwningTeam));

      // If someone destroyed enemy core
      if(destroyer->getTeamIndex() != coreOwningTeam)
      {
         static StringTableEntry capString("%e0 destroyed a %e1 Core!");
         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, capString, e);

         updateScore(NULL, coreOwningTeam, EnemyCoreDestroyed, score);
      }
      else
      {
         static StringTableEntry capString("%e0 destroyed own %e1 Core!");
         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, capString, e);

         updateScore(NULL, coreOwningTeam, OwnCoreDestroyed, score);
      }
   }
   else
   {
      e.push_back(getGame()->getTeamName(coreOwningTeam));

      static StringTableEntry capString("Something destroyed a %e0 Core!");
      broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, capString, e);

      updateScore(NULL, coreOwningTeam, EnemyCoreDestroyed, score);
   }
}


void CoreGameType::setRedistMethod(RedistMethod method)
{
   mRedistMethod = method;
}


CoreGameType::RedistMethod CoreGameType::getRedistMethod()
{
   return mRedistMethod;
}


static bool teamScoreCompare(Team * const &a, Team* const &b)
{
   return (a->getScore() < b->getScore());
}


static bool balanceCompare(Team * const &a, Team* const &b)
{
   // First sort by player count
   // Game::countTeamPlayers() must be run before this call to getPlayerCount()
   if(a->getPlayerBotCount() < b->getPlayerBotCount())
      return true;
   else if(a->getPlayerBotCount() > b->getPlayerBotCount())
      return false;

   // Else sort by Core count
   return (a->getScore() < b->getScore());
}


static bool humanBotCompare(ClientInfo* const &a, ClientInfo* const &b)
{
   // True if a is human
   return !(a->isRobot());
}


// Redistribute all players on the given team to the remaining ones, using the
// method chosen in the level
void CoreGameType::handleRedistribution(Vector<ClientInfo*> &players)
{
   // Make sure humans are sorted first and have bots fill the gaps
   players.sort(humanBotCompare);

   // Get all remaining teams
   Vector<Team*> remainingTeams;
   S32 teamsCount = getGame()->getTeamCount();

   // Check to make sure at least one team has at least one player...
   for(S32 i = 0; i < teamsCount; i++)
   {
      // Add teams to list if they still have Cores
      Team *team = (Team *)getGame()->getTeam(i);
      if(team->getScore() != 0)
         remainingTeams.push_back(team);
   }

   // Sorts ascending score (fewest cores first)
   remainingTeams.sort(teamScoreCompare);

   bool playersMoved = true;

   // Divvy up players according the the chosen algorithm
   switch(mRedistMethod)
   {
      case RedistBalanced:
      // Like the 'Balanced' option but skip the winning team
      case RedistBalancedNonWinners:
      {
         // Remove winning team if we're using non-winners method
         if(mRedistMethod == RedistBalancedNonWinners)
            remainingTeams.pop_back();

         // Duplicate and sort by the balancing algorithm
         Vector<Team*> balancedSortedTeams(remainingTeams);

         for(S32 i = 0; i < players.size(); i++)
         {
            // Update mTeams, must be run before anything that calls Team::getPlayerCount()
            getGame()->countTeamPlayers();

            // Sort teams according to balance algo
            balancedSortedTeams.sort(balanceCompare);

            ClientInfo *clientInfo = players[i];

            // Grab team from sorted list, approprate team should be first after
            // re-sort
            Team *receivingTeam = balancedSortedTeams[0];

            // Send player to new team
            changeClientTeam(clientInfo, receivingTeam->getTeamIndex());
         }
      }
      break;

      case RedistRandom:
      {
         for(S32 i = 0; i < players.size(); i++)
         {
            // Randomly grab a team index
            S32 randomIndex = TNL::Random::readI(0, remainingTeams.size() - 1);
            Team *receivingTeam = remainingTeams[randomIndex];

            ClientInfo *clientInfo = players[i];
            // Send player to new team
            changeClientTeam(clientInfo, receivingTeam->getTeamIndex());
         }
      }
      break;

      case RedistLoser:
      {
         Team *receivingTeam = remainingTeams.first();  // Losers at index 0

         for(S32 i = 0; i < players.size(); i++)
         {
            ClientInfo *clientInfo = players[i];
            // Send player to new team
            changeClientTeam(clientInfo, receivingTeam->getTeamIndex());
         }
      }
      break;

      case RedistWinner:
      {
         Team *receivingTeam = remainingTeams.last();  // Winners at last index

         for(S32 i = 0; i < players.size(); i++)
         {
            ClientInfo *clientInfo = players[i];
            // Send player to new team
            changeClientTeam(clientInfo, receivingTeam->getTeamIndex());
         }
      }
      break;

      // Do nothing, players stay on same team and just harass other teams
      case RedistNone:
      default:
         playersMoved = false;
         break;
   }

   // Send server message to players that they've been moved
   if(playersMoved)
   {
      for(S32 i = 0; i < players.size(); i++)
      {
         ClientInfo *clientInfo = players[i];
         if(!clientInfo->isRobot())
            clientInfo->getConnection()->s2cDisplayMessage(GameConnection::ColorRed, SFXNone,
                  "Failed to defend Cores. Moved to a different team.");
      }
   }
}


void CoreGameType::handleNewClient(ClientInfo *clientInfo)
{
   S32 teamsCount = getGame()->getTeamCount();

   // Find if any teams have already lost
   bool hasLostTeam = false;
   for(S32 i = 0; i < teamsCount; i++)
   {
      // Add teams to list if they still have Cores
      Team *team = (Team *)getGame()->getTeam(i);
      if(team->getScore() == 0)
         hasLostTeam = true;
   }

   // If we have at least one team that has already lost, redistribute
   // this player properly
   if(hasLostTeam)
   {
      // Redistribute the newly joined player
      Vector<ClientInfo*> players;
      players.push_back(clientInfo);

      handleRedistribution(players);
   }
}


GameTypeId CoreGameType::getGameTypeId() const { return CoreGame; }

const char *CoreGameType::getShortName() const { return "Core"; }

static const char *instructions[] = { "Destroy enemy Cores",  0 };
const char **CoreGameType::getInstructionString() const { return instructions; }
HelpItem CoreGameType::getGameStartInlineHelpItem() const { return CoreGameStartItem; }


bool CoreGameType::canBeTeamGame()       const { return true;  }
bool CoreGameType::canBeIndividualGame() const { return false; }


TNL_IMPLEMENT_NETOBJECT(CoreGameType);


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(CoreItem);
//class LuaCore;


// Ratio at which damage is reduced so that Core Health can fit between 0 and 1.0
// for easier bit transmission
const F32 CoreItem::DamageReductionRatio = 1000.0f;

const F32 CoreItem::PANEL_ANGLE = FloatTau / (F32) CORE_PANELS;

// Historical default = 1
const U32 CoreItem::CoreDefaultRotationSpeed = 1;
const U32 CoreItem::CoreMaxRotationSpeed = 15;

/**
 * @luafunc CoreItem::CoreItem()
 * @luafunc CoreItem::CoreItem(Geom geom, int team)
 * @luafunc CoreItem::CoreItem(Geom geom, int team, num health)
 */
// Combined Lua / C++ default constructor
CoreItem::CoreItem(lua_State *L) : Parent(F32(CoreRadius * 2))    
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = CoreTypeNumber;
   setStartingHealth(F32(CoreDefaultStartingHealth));      // Hits to kill

   mHasExploded = false;
   mHeartbeatTimer.reset(CoreHeartbeatStartInterval);
   mCurrentExplosionNumber = 0;
   mPanelGeom.isValid = false;
   mRotationSpeed = CoreDefaultRotationSpeed;


   // Read some params from our L, if we have it
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, TEAM_INDX, END }, { PT, TEAM_INDX, INT, END }}, 3 };
      S32 profile = checkArgList(L, constructorArgList, "CoreItem", "constructor");
      if(profile == 1)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }
      else if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
         setStartingHealth(getFloat(L, 3));
      }
   }

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
CoreItem::~CoreItem()
{
   LUAW_DESTRUCTOR_CLEANUP;

   // Alert the gameType, if it still exists (it might not when the game is over)
   if(getGame())
   {
      GameType *gameType = getGame()->getGameType();
      if(gameType && gameType->getGameTypeId() == CoreGame)
         static_cast<CoreGameType *>(gameType)->removeCore(this);
   }
}


CoreItem *CoreItem::clone() const
{
   return new CoreItem(*this);
}


F32 CoreItem::getCoreAngle(U32 time)
{
   // This takes the time (in ms) since the start of the level and normalizes
   // it to one rotation every 16384 ms
   F32 fraction = F32(time & (CoreRotationTimeDefault-1)) / CoreRotationTimeDefault;

   return fraction * FloatTau;  // Portion of a circle
}


void CoreItem::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(shouldRender())
   {
      GameType *gameType = getGame()->getGameType();
      S32 time = gameType->getTotalGamePlayedInMs();
      const Color *color = getColor();
      const Color &hbc = getHealthBarColor();
      PanelGeom *panelGeom = getPanelGeom();
      renderCore(pos, color, hbc, time, panelGeom, mPanelHealth, mStartingPanelHealth);
   }
#endif
}


bool CoreItem::shouldRender() const
{
   return !mHasExploded;
}


void CoreItem::renderDock()
{
#ifndef ZAP_DEDICATED
   Point pos = getPos();
   renderCoreSimple(pos, &Colors::white, 10);
#endif
}


void CoreItem::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
#ifndef ZAP_DEDICATED
   Point pos = getPos();
   renderCoreSimple(pos, getColor(), CoreRadius * 2);
#endif
}


#ifndef ZAP_DEDICATED
// xpos and ypos are coords of upper left corner of the adjacent score.  We'll need to adjust those coords
// to position our ornament correctly.
void CoreGameType::renderScoreboardOrnament(S32 teamIndex, S32 xpos, S32 ypos) const
{
   Point center(xpos, ypos + 16);
   renderCoreSimple(center, getGame()->getTeam(teamIndex)->getColor(), 20);

   // Flash the ornament if the Core is being attacked
   if(isTeamCoreBeingAttacked(teamIndex)) 
   {
      const S32 flashCycleTime = 300;  
      const Color *color = &Colors::red80;
      F32 alpha = 1;

      if(getGame()->getCurrentTime() % flashCycleTime <= flashCycleTime / 2)
      {
         color = &Colors::yellow;
         alpha = 0.6f;
      }
         
      drawCircle(center, 15, color, alpha);
   }
}

#endif


// Render some attributes when item is selected but not being edited
void CoreItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("Health");   values.push_back(itos(S32(mStartingHealth * DamageReductionRatio + 0.5)));
   keys.push_back("Speed");   values.push_back(itos(S32(mRotationSpeed)));
}


const char *CoreItem::getOnScreenName()     { return "Core";  }
const char *CoreItem::getOnDockName()       { return "Core";  }
const char *CoreItem::getPrettyNamePlural() { return "Cores"; }
const char *CoreItem::getEditorHelpString() { return "Core.  Destroy to score."; }


F32 CoreItem::getEditorRadius(F32 currentScale)
{
   return CoreRadius * currentScale + 5;
}


bool CoreItem::getCollisionCircle(U32 state, Point &center, F32 &radius) const
{
   center = getPos();
   radius = CoreRadius;
   return true;
}


const Vector<Point> *CoreItem::getCollisionPoly() const
{
   return NULL;
}


void CoreItem::getBufferForBotZone(F32 bufferRadius, Vector<Point> &outputPoly) const
{
   // Simple core - 10 sides means rotation vertices won't affect the buffer much
   Vector<Point> simpleCore;
   calcPolygonVerts(getPos(), CORE_PANELS, CoreRadius, 0, simpleCore);

   // Expand polygon, use mitering to reduce complexity
   offsetPolygon(&simpleCore, outputPoly, bufferRadius, ClipperLib::jtMiter);
}


bool CoreItem::isPanelDamaged(S32 panelIndex)
{
   return mPanelHealth[panelIndex] < mStartingPanelHealth && mPanelHealth[panelIndex] > 0;
}


bool CoreItem::isPanelInRepairRange(const Point &origin, S32 panelIndex)
{
   PanelGeom *panelGeom = getPanelGeom();

   F32 distanceSq1 = (panelGeom->getStart(panelIndex)).distSquared(origin);
   F32 distanceSq2 = (panelGeom->getEnd(panelIndex)).distSquared(origin);
   S32 radiusSq = Ship::RepairRadius * Ship::RepairRadius;

   // Ignoring case where center is in range while endpoints are not...
   return (distanceSq1 < radiusSq) || (distanceSq2 < radiusSq);
}


void CoreItem::damageObject(DamageInfo *theInfo)
{
   if(mHasExploded)
      return;

   if(theInfo->damageAmount == 0)
      return;

   // Special logic for handling the repairing of Core panels
   if(theInfo->damageAmount < 0)
   {
      // Heal each damaged core if it is in range
      for(S32 i = 0; i < CORE_PANELS; i++)
         if(isPanelDamaged(i))
            if(isPanelInRepairRange(theInfo->damagingObject->getPos(), i))
            {
               mPanelHealth[i] -= theInfo->damageAmount / DamageReductionRatio;

               // Don't overflow
               if(mPanelHealth[i] > mStartingPanelHealth)
                  mPanelHealth[i] = mStartingPanelHealth;

               setMaskBits(PanelDamagedMask << i);
            }

      // We're done if we're repairing
      return;
   }

   // Check for friendly fire
   if(theInfo->damagingObject->getTeam() == getTeam())
      return;

   // Which panel was hit?  Look at shot position, compare it to core position
   F32 shotAngle;
   Point p = getPos();

   // Determine angle for Point projectiles like Phaser
   if(theInfo->damageType == DamageTypePoint)
      shotAngle = p.angleTo(theInfo->collisionPoint);

   // Area projectiles
   else
      shotAngle = p.angleTo(theInfo->damagingObject->getPos());


   PanelGeom *panelGeom = getPanelGeom();
   F32 coreAngle = panelGeom->angle;

   F32 combinedAngle = (shotAngle - coreAngle);

   // Make sure combinedAngle is between 0 and Tau -- sometimes angleTo returns odd values
   while(combinedAngle < 0)
      combinedAngle += FloatTau;
   while(combinedAngle >= FloatTau)
      combinedAngle -= FloatTau;

   S32 hit = (S32) (combinedAngle / PANEL_ANGLE);

   if(mPanelHealth[hit] > 0)
   {
      mPanelHealth[hit] -= theInfo->damageAmount / DamageReductionRatio;

      if(mPanelHealth[hit] < 0)
         mPanelHealth[hit] = 0;

      setMaskBits(PanelDamagedMask << hit);
   }

   // Determine if Core is destroyed by checking all the panel healths
   bool coreDestroyed = false;

   if(mPanelHealth[hit] == 0)
   {
      coreDestroyed = true;
      for(S32 i = 0; i < CORE_PANELS; i++)
         if(mPanelHealth[i] > 0)
         {
            coreDestroyed = false;
            break;
         }
   }

   if(coreDestroyed)
   {
      // Send Lua event
      EventManager::get()->fireEvent(EventManager::CoreDestroyedEvent, this);

      // We've scored!
      GameType *gameType = getGame()->getGameType();
      if(gameType)
      {
         ClientInfo *destroyer = theInfo->damagingObject->getOwner();
         if(gameType->getGameTypeId() == CoreGame)
            static_cast<CoreGameType*>(gameType)->score(destroyer, getTeam(), CoreGameType::DestroyedCoreScore);
      }

      mHasExploded = true;
      deleteObject(ExplosionCount * ExplosionInterval);  // Must wait for triggered explosions
      setMaskBits(ExplodedMask);                         
      disableCollision();

      return;
   }

   // Reset the attacked warning timer if we're not healing
   if(theInfo->damageAmount > 0)
      mAttackedWarningTimer.reset(CoreAttackedWarningDuration);
}


#ifndef ZAP_DEDICATED
void CoreItem::doExplosion(const Point &pos)
{
   ClientGame *game = static_cast<ClientGame *>(getGame());

   Color teamColor = *(getColor());
   Color CoreExplosionColors[12] = {
      Colors::red,
      teamColor,
      Colors::white,
      teamColor,
      Colors::blue,
      teamColor,
      Colors::white,
      teamColor,
      Colors::yellow,
      teamColor,
      Colors::white,
      teamColor,
   };

   bool isStart = mCurrentExplosionNumber == 0;

   S32 xNeg = TNL::Random::readB() ? 1 : -1;
   S32 yNeg = TNL::Random::readB() ? 1 : -1;

   F32 x = TNL::Random::readF() * xNeg * FloatSqrtHalf * CoreRadius;  // exactly sin(45)
   F32 y = TNL::Random::readF() * yNeg * FloatSqrtHalf * CoreRadius;

   // First explosion is at the center
   Point blastPoint = isStart ? pos : pos + Point(x, y);

   // Also add in secondary sound at start
//   if(isStart)
//      SoundSystem::playSoundEffect(SFXCoreExplodeSecondary, blastPoint);

   SoundSystem::playSoundEffect(SFXCoreExplode, blastPoint, Point(), 1 - 0.25f * F32(mCurrentExplosionNumber));

   game->emitBlast(blastPoint, 600 - 100 * mCurrentExplosionNumber);
   game->emitExplosion(blastPoint, 4.f - F32(mCurrentExplosionNumber), CoreExplosionColors, ARRAYSIZE(CoreExplosionColors));

   mCurrentExplosionNumber++;
}
#endif


PanelGeom *CoreItem::getPanelGeom()
{
   if(!mPanelGeom.isValid)
      fillPanelGeom(getPos(), getGame()->getGameType()->getTotalGamePlayedInMs() * mRotationSpeed, mPanelGeom);

   return &mPanelGeom;
}


// static method
void CoreItem::fillPanelGeom(const Point &pos, U32 time, PanelGeom &panelGeom)
{
   F32 size = CoreRadius;

   F32 angle = getCoreAngle(time);
   panelGeom.angle = angle;

   F32 angles[CORE_PANELS];

   for(S32 i = 0; i < CORE_PANELS; i++)
      angles[i] = i * PANEL_ANGLE + angle;

   for(S32 i = 0; i < CORE_PANELS; i++)
      panelGeom.vert[i].set(pos.x + cos(angles[i]) * size, pos.y + sin(angles[i]) * size);

   Point start, end, mid;
   for(S32 i = 0; i < CORE_PANELS; i++)
   {
      start = panelGeom.vert[i];
      end   = panelGeom.vert[(i + 1) % CORE_PANELS];      // Next point, with wrap-around
      mid   = (start + end) * .5;

      panelGeom.mid[i].set(mid);
      panelGeom.repair[i].interp(.6f, mid, pos);
   }

   panelGeom.isValid = true;
}

#ifndef ZAP_DEDICATED

void CoreItem::doPanelDebris(S32 panelIndex)
{
   ClientGame *game = static_cast<ClientGame *>(getGame());

   Point pos = getPos();               // Center of core

   PanelGeom *panelGeom = getPanelGeom();
   
   Point dir = panelGeom->mid[panelIndex] - pos;   // Line extending from the center of the core towards the center of the panel
   dir.normalize(100);
   Point cross(dir.y, -dir.x);         // Line parallel to the panel, perpendicular to dir

   // Debris line is relative to (0,0)
   Vector<Point> points;
   points.push_back(Point(0, 0));
   points.push_back(Point(0, 0));      // Dummy point will be replaced below

   // Draw debris for the panel
   S32 num = Random::readI(5, 15);
   const Color *teamColor = getColor();

   Point chunkPos, chunkVel;           // Reusable containers

   for(S32 i = 0; i < num; i++)
   {
      static const S32 MAX_CHUNK_LENGTH = 10;
      points[1].set(0, Random::readF() * MAX_CHUNK_LENGTH);

      chunkPos = panelGeom->getStart(panelIndex) + (panelGeom->getEnd(panelIndex) - panelGeom->getStart(panelIndex)) * Random::readF();
      chunkVel = dir * (Random::readF() * 10  - 3) * .2f + cross * (Random::readF() * 30  - 15) * .05f;

      S32 ttl = Random::readI(2500, 3000);
      F32 startAngle = Random::readF() * FloatTau;
      F32 rotationRate = Random::readF() * 4 - 2;

      // Every-other chunk is team color instead of panel color
      Color chunkColor = i % 2 == 0 ? Colors::gray80 : *teamColor;

      game->emitDebrisChunk(points, chunkColor, chunkPos, chunkVel, ttl, startAngle, rotationRate);
   }


   // Draw debris for the panel health 'stake'
   num = Random::readI(5, 15);
   for(S32 i = 0; i < num; i++)
   {
      points.erase(1);
      points.push_back(Point(0, Random::readF() * 10));

      Point sparkVel = cross * (Random::readF() * 20  - 10) * .05f + dir * (Random::readF() * 2  - .5f) * .2f;
      S32 ttl = Random::readI(2500, 3000);
      F32 angle = Random::readF() * FloatTau;
      F32 rotation = Random::readF() * 4 - 2;

      game->emitDebrisChunk(points, Colors::gray20, (panelGeom->mid[i] + pos) / 2, sparkVel, ttl, angle, rotation);
   }

   // And do the sound effect
   SoundSystem::playSoundEffect(SFXCorePanelExplode, panelGeom->mid[panelIndex]);
}

#endif


void CoreItem::idle(BfObject::IdleCallPath path)
{
   mPanelGeom.isValid = false;      // Force recalculation of panel geometry next time it's needed

   // Update attack timer on the server
   if(path == ServerIdleMainLoop)
   {
      // If timer runs out, then set this Core as having a changed state so the client
      // knows it isn't being attacked anymore
      if(mAttackedWarningTimer.update(mCurrentMove.time))
         setMaskBits(ItemChangedMask);
   }

#ifndef ZAP_DEDICATED
   // Only run the following on the client
   if(path != ClientIdlingNotLocalShip)
      return;

   // Update Explosion Timer
   if(mHasExploded)
   {
      if(mExplosionTimer.getCurrent() != 0)
         mExplosionTimer.update(mCurrentMove.time);
      else
         if(mCurrentExplosionNumber < ExplosionCount)
         {
            doExplosion(getPos());
            mExplosionTimer.reset(ExplosionInterval);
         }
   }

   if(mHeartbeatTimer.getCurrent() != 0)
      mHeartbeatTimer.update(mCurrentMove.time);
   else
   {
      // Thump thump
      SoundSystem::playSoundEffect(SFXCoreHeartbeat, getPos());

      // Now reset the timer as a function of health
      // Exponential
      F32 health = getHealth();
      U32 soundInterval = CoreHeartbeatMinInterval + U32(F32(CoreHeartbeatStartInterval - CoreHeartbeatMinInterval) * health * health);

      mHeartbeatTimer.reset(soundInterval);
   }

   // Emit some sparks from dead panels
   if(Platform::getRealMilliseconds() % 100 < 20)  // 20% of the time...
   {
      Point cross, dir;

      for(S32 i = 0; i < CORE_PANELS; i++)
      {
         // Panel is dead (ensured by damageObject() )
         if(mPanelHealth[i] == 0)
         {
            Point sparkEmissionPos = getPos();
            sparkEmissionPos += dir * 3;

            Point dir = getPanelGeom()->mid[i] - getPos();  // Line extending from the center of the core towards the center of the panel
            dir.normalize(100);
            Point cross(dir.y, -dir.x);                     // Line parallel to the panel, perpendicular to dir

            Point vel = dir * (Random::readF() * 3 + 2) + cross * (Random::readF() - .2f);
            U32 ttl = Random::readI(0, 1000) + 500;

            static_cast<ClientGame *>(getGame())->emitSpark(sparkEmissionPos, vel, Colors::gray20, ttl, UI::SparkTypePoint);
         }
      }
   }
#endif
}


void CoreItem::setStartingHealth(F32 health)
{
   mStartingHealth = health / DamageReductionRatio;

   // Now that starting health has been set, divide it amongst the panels
   mStartingPanelHealth = mStartingHealth / CORE_PANELS;
   
   // Core's total health is divided evenly amongst its panels
   for(S32 i = 0; i < 10; i++)
      mPanelHealth[i] = mStartingPanelHealth;
}


F32 CoreItem::getStartingHealth() const
{
   return mStartingHealth * DamageReductionRatio;
}


F32 CoreItem::getTotalCurrentHealth() const
{
   F32 total = 0;

   for(S32 i = 0; i < CORE_PANELS; i++)
      total += mPanelHealth[i];

   return total;
}


F32 CoreItem::getHealth() const
{
   // health is from 0 to 1.0
   return getTotalCurrentHealth() / mStartingHealth;
}


U32 CoreItem::getRotationSpeed() const
{
   return mRotationSpeed;
}


void CoreItem::setRotationSpeed(U32 speed)
{
   if(speed > CoreMaxRotationSpeed)
      speed = CoreMaxRotationSpeed;

   mRotationSpeed = speed;
}


Vector<Point> CoreItem::getRepairLocations(const Point &repairOrigin)
{
   Vector<Point> repairLocations;

   PanelGeom *panelGeom = getPanelGeom();

   for(S32 i = 0; i < CORE_PANELS; i++)
      if(isPanelDamaged(i))
         if(isPanelInRepairRange(repairOrigin, i))
            repairLocations.push_back(panelGeom->repair[i]);

   return repairLocations;
}


void CoreItem::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   // Make cores always visible
   if(!isGhost())
      setScopeAlways();

   GameType *gameType = theGame->getGameType();

   if(!gameType)                 // Sam has observed this under extreme network packet loss
      return;

   // Alert the gameType
   if(gameType->getGameTypeId() == CoreGame)
      static_cast<CoreGameType *>(gameType)->addCore(this, getTeam());
}


// Compatible with readFloat at the same number of bits
static void writeFloatZeroOrNonZero(BitStream &s, F32 &val, U8 bitCount)
{
   TNLAssert(val >= 0 && val <= 1, "writeFloat Must be between 0.0 and 1.0");
   if(val == 0)
      s.writeInt(0, bitCount);  // always writes zero
   else
   {
      U32 transmissionValue = U32(val * ((1 << bitCount) - 1));  // rounds down

      // If we're not truly at zero, don't send '0', send '1'
      if(transmissionValue == 0)
         s.writeInt(1, bitCount);
      else
         s.writeInt(transmissionValue, bitCount);
   }
}


U32 CoreItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);

   if(stream->writeFlag(updateMask & (InitialMask | TeamMask)))
   {
      writeThisTeam(stream);
      stream->writeInt(mRotationSpeed, 4);
   }

   stream->writeFlag(mHasExploded);

   if(!mHasExploded)
   {
      // Don't bother with health report if we've exploded
      for(S32 i = 0; i < CORE_PANELS; i++)
      {
         if(stream->writeFlag(updateMask & (PanelDamagedMask << i))) // go through each bit mask
         {
            // Normalize between 0.0 and 1.0 for transmission
            F32 panelHealthRatio = mPanelHealth[i] / mStartingPanelHealth;

            // writeFloatZeroOrNonZero will compensate for low resolution by sending zero only if it is actually zero
            // 4 bits -> 1/16 increments, all we really need - this means that client-side
            // will NOT have the true health, rather a ratio of precision 4 bits
            writeFloatZeroOrNonZero(*stream, panelHealthRatio, 4);
         }
      }
   }

   stream->writeFlag(mAttackedWarningTimer.getCurrent());

   return retMask;
}


#ifndef ZAP_DEDICATED

void CoreItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
   {
      readThisTeam(stream);
      mRotationSpeed = stream->readInt(4);
   }

   if(stream->readFlag())     // Exploding!  Take cover!!
   {
      for(S32 i = 0; i < CORE_PANELS; i++)
         mPanelHealth[i] = 0;

      if(!mHasExploded)    // Just exploded!
      {
         mHasExploded = true;
         disableCollision();
         onItemExploded(getPos());
      }
   }
   else                             // Haven't exploded, getting health
   {
      for(S32 i = 0; i < CORE_PANELS; i++)
      {
         if(stream->readFlag())                    // Panel damaged
         {
            // De-normalize to real health
            bool hadHealth = mPanelHealth[i] > 0;
            mPanelHealth[i] = mStartingPanelHealth * stream->readFloat(4);

            // Check if panel just died
            if(hadHealth && mPanelHealth[i] == 0)  
               doPanelDebris(i);
         }
      }
   }

   mBeingAttacked = stream->readFlag();
}

#endif


bool CoreItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 4)         // CoreItem <team> <health> <x> <y> ...
      return false;

   setTeam(atoi(argv[0]));
   setStartingHealth((F32)atof(argv[1]));

   if(!Parent::processArguments(argc-2, argv+2, game))
      return false;

   // 019h added rotation speed
   if(argc >=5)
      setRotationSpeed((U32)atoi(argv[4]));

   return true;
}


string CoreItem::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + itos(getTeam()) + " " +
         ftos(mStartingHealth * DamageReductionRatio) + " " + geomToLevelCode() + " " +
         itos(mRotationSpeed);
}


bool CoreItem::isBeingAttacked()
{
   return mBeingAttacked;
}


bool CoreItem::collide(BfObject *otherObject)
{
   return true;
}


#ifndef ZAP_DEDICATED
// Client only
void CoreItem::onItemExploded(Point pos)
{
   mCurrentExplosionNumber = 0;
   mExplosionTimer.reset(ExplosionInterval);

   // Start with an explosion at the center.  See idle() for other called explosions
   doExplosion(pos);
}

void CoreItem::onGeomChanged()
{
   Parent::onGeomChanged();

   GameType *gameType = getGame()->getGameType();
   fillPanelGeom(getPos(), gameType->getTotalGamePlayedInMs() * mRotationSpeed, mPanelGeom);
}
#endif


bool CoreItem::canBeHostile() { return true; }
bool CoreItem::canBeNeutral() { return true; }


S32 CoreItem::lua_setTeam(lua_State *L)
{
   S32 oldTeamIndex = this->getTeam();
   S32 results = Parent::lua_setTeam(L);
   S32 newTeamIndex = this->getTeam();

   if(getGame() && getGame()->getGameType() && getGame()->getGameType()->getGameTypeId() == CoreGame)
   {
      if(oldTeamIndex >= 0 && oldTeamIndex < getGame()->getTeamCount())
      {
         Team* oldTeam = dynamic_cast<Team *>(getGame()->getTeam(oldTeamIndex));
         oldTeam->addScore(-1);
         getGame()->getGameType()->s2cSetTeamScore(oldTeamIndex, oldTeam->getScore());
      }
   
      if(newTeamIndex >= 0)
      {
         Team* newTeam = dynamic_cast<Team *>(getGame()->getTeam(newTeamIndex));
         newTeam->addScore(1);
         getGame()->getGameType()->s2cSetTeamScore(newTeamIndex, newTeam->getScore());
      }
   }

   return results;
}


/////
// Lua interface
/**
 * @luaclass CoreItem
 * 
 * @brief Objective items in Core games
 */
//               Fn name    Param profiles         Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getCurrentHealth, ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, getFullHealth,    ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setFullHealth,    ARRAYDEF({{ NUM_GE0, END }}), 1 ) \
   METHOD(CLASS, getRotationSpeed, ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setRotationSpeed, ARRAYDEF({{ INT_GE0, END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(CoreItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(CoreItem, LUA_METHODS);

#undef LUA_METHODS


const char *CoreItem::luaClassName = "CoreItem";
REGISTER_LUA_SUBCLASS(CoreItem, Item);


/**
 * @luafunc num CoreItem::getCurrentHealth()
 * 
 * @brief Returns the item's current health. This will be between 0 and the
 * result of getFullHealth().
 * 
 * @return The current health .
 */
S32 CoreItem::lua_getCurrentHealth(lua_State *L) 
{ 
   return returnFloat(L, getTotalCurrentHealth() * DamageReductionRatio);
}


/**
 * @luafunc num CoreItem::getFullHealth()
 * 
 * @brief Returns the item's full health.
 * 
 * @return The total full health.
 */
S32 CoreItem::lua_getFullHealth(lua_State *L) 
{ 
   return returnFloat(L, mStartingHealth * DamageReductionRatio);
}


/**
 * @luafunc CoreItem::setFullHealth(num health)
 * 
 * @brief Sets the item's full health. Has no effect on current health.
 *
 * @descr The maximum health of each panel is the full health of the core divided
 * by the number of panels.
 * 
 * @param health The item's new full health 
 */
S32 CoreItem::lua_setFullHealth(lua_State *L) 
{ 
   checkArgList(L, functionArgs, "CoreItem", "setFullHealth");
   setStartingHealth(getFloat(L, 1));

   return 0;     
}
/**
 * @luafunc CoreItem::getRotationSpeed()
 *
 * @brief Get the Core's rotation speed factor
 *
 * @return The rotation speed.
 */
S32 CoreItem::lua_getRotationSpeed(lua_State *L)
{
   return returnInt(L, mRotationSpeed);
}


/**
 * @luafunc CoreItem::setRotationSpeed(int speed)
 *
 * @brief Sets the Core's rotation speed factor.
 *
 * @descr The rotation speed is a multiple of the default (1x). The default time
 * it takes for one rotation is 16384 ms. Therefore, the rotation time is:
 *
 *    16384 ms / speed
 *
 * Speeds of 0 (stopped) to 15 are allowed.
 *
 * @param speed
 */
S32 CoreItem::lua_setRotationSpeed(lua_State *L)
{
   checkArgList(L, functionArgs, "CoreItem", "setRotationSpeed");

   U32 speed = getInt2<U32>(L, 1);
   mRotationSpeed = min(speed, CoreMaxRotationSpeed);

   // Update clients over network
   // Use TeamMask because it's already hooked up to send rotation speed
   setMaskBits(TeamMask);

   return 0;
}


}; /* namespace Zap */
