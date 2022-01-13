//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "NexusGame.h"

#include "stringUtils.h"      // For ftos et al

#include "Colors.h"

#ifndef ZAP_DEDICATED
#  include "gameObjectRender.h"
#  include "ClientGame.h"
#  include "UIMenuItems.h"
#  include "Renderer.h"
#  include "RenderUtils.h"
#endif


#include <cmath>

namespace Zap
{

using namespace LuaArgs;

TNL_IMPLEMENT_NETOBJECT(NexusGameType);


TNL_IMPLEMENT_NETOBJECT_RPC(NexusGameType, s2cSetNexusTimer, (S32 nextChangeTime, bool isOpen), (nextChangeTime, isOpen), 
                            NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   mNexusChangeAtTime = nextChangeTime;
   mNexusIsOpen = isOpen;
}


TNL_IMPLEMENT_NETOBJECT_RPC(NexusGameType, s2cSendNexusTimes, (S32 nexusClosedTime, S32 nexusOpenTime), (nexusClosedTime, nexusOpenTime),                                            NetClassGroupGameMask, RPCGuaranteed, RPCToGhost, 0)
{
   mNexusClosedTime = nexusClosedTime;
   mNexusOpenTime = nexusOpenTime;
}


GAMETYPE_RPC_S2C(NexusGameType, s2cAddYardSaleWaypoint, (F32 x, F32 y), (x, y))
{
   YardSaleWaypoint w;
   w.timeLeft.reset(YardSaleWaypointTime);
   w.pos.set(x,y);
   mYardSaleWaypoints.push_back(w);
}


TNL_IMPLEMENT_NETOBJECT_RPC(NexusGameType, s2cNexusMessage,
   (U32 msgIndex, StringTableEntry clientName, U32 flagCount, U32 score), (msgIndex, clientName, flagCount, score),
   NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   if(msgIndex == NexusMsgScore)
   {
      getGame()->displayMessage(Color(0.6f, 1.0f, 0.8f), "%s returned %d flag%s to the Nexus for %d points!", 
                                 clientName.getString(), flagCount, flagCount > 1 ? "s" : "", score);
      getGame()->playSoundEffect(SFXFlagCapture);

      Ship *ship = getGame()->findShip(clientName);
      if(ship && score >= 100)
         getGame()->emitTextEffect(itos(score) + " POINTS!", Colors::red80, ship->getRenderPos());
   }
   else if(msgIndex == NexusMsgYardSale)
   {
      getGame()->displayMessage(Color(0.6f, 1.0f, 0.8f), "%s is having a YARD SALE!", clientName.getString());
      getGame()->playSoundEffect(SFXFlagSnatch);

      Ship *ship = getGame()->findShip(clientName);
      if(ship)
         getGame()->emitTextEffect("YARD SALE!", Colors::red80, ship->getRenderPos());
   }
   else if(msgIndex == NexusMsgGameOverWin)
   {
      getGame()->displayMessage(Color(0.6f, 1.0f, 0.8f), "Player %s wins the game!", clientName.getString());
      getGame()->playSoundEffect(SFXFlagCapture);
   }
   else if(msgIndex == NexusMsgGameOverTie)
   {
      getGame()->displayMessage(Color(0.6f, 1.0f, 0.8f), "The game ended in a tie.");
      getGame()->playSoundEffect(SFXFlagDrop);
   }
}


// Constructor
NexusGameType::NexusGameType() : GameType(100)
{
   mNexusClosedTime = 60 * 1000;
   mNexusOpenTime = 15 * 1000;
   mNexusIsOpen = false;
   mNexusChangeAtTime = 0;
}

// Destructor
NexusGameType::~NexusGameType()
{
   // Do nothing
}


bool NexusGameType::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc > 0)
   {
      setGameTime(F32(atof(argv[0]) * 60.f * 1000));          // Game time, stored in minutes in level file

      if(argc > 1)
      {
         mNexusClosedTime = S32(atof(argv[1]) * 60.f * 1000.f + 0.5); // Time until nexus opens, specified in minutes (0.5 converts truncation into rounding)

         if(argc > 2)
         {
            mNexusOpenTime = S32(atof(argv[2]) * 1000.f);     // Time nexus remains open, specified in seconds

            if(argc > 3)
               setWinningScore(atoi(argv[3]));                // Winning score
         }
      }
   }

   mNexusChangeAtTime = mNexusClosedTime;
   return true;
}


string NexusGameType::toLevelCode() const
{
   return string(getClassName()) + " " + getRemainingGameTimeInMinutesString() + " " + ftos(F32(mNexusClosedTime) / (60.f * 1000.f)) + " " + 
                                         ftos(F32(mNexusOpenTime) / 1000.f, 3)  + " " + itos(getWinningScore());
}


// Returns time left in current Nexus cycle -- if we're open, this will be the time until Nexus closes; if we're closed,
// it will return the time until Nexus opens
// Client only
S32 NexusGameType::getNexusTimeLeftMs() const
{
   return mNexusChangeAtTime == 0 ? 0 : (mNexusChangeAtTime - getTotalGamePlayedInMs());
}


