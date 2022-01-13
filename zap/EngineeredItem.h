//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _ENGINEEREDITEM_H_
#define _ENGINEEREDITEM_H_

#include "item.h"             // Parent
#include "Engineerable.h"     // Parent

#include "TeamConstants.h"    // For TEAM_NEUTRAL constant
#include "WeaponInfo.h"


namespace Zap
{

class EngineeredItem : public Item, public Engineerable
{
private:
   typedef Item Parent;

   static const F32 EngineeredItemRadius;
   
   void computeExtent();

   virtual F32 getSelectionOffsetMagnitude();         // Provides base magnitude for getEditorSelectionOffset()

protected:
   static const F32 DamageReductionFactor;
   static const F32 DisabledLevel;

   F32 mHealth;
   Point mAnchorNormal;
   bool mIsDestroyed;
   S32 mOriginalTeam;

   bool mSnapped;             // Item is snapped to a wall

   S32 mHealRate;             // Rate at which items will heal themselves, defaults to 0;  Heals at 10% per mHealRate seconds.
   Timer mHealTimer;          // Timer for tracking mHealRate

   Vector<Point> mCollisionPolyPoints;    // Used on server, also used for rendering on client -- computed when item is added to game
   void computeObjectGeometry();          // Populates mCollisionPolyPoints

   void findMountPoint(Game *game, const Point &pos);     // Figure out where to mount this item during construction


   WallSegment *mMountSeg;    // Segment we're mounted to in the editor (don't care in the game)

   enum MaskBits
   {
      InitialMask   = Parent::FirstFreeMask << 0,
      HealthMask    = Parent::FirstFreeMask << 1,
      HealRateMask  = Parent::FirstFreeMask << 2,
      FirstFreeMask = Parent::FirstFreeMask << 3
   };

public:
   EngineeredItem(S32 team = TEAM_NEUTRAL, const Point &anchorPoint = Point(0,0), const Point &anchorNormal = Point(1,0));  // Constructor
   virtual ~EngineeredItem();                                                                                               // Destructor

   virtual bool processArguments(S32 argc, const char **argv, Game *game);

   virtual void onAddedToGame(Game *theGame);

   static bool checkDeploymentPosition(const Vector<Point> &thisBounds, const GridDatabase *gb);
   void onConstructed();

   virtual void onDestroyed();
   virtual void onDisabled();
   virtual void onEnabled();

   virtual Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;

   virtual void setPos(lua_State *L, S32 stackIndex);
   virtual void setPos(const Point &p);


#ifndef ZAP_DEDICATED
   Point getEditorSelectionOffset(F32 currentScale);
#endif

   bool isEnabled();    // True if still active, false otherwise

   void explode();
   bool isDestroyed();

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void setHealRate(S32 rate);
   S32 getHealRate() const;

   void damageObject(DamageInfo *damageInfo);
   void checkHealthBounds();
   bool collide(BfObject *hitObject);
   F32 getHealth() const;
   void healObject(S32 time);
   Point mountToWall(const Point &pos, const WallSegmentManager *wallSegmentManager, const Vector<S32> *excludedWallList);

   void onGeomChanged();

   void getBufferForBotZone(F32 bufferRadius, Vector<Point> &points) const;

   // Figure out where to put our turrets and forcefield projectors.  Will return NULL if no mount points found.
   static DatabaseObject *findAnchorPointAndNormal(GridDatabase *db, const Point &pos, F32 snapDist, 
                                                   const Vector<S32> *excludedWallList,
                                                   bool format, Point &anchor, Point &normal);

   // Pass NULL if there is no excludedWallList
   static DatabaseObject *findAnchorPointAndNormal(GridDatabase *db, const Point &pos, F32 snapDist, 
                                                   const Vector<S32> *excludedWallList,
                                                   bool format, TestFunc testFunc, Point &anchor, Point &normal);

