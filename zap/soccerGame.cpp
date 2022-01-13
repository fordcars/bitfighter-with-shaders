//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "soccerGame.h"

#include "gameNetInterface.h"
#include "projectile.h"
#include "goalZone.h"
#include "Spawn.h"      // For AbstractSpawn def

#include "Colors.h"
#include "stringUtils.h"

#include "gameObjectRender.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#  include "UIEditorMenus.h"
#endif

namespace Zap
{

using namespace LuaArgs;

// Constructor
SoccerGameType::SoccerGameType()
{
   mPossibleHatTrickPlayer = NULL;
   mHatTrickCounter = 0;
}

// Destructor
SoccerGameType::~SoccerGameType()
{
   // Do nothing
}


TNL_IMPLEMENT_NETOBJECT(SoccerGameType);

TNL_IMPLEMENT_NETOBJECT_RPC(SoccerGameType, s2cSoccerScoreMessage,
   (U32 msgIndex, StringTableEntry scorerName, RangedU32<0, GameType::gMaxTeamCount> rawTeamIndex, Point scorePos), 
   (msgIndex, scorerName, rawTeamIndex, scorePos),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   // Before calling this RPC, we subtracted FirstTeamNumber, so we need to add it back here...
   S32 teamIndex = (S32)rawTeamIndex + GameType::FirstTeamNumber;      
   string msg;
   S32 scorerTeam = TEAM_NEUTRAL;
   string txtEffect = "Goal!";      // Will work for most cases, may be changed below
   static const string NegativePoints = "Negative Points!";
   getGame()->playSoundEffect(SFXFlagCapture);

   // Compose the message

   if(scorerName.isNull())    // Unknown player scored
   {
      if(teamIndex >= 0)
         msg = "A goal was scored on team " + string(getGame()->getTeamName(teamIndex).getString());
      else if(teamIndex == -1)
         msg = "A goal was scored on a neutral goal!";
      else if(teamIndex == -2)
         msg = "A goal was scored on a hostile goal!";
      else
         msg = "A goal was scored on an unknown goal!";
   }
   else                       // Known scorer
   {
      if(msgIndex == SoccerMsgScoreGoal)
      {
         if(isTeamGame())
         {
            if(teamIndex >= 0)
               msg = string(scorerName.getString()) + " scored a goal on team " + string(getGame()->getTeamName(teamIndex).getString());
            else if(teamIndex == -1)
               msg = string(scorerName.getString()) + " scored a goal on a neutral goal!";
            else if(teamIndex == -2)
            {
               msg = string(scorerName.getString()) + " scored a goal on a hostile goal (for negative points!)";
               txtEffect = NegativePoints;
            }
            else
               msg = string(scorerName.getString()) + " scored a goal on an unknown goal!";
         }
         else  // every man for himself
         {
            if(teamIndex >= -1)      // including neutral goals
               msg = string(scorerName.getString()) + " scored a goal!";
            else if(teamIndex == -2)
            {
               msg = string(scorerName.getString()) + " scored a goal on a hostile goal (for negative points!)";
               txtEffect = NegativePoints;
            }
         }
      }
      else if(msgIndex == SoccerMsgScoreOwnGoal)
      {
         msg = string(scorerName.getString()) + " scored an own-goal, giving the other team" + 
                     (getGame()->getTeamCount() == 2 ? "" : "s") + " a point!";
         txtEffect = "Own Goal!";
      }

      ClientInfo *scorer = getGame()->findClientInfo(scorerName);
      if(scorer)
         scorerTeam = scorer->getTeamIndex();
   }

   // Print the message and emit the text effect
   getGame()->displayMessage(Color(0.6f, 1.0f, 0.8f), msg.c_str());
   getGame()->emitTextEffect(txtEffect, *getTeamColor(scorerTeam), scorePos);
}


void SoccerGameType::setBall(SoccerBallItem *theBall)
{
   mBall = theBall;
}


// Helper function to make sure the two-arg version of updateScore doesn't get a null ship
void SoccerGameType::updateSoccerScore(Ship *ship, S32 scoringTeam, ScoringEvent scoringEvent, S32 score)
{
   if(ship)
      updateScore(ship, scoringEvent, score);
   else
      updateScore(NULL, scoringTeam, scoringEvent, score);
}


void SoccerGameType::scoreGoal(Ship *ship, const StringTableEntry &scorerName, S32 scoringTeam, const Point &scorePos, S32 goalTeamIndex, S32 score)
{
   // How can this ever be triggered?
   if(scoringTeam == NO_TEAM)
   {
      s2cSoccerScoreMessage(SoccerMsgScoreGoal, scorerName, (U32) (goalTeamIndex - FirstTeamNumber), scorePos);
      return;
   }


   bool isOwnGoal = scoringTeam == TEAM_NEUTRAL || scoringTeam == goalTeamIndex;
   if(isTeamGame() && isOwnGoal)    // Own-goal
   {
      updateSoccerScore(ship, scoringTeam, ScoreGoalOwnTeam, score);

      // Subtract FirstTeamNumber to fit goalTeamIndex into a neat RangedU32 container
      s2cSoccerScoreMessage(SoccerMsgScoreOwnGoal, scorerName, (U32) (goalTeamIndex - FirstTeamNumber), scorePos);
   }
   else     // Goal on someone else's goal
   {
      if(goalTeamIndex == TEAM_HOSTILE)
         updateSoccerScore(ship, scoringTeam, ScoreGoalHostileTeam, score);
      else
         updateSoccerScore(ship, scoringTeam, ScoreGoalEnemyTeam, score);

      s2cSoccerScoreMessage(SoccerMsgScoreGoal, scorerName, (U32) (goalTeamIndex - FirstTeamNumber), scorePos);      // See comment above
   }

   // Check for Hat trick badge, hostile goals excluded
   ClientInfo *clientInfo = ship ? ship->getClientInfo() : NULL;
   if(clientInfo != NULL && goalTeamIndex != TEAM_HOSTILE)
   {
      // If our current scorer was the last scorer and is wasn't an own-goal
      if(clientInfo == mPossibleHatTrickPlayer && !isOwnGoal)
      {
         mHatTrickCounter++;

         // Now test if we got the badge!
         if(mHatTrickCounter == 3 &&                              // Must have scored 3 in a row !
            mPossibleHatTrickPlayer->isAuthenticated() &&         // Player must be authenticated
            getGame()->getPlayerCount() >= 4 &&                   // Game must have 4+ human players
            getGame()->getAuthenticatedPlayerCount() >= 2 &&      // Two of whom must be authenticated
            !mPossibleHatTrickPlayer->hasBadge(BADGE_HAT_TRICK))  // Player doesn't already have the badge
         {
            achievementAchieved(BADGE_HAT_TRICK, mPossibleHatTrickPlayer->getName());
         }
      }

      // Else keep track of the new scorer and reset the counter
      else
      {
         mPossibleHatTrickPlayer = clientInfo;

         if(isOwnGoal)
            mHatTrickCounter = 0;
         else
            mHatTrickCounter = 1;
      }
   }
}


// Runs on client
void SoccerGameType::renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const
{
#ifndef ZAP_DEDICATED

   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship) {
      Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);
      return;
   }

   S32 team = ship->getTeam();

   const Vector<DatabaseObject *> *zones = getGame()->getGameObjDatabase()->findObjects_fast(GoalZoneTypeNumber);

   for(S32 i = 0; i < zones->size(); i++)
   {
      GoalZone *zone = static_cast<GoalZone *>(zones->get(i));

      if(zone->getTeam() != team)
         renderObjectiveArrow(zone, canvasWidth, canvasHeight);
   }

   if(mBall.isValid())
      renderObjectiveArrow(mBall, canvasWidth, canvasHeight);

   Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);