bool NexusGameType::nexusShouldChange()
{
   if(mNexusChangeAtTime == 0)     
      return false;

   return getNexusTimeLeftMs() <= 0;
}


bool NexusGameType::isSpawnWithLoadoutGame()
{
   return true;
}


void NexusGameType::addNexus(NexusZone *nexus)
{
   mNexus.push_back(nexus);
}

// Count flags on a ship.  This function assumes that all carried flags are NexusFlags, each of which can represent multiple flags
// (see getFlagCount()).  This code will support a ship having several flags, but in practice, each ship will have exactly one.
static S32 getMountedFlagCount(Ship *ship)
{
   S32 flagCount = 0;
   S32 itemCount = ship->getMountedItemCount();

   for(S32 i = 0; i < itemCount; i++)
   {
      MountableItem *mountedItem = ship->getMountedItem(i);

      if(mountedItem->getObjectTypeNumber() == FlagTypeNumber)      // All flags are NexusFlags here!
      {
         NexusFlagItem *flag = static_cast<NexusFlagItem *>(mountedItem);
         flagCount += flag->getFlagCount();
      }
   }

   return flagCount;
}


// Currently only used when determining if there is something to drop
bool NexusGameType::isCarryingItems(Ship *ship)
{
   S32 itemCount = ship->getMountedItemCount();

   for(S32 i = 0; i < itemCount; i++)
   {
      MountableItem *mountedItem = ship->getMountedItem(i);
      if(!mountedItem)        // Could be null when a player drop their flags and gets destroyed at the same time
         continue;

      if(mountedItem->getObjectTypeNumber() == FlagTypeNumber)      
      {
         FlagItem *flag = static_cast<FlagItem *>(mountedItem);
         if(flag->getFlagCount() > 0)
            return true;
      }
      else     // Must be carrying something other than a flag.  Maybe we could drop that!
         return true;
   }

   return false;
}


// Cycle through mounted items and find the first one that's a FlagItem.
// In practice, this will always be a NexusFlagItem... I think...
// Returns NULL if it can't find a flag.
static FlagItem *findFirstFlag(Ship *ship)
{
   return static_cast<FlagItem *>(ship->getMountedItem(ship->getFlagIndex()));
}


// The flag will come from ship->mount.  *item is used as it is posssible to carry and drop multiple items.
// This method doesn't actually do any dropping; it only sends out an appropriate flag-drop message.
void NexusGameType::itemDropped(Ship *ship, MoveItem *item, DismountMode dismountMode)
{
   Parent::itemDropped(ship, item, dismountMode);

   if(item->getObjectTypeNumber() == FlagTypeNumber)
   {
      if(dismountMode != DISMOUNT_SILENT)
      {
         FlagItem *flag = static_cast<FlagItem *>(item);

         U32 flagCount = flag->getFlagCount();

         if(flagCount == 0)  // Needed if you drop your flags, then pick up a different item type (like resource item), and drop it
            return;

         if(!ship->getClientInfo())
            return;

         Vector<StringTableEntry> e;
         e.push_back(ship->getClientInfo()->getName());

         static StringTableEntry dropOneString(  "%e0 dropped a flag!");
         static StringTableEntry dropManyString( "%e0 dropped %e1 flags!");

         StringTableEntry *ste;

         if(flagCount == 1)
            ste = &dropOneString;
         else
         {
            ste = &dropManyString;
            e.push_back(itos(flagCount).c_str());
         }
      
         broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagDrop, *ste, e);
      }
   }
}


#ifndef ZAP_DEDICATED
// Any unique items defined here must be handled in both getMenuItem() and saveMenuItem() below!
Vector<string> NexusGameType::getGameParameterMenuKeys()
{
   Vector<string> items = Parent::getGameParameterMenuKeys();
   
   // Remove Win Score, replace it with some Nexus specific items
   for(S32 i = 0; i < items.size(); i++)
      if(items[i] == "Win Score")
      {
         items.erase(i);      // Delete "Win Score"

         // Create slots for 3 new items, and fill them with our Nexus specific items
         items.insert(i,     "Nexus Time to Open");
         items.insert(i + 1, "Nexus Time Remain Open");
         items.insert(i + 2, "Nexus Win Score");

         break;
      }

   return items;
}


// Definitions for those items
shared_ptr<MenuItem> NexusGameType::getMenuItem(const string &key)
{
   if(key == "Nexus Time to Open")
      return shared_ptr<MenuItem>(new TimeCounterMenuItem("Time for Nexus to Open:", (mNexusClosedTime + 500) / 1000, MaxMenuScore * 60,
                                                          "Never", "Time it takes for the Nexus to open"));

   else if(key == "Nexus Time Remain Open")
      return shared_ptr<MenuItem>(new TimeCounterMenuItemSeconds("Time Nexus Remains Open:", (mNexusOpenTime + 500) / 1000, MaxMenuScore * 60,
                                                                 "Always", "Time that the Nexus will remain open"));

   else if(key == "Nexus Win Score")
      return shared_ptr<MenuItem>(new CounterMenuItem("Score to Win:", getWinningScore(), 100, 100, S32_MAX, "points", 
                                                      "", "Game ends when one player or team gets this score"));
   else return Parent::getMenuItem(key);
}