   void setAnchorNormal(const Point &nrml);
   WallSegment *getMountSegment();
   void setMountSegment(WallSegment *mountSeg);

   // These methods are overriden in ForceFieldProjector
   virtual WallSegment *getEndSegment();
   virtual void setEndSegment(WallSegment *endSegment);

   //// Is item sufficiently snapped?  
   void setSnapped(bool snapped);
   bool isSnapped() const;


   /////
   // Editor stuff
   virtual string toLevelCode() const;
   virtual void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);

	///// Lua interface
	LUAW_DECLARE_CLASS(EngineeredItem);

	static const char *luaClassName;
	static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   // More Lua methods that are inherited by turrets and forcefield projectors
   S32 lua_getHealth(lua_State *L);
   S32 lua_setHealth(lua_State *L);
   S32 lua_isActive(lua_State *L);
   S32 lua_getMountAngle(lua_State *L);
   S32 lua_getDisabledThreshold(lua_State *L);
   S32 lua_getHealRate(lua_State *L);
   S32 lua_setHealRate(lua_State *L);
   S32 lua_setEngineered(lua_State *L);
   S32 lua_getEngineered(lua_State *L);

   // Some overrides
   S32 lua_setGeom(lua_State *L);
   S32 lua_setPos(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class ForceField : public BfObject
{
   typedef BfObject Parent;

private:
   Point mStart, mEnd;
   Vector<Point> mOutline;    

   Timer mDownTimer;
   bool mFieldUp;

   F32 mHealth;  // Different than ForceFieldProjector health

public:
   enum MaskBits {
      InitialMask   = Parent::FirstFreeMask << 0,
      StatusMask    = Parent::FirstFreeMask << 1,
      HealthMask    = Parent::FirstFreeMask << 2,
      FirstFreeMask = Parent::FirstFreeMask << 3
   };

   static const S32 FieldDownTime = 250;
   static const S32 MAX_FORCEFIELD_LENGTH = 2500;

   static const F32 ForceFieldHalfWidth;

   ForceField(S32 team = -1, Point start = Point(), Point end = Point());
   virtual ~ForceField();

   bool collide(BfObject *hitObject);
   bool intersects(ForceField *forceField);     // Return true if forcefields intersect
   void onAddedToGame(Game *theGame);
   void idle(BfObject::IdleCallPath path);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void setHealth(F32 health);
   void setEndPoints(const Point &start, const Point &end);
   void updateGeomAndExtents();

   const Vector<Point> *getCollisionPoly() const;

   const Vector<Point> *getOutline() const;

   static Vector<Point> computeGeom(const Point &start, const Point &end);
   static bool findForceFieldEnd(const GridDatabase *db, const Point &start, const Point &normal, 
                                 Point &end, DatabaseObject **collObj);

   void render();
   S32 getRenderSortValue();

   void getForceFieldStartAndEndPoints(Point &start, Point &end);

   TNL_DECLARE_CLASS(ForceField);
};


////////////////////////////////////////
////////////////////////////////////////

class ForceFieldProjector : public EngineeredItem
{
   typedef EngineeredItem Parent;

private:
   SafePtr<ForceField> mField;
   WallSegment *mForceFieldEndSegment;
   Point forceFieldEnd;

   void initialize();

   Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;  

   F32 getSelectionOffsetMagnitude();

public:
   static const S32 defaultRespawnTime = 0;

   explicit ForceFieldProjector(lua_State *L = NULL);                                     // Combined Lua / C++ default constructor
   ForceFieldProjector(S32 team, const Point &anchorPoint, const Point &anchorNormal);    // Constructor for when ffp is built with engineer
   virtual ~ForceFieldProjector();                                                        // Destructor

   ForceFieldProjector *clone() const;
   
   const Vector<Point> *getCollisionPoly() const;
   
   static Vector<Point> getForceFieldProjectorGeometry(const Point &anchor, const Point &normal);
   static Point getForceFieldStartPoint(const Point &anchor, const Point &normal);

   // Get info about the forcfield that might be projected from this projector
   void getForceFieldStartAndEndPoints(Point &start, Point &end);

   WallSegment *getEndSegment();
   void setEndSegment(WallSegment *endSegment);

   void onAddedToGame(Game *theGame);
   void idle(BfObject::IdleCallPath path);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void render();
   void onEnabled();
   void onDisabled();

   TNL_DECLARE_CLASS(ForceFieldProjector);

   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();
   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void renderDock();
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);

   void onGeomChanged();
   void findForceFieldEnd();                      // Find end of forcefield in editor

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(ForceFieldProjector);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getPos(lua_State *L);
   S32 lua_setPos(lua_State *L);
   S32 lua_setTeam(lua_State *L);
   S32 lua_removeFromGame(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class Turret : public EngineeredItem
{
   typedef EngineeredItem Parent;

private:
   Timer mFireTimer;
   F32 mCurrentAngle;

   void initialize();

   F32 getSelectionOffsetMagnitude();

#ifndef ZAP_DEDICATED
   static EditorAttributeMenuUI *mAttributeMenuUI; // Menu for attribute editing
#endif

public:
   explicit Turret(lua_State *L = NULL);                                   // Combined Lua / C++ default constructor
   Turret(S32 team, const Point &anchorPoint, const Point &anchorNormal);  // Constructor for when turret is built with engineer
   virtual ~Turret();                                                      // Destructor

   Turret *clone() const;

   WeaponType mWeaponFireType;

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toLevelCode() const;

   static const S32 defaultRespawnTime = 0;

   static const S32 TURRET_OFFSET = 15;               // Distance of the turret's render location from it's attachment location
                                                      // Also serves as radius of circle of turret's body, where the turret starts
   static const S32 TurretTurnRate = 4;               // How fast can turrets turn to aim?
   static const S32 TurretPerceptionDistance = 800;   // Area to search for potential targets...

   static const S32 AimMask = Parent::FirstFreeMask;


   Vector<Point> getObjectGeometry(const Point &anchor, const Point &normal) const;
   static Vector<Point> getTurretGeometry(const Point &anchor, const Point &normal);
   
   const Vector<Point> *getCollisionPoly() const;
   const Vector<Point> *getOutline() const;

   F32 getEditorRadius(F32 currentScale);

   void render();
   void idle(IdleCallPath path);
   void onAddedToGame(Game *theGame);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   TNL_DECLARE_CLASS(Turret);

   /////
   // Some properties about the item that will be needed in the editor
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *getAttributeMenu();
   void startEditingAttrs(EditorAttributeMenuUI *attributeMenu);
   void doneEditingAttrs(EditorAttributeMenuUI *attributeMenu);
   void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);
#endif

   bool hasTeam();
   bool canBeHostile();
   bool canBeNeutral();

   void onGeomChanged();

   void renderDock();
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);

   ///// Lua interface
	LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Turret);

	static const char *luaClassName;
	static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   // LuaItem methods
   S32 lua_getRad(lua_State *L);
   S32 lua_getPos(lua_State *L);
   S32 lua_getAimAngle(lua_State *L);
   S32 lua_setAimAngle(lua_State *L);
   S32 lua_setWeapon(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class EngineerModuleDeployer
{
private:
   Point mDeployPosition, mDeployNormal;
   string mErrorMessage;

public:
   // Check potential deployment position
   bool canCreateObjectAtLocation(const GridDatabase *database, const Ship *ship, U32 objectType);

   bool deployEngineeredItem(ClientInfo *clientInfo, U32 objectType);  // Deploy!
   string getErrorMessage();

   static bool findDeployPoint(const Ship *ship, U32 objectType, Point &deployPosition, Point &deployNormal);
   static string checkResourcesAndEnergy(const Ship *ship);
};


};
#endif

