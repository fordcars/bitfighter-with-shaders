//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _MOVEOBJECT_H_
#define _MOVEOBJECT_H_

#include "item.h"          // Parent class
#include "LuaWrapper.h"
#include "DismountModesEnum.h"

namespace Zap
{

enum MoveStateNames {
   ActualState = 0,
   RenderState,
   LastUnpackUpdateState,
   MoveStateCount,
};


class MoveStates
{
private:
   struct MoveState  // need public, not protected, for SpeedZone handling...  TODO: fix this flaw
   {
      Point pos;        // Actual position of the ship/object
      float angle;      // Actual angle of the ship/object
      Point vel;        // Actual velocity of the ship/object
   };

   MoveState mMoveState[MoveStateCount];     // MoveStateCount = 3, as per enum above

public:
   virtual ~MoveStates();

   virtual Point getPos(S32 state) const;
   virtual void setPos(S32 state, const Point &pos);

   virtual Point getVel(S32 state) const;
   virtual void setVel(S32 state, const Point &vel);

   F32 getAngle(S32 state) const;
   void setAngle(S32 state, F32 angle);
};


////////////////////////////////////////
////////////////////////////////////////

class MoveObject : public Item
{
   typedef Item Parent;

private:
   S32 mHitLimit;             // Internal counter for processing collisions
   MoveStates mMoveStates;

   // For maintaining a list of zones the object is currently in
   Vector<SafePtr<Zone> > mZones1;      
   Vector<SafePtr<Zone> > mZones2;
   bool mZones1IsCurrent;        // "Pointer" to one of the above

   Vector<SafePtr<Zone> > &getCurrZoneList();                  // Get list of zones object is currently in
   Vector<SafePtr<Zone> > &getPrevZoneList();                  // Get list of zones object was in last tick

protected:
   enum {
      InterpMaxVelocity = 900, // velocity to use to interpolate to proper position
      InterpAcceleration = 1800,
   };

   bool mInterpolating;
   F32 mMass;
   bool mWaitingForMoveToUpdate;  // client only

   enum MaskBits {
      PositionMask     = Parent::FirstFreeMask << 0,     // Position has changed and needs to be updated
      WarpPositionMask = Parent::FirstFreeMask << 1,     // A large change in position not requiring client-side "smoothing"
      FirstFreeMask    = Parent::FirstFreeMask << 2
   };

   virtual void onEnteredZone(Zone *zone);
   virtual void onLeftZone(Zone *zone);
   void getZonesObjectIsIn(Vector<SafePtr<Zone> > &zoneList);

public:
   MoveObject(const Point &p = Point(0,0), float radius = 1, float mass = 1);     // Constructor
   virtual ~MoveObject();                                                                // Destructor
      
   virtual bool processArguments(S32 argc, const char **argv, Game *game);
   virtual string toLevelCode() const;

   static const F32 CollisionElasticity;

   void onAddedToGame(Game *game);
   void idle(BfObject::IdleCallPath path);    // Called from child object idle methods
   virtual void updateInterpolation();
   virtual Rect calcExtents();

   bool isMoveObject();

   // These methods will be overridden by MountableItem
   virtual Point getRenderPos() const;
   virtual Point getActualPos() const;
   virtual Point getRenderVel() const;      // Distance/sec
   virtual Point getActualVel() const;      // Distance/sec

   F32 getRenderAngle() const;
   F32 getActualAngle() const;

   // Because MoveObjects have multiple positions (actual, render), we need to implement the following
   // functions differently than most objects do
   Point getPos() const;      // Maps to getActualPos
   Point getVel() const;      // Maps to getActualVel

   Point getPos(S32 stateIndex) const;
   Point getVel(S32 stateIndex) const;
   F32 getAngle(S32 stateIndex) const;

   void setPos(lua_State *L, S32 stackIndex);

   void setPos(S32 stateIndex, const Point &pos);
   void setVel(S32 stateIndex, const Point &vel);     // Distance/sec
   void setAngle(S32 stateIndex, F32 angle);

   void copyMoveState(S32 from, S32 to);

   virtual void setActualPos(const Point &pos);
   virtual void setActualVel(const Point &vel);

   void setRenderPos(const Point &pos);
   void setRenderVel(const Point &vel);

   void setRenderAngle(F32 angle);
   void setActualAngle(F32 angle);

   void setPos(const Point &pos);

   void setPosVelAng(const Point &pos, const Point &vel, F32 ang);
   virtual void setInitialPosVelAng(const Point &pos, const Point &vel, F32 ang);