#endif
}


GameTypeId SoccerGameType::getGameTypeId() const { return SoccerGame; }
const char *SoccerGameType::getShortName() const { return "S"; }

static const char *instructions[] = { "Push the ball into the",  "opposing team's goal!" };
const char **SoccerGameType::getInstructionString() const { return instructions; } 

HelpItem SoccerGameType::getGameStartInlineHelpItem() const { return SGameStartItem; }

bool SoccerGameType::canBeTeamGame()       const { return true;  }
bool SoccerGameType::canBeIndividualGame() const { return true;  }


// What does a particular scoring event score?
S32 SoccerGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
{
   if(scoreGroup == TeamScore)
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 0;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return 0;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 0;
         case KillOwnTurret:
            return 0;
         case ScoreGoalEnemyTeam:
            return data;
         case ScoreGoalOwnTeam:
            return -data;
         case ScoreGoalHostileTeam:
            return -data;
         default:
            return naScore;
      }
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
         case ScoreGoalEnemyTeam:
            return 5 * data;
         case ScoreGoalOwnTeam:
            return -5 * data;
         case ScoreGoalHostileTeam:
            return -5 * data;
         default:
            return naScore;
      }
   }
}

////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(SoccerBallItem);

// Statics
const F32 SoccerBallItem::Radius = 30;
const F32 SoccerBallItem::Mass = 4;

#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *SoccerBallItem::mAttributeMenuUI = NULL;
#endif

/**
 * @luafunc SoccerBallItem::SoccerBallItem()
 * @luafunc SoccerBallItem::SoccerBallItem(point pos)
 */
