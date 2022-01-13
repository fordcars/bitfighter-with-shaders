//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------


#ifndef _SPAWN_H_
#define _SPAWN_H_

#include "PointObject.h"     // For PointObject def
#include "Timer.h"


namespace Zap
{

class EditorAttributeMenuUI;     // Needed in case class def hasn't been included in dedicated build

// Parent class for spawns that generate items
class AbstractSpawn : public PointObject
{
   typedef PointObject Parent;

private:
   static EditorAttributeMenuUI *mAttributeMenuUI;

protected:
   S32 mSpawnTime;
   Timer mTimer;

   enum MaskBits {
      InitialMask     = Parent::FirstFreeMask << 0,
   };

   virtual void setRespawnTime(S32 time);

public:
   AbstractSpawn(const Point &pos = Point(), S32 time = 0); // Constructor
   AbstractSpawn(const AbstractSpawn &copy);                // Copy constructor
   virtual ~AbstractSpawn();                                // Destructor
   
   virtual bool processArguments(S32 argc, const char **argv, Game *game);

   virtual void fillAttributesVectors(Vector<string> &keys, Vector<string> &values);

   virtual const char *getClassName() const = 0;

   virtual S32 getDefaultRespawnTime() = 0;

   void setSpawnTime(S32 spawnTime);
   S32 getSpawnTime() const;

   virtual string toLevelCode() const;

   F32 getRadius();
   F32 getEditorRadius(F32 currentScale);

   bool updateTimer(U32 deltaT);
   void resetTimer();
   U32 getPeriod();     // temp debugging

   virtual void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false) = 0;
   virtual void renderDock() = 0;
};


////////////////////////////////////////
////////////////////////////////////////

class Spawn : public AbstractSpawn
{
   typedef AbstractSpawn Parent;

private:
   void initialize();

public:
   Spawn(const Point &pos = Point(0,0));  // C++ constructor
   explicit Spawn(lua_State *L);          // Lua constructor
   virtual ~Spawn();                      // Destructor

   Spawn *clone() const;

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   string toLevelCode() const;
   bool processArguments(S32 argc, const char **argv, Game *game);

   S32 getDefaultRespawnTime();    // Somewhat meaningless in this context

   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);
   void renderDock();

   TNL_DECLARE_CLASS(Spawn);

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(Spawn);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

// Class of spawns that spawn items, rather than places ships might appear
class ItemSpawn : public AbstractSpawn
{
   typedef AbstractSpawn Parent;

public:
   ItemSpawn(const Point &pos, S32 time);    // C++ constructor
   virtual ~ItemSpawn();                     // Destructor

   virtual void spawn();                     // All ItemSpawns will use this to spawn things
   void onAddedToGame(Game *game);
   void idle(IdleCallPath path);

   // These methods exist solely to make ItemSpawn instantiable so it can be instantiated by Lua... even though it never will
   virtual const char *getClassName() const;
   virtual S32 getDefaultRespawnTime();
   virtual void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);
   virtual void renderDock();


   ///// Lua interface
   LUAW_DECLARE_ABSTRACT_CLASS(ItemSpawn);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

   S32 lua_getSpawnTime(lua_State *L);
   S32 lua_setSpawnTime(lua_State *L);
   S32 lua_spawnNow(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////

class AsteroidSpawn : public ItemSpawn    
{
   typedef ItemSpawn Parent;

private:
   void initialize();

   S32 mAsteroidSize;

public:
   static const S32 DEFAULT_RESPAWN_TIME = 30;    // in seconds

   AsteroidSpawn(const Point &pos = Point(), S32 time = DEFAULT_RESPAWN_TIME);  // C++ constructor
   explicit AsteroidSpawn(lua_State *L);                                        // Lua constructor
   virtual ~AsteroidSpawn();

   virtual void onGhostAvailable(GhostConnection *theConnection);

   AsteroidSpawn *clone() const;

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   S32 getDefaultRespawnTime();

   virtual void setRespawnTime(S32 spawnTime);
   void spawn();

   S32  getAsteroidSize();
   void setAsteroidSize(S32 asteroidSize);

   U32 packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream);
   void unpackUpdate(GhostConnection *connection, BitStream *stream);

   void renderLayer(S32 layerIndex);
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);
   void renderDock();

   bool processArguments(S32 argc, const char** argv, Game* game);
   string toLevelCode() const;

   TNL_DECLARE_CLASS(AsteroidSpawn);
   TNL_DECLARE_RPC(s2cSetTimeUntilSpawn, (S32 millis));


   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(AsteroidSpawn);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

static const S32 TeamNotSpecified = -99999;

class FlagSpawn : public ItemSpawn
{
   typedef ItemSpawn Parent;

private:
   void initialize();

public:
   TNL_DECLARE_CLASS(FlagSpawn);

   static const S32 DEFAULT_RESPAWN_TIME = 30;    // in seconds

   FlagSpawn(const Point &pos = Point(), S32 time = DEFAULT_RESPAWN_TIME, S32 team = TeamNotSpecified);  // C++ constructor
   explicit FlagSpawn(lua_State *L);                                                                     // Lua constructor
   virtual ~FlagSpawn();                                                                                 // Destructor

   FlagSpawn *clone() const;

   bool updateTimer(S32 deltaT);
   void resetTimer();

   void spawn();

   const char *getEditorHelpString();
   const char *getPrettyNamePlural();
   const char *getOnDockName();
   const char *getOnScreenName();

   const char *getClassName() const;

   S32 getDefaultRespawnTime();

   //void spawn(Game *game);
   void renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);
   void renderDock();

   bool processArguments(S32 argc, const char **argv, Game *game);
   string toLevelCode() const;

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(FlagSpawn);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


};    // namespace


#endif
