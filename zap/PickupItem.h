//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _PICKUP_ITEM_H_
#define _PICKUP_ITEM_H_

#include "item.h"

namespace Zap
{


// Base class for things that can be picked up, such as RepairItems and EnergyItems
class PickupItem : public Item
{
   typedef Item Parent;

private:
   bool mIsVisible;
   Timer mRepopTimer;

protected:
   enum MaskBits {
      PickupMask    = Parent::FirstFreeMask << 0,
      SoundMask     = Parent::FirstFreeMask << 1,
      FirstFreeMask = Parent::FirstFreeMask << 2
   };

   S32 mRepopDelay;            // Period of mRepopTimer, in seconds


public:
   PickupItem(float radius = 1, S32 repopDelay = 20);   // Constructor
   virtual ~PickupItem();                               // Destructor

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toLevelCode() const;

   void onAddedToGame(Game *game);
   void idle(BfObject::IdleCallPath path);
   bool isVisible() const;
   bool shouldRender() const;
   S32 getRenderSortValue();

   U32 getRepopDelay();
   void setRepopDelay(U32 delay);

   virtual void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   bool collide(BfObject *otherObject);
   void hide();
   void show();
   virtual bool pickup(Ship *theShip);
   virtual void onClientPickup();

	///// Lua interface
	LUAW_DECLARE_CLASS(PickupItem);

	static const char *luaClassName;
	static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_isVis(lua_State *L);
   S32 lua_setVis(lua_State *L);
   S32 lua_setRegenTime(lua_State *L);
   S32 lua_getRegenTime(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class RepairItem : public PickupItem
{
protected:
   typedef PickupItem Parent;

public:
   static const S32 DEFAULT_RESPAWN_TIME = 20;    // In seconds
   static const S32 REPAIR_ITEM_RADIUS = 20;

   explicit RepairItem(lua_State *L = NULL);    // Combined Lua / C++ default constructor
   virtual ~RepairItem();              // Destructor

   RepairItem *clone() const;

   bool pickup(Ship *theShip);
   void onClientPickup();
   void renderItem(const Point &pos);

   TNL_DECLARE_CLASS(RepairItem);

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   virtual S32 getDockRadius();
   F32 getEditorRadius(F32 currentScale);
   void renderDock();

   ///// Lua interface
	LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(RepairItem);

	static const char *luaClassName;
	static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

class EnergyItem : public PickupItem
{
private:
   typedef PickupItem Parent;

public:
   static const S32 DEFAULT_RESPAWN_TIME = 20;    // In seconds

   explicit EnergyItem(lua_State *L = NULL);    // Combined Lua / C++ default constructor
   virtual ~EnergyItem();              // Destructor

   EnergyItem *clone() const;

   bool pickup(Ship *theShip);
   void onClientPickup();
   void renderItem(const Point &pos);

   TNL_DECLARE_CLASS(EnergyItem);

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   virtual S32 getDockRadius();
   F32 getEditorRadius(F32 currentScale);
   void renderDock();

   ///// Lua interface
	LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(EnergyItem);

	static const char *luaClassName;
	static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


};    // namespace

#endif