// Combined Lua / C++ default constructor
SoccerBallItem::SoccerBallItem(lua_State *L) : Parent(Point(0,0), true, SoccerBallItem::Radius, SoccerBallItem::Mass)
{
   mObjectTypeNumber = SoccerBallItemTypeNumber;
   mNetFlags.set(Ghostable);
   mLastPlayerTouch = NULL;
   mLastPlayerTouchTeam = NO_TEAM;
   mLastPlayerTouchName = StringTableEntry(NULL);

   mSendHomeTimer.setPeriod(1500);     // Ball will linger in goal for 1500 ms before being sent home

   mDragFactor = 0.0;      // No drag
   mLuaBall = false;
   mSpawnLock = false;

   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }}, 2 };

      S32 profile = checkArgList(L, constructorArgList, "SoccerBallItem", "constructor");

      if(profile == 1)
         setPos(L, 1);

      mLuaBall = true;
   }

   mInitialPos = getPos();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


SoccerBallItem::~SoccerBallItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


SoccerBallItem *SoccerBallItem::clone() const
{
   return new SoccerBallItem(*this);
}


bool SoccerBallItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(!Parent::processArguments(argc, argv, game))
      return false;

   // Boolean to tie the ball to its FlagSpawn, should be 1 or 0
   if(argc >= 3)
   {
      if(strcmp(argv[2], "SpawnLock") == 0)
         mSpawnLock = true;
      else
         mSpawnLock = false;
   }

   mInitialPos = getActualPos();

   // Add a spawn point at the ball's starting location
   FlagSpawn *spawn = new FlagSpawn(mInitialPos, 0);
   spawn->addToGame(game, game->getGameObjDatabase());

   return true;
}


string SoccerBallItem::toLevelCode() const
{
   string line = Parent::toLevelCode();
   if(mSpawnLock)
      line += " SpawnLock";

   return line;
}



#ifndef ZAP_DEDICATED
EditorAttributeMenuUI *SoccerBallItem::getAttributeMenu()
{
   // Lazily initialize
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = static_cast<ClientGame *>(getGame());

      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      mAttributeMenuUI->addMenuItem(new YesNoMenuItem("SpawnLock:", true, "Lock to initial spawn point"));

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void SoccerBallItem::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mSpawnLock ? 1 : 0);
}


// Retrieve the values we need from the menu
void SoccerBallItem::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mSpawnLock = attributeMenu->getMenuItem(0)->getIntValue();  // Returns 0 or 1
}


// Render some attributes when item is selected but not being edited
void SoccerBallItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("SpawnLock");    values.push_back(mSpawnLock ? "Yes" : "No");
}
#endif

void SoccerBallItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   // Make soccer ball always visible
   if(!isGhost())
      setScopeAlways();

   // Make soccer ball only visible when in scope
   //if(!isGhost())
   //   theGame->getGameType()->addItemOfInterest(this);

   //((SoccerGameType *) theGame->getGameType())->setBall(this);
   GameType *gt = game->getGameType();
   if(gt)
   {
      if(gt->getGameTypeId() == SoccerGame)
         static_cast<SoccerGameType *>(gt)->setBall(this);
   }

   // If this ball was added by Lua, make sure there is a spawn point at its
   // starting position
   if(mLuaBall)
   {
      FlagSpawn *spawn = new FlagSpawn(mInitialPos, 0);
      spawn->addToGame(mGame, mGame->getGameObjDatabase());
   }
}


void SoccerBallItem::renderItem(const Point &pos)
{
   renderSoccerBall(pos);
}


const char *SoccerBallItem::getOnScreenName()     { return "Soccer Ball";  }
const char *SoccerBallItem::getOnDockName()       { return "Ball";         }
const char *SoccerBallItem::getPrettyNamePlural() { return "Soccer Balls"; }
const char *SoccerBallItem::getEditorHelpString() { return "Soccer ball, can only be used in Soccer games."; }


bool SoccerBallItem::hasTeam()      { return false; }
bool SoccerBallItem::canBeHostile() { return false; }
bool SoccerBallItem::canBeNeutral() { return false; }


const Color *SoccerBallItem::getColor() const
{ 
   return getGame()->getTeamColor(TEAM_NEUTRAL);
}


void SoccerBallItem::renderDock()
{
   renderSoccerBall(getRenderPos(), 7);
}


void SoccerBallItem::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
   renderItem(getRenderPos());
}


void SoccerBallItem::idle(BfObject::IdleCallPath path)
{
   if(mSendHomeTimer.update(mCurrentMove.time))
   {
      if(!isGhost())
         sendHome();
   }
   else if(mSendHomeTimer.getCurrent())      // Goal has been scored, waiting for ball to reset
   {
      F32 accelFraction = 1 - (0.95f * mCurrentMove.time * 0.001f);

      setActualVel(getActualVel() * accelFraction);
   }
   
   else if(getActualVel().lenSquared() > 0)  // Add some friction to the soccer ball
   {
      F32 accelFraction = 1 - (mDragFactor * mCurrentMove.time * 0.001f);
   
      setActualVel(getActualVel() * accelFraction);
   }

   Parent::idle(path);
}


