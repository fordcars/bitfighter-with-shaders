//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _ITEM_H_
#define _ITEM_H_

#include "BfObject.h"               // Parent class
#include "PointObject.h"           // Parent class
#include "LuaScriptRunner.h"        // Parent class

#include "Timer.h"

namespace Zap
{

// A note on terminology here: an "object" is any game object, whereas an "item" is a point object that the player will interact with
// Item is now parent class of MoveItem, EngineeredItem, PickupItem

class Item : public PointObject
{
   typedef PointObject Parent;

private:
   U16 mItemId;                     // Item ID, shared between client and server

protected:
   F32 mRadius;
   Vector<Point> mOutlinePoints;    // Points representing an outline of the item, recalculated when position set

   enum MaskBits {
      InitialMask     = Parent::FirstFreeMask << 0,
      ItemChangedMask = Parent::FirstFreeMask << 1,
      ExplodedMask    = Parent::FirstFreeMask << 2,
      FirstFreeMask   = Parent::FirstFreeMask << 3
   };

   static bool mInitial;     // True on initial unpack, false thereafter

public:
   explicit Item(F32 radius = 1);   // Constructor
   virtual ~Item();                 // Destructor

   virtual bool getCollisionCircle(U32 stateIndex, Point &point, F32 &radius) const;

   virtual bool processArguments(S32 argc, const char **argv, Game *game);

   virtual U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   virtual void unpackUpdate(GhostConnection *connection, BitStream *stream);

   F32 getRadius();
   virtual void setRadius(F32 radius);

   virtual void setPos(const Point &pos);
   virtual void setPos(lua_State *L, S32 stackIndex);

   virtual void setOutline();
   virtual const Vector<Point> *getOutline() const;   // Overridden by SpeedZones and Turrets

   virtual void renderItem(const Point &pos);         // Generic renderer -- will be overridden
   virtual void render();

   U16 getItemId();
   void setItemId(U16 id);

   // Editor interface
   virtual void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);
   virtual F32 getEditorRadius(F32 currentScale);
   virtual string toLevelCode() const;

   virtual Rect calcExtents(); 

   // LuaItem interface

   LUAW_DECLARE_CLASS(Item);

	static const char *luaClassName;
	static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   virtual S32 lua_getRad(lua_State *L);
   virtual S32 lua_isInCaptureZone(lua_State *L);      // Non-moving item is never in capture zone, even if it is!
   virtual S32 lua_getCaptureZone(lua_State *L);
   virtual S32 lua_getShip(lua_State *L);
};


};

#endif


