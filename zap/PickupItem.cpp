//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "PickupItem.h"

#include "game.h"
#include "gameConnection.h"
#include "ClientInfo.h"

#include "gameObjectRender.h"
#include "stringUtils.h"         // For itos()


namespace Zap
{

// Constructor
PickupItem::PickupItem(float radius, S32 repopDelay) : Parent(radius)
{
   show();

   mRepopDelay = repopDelay;
   mNetFlags.set(Ghostable);
   mRadius = 18;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
PickupItem::~PickupItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void PickupItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}


void PickupItem::idle(BfObject::IdleCallPath path)
{
   if(!mIsVisible && path == BfObject::ServerIdleMainLoop)
   {
      if(mRepopTimer.update(mCurrentMove.time))
      {
         show();

         // Check if there is a ship sitting on this item... it so, ship gets the pickup!
         for(S32 i = 0; i < getGame()->getClientCount(); i++)
         {
            Ship *ship = getGame()->getClientInfo(i)->getShip();

            if(ship && ship->isOnObject(this))
               collide(ship);
         }
      }
   }
   // else ... check onAddedToGame to enable client side idle()
}


bool PickupItem::isVisible() const
{
   return mIsVisible;
}


bool PickupItem::shouldRender() const
{
   return isVisible();
}


S32 PickupItem::getRenderSortValue()
{
   return 1;
}


U32 PickupItem::getRepopDelay()
{
   return mRepopDelay;
}


void PickupItem::setRepopDelay(U32 delay)
{
   mRepopDelay = delay;
}


bool PickupItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 2)
      return false;
   else if(!Parent::processArguments(argc, argv, game))
      return false;

   if(argc == 3)
   {
      S32 repopDelay = atoi(argv[2]);    // 3rd param is time for this to regenerate in seconds
      if(repopDelay > 0)
         mRepopDelay = repopDelay;
      else
         mRepopDelay = 0;
   }

   return true;
}


string PickupItem::toLevelCode() const
{
   return Parent::toLevelCode() + " " + (mRepopDelay > 0 ? itos(mRepopDelay) : "0");
}


U32 PickupItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection, updateMask, stream);       // Writes id and pos

   stream->writeFlag(mIsVisible);
   stream->writeFlag((updateMask & SoundMask) && (updateMask != 0xFFFFFFFF));

   return retMask;
}


void PickupItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);    // Get id and pos

   mIsVisible = stream->readFlag();

   if(stream->readFlag())
      onClientPickup();
}


// Runs on both client and server, but does nothing on client
bool PickupItem::collide(BfObject *otherObject)
{
   if(mIsVisible && !isGhost() && isShipType(otherObject->getObjectTypeNumber()))
   {
      if(pickup(static_cast<Ship *>(otherObject)))
      {
         hide();
         setMaskBits(SoundMask);       // Trigger SFX on client
      }
   }
   return false;
}


void PickupItem::hide()
{
   mRepopTimer.reset(mRepopDelay * 1000);

   mIsVisible = false;
   setMaskBits(PickupMask);   // Triggers update
}


void PickupItem::show()
{
   mIsVisible = true;
   setMaskBits(PickupMask);   // Triggers update
}


// Implementations provided to keep class from being abstract; need non-abstract class
// so luaW can (theoretically) instantiate this class, even though it never will.  If
// that issue gets resolved, we can remove this code and revert the class to abstract.
bool PickupItem::pickup(Ship *ship) 
{ 
   TNLAssert(false, "Function not implemented!"); 
   return false;
}


// Plays a sound on the client
void PickupItem::onClientPickup()
{
   TNLAssert(false, "Function not implemented!");
}


// Render some attributes when item is selected but not being edited
void PickupItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("Regen");

   if(mRepopDelay == 0)
      values.push_back("None");
   else
      values.push_back(itos(mRepopDelay) + " sec" + ( mRepopDelay != 1 ? "s" : ""));
}


/////
// Lua interface