void SoccerBallItem::damageObject(DamageInfo *theInfo)
{
   computeImpulseDirection(theInfo);

   if(theInfo->damagingObject)
   {
      U8 typeNumber = theInfo->damagingObject->getObjectTypeNumber();

      if(isShipType(typeNumber))
      {
         mLastPlayerTouch = static_cast<Ship *>(theInfo->damagingObject);
         mLastPlayerTouchTeam = mLastPlayerTouch->getTeam();
         if(mLastPlayerTouch->getClientInfo())
            mLastPlayerTouchName = mLastPlayerTouch->getClientInfo()->getName();
         else
            mLastPlayerTouchName = NULL;
      }

      else if(isProjectileType(typeNumber))
      {
         BfObject *shooter = WeaponInfo::getWeaponShooterFromObject(theInfo->damagingObject);

         if(shooter && isShipType(shooter->getObjectTypeNumber()))
         {
            Ship *ship = static_cast<Ship *>(shooter);
            mLastPlayerTouch = ship;             // If shooter was a turret, say, we'd expect s to be NULL.
            mLastPlayerTouchTeam = theInfo->damagingObject->getTeam(); // Projectile always have a team from what fired it, can be used to credit a team.
            if(ship->getClientInfo())
               mLastPlayerTouchName = ship->getClientInfo()->getName();
            else
               mLastPlayerTouchName = NULL;
         }
      }
      else
         resetPlayerTouch();
   }
}


void SoccerBallItem::resetPlayerTouch()
{
   mLastPlayerTouch = NULL;
   mLastPlayerTouchTeam = NO_TEAM;
   mLastPlayerTouchName = StringTableEntry(NULL);
}


void SoccerBallItem::sendHome()
{
   TNLAssert(!isGhost(), "Should only run on server!");

   // In soccer game, we use flagSpawn points to designate where the soccer ball should spawn.
   // If this ball is not locked to it's initial spawn point we'll simply redefine "initial pos"
   // as a random selection of the flag spawn points
   if(!mSpawnLock)
   {
      Vector<AbstractSpawn *> spawnPoints = getGame()->getGameType()->getSpawnPoints(FlagSpawnTypeNumber);

      S32 spawnIndex = TNL::Random::readI() % spawnPoints.size();
      mInitialPos = spawnPoints[spawnIndex]->getPos();
   }

   setPosVelAng(mInitialPos, Point(0,0), 0);

   setMaskBits(WarpPositionMask | PositionMask);      // By warping, we eliminate the "drifting" effect we got when we used PositionMask

   updateExtentInDatabase();

   resetPlayerTouch();
}


bool SoccerBallItem::collide(BfObject *hitObject)
{
   if(mSendHomeTimer.getCurrent())     // If we've already scored, and we're waiting for the ball to reset, there's nothing to do
      return true;

   if(isShipType(hitObject->getObjectTypeNumber()))
   {
      if(!isGhost())    //Server side
      {
         Ship *ship = static_cast<Ship *>(hitObject);
         mLastPlayerTouch = ship;
         mLastPlayerTouchTeam = mLastPlayerTouch->getTeam();                  // Used to credit team if ship quits game before goal is scored
         if(mLastPlayerTouch->getClientInfo())
            mLastPlayerTouchName = mLastPlayerTouch->getClientInfo()->getName(); // Used for making nicer looking messages in same situation
         else
            mLastPlayerTouchName = NULL;
      }
   }
   else if(hitObject->getObjectTypeNumber() == GoalZoneTypeNumber)      // SCORE!!!!
   {
      GoalZone *goal = static_cast<GoalZone *>(hitObject);

      if(!isGhost())
      {
         GameType *gameType = getGame()->getGameType();
         if(gameType && gameType->getGameTypeId() == SoccerGame)
            static_cast<SoccerGameType *>(gameType)->scoreGoal(mLastPlayerTouch, mLastPlayerTouchName, mLastPlayerTouchTeam, getActualPos(), goal->getTeam(), goal->getScore());

         mSendHomeTimer.reset();
      }
      return false;
   }
   return true;
}


U32 SoccerBallItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   return Parent::packUpdate(connection, updateMask, stream);
}


void SoccerBallItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);
}


/////
// Lua interface

/**
 * @luaclass SoccerBallItem
 * 
 * @brief Target object used in Soccer games
 */
// No soccerball specific methods!
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(SoccerBallItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(SoccerBallItem, LUA_METHODS);

#undef LUA_METHODS



const char *SoccerBallItem::luaClassName = "SoccerBallItem";
REGISTER_LUA_SUBCLASS(SoccerBallItem, MoveObject);


};

