//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef COREGAME_H_
#define COREGAME_H_

#include "gameType.h"   // Parent class for CoreGameType
#include "item.h"       // Parent class for CoreItem


namespace Zap {

// Forward Declarations
class CoreItem;
class ClientInfo;


//  Enum,  KeyString, Name,  Description
#define COREGAME_REDIST_TABLE \
   COREGAME_REDIST_ITEM(RedistNone,               "RedistNone",               "None",                        "Team is not redistributed") \
   COREGAME_REDIST_ITEM(RedistBalanced,           "RedistBalanced",           "Balanced",                    "Divide players amongst all teams, losers first") \
   COREGAME_REDIST_ITEM(RedistBalancedNonWinners, "RedistBalancedNonWinners", "Balanced, Non-Winning Teams", "Divide players amongst all but the winning team") \
   COREGAME_REDIST_ITEM(RedistRandom,             "RedistRandom",             "Random",                      "Randomly move players to remaining teams") \
   COREGAME_REDIST_ITEM(RedistLoser,              "RedistLoser",              "Losing Team",                 "Move players to team with fewest Cores") \
   COREGAME_REDIST_ITEM(RedistWinner,             "RedistWinner",             "Winning Team",                "Move players to team with most Cores") \
// End XMacro

class CoreGameType : public GameType
{
   typedef GameType Parent;

public:
   // What method should be used to redistribute players if they lost all their
   // Cores
   enum RedistMethod
   {
#  define COREGAME_REDIST_ITEM(enumValue, b, c, d) enumValue,
      COREGAME_REDIST_TABLE
#  undef COREGAME_REDIST_ITEM
      RedistCount   // Always last
   };

private:
   Vector<SafePtr<CoreItem> > mCores;

   RedistMethod mRedistMethod;

public:
   static const S32 DestroyedCoreScore = 1;

   CoreGameType();            // Constructor
   virtual ~CoreGameType();   // Destructor

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toLevelCode() const;

   bool isTeamCoreBeingAttacked(S32 teamIndex) const;

   // Runs on client
   void renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const;

   void addCore(CoreItem *core, S32 team);
   void removeCore(CoreItem *core);

   // What does aparticular scoring event score?
   void updateScore(ClientInfo *player, S32 team, ScoringEvent event, S32 data = 0);
   S32 getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data);
   void score(ClientInfo *destroyer, S32 coreOwningTeam, S32 score);

   void setRedistMethod(RedistMethod method);
   RedistMethod getRedistMethod();
   void handleRedistribution(Vector<ClientInfo*> &players);

   void handleNewClient(ClientInfo *clientInfo);

#ifndef ZAP_DEDICATED
   Vector<string> getGameParameterMenuKeys();
   shared_ptr<MenuItem> getMenuItem(const string &key);
   bool saveMenuItem(const MenuItem *menuItem, const string &key);
   void renderScoreboardOrnament(S32 teamIndex, S32 xpos, S32 ypos) const;
#endif

   GameTypeId getGameTypeId() const;
   const char *getGameTypeName() const;
   const char *getShortName() const;
   const char **getInstructionString() const;
   HelpItem getGameStartInlineHelpItem() const;
   bool canBeTeamGame() const;
   bool canBeIndividualGame() const;


   TNL_DECLARE_CLASS(CoreGameType);
};


////////////////////////////////////////
////////////////////////////////////////

static const S32 CORE_PANELS = 10;     // Note that changing this will require update of all clients, and a new CS_PROTOCOL_VERSION

struct PanelGeom 
{
   Point vert[CORE_PANELS];            // Panel 0 stretches from vert 0 to vert 1
   Point mid[CORE_PANELS];             // Midpoint of Panel 0 is mid[0]
   Point repair[CORE_PANELS];
   F32 angle;
   bool isValid;

   Point getStart(S32 i) { return vert[i % CORE_PANELS]; }
   Point getEnd(S32 i)   { return vert[(i + 1) % CORE_PANELS]; }
};


////////////////////////////////////////
////////////////////////////////////////

class EditorAttributeMenuUI;