   F32 getMass();
   void setMass(F32 mass);

   virtual void playCollisionSound(U32 stateIndex, MoveObject *moveObjectThatWasHit, F32 velocity);

   F32 move(F32 time, U32 stateIndex, bool displacing = false, Vector<SafePtr<MoveObject> > = Vector<SafePtr<MoveObject> >());
   virtual bool collide(BfObject *otherObject);

   // CollideTypes is used to improve speed on findFirstCollision
   virtual TestFunc collideTypes();

   BfObject *findFirstCollision(U32 stateIndex, F32 &collisionTime, Point &collisionPoint);
   void computeCollisionResponseMoveObject(U32 stateIndex, MoveObject *objHit);
   void computeCollisionResponseBarrier(U32 stateIndex, Point &collisionPoint);
   F32 computeMinSeperationTime(U32 stateIndex, MoveObject *contactObject, Point intendedPos);

   void checkForZones();                                       // See if object entered or left any zones
        
   void computeImpulseDirection(DamageInfo *damageInfo);

   virtual bool getCollisionCircle(U32 stateIndex, Point &point, F32 &radius) const;

   virtual void onGeomChanged();

   ///// Lua interface
   LUAW_DECLARE_CLASS(MoveObject);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   // Get/set object's velocity vector
   virtual S32 lua_getVel(lua_State *L);
   virtual S32 lua_setVel(lua_State *L);

   // Get/set object's rotation angle
   virtual S32 lua_getAngle(lua_State *L);
   virtual S32 lua_setAngle(lua_State *L);
};


class MoveItem : public MoveObject
{
   typedef MoveObject Parent;

private:
   F32 updateTimer;
   Point prevMoveVelocity;

protected:
   bool mIsCollideable;

public:
   MoveItem(const Point &p = Point(0,0), bool collideable = false, float radius = 1, float mass = 1);   // Constructor
   virtual ~MoveItem();                                                                                 // Destructor

   virtual void idle(BfObject::IdleCallPath path);

   virtual U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   virtual void unpackUpdate(GhostConnection *connection, BitStream *stream);

   virtual void setActualPos(const Point &pos);
   virtual void setActualVel(const Point &vel);

   void setCollideable(bool isCollideable);
   void setPositionMask();

   virtual void render();

   virtual void renderItem(const Point &pos);                  // Does actual rendering, allowing render() to be generic for all Items
   virtual void renderItemAlpha(const Point &pos, F32 alpha);  // Used for mounted items when cloaked

   virtual bool collide(BfObject *otherObject);
};


////////////////////////////////////////
////////////////////////////////////////

class MountableItem : public MoveItem
{
   typedef MoveItem Parent;

protected:
   enum MaskBits {
      MountMask        = Parent::FirstFreeMask << 0,
      FirstFreeMask    = Parent::FirstFreeMask << 1
   };

   bool mIsMounted;
   SafePtr<Ship> mMount;

   Timer mDroppedTimer;                   // Make flags have a tiny bit of delay before they can be picked up again

public:
   MountableItem(const Point &pos = Point(0,0), bool collideable = false, float radius = 1, float mass = 1);   // Constructor
   virtual ~MountableItem();                                                                                   // Destructor

   // Override some parent functions
   void idle(BfObject::IdleCallPath path);
   void render();
   virtual U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   virtual void unpackUpdate(GhostConnection *connection, BitStream *stream);
   bool collide(BfObject *otherObject);

   // Mounting related functions
   Ship *getMount();

   virtual void dismount(DismountMode dismountMode);

   virtual void mountToShip(Ship *theShip);

   bool isMounted();
   virtual bool isItemThatMakesYouVisibleWhileCloaked();      // NexusFlagItem overrides to false

   Point getRenderPos() const;
   Point getActualPos() const;
   Point getRenderVel() const;      // Distance/sec
   Point getActualVel() const;      // Distance/sec