bool NexusGameType::saveMenuItem(const MenuItem *menuItem, const string &key)
{
   if(key == "Nexus Time to Open")
      mNexusClosedTime = menuItem->getIntValue() * 1000;
   else if(key == "Nexus Time Remain Open")
      mNexusOpenTime = menuItem->getIntValue() * 1000;
   else if(key == "Nexus Win Score")
      setWinningScore(menuItem->getIntValue());
   else 
      return Parent::saveMenuItem(menuItem, key);

   return true;
}
#endif


TNL_IMPLEMENT_NETOBJECT(NexusZone);


TNL_IMPLEMENT_NETOBJECT_RPC(NexusZone, s2cFlagsReturned, (), (), NetClassGroupGameMask, RPCGuaranteedOrdered, RPCToGhost, 0)
{
   getGame()->getGameType()->mZoneGlowTimer.reset();
}


// The nexus is open.  A ship has entered it.  Now what?
// Runs on server only
void NexusGameType::shipTouchNexus(Ship *ship, NexusZone *theNexus)
{
   FlagItem *flag = findFirstFlag(ship);

   if(!flag)      // findFirstFlag can return NULL
      return;

   updateScore(ship, ReturnFlagsToNexus, flag->getFlagCount());

   S32 flagsReturned = flag->getFlagCount();
   ClientInfo *scorer = ship->getClientInfo();

   if(flagsReturned > 0 && scorer)
   {
      if(!isGameOver())  // Avoid flooding messages on game over.
         s2cNexusMessage(NexusMsgScore, scorer->getName().getString(), flag->getFlagCount(), 
                      getEventScore(TeamScore, ReturnFlagsToNexus, flag->getFlagCount()) );
      theNexus->s2cFlagsReturned();    // Alert the Nexus that someone has returned flags to it

      // See if this event qualifies for an achievement
      if(flagsReturned >= 25 &&                                   // Return 25+ flags
         scorer && scorer->isAuthenticated() &&                   // Player must be authenticated
         getGame()->getPlayerCount() >= 4 &&                      // Game must have 4+ human players
         getGame()->getAuthenticatedPlayerCount() >= 2 &&         // Two of whom must be authenticated
         !hasFlagSpawns() && !hasPredeployedFlags() &&            // Level can have no flag spawns, nor any predeployed flags
         !scorer->hasBadge(BADGE_TWENTY_FIVE_FLAGS))              // Player doesn't already have the badge
      {
         achievementAchieved(BADGE_TWENTY_FIVE_FLAGS, scorer->getName());
      }
   }

   flag->changeFlagCount(0);
}


// Runs on the server
void NexusGameType::onGhostAvailable(GhostConnection *theConnection)
{
   Parent::onGhostAvailable(theConnection);

   NetObject::setRPCDestConnection(theConnection);

   s2cSendNexusTimes(mNexusClosedTime, mNexusOpenTime);     // Send info about Nexus hours of business
   s2cSetNexusTimer(mNexusChangeAtTime, mNexusIsOpen);      // Send info about current state of Nexus
   
   NetObject::setRPCDestConnection(NULL);
}


// Emit a flag in a random direction at a random speed.
// Server only.
// If a flag is released from a ship, it will have underlying startVel, to which a random vector will be added
void NexusGameType::releaseFlag(const Point &pos, const Point &startVel, S32 count)
{
   static const S32 MAX_SPEED = 100;

   Game *game = getGame();

   F32 th = TNL::Random::readF() * FloatTau;
   F32 f = (TNL::Random::readF() * 2 - 1) * MAX_SPEED;

   Point vel(cos(th) * f, sin(th) * f);
   vel += startVel;

   NexusFlagItem *newFlag = new NexusFlagItem(pos, vel, count, true);
   newFlag->addToGame(game, game->getGameObjDatabase());
}


// Runs on client and server
void NexusGameType::idle(BfObject::IdleCallPath path, U32 deltaT)
{
   Parent::idle(path, deltaT);

   if(isGhost()) 
      idle_client(deltaT);
   else
      idle_server(deltaT);
}


static U32 getNextChangeTime(U32 changeTime, S32 duration)
{
   if(duration == 0)    // Handle special case of never opening/closing nexus
      return 0;

   return changeTime + duration;
}