class CoreItem : public Item
{

typedef Item Parent;

public:
   static const F32 PANEL_ANGLE;                         // = FloatTau / (F32) CoreItem::CORE_PANELS;
   static const F32 DamageReductionRatio;
   static const U32 CoreRadius = 100;
   static const U32 CoreDefaultStartingHealth = 40;      // In ship-damage equivalents; these will be divided amongst all panels
   static const U32 CoreDefaultRotationSpeed;
   static const U32 CoreMaxRotationSpeed;

private:
   static const U32 CoreMinWidth = 20;
   static const U32 CoreHeartbeatStartInterval = 2000;   // In milliseconds
   static const U32 CoreHeartbeatMinInterval = 500;
   static const U32 CoreAttackedWarningDuration = 600;
   static const U32 CoreRotationTimeDefault = 16384;     // In milliseconds, must be power of 2
   static const U32 ExplosionInterval = 600;
   static const U32 ExplosionCount = 3;

   U32 mCurrentExplosionNumber;
   PanelGeom mPanelGeom;

   bool mHasExploded;
   bool mBeingAttacked;
   F32 mStartingHealth;          // Health stored in the level file, will be divided amongst panels
   F32 mStartingPanelHealth;     // Health divided up amongst panels

   F32 mPanelHealth[CORE_PANELS];
   Timer mHeartbeatTimer;        // Client-side timer
   Timer mExplosionTimer;        // Client-side timer
   Timer mAttackedWarningTimer;  // Server-side timer
   U32 mRotationSpeed;           // 4 bits are used, values 0-15 will work

protected:
   enum MaskBits {
      PanelDamagedMask = Parent::FirstFreeMask << 0,  // each bit mask have own panel updates (PanelDamagedMask << n)
      PanelDamagedAllMask = ((1 << CORE_PANELS) - 1) * PanelDamagedMask,  // all bits of PanelDamagedMask
      FirstFreeMask   = Parent::FirstFreeMask << CORE_PANELS
   };

public:
   explicit CoreItem(lua_State *L = NULL);   // Combined Lua / C++ default constructor
   virtual ~CoreItem();                      // Destructor
   CoreItem *clone() const;

   static F32 getCoreAngle(U32 time);
   void renderItem(const Point &pos);
   bool shouldRender() const;

   const Vector<Point> *getCollisionPoly() const;
   bool getCollisionCircle(U32 state, Point &center, F32 &radius) const;
   bool collide(BfObject *otherObject);

   bool isBeingAttacked();

   void setStartingHealth(F32 health);
   F32 getStartingHealth() const;
   F32 getTotalCurrentHealth() const;     // Returns total current health of all panels
   F32 getHealth() const;                 // Returns overall current health of item as a ratio between 0 and 1
   bool isPanelDamaged(S32 panelIndex);
   bool isPanelInRepairRange(const Point &origin, S32 panelIndex);

   U32 getRotationSpeed() const;
   void setRotationSpeed(U32 speed);

   Vector<Point> getRepairLocations(const Point &repairOrigin);
   PanelGeom *getPanelGeom();
   static void fillPanelGeom(const Point &pos, U32 time, PanelGeom &panelGeom);

   void getBufferForBotZone(F32 bufferRadius, Vector<Point> &outputPoly) const;


   void onAddedToGame(Game *theGame);
#ifndef ZAP_DEDICATED
   void onGeomChanged();
#endif

   void damageObject(DamageInfo *theInfo);
   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);

#ifndef ZAP_DEDICATED
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void onItemExploded(Point pos);
   void doExplosion(const Point &pos);
   void doPanelDebris(S32 panelIndex);
#endif

   void idle(BfObject::IdleCallPath path);

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toLevelCode() const;

   TNL_DECLARE_CLASS(CoreItem);

   void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);

   ///// Editor methods
   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   F32 getEditorRadius(F32 currentScale);
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);    
   void renderDock();

   bool canBeHostile();
   bool canBeNeutral();

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(CoreItem);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getCurrentHealth(lua_State *L);    // Current health = FullHealth - damage sustained
   S32 lua_getFullHealth(lua_State *L);       // Health with no damange
   S32 lua_setFullHealth(lua_State *L);     
   S32 lua_setTeam(lua_State *L);

   S32 lua_getRotationSpeed(lua_State *L);
   S32 lua_setRotationSpeed(lua_State *L);
};



} /* namespace Zap */
#endif /* COREGAME_H_ */