   ///// Lua interface
   LUAW_DECLARE_CLASS(MountableItem);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   virtual S32 lua_isOnShip(lua_State *L);                 // Is flag being carried by a ship?
   virtual S32 lua_getShip(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

// A class of items that has a more-or-less constant velocity
class VelocityItem : public MoveItem
{
   typedef MoveItem Parent; 

private:
   F32 mInherentSpeed;

public:
   VelocityItem(const Point &pos, F32 speed, F32 radius, F32 mass);     // Constructor
   virtual ~VelocityItem();

   void setPosAng(Point pos, F32 ang);
   void setInitialPosVelAng(const Point &pos, const Point &vel, F32 ang);
};


////////////////////////////////////////
////////////////////////////////////////


class Asteroid : public VelocityItem
{

typedef VelocityItem Parent;    

private:
   S32 mSizeLeft;
   bool hasExploded;
   S32 mDesign;
   S32 mTeam;

protected:
   enum MaskBits {
      ItemChangedMask  = Parent::FirstFreeMask << 0,
      FirstFreeMask    = Parent::FirstFreeMask << 1
   };

   static const F32 ASTEROID_MASS_SIZE1;
   static const F32 ASTEROID_RADIUS_SIZE1;
   static const F32 ASTEROID_SPEED;

public:
   explicit Asteroid(lua_State *L = NULL); // Combined Lua / C++ default constructor
   virtual ~Asteroid();           // Destructor

   static const U8 ASTEROID_SIZELEFT_BIT_COUNT;

   // For editor attribute. real limit based on bit count is (1 << ASTEROID_SIZELEFT_BIT_COUNT) - 1; // = 7
   static const S32 ASTEROID_SIZELEFT_MAX;
   static const S32 ASTEROID_INITIAL_SIZELEFT;      // Starting size

   Asteroid *clone() const;

   static F32 getAsteroidRadius(S32 size_left);
   static F32 getAsteroidMass(S32 size_left);

   void renderItem(const Point &pos);
   bool shouldRender() const;
   const Vector<Point> *getCollisionPoly() const;
   bool collide(BfObject *otherObject);

   // Asteroid does not collide to another asteroid
   TestFunc collideTypes();

   void damageObject(DamageInfo *theInfo);
   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);
   void onItemExploded(Point pos);

   bool processArguments(S32 argc2, const char **argv2, Game *game);
   string toLevelCode() const;

//#ifndef ZAP_DEDICATED
//private:
//   static EditorAttributeMenuUI *mAttributeMenuUI;
//
//public:
//   // These four methods are all that's needed to add an editable attribute to a class...
//   EditorAttributeMenuUI *getAttributeMenu();
//   void startEditingAttrs(EditorAttributeMenuUI *attributeMenu);    // Called when we start editing to get menus populated
//   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);     // Called when we're done to retrieve values set by the menu

   virtual void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);
//#endif

   static U32 getDesignCount();
   S32 getCurrentSize() const;
   void setCurrentSize(S32 size);

   TNL_DECLARE_CLASS(Asteroid);

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   //virtual S32 getDockRadius();
   F32 getEditorRadius(F32 currentScale);
   void renderDock();

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Asteroid);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getSizeIndex(lua_State *L);   // Index of current asteroid size (0 = initial size, 1 = next smaller, 2 = ...) (returns int)
   S32 lua_getSizeCount(lua_State *L);   // Number of indexes of size we can have (returns int)
   S32 lua_setSize(lua_State *L);        // Sets asteroid size
};


////////////////////////////////////////
////////////////////////////////////////

class TestItem : public MoveItem
{
   typedef MoveItem Parent;

private:
   void setOutline();

public:
   explicit TestItem(lua_State *L = NULL); // Combined Lua / C++ default constructor
   virtual ~TestItem();           // Destructor
   TestItem *clone() const;

   // Test methods
   void idle(BfObject::IdleCallPath path);

   static const S32 TEST_ITEM_RADIUS = 60;
   static const S32 TEST_ITEM_SIDES  =  7;

   void renderItem(const Point &pos);
   void damageObject(DamageInfo *theInfo);
   const Vector<Point> *getCollisionPoly() const;


   TNL_DECLARE_CLASS(TestItem);

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   F32 getEditorRadius(F32 currentScale);
   void renderDock();

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(TestItem);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

class ResourceItem : public MountableItem
{
   typedef MountableItem Parent; 

private:
   void setOutline();

public:
   explicit ResourceItem(lua_State *L = NULL); // Combined Lua / C++ default constructor
   virtual ~ResourceItem();           // Destructor
   ResourceItem *clone() const;

   static const S32 RESOURCE_ITEM_RADIUS = 20;

   void renderItem(const Point &pos);
   void renderItemAlpha(const Point &pos, F32 alpha);
   bool collide(BfObject *hitObject);
   void damageObject(DamageInfo *theInfo);
   void dismount(DismountMode dismountMode);
   bool isItemThatMakesYouVisibleWhileCloaked();

   static void generateOutlinePoints(const Point &pos, F32 scale, Vector<Point> &points);


   TNL_DECLARE_CLASS(ResourceItem);

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   void renderDock();

   ///// Lua Interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(ResourceItem);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};



};

#endif