void NexusGameType::idle_client(U32 deltaT)
{
#ifndef ZAP_DEDICATED
   if(!mNexusIsOpen && nexusShouldChange())         // Nexus has just opened
   {
      if(!isGameOver())
      {
         getGame()->displayMessage(Color(0.6f, 1, 0.8f), "The Nexus is now OPEN!");
         getGame()->playSoundEffect(SFXFlagSnatch);
      }

      mNexusIsOpen = true;
      mNexusChangeAtTime = getNextChangeTime(mNexusChangeAtTime, mNexusOpenTime);
   }

   else if(mNexusIsOpen && nexusShouldChange())       // Nexus has just closed
   {
      if(!isGameOver())
      {
         getGame()->displayMessage(Color(0.6f, 1, 0.8f), "The Nexus is now CLOSED!");
         getGame()->playSoundEffect(SFXFlagDrop);
      }

      mNexusIsOpen = false;
      mNexusChangeAtTime = getNextChangeTime(mNexusChangeAtTime, mNexusClosedTime);
   }

   for(S32 i = 0; i < mYardSaleWaypoints.size();)
   {
      if(mYardSaleWaypoints[i].timeLeft.update(deltaT))
         mYardSaleWaypoints.erase_fast(i);
      else
         i++;
   }
#endif
}


void NexusGameType::idle_server(U32 deltaT)
{
   if(nexusShouldChange())
   {
      if(mNexusIsOpen) 
         closeNexus(mNexusChangeAtTime);
      else
         openNexus(mNexusChangeAtTime);
   }
}


// Server only
void NexusGameType::openNexus(S32 timeNexusOpened)
{
   mNexusIsOpen = true;
   mNexusChangeAtTime = getNextChangeTime(timeNexusOpened, mNexusOpenTime);

   // Check if anyone is already in the Nexus, examining each client's ship in turn...
   for(S32 i = 0; i < getGame()->getClientCount(); i++)
   {
      Ship *client_ship = getGame()->getClientInfo(i)->getShip();

      if(!client_ship)
         continue;

      BfObject *zone = client_ship->isInZone(NexusTypeNumber);

      if(zone)
         shipTouchNexus(client_ship, static_cast<NexusZone *>(zone));
   }

   // Fire an event
   EventManager::get()->fireEvent(EventManager::NexusOpenedEvent);
}


// Server only
void NexusGameType::closeNexus(S32 timeNexusClosed)
{
   mNexusIsOpen = false;
   mNexusChangeAtTime = getNextChangeTime(timeNexusClosed, mNexusClosedTime);

   // Fire an event
   EventManager::get()->fireEvent(EventManager::NexusClosedEvent);
}


// Server only -- only called by scripts
void NexusGameType::setNexusState(bool open)
{
   if(open)
      openNexus(getRemainingGameTime());
   else
      closeNexus(getRemainingGameTime());

   s2cSetNexusTimer(mNexusChangeAtTime, open);      // Broacast new Nexus opening hours
}


// Server only -- only called by scripts
void NexusGameType::setNewOpenTime(S32 timeInSeconds)
{
   mNexusOpenTime = timeInSeconds;
   s2cSendNexusTimes(mNexusClosedTime, mNexusOpenTime);

   // Trigger update of new opening time if we are currently open
   if(mNexusIsOpen)
      setNexusState(true);
}


// Server only -- only called by scripts
void NexusGameType::setNewClosedTime(S32 timeInSeconds)
{
   mNexusClosedTime = timeInSeconds;
   s2cSendNexusTimes(mNexusClosedTime, mNexusOpenTime);

   // Trigger update of new closing time if we are currently closed
   if(!mNexusIsOpen)
      setNexusState(false);
}


// What does a particular scoring event score?
S32 NexusGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 flags)
{
   // 10 * n(n+1)/2
   // This means 1 flag == 10 points; 2 flags == 30; 3 flags == 60, etc.
   S32 score = (10 * flags * (flags + 1)) / 2;
   // Min number of flags from a single drop required to achieve a certain
   // score can be found by using the inverse of above:
   // n = upper( (-1 + sqrt(1 + 4*2*score/10)) / 2 )


   if(scoreGroup == TeamScore)
   {
      switch(scoreEvent)
      {
         case KillEnemy:         // Fall through OK
         case KilledByAsteroid:  //       .
         case KilledByTurret:    //       .
         case KillSelf:          //       .
         case KillTeammate:      //       .
         case KillEnemyTurret:   //       .
         case KillOwnTurret:     //       .
            return 0;
         case ReturnFlagsToNexus:
            return score;
         default:
            return naScore;
      }
   }
   else  // scoreGroup == IndividualScore
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
         case ReturnFlagsToNexus:
            return score;
         default:
            return naScore;
      }
   }
}


GameTypeId NexusGameType::getGameTypeId() const { return NexusGame; }
const char *NexusGameType::getShortName() const { return "Nex"; }

static const char *instructions[] = { "Collect flags and deliver",  "them to the Nexus!" };
const char **NexusGameType::getInstructionString() const { return instructions; }
HelpItem NexusGameType::getGameStartInlineHelpItem() const { return NexGameStartItem; }

bool NexusGameType::isFlagGame()          const { return true;  } // Well, technically not, but we'll pervert flags as we load the level
bool NexusGameType::canBeTeamGame()       const { return true;  }
bool NexusGameType::canBeIndividualGame() const { return true;  }