/**
 * @luaclass PickupItem
 *
 * @brief Parent class representing items that can be picked up, such as
 * RepairItem or EnergyItem.
 *
 * @descr PickupItems are items that can be picked up by ships to confer some
 * benefit, such as increased health or energy. When PickupItems are picked up,
 * they will regenerate after a time, called the regenTime. PickupItems continue
 * to exist, even when they are not visible.
 */
//               Fn name  Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, isVis,        ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setVis,       ARRAYDEF({{ BOOL,    END }}), 1 ) \
   METHOD(CLASS, setRegenTime, ARRAYDEF({{ INT_GE0, END }}), 1 ) \
   METHOD(CLASS, getRegenTime, ARRAYDEF({{ INT_GE0, END }}), 1 ) \


GENERATE_LUA_METHODS_TABLE(PickupItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(PickupItem, LUA_METHODS);

#undef LUA_METHODS


const char *PickupItem::luaClassName = "PickupItem";
REGISTER_LUA_SUBCLASS(PickupItem, Item);


/**
 * @luafunc bool PickupItem::isVis()
 *
 * @return `true` if item is currently visible, `false` if not.
*/
S32 PickupItem::lua_isVis(lua_State *L) { return returnBool(L, isVisible()); }


/**
 * @luafunc PickupItem::setVis(bool visible)
 *
 * @brief Show or hide the item. Note that hiding an item will reset the
 * timer that makes it visible again, just as if it had been picked up by a
 * player.
 *
 * @param visible Pass `true` to make the item visible, `false` to hide it.
*/
S32 PickupItem::lua_setVis(lua_State *L)
{
   checkArgList(L, functionArgs, "PickupItem", "setVis");

   if(getBool(L, 1))
      show();
   else
      hide();

   return 0;
}


/**
 * @luafunc PickupItem::setRegenTime(int time)
 *
 * @brief Sets the time (in seconds) for the PickupItem to regenerate itself.
 * Default is 20 seconds. Setting regen time to a negative value will produce an
 * error.
 *
 * @param time Time in seconds for the item to remain hidden.
 */
S32 PickupItem::lua_setRegenTime(lua_State *L)
{ 
   checkArgList(L, functionArgs, "PickupItem", "setRegenTime");

   mRepopDelay = S32(getInt(L, 1));

   return 0;
}


/**
 * @luafunc int PickupItem::getRegenTime()
 *
 * @brief Returns the time (in seconds) for the PickupItem to regenerate itself.
 *
 * @return Time in seconds for the item will remain hidden.
*/
S32 PickupItem::lua_getRegenTime(lua_State *L) { return returnInt(L, mRepopDelay); }


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(RepairItem);

/**
 * @luafunc RepairItem::RepairItem()
 * @luafunc RepairItem::RepairItem(point pos)
 * @luafunc RepairItem::RepairItem(point pos, int time)
 */
RepairItem::RepairItem(lua_State *L) : Parent((F32)REPAIR_ITEM_RADIUS, DEFAULT_RESPAWN_TIME)   // Combined Lua / C++ default constructor
{ 
   mObjectTypeNumber = RepairItemTypeNumber;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, INT, END }}, 3 };

      S32 profile = checkArgList(L, constructorArgList, "RepairItem", "constructor");

      if(profile == 1)
         setPos(L, 1);

      else if(profile == 2)
      {
         setPos(L, 1);
         mRepopDelay = S32(getInt(L, 2));
      }
   }
}


// Destructor
RepairItem::~RepairItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


RepairItem *RepairItem::clone() const
{
   return new RepairItem(*this);
}


// Runs on server, returns true if we're doing the pickup, false otherwise
bool RepairItem::pickup(Ship *ship)
{
   if(ship->getHealth() >= 1)
      return false;

   DamageInfo di;
   di.damageAmount = -0.5f;      // Negative damage = repair!
   di.damageType = DamageTypePoint;
   di.damagingObject = this;

   ship->damageObject(&di);
   return true;
}


// Runs on client when item's unpack method signifies the item has been picked up
void RepairItem::onClientPickup()
{
   getGame()->playSoundEffect(SFXShipHeal, getPos());
}


void RepairItem::renderItem(const Point &pos)
{
   if(shouldRender())
      renderRepairItem(pos);
}