//////////  Client only code:


#ifndef ZAP_DEDICATED

// If render is false, will return height, but not actually draw
S32 NexusGameType::renderTimeLeftSpecial(S32 right, S32 bottom, bool render) const
{
   Renderer& r = Renderer::get();

   const S32 size = 20;
   const S32 gap = 4;
   const S32 x = right;
   const S32 y = bottom;

   if(render)
   {
      r.setColor(mNexusIsOpen ? Colors::NexusOpenColor : Colors::NexusClosedColor);      // Display timer in appropriate color

      if(mNexusIsOpen && mNexusOpenTime == 0)
         drawStringfr(x, y - size, size, "Nexus never closes");
      else if(!mNexusIsOpen && mNexusClosedTime == 0)
         drawStringfr(x, y - size, size, "Nexus never opens");
      else if(!mNexusIsOpen && !isTimeUnlimited() && getRemainingGameTimeInMs() <= getNexusTimeLeftMs())
         drawStringfr(x, y - size, size, "Nexus closed until end of game");
      else if(!isGameOver())
      {
         static const U32 w0      = getStringWidth(size, "0");
         static const U32 wCloses = getStringWidth(size, "Nexus closes: ");
         static const U32 wOpens  = getStringWidth(size, "Nexus opens: ");

         S32 timeLeft = getNexusTimeLeftMs();

         // Get the width of the minutes and 10 seconds digit(s), account for two leading 0s (00:45)
         const U32 minsRemaining = timeLeft / (60 * 1000);
         const U32 tenSecsRemaining = timeLeft / 1000 % 60 / 10;
         string timestr = itos(minsRemaining) + ":" + itos(tenSecsRemaining);
         const U32 minsWidth = getStringWidth(size, timestr.c_str()) + (minsRemaining < 10 ? w0 : 0);

         S32 w = minsWidth + w0 + (mNexusIsOpen ? wCloses : wOpens);

         drawTime(x - w, y - size, size, timeLeft, mNexusIsOpen ? "Nexus closes: " : "Nexus opens: ");
      }
   }

   return size + gap;
}


void NexusGameType::renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const
{
   for(S32 i = 0; i < mYardSaleWaypoints.size(); i++)
      renderObjectiveArrow(mYardSaleWaypoints[i].pos, &Colors::white, canvasWidth, canvasHeight);

   const Color *color = mNexusIsOpen ? &Colors::NexusOpenColor : &Colors::NexusClosedColor;

   for(S32 i = 0; i < mNexus.size(); i++)
      renderObjectiveArrow(mNexus[i], color, canvasWidth, canvasHeight);

   Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);
}
#endif


//////////  END Client only code

// Server only
void NexusGameType::controlObjectForClientKilled(ClientInfo *theClient, BfObject *clientObject, BfObject *killerObject)
{
   if(isGameOver())  // Avoid flooding messages when game is over
      return;

   Parent::controlObjectForClientKilled(theClient, clientObject, killerObject);

   if(!clientObject || !isShipType(clientObject->getObjectTypeNumber()))
      return;

   Ship *ship = static_cast<Ship *>(clientObject);

   // Check for yard sale  (i.e. tons of flags released at same time)
   S32 flagCount = getMountedFlagCount(ship);

   static const S32 YARD_SALE_THRESHOLD = 8;

   if(flagCount >= YARD_SALE_THRESHOLD)
   {
      Point pos = ship->getActualPos();

      // Notify the clients
      s2cAddYardSaleWaypoint(pos.x, pos.y);
      s2cNexusMessage(NexusMsgYardSale, ship->getClientInfo()->getName().getString(), 0, 0);
   }
}


void NexusGameType::shipTouchFlag(Ship *ship, FlagItem *touchedFlag)
{
   // Don't mount to ship, instead increase current mounted NexusFlag
   //    flagCount, and remove collided flag from game

   FlagItem *shipFlag = findFirstFlag(ship);

   TNLAssert(shipFlag, "Expected to find a flag on this ship!");

   if(!shipFlag)      // findFirstFlag can return NULL... but probably won't
      return;

   U32 shipFlagCount = shipFlag->getFlagCount();

   if(touchedFlag)
      shipFlagCount += touchedFlag->getFlagCount();
   else
      shipFlagCount++;

   shipFlag->changeFlagCount(shipFlagCount);


   // Now that the touchedFlag has been absorbed into the ship, remove it from the game.  Be sure to use deleteObject, as having the database
   // delete the object directly leads to memory corruption errors.
   // Not using RemoveFromDatabase, causes problems with idle loop when Database Vector size changes, idle loop just points to Database's Vector through findObjects_fast
   touchedFlag->setCollideable(false);
   touchedFlag->deleteObject();

   if(mNexusIsOpen)
   {
      // Check if ship is sitting on an open Nexus (can use static_cast because we already know the type, even though it could be NULL)
      NexusZone *nexus = static_cast<NexusZone *>(ship->isInZone(NexusTypeNumber));

      if(nexus)         
         shipTouchNexus(ship, nexus);
   }   
}


// Special spawn function for Nexus games (runs only on server)
bool NexusGameType::spawnShip(ClientInfo *clientInfo)
{
   if(!Parent::spawnShip(clientInfo))
      return false;

   Ship *ship = clientInfo->getShip();

   TNLAssert(ship, "Expected a ship here!");
   if(!ship)
      return false;

   NexusFlagItem *newFlag = new NexusFlagItem(ship->getActualPos());
   newFlag->addToGame(getGame(), getGame()->getGameObjDatabase());
   newFlag->mountToShip(ship);    // mountToShip() can handle NULL
   newFlag->changeFlagCount(0);

   return true;
}

////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(NexusFlagItem);

// C++ constructor
NexusFlagItem::NexusFlagItem(Point pos, Point vel, S32 count, bool useDropDelay) : Parent(pos, vel, useDropDelay)
{
   mFlagCount = count;
}


// Destructor
NexusFlagItem::~NexusFlagItem()
{
   // Do nothing
}


//////////  Client only code:

void NexusFlagItem::renderItem(const Point &pos)
{
   renderItemAlpha(pos, 1.0f);
}


void NexusFlagItem::renderItemAlpha(const Point &pos, F32 alpha)
{
#ifndef ZAP_DEDICATED
   Renderer& r = Renderer::get();
   Point offset;

   if(mIsMounted)
      offset.set(15, -15);

   renderFlag(pos + offset, getColor(), NULL, alpha);

   if(mIsMounted && mFlagCount > 0)
   {
      if     (mFlagCount >= 40) r.setColor(Colors::paleRed, alpha);   // like, rad!
      else if(mFlagCount >= 20) r.setColor(Colors::yellow,  alpha);   // cool!
      else if(mFlagCount >= 10) r.setColor(Colors::green,   alpha);   // ok, I guess
      else                      r.setColor(Colors::white,   alpha);   // lame

      drawStringf(pos.x + 10, pos.y - 46, 12, "%d", mFlagCount);
   }
#endif
}

//////////  END Client only code



// Private helper function
void NexusFlagItem::dropFlags(U32 flags)
{
   if(!mMount.isValid())
      return;

   // This is server only, folks -- avoids problem with adding flag on client when it doesn't really exist on server
   if(isGhost())
      return;

   static const U32 MAX_DROP_FLAGS = 200;    // If we drop too many flags, things just get bogged down.  This limit is rarely hit.

   if(flags > MAX_DROP_FLAGS)
   {
      for(U32 i = MAX_DROP_FLAGS; i > 0; i--)
      {
         // By dividing and subtracting, it works by using integer divide, subtracting from "flags" left, 
         // and the last loop is (i == 1), dropping exact amount using only limited FlagItems
         U32 flagValue = flags / i;

         getGame()->releaseFlag(mMount->getActualPos(), mMount->getActualVel(), flagValue);

         flags -= flagValue;
      }
   }
   else     // Normal situation
      for(U32 i = 0; i < flags; i++)
         getGame()->releaseFlag(mMount->getActualPos(), mMount->getActualVel());

   changeFlagCount(0);
}


void NexusFlagItem::dismount(DismountMode dismountMode)
{
   if(isGhost())      // Server only
      return;
   if(getDatabase() == NULL)  // must be in database, switching levels makes database NULL
      return;

   if(dismountMode == DISMOUNT_MOUNT_WAS_KILLED)
   {
      // Should getting shot up count as a flag drop event for statistics purposes?
      if(mMount && mMount->getClientInfo())
         mMount->getClientInfo()->getStatistics()->mFlagDrop += mFlagCount + 1;

      dropFlags(mFlagCount + 1);    // Drop at least one flag plus as many as the ship carries

      // Now delete the flag itself
      removeFromDatabase(false);   
      deleteObject();
   }
   else
   {
      GameType *gameType = getGame()->getGameType();
      if(!gameType)        // Crashed here once, don't know why, so I added the check
         return;

      gameType->itemDropped(mMount, this, dismountMode); // Sends messages; no flags actually dropped here; server only method
      dropFlags(mFlagCount);                             // Only dropping the flags we're carrying, not the "extra" one that comes when we die
   }
}


U32 NexusFlagItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & FlagCountMask))
      stream->write(mFlagCount);

   return Parent::packUpdate(connection, updateMask, stream);
}


void NexusFlagItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   if(stream->readFlag())
      stream->read(&mFlagCount);

   Parent::unpackUpdate(connection, stream);
}


bool NexusFlagItem::isItemThatMakesYouVisibleWhileCloaked()
{
   return false;
}


void NexusFlagItem::changeFlagCount(U32 change)
{
   mFlagCount = change;
   setMaskBits(FlagCountMask);
}


U32 NexusFlagItem::getFlagCount()
{
   return mFlagCount;
}


bool NexusFlagItem::isAtHome()
{
   return false;
}


void NexusFlagItem::sendHome()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////