const char *RepairItem::getOnScreenName()     { return "Repair";       }
const char *RepairItem::getOnDockName()       { return "Rep";       }
const char *RepairItem::getPrettyNamePlural() { return "Repair Items"; }
const char *RepairItem::getEditorHelpString() { return "Repairs damage to ships. [B]"; }

S32 RepairItem::getDockRadius() { return 11; }


void RepairItem::renderDock()
{
   renderRepairItem(getPos(), true, 0, 1);
}


F32 RepairItem::getEditorRadius(F32 currentScale)
{
   return mRadius * currentScale + 5;
}


/////
// Lua interface

/**
 * @luaclass RepairItem
 * 
 * @brief Adds health to ships that pick them up.
 */
// Only implements inherited methods
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(RepairItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(RepairItem, LUA_METHODS);

#undef LUA_METHODS



const char *RepairItem::luaClassName = "RepairItem";
REGISTER_LUA_SUBCLASS(RepairItem, PickupItem);



////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(EnergyItem);
/**
 * @luafunc EnergyItem::EnergyItem()
 * @luafunc EnergyItem::EnergyItem(point pos)
 * @luafunc EnergyItem::EnergyItem(point pos, int time)
 */
EnergyItem::EnergyItem(lua_State *L) : Parent(20, DEFAULT_RESPAWN_TIME)    // Combined Lua / C++ default constructor
{
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, INT, END }}, 3 };

      S32 profile = checkArgList(L, constructorArgList, "EnergyItem", "constructor");

      if(profile == 1)
         setPos(L, 1);

      else if(profile == 2)
      {
         setPos(L, 1);
         mRepopDelay = S32(getInt(L, 2));
      }
   }
   
   mObjectTypeNumber = EnergyItemTypeNumber;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
EnergyItem::~EnergyItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


EnergyItem *EnergyItem::clone() const
{
   return new EnergyItem(*this);
}



// Runs on server, returns true if we're doing the pickup, false otherwise
bool EnergyItem::pickup(Ship *ship)
{
   S32 energy = ship->getEnergy();

   if(energy >= Ship::EnergyMax)             // Energy?  We don't need no stinkin' energy!!
      return false;

   static const S32 EnergyItemFillip = Ship::EnergyMax / 2;

   // Credit the ship 
   ship->creditEnergy(EnergyItemFillip);  // Bump up energy by 50%, changeEnergy() sets energy delta

   // And tell the client to do the same.  Note that we are handling energy with a s2c because it is possible to be
   // traveling so fast that the EnergyItem goes out of scope before there is a chance to use the pack/unpack mechanims
   // to get the energy credit to the client.  s2c will work regardless.
   if(!ship->isRobot() && ship->getControllingClient() != NULL)
      ship->getControllingClient()->s2cCreditEnergy(EnergyItemFillip);

   return true;
}


// Runs on client when item's unpack method signifies the item has been picked up
void EnergyItem::onClientPickup()
{
   getGame()->playSoundEffect(SFXShipHeal, getPos());
}


void EnergyItem::renderItem(const Point &pos)
{
   if(shouldRender())
      renderEnergyItem(pos);
}


const char *EnergyItem::getOnScreenName()     { return "Energy";       }
const char *EnergyItem::getOnDockName()       { return "Ener";       }
const char *EnergyItem::getPrettyNamePlural() { return "Energy Items"; }
const char *EnergyItem::getEditorHelpString() { return "Restores energy to ships"; }


S32 EnergyItem::getDockRadius() { return 11; }


void EnergyItem::renderDock()
{
   renderEnergyItem(getPos(), true);
}


F32 EnergyItem::getEditorRadius(F32 currentScale)
{
   return mRadius * currentScale + 5;
}

/////
// Lua interface

/**
 * @luaclass EnergyItem
 * 
 * @brief Adds energy to ships that pick them up.
 */
// Only implements inherited methods
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(EnergyItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(EnergyItem, LUA_METHODS);

#undef LUA_METHODS


const char *EnergyItem::luaClassName = "EnergyItem";
REGISTER_LUA_SUBCLASS(EnergyItem, PickupItem);


};