/**
 * @luafunc NexusZone::NexusZone()
 * @luafunc NexusZone::NexusZone(Geom polyGeom)
 */
// Combined Lua / C++ constructor)
NexusZone::NexusZone(lua_State *L)
{
   mObjectTypeNumber = NexusTypeNumber;
   mNetFlags.set(Ghostable);
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { POLY, END }}, 2 };
      S32 profile = checkArgList(L, constructorArgList, "NexusZone", "constructor");
         
      if(profile == 1)
         setGeom(L, 1);
   }

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
NexusZone::~NexusZone()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


NexusZone *NexusZone::clone() const
{
   return new NexusZone(*this);
}


// The Nexus object itself
// If there are 2 or 4 params, this is an Zap! rectangular format object
// If there are more, this is a Bitfighter polygonal format object
// Note parallel code in EditorUserInterface::processLevelLoadLine
bool NexusZone::processArguments(S32 argc2, const char **argv2, Game *game)
{
   // Need to handle or ignore arguments that starts with letters,
   // so a possible future version can add parameters without compatibility problem.
   S32 argc = 0;
   const char *argv[Geometry::MAX_POLY_POINTS * 2];
   for(S32 i = 0; i < argc2; i++)  // the idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char c = argv2[i][0];
      //switch(c)
      //{
      //case 'A': Something = atof(&argv2[i][1]); break;  // using second char to handle number
      //}
      if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
      {
         if(argc < Geometry::MAX_POLY_POINTS * 2)
         {  argv[argc] = argv2[i];
            argc++;
         }
      }
   }

   if(argc < 2)
      return false;

   if(argc <= 4)     // Archaic Zap! format
      processArguments_ArchaicZapFormat(argc, argv, game->getLegacyGridSize());
   else              // Sleek, modern Bitfighter format
      return Parent::processArguments(argc, argv, game);

   return true;
}


// Read and process NexusZone format used in Zap -- we need this for backwards compatibility
void NexusZone::processArguments_ArchaicZapFormat(S32 argc, const char **argv, F32 gridSize)
{
   Point pos;
   pos.read(argv);
   pos *= gridSize;

   Point ext(50, 50);

   if(argc == 4)
      ext.set(atoi(argv[2]), atoi(argv[3]));

   addVert(Point(pos.x - ext.x, pos.y - ext.y));   // UL corner
   addVert(Point(pos.x + ext.x, pos.y - ext.y));   // UR corner
   addVert(Point(pos.x + ext.x, pos.y + ext.y));   // LR corner
   addVert(Point(pos.x - ext.x, pos.y + ext.y));   // LL corner
   
   updateExtentInDatabase(); 
}


const char *NexusZone::getOnScreenName()     { return "Nexus"; }
const char *NexusZone::getOnDockName()       { return "Nexus"; }
const char *NexusZone::getPrettyNamePlural() { return "Nexii"; }
const char *NexusZone::getEditorHelpString() { return "Area to bring flags in Hunter game.  Cannot be used in other games."; }


bool NexusZone::hasTeam()      { return false; }
bool NexusZone::canBeHostile() { return false; }
bool NexusZone::canBeNeutral() { return false; }


string NexusZone::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + geomToLevelCode();
}


void NexusZone::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   if(!isGhost())
      setScopeAlways();    // Always visible!

   GameType *gameType = getGame()->getGameType();

   if(gameType && gameType->getGameTypeId() == NexusGame)
      static_cast<NexusGameType *>(gameType)->addNexus(this);
}


void NexusZone::idle(BfObject::IdleCallPath path)
{
   // Do nothing
}


void NexusZone::render()
{
#ifndef ZAP_DEDICATED
   GameType *gameType = getGame()->getGameType();
   NexusGameType *nexusGameType = NULL;

   if(gameType && gameType->getGameTypeId() == NexusGame)
      nexusGameType = static_cast<NexusGameType *>(gameType);

   bool isOpen = nexusGameType && nexusGameType->mNexusIsOpen;
   F32 glowFraction = gameType ? gameType->mZoneGlowTimer.getFraction() : 0;

   renderNexus(getOutline(), getFill(), getCentroid(), getLabelAngle(), isOpen, glowFraction);
#endif
}


void NexusZone::renderDock()
{
#ifndef ZAP_DEDICATED
  renderNexus(getOutline(), getFill(), false, 0);
#endif
}


void NexusZone::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
   render();
   PolygonObject::renderEditor(currentScale, snappingToWallCornersEnabled, true);
}


const Vector<Point> *NexusZone::getCollisionPoly() const
{
   return getOutline();
}


bool NexusZone::collide(BfObject *hitObject)
{
   if(isGhost())
      return false;

   // From here on out, runs on server only

   if( ! (isShipType(hitObject->getObjectTypeNumber())) )
      return false;

   Ship *theShip = static_cast<Ship *>(hitObject);

   if(theShip->mHasExploded)                             // Ignore collisions with exploded ships
      return false;

   GameType *gameType = getGame()->getGameType();
   NexusGameType *nexusGameType = NULL;

   if(gameType && gameType->getGameTypeId() == NexusGame)
      nexusGameType = static_cast<NexusGameType *>(getGame()->getGameType());

   if(nexusGameType && nexusGameType->mNexusIsOpen)      // Is the nexus open?
      nexusGameType->shipTouchNexus(theShip, this);

   return false;
}


/////
// Lua interface
/**
 * @luaclass NexusZone
 *
 * @brief Players return flags to a NexusZone in a Nexus game.
 *
 * @descr NexusZone represents a flag return area in a Nexus game. It plays no
 * role in any other game type. Nexus opening and closing times are actually
 * game parameters, so these methods serve only as a convenient and intuitive
 * way to access those parameters. Therefore, modifying the opening/closing
 * schedule or status of any NexusZone will have the same effect on all
 * NexusZones in the game.
 */
//               Fn name         Param profiles     Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, setOpen,       ARRAYDEF({{ BOOL,    END }}), 1 ) \
   METHOD(CLASS, isOpen,        ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setOpenTime,   ARRAYDEF({{ INT_GE0, END }}), 1 ) \
   METHOD(CLASS, setClosedTime, ARRAYDEF({{ INT_GE0, END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(NexusZone, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(NexusZone, LUA_METHODS);

#undef LUA_METHODS


const char *NexusZone::luaClassName = "NexusZone";
REGISTER_LUA_SUBCLASS(NexusZone, Zone);


// All these methods are really just passthroughs to the underlying game object.  They will have no effect if the
// NexusZone has not been added to the game.  Perhaps this is bad design...

/**
 * @luafunc bool NexusZone::isOpen()
 *
 * @return The current state of the Nexus. `true` for open, `false` for closed.
 *
 * @note Since all Nexus items open and close together, this method will return
 * the same value for all Nexus zones in a game at any given time.
 */
S32 NexusZone::lua_isOpen(lua_State *L)
{ 
   if(!mGame)
      return returnBool(L, false);

   GameType *gameType = mGame->getGameType();

   if(gameType->getGameTypeId() == NexusGame)
      return returnBool(L, static_cast<NexusGameType *>(gameType)->mNexusIsOpen);
   else
      return returnBool(L, false);     // If not a Nexus game, Nexus will never be open
}  


/**
 * @luafunc NexusZone::setOpen(bool open)
 *
 * @brief Set whether the Nexus is open or close.
 *
 * @param open `true` to open the Nexus, `false` to close it.
 *
 * @note Since all Nexus items open and close together, this method will affect
 * all Nexus items in a game.
*/
S32 NexusZone::lua_setOpen(lua_State *L)
{ 
   checkArgList(L, functionArgs, "NexusZone", "setOpen");

   if(!mGame)
      return 0;

   GameType *gameType = mGame->getGameType();

   if(gameType->getGameTypeId() != NexusGame)       // Do nothing if this is not a Nexus game
      return 0;
  
   static_cast<NexusGameType *>(gameType)->setNexusState(getBool(L, 1));

   return 0;
}  


/**
 * @luafunc NexusZone::setOpenTime(int seconds)
 * 
 * @brief Set the time (in seconds) that the Nexus should remain open. 
 *
 * @descr Pass 0 if the Nexus should never close, causing the Nexus to remain
 * open permanently. Passing a negative time will generate an error.
 *
 * @param seconds Time in seconds that the Nexus should remain open.
 *
 * @note Since all Nexus items open and close together, this method will affect
 * all Nexus items in a game.
 */
S32 NexusZone::lua_setOpenTime(lua_State *L)
{
   checkArgList(L, functionArgs, "NexusZone", "setOpenTime");

   if(!mGame)
      return 0;

   GameType *gameType = mGame->getGameType();

   if(gameType->getGameTypeId() != NexusGame)       // Do nothing if this is not a Nexus game
      return 0;
  
   static_cast<NexusGameType *>(gameType)->setNewOpenTime(S32(getInt(L, 1)));

   return 0;
}


/**
 * @luafunc NexusZone::setClosedTime(int seconds)
 *
 * @brief Set the time (in seconds) that the Nexus will remain closed. 
 *
 * @descr Pass 0 if the Nexus should never open, causing the Nexus to remain
 * closed permanently. Passing a negative time will generate an error.
 *
 * @param seconds Time in seconds that the Nexus should remain closed.
 *
 * @note Since all Nexus items open and close together, this method will affect
 * all Nexus items in a game.
 *
 * Also note that in a level file, closing times are specified in fractions of
 * minutes, whereas this method works with seconds.
 */
S32 NexusZone::lua_setClosedTime(lua_State *L)
{
   checkArgList(L, functionArgs, "NexusZone", "setCloseTime");

   if(!mGame)
      return 0;

   GameType *gameType = mGame->getGameType();

   if(gameType->getGameTypeId() != NexusGame)       // Do nothing if this is not a Nexus game
      return 0;
  
   static_cast<NexusGameType *>(gameType)->setNewClosedTime(S32(getInt(L, 1)));

   return 0;
}


};
