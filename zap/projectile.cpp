//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "projectile.h"
#include "ship.h"
#include "game.h"
#include "gameConnection.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#endif

#include "Colors.h"
#include "gameObjectRender.h"

#include "stringUtils.h"
#include "MathUtils.h"


namespace Zap 
{

using namespace LuaArgs;


TNL_IMPLEMENT_NETOBJECT(Projectile);

// Constructor -- used when weapon is fired  
Projectile::Projectile(WeaponType type, const Point &pos, const Point &vel, BfObject *shooter)
{
   initialize(type, pos, vel, shooter);
}

/**
 * @luafunc Projectile::Projectile()
 * @luafunc Projectile::Projectile(Weapon weap)
 */
// Combined Lua / C++ default constructor -- only used in Lua at the moment
Projectile::Projectile(lua_State *L)
{
   WeaponType type = WeaponPhaser;
   if(L)
   {
      // These are the signatures we'll accept for Lua to construct this object
      static LuaFunctionArgList constructorArgList = { {{ END }, { WEAP_ENUM, END }}, 2 };

      if(checkArgList(L, constructorArgList, "Projectile", "constructor") == 1)
      {
         WeaponType newType = getWeaponType(L, 1);

         // Only allow projectile types that use this class
         if(WeaponInfo::getWeaponInfo(newType).projectileStyle != ProjectileStyleNotAProjectile)
            type = newType;
      }
   }

   initialize(type, Point(0,0), Point(0,0), NULL);
}


void Projectile::initialize(WeaponType type, const Point &pos, const Point &vel, BfObject *shooter)
{
   mObjectTypeNumber = BulletTypeNumber;
   setNewGeometry(geomPoint, getRadius());

   mNetFlags.set(Ghostable);
   setPos(pos);
   mVelocity = vel;

   mTimeRemaining = WeaponInfo::getWeaponInfo(type).projLiveTime;
   mCollided = false;
   hitShip = false;
   mAlive = true;
   mBounced = false;
   mLiveTimeIncreases = 0;
   mShooter = shooter;
   mLastHitObject = NULL;

   setOwner(NULL);

   // Copy some attributes from the shooter
   if(shooter)
   {
      if(isShipType(shooter->getObjectTypeNumber()))
      {
         Ship *ship = static_cast<Ship *>(shooter);
         setOwner(ship->getClientInfo());    // Else owner remains NULL
      }

      setTeam(shooter->getTeam());
      mKillString = shooter->getKillString();
   }

   // Set default projectile style
   mStyle = WeaponInfo::getWeaponInfo(type).projectileStyle;

   // If a turret, keep a coherent style
   if(shooter && shooter->getObjectTypeNumber() == TurretTypeNumber)
      mStyle = ProjectileStyleTurret;  // Forces Triple to use Turret style

   mWeaponType = type;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
Projectile::~Projectile()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


U32 Projectile::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & PositionMask))
   {
      ((GameConnection *) connection)->writeCompressedPoint(getPos(), stream);
      writeCompressedVelocity(mVelocity, COMPRESSED_VELOCITY_MAX, stream);
   }

   if(stream->writeFlag(updateMask & InitialMask))
   {
      stream->writeEnum(mStyle, ProjectileStyleCount);

      S32 index = -1;
      if(mShooter.isValid())
         index = connection->getGhostIndex(mShooter);
      if(stream->writeFlag(index != -1))
         stream->writeInt(index, GhostConnection::GhostIdBitSize);
   }

   stream->writeFlag(mCollided);
   if(mCollided)
      stream->writeFlag(hitShip);
   stream->writeFlag(mAlive);

   return 0;
}


void Projectile::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   if(stream->readFlag())  // Read position, for correcting bouncers, needs to be before inital for getGame()->playSoundEffect
   {
      static Point pos;    // Reusable container
      ((GameConnection *) connection)->readCompressedPoint(pos, stream);
      setPos(pos);

      readCompressedVelocity(mVelocity, COMPRESSED_VELOCITY_MAX, stream);
   }

   if(stream->readFlag())         // Initial chunk of data, sent once for this object
   {

      mStyle = (ProjectileStyle) stream->readEnum(ProjectileStyleCount);

      TNLAssert(connection, "Defunct connection to server in projectile.cpp!");

      if(stream->readFlag())
         mShooter = static_cast<Ship *>(connection->resolveGhost(stream->readInt(GhostConnection::GhostIdBitSize)));

      setExtent(Rect(getPos(), 0));
      initial = true;
      getGame()->playSoundEffect(GameWeapon::projectileInfo[mStyle].projectileSound, getPos(), mVelocity);
   }
   bool preCollided = mCollided;
   mCollided = stream->readFlag();
   
   if(mCollided)
      hitShip = stream->readFlag();

   mAlive = stream->readFlag();

   if(!preCollided && mCollided)     // Projectile has "become" collided
      explode(NULL, getPos());

   if(!mCollided && initial)
      mCurrentMove.time = U32(connection->getOneWayTime());
}


// The projectile has collided with hitObject at collisionPoint
void Projectile::handleCollision(BfObject *hitObject, Point collisionPoint)
{
   mCollided = true;

   if(isShipType(hitObject->getObjectTypeNumber()))
      hitShip = true;

   if(!isGhost())    // If we're on the server, that is
   {
      DamageType damageType = DamageTypePoint;
      Point impulseVector = mVelocity;

      if(mWeaponType == WeaponRailgun)
      {
         static const F32 RAILGUN_IMPULSE_MAGNITUDE = 700.0f;
         damageType = DamageTypeVector;
         if(hitShip)
            impulseVector.normalize(RAILGUN_IMPULSE_MAGNITUDE);
      }

      DamageInfo damageInfo;

      damageInfo.collisionPoint       = collisionPoint;
      damageInfo.damageAmount         = WeaponInfo::getWeaponInfo(mWeaponType).damageAmount;
      damageInfo.damageType           = damageType;
      damageInfo.impulseVector        = impulseVector;
      damageInfo.damageSelfMultiplier = WeaponInfo::getWeaponInfo(mWeaponType).damageSelfMultiplier;
      damageInfo.damagingObject       = this;


      hitObject->damageObject(&damageInfo);

      // Log the shot to the shooter's stats
      Ship *shooter = NULL;
      if(mShooter.getPointer() != NULL && isShipType(mShooter.getPointer()->getObjectTypeNumber()))
         shooter = static_cast<Ship *>(mShooter.getPointer());

      if(hitShip && shooter && shooter->getClientInfo())
         shooter->getClientInfo()->getStatistics()->countHit(mWeaponType);
   }

   // Client and server:

   // Railgun goes right through ships and keeps going
   if((mWeaponType == WeaponRailgun) && hitShip)
      ; // Keep time remaining
   else
      mTimeRemaining = 0;

   explode(hitObject, collisionPoint);
}

void Projectile::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}

void Projectile::idle(BfObject::IdleCallPath path)
{
   U32 deltaT = mCurrentMove.time;

   if(mAlive)
   {
      U32 objAge = getGame()->getCurrentTime() - getCreationTime();  // Age of object, in ms
      F32 timeLeft = (F32)deltaT;
      S32 loopcount = 32;

      Point startPos, collisionPoint;

      while(timeLeft > 0.01f && loopcount != 0)    // This loop is to prevent slow bounce on low frame rate / high time left
      {
         loopcount--;
         
         startPos = getPos();

         // Calculate where projectile will be at the end of the current interval
         Point endPos = startPos + (mVelocity * .001f) * timeLeft;    // mVelocity in units/sec, timeLeft in ms

         // Check for collision along projected route of movement
         static Vector<BfObject *> disabledList;

         Rect queryRect(startPos, endPos);     // Bounding box of our travels

         disabledList.clear();


         // Don't collide with shooter during first 500ms of life
         if(mShooter.isValid() && objAge < 500 && !mBounced)
         {
            disabledList.push_back(mShooter);
            mShooter->disableCollision();
         }

         BfObject *hitObject;

         F32 collisionTime;
         Point surfNormal;

         // Do the search
         while(true)  
         {
            hitObject = findObjectLOS((TestFunc)isWeaponCollideableType, RenderState, startPos, endPos, collisionTime, surfNormal);

            if((!hitObject || hitObject->collide(this)))
               break;

            // Disable collisions with things that don't want to be
            // collided with (i.e. whose collide methods return false)
            disabledList.push_back(hitObject);
            hitObject->disableCollision();
         }

         // Re-enable collison flag for ship and items in our path that don't want to be collided with
         // Note that if we hit an object that does want to be collided with, it won't be in disabledList
         // and thus collisions will not have been disabled, and thus don't need to be re-enabled.
         // Our collision detection is done, and hitObject contains the first thing that the projectile hit.
         for(S32 i = 0; i < disabledList.size(); i++)
            disabledList[i]->enableCollision();

         // This logic lets the Railgun go through ships. It assumes that the
         // object search will return the same order of objects during this time frame
         //
         // If we already hit this object, don't do it again
         if(hitObject == mLastHitObject)
            hitObject = NULL;
         // Otherwise save the one we found for next iteration
         else
            mLastHitObject = hitObject;


         if(hitObject)  // Hit something...  should we bounce?
         {
            bool bounce = false;
            bool hitAShip = isShipType(hitObject->getObjectTypeNumber());

            // Bounce off a wall and off a ship that has its shields up
            if(mStyle == ProjectileStyleBouncer && isWallType(hitObject->getObjectTypeNumber()))
               bounce = true;
            else if(hitAShip)
            {
               Ship *ship = static_cast<Ship *>(hitObject);
               if(ship->isModulePrimaryActive(ModuleShield))
                  bounce = true;
            }

            if(bounce)
            {
               mBounced = true;

               static const U32 MAX_LIVETIME_INCREASES = 6;
               static const U32 LIVETIME_INCREASE = 250;

               // Let's extend the projectile life time on each bounce, up to twice the normal
               // live-time
               if(mLiveTimeIncreases < MAX_LIVETIME_INCREASES &&
                     (S32)mTimeRemaining < WeaponInfo::getWeaponInfo(mWeaponType).projLiveTime)
               {
                  mTimeRemaining += LIVETIME_INCREASE;
                  mLiveTimeIncreases++;
               }

               // We hit something that we should bounce from, so bounce!
               F32 float1 = surfNormal.dot(mVelocity) * 2;
               mVelocity -= surfNormal * float1;

               if(float1 > 0)
                  surfNormal = -surfNormal;      // This is to fix going through polygon barriers

               startPos = getPos();
               collisionPoint = startPos + (endPos - startPos) * collisionTime;

               setPos(collisionPoint + surfNormal);
               timeLeft = timeLeft * (1 - collisionTime);

               if(hitObject->isMoveObject())
               {
                  MoveObject *obj = static_cast<MoveObject *>(hitObject);  

                  startPos = getPos();

                  float1 = startPos.distanceTo(obj->getRenderPos());
                  if(float1 < obj->getRadius())
                  {
                     float1 = obj->getRadius() * 1.01f / float1;
                     setVert(startPos * float1 + obj->getRenderPos() * (1 - float1), 0);  // Fix bouncy stuck inside shielded ship
                  }
               }

               // Bouncing off anything can easily get desync'd
               setMaskBits(PositionMask);

               if(isGhost())
                  getGame()->playSoundEffect(SFXBounceShield, collisionPoint, surfNormal * surfNormal.dot(mVelocity) * 2);
            }
            else  // Not bouncing
            {
               // Since we didn't bounce, advance to location of collision
               startPos = getPos();
               collisionPoint = startPos + (endPos - startPos) * collisionTime;
               handleCollision(hitObject, collisionPoint);     // What we hit, where we hit it

               // Advance the railgun through ships
               if((mWeaponType == WeaponRailgun) && hitAShip)
                  setPos(endPos);

               timeLeft = 0;
            }
         }
         else        // Hit nothing, advance projectile to endPos
         {
            timeLeft = 0;

            setPos(endPos);
         }
      }
   }


#ifndef ZAP_DEDICATED
   // Draw trail for Railgun
   if(mStyle == ProjectileStyleRailgun)
   {
      mTrail.idle(deltaT);

      // Start a little behind the projectile
      Point offsetPos = mVelocity;
      offsetPos.normalize(10.0);
      Point updatePos = getPos() - offsetPos;

      mTrail.update(updatePos, UI::RailgunProfile);
   }
#endif

   // Kill old projectiles
   if(mAlive && path == BfObject::ServerIdleMainLoop)
   {
      if(mTimeRemaining > deltaT)
         mTimeRemaining -= deltaT;     // Decrement time left to live
      else
      {
         deleteObject(500);
         mTimeRemaining = 0;
         mAlive = false;
         setMaskBits(ExplodedMask);
      }
   }
}


F32 Projectile::getRadius()
{
   return 10;     // Or so...  currently only used for inserting objects into database and for Lua on the odd chance someone asks
}


// Gets run when projectile suffers damage, like from a burst going off
void Projectile::damageObject(DamageInfo *info)
{
   mTimeRemaining = 0;     // This will kill projectile --> remove this to have projectiles unaffected
}


void Projectile::explode(BfObject *hitObject, Point pos)
{
#ifndef ZAP_DEDICATED
   // Do some particle spew...
   if(isGhost())
   {
      TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");
      static_cast<ClientGame *>(getGame())->emitExplosion(pos, 0.3f, GameWeapon::projectileInfo[mStyle].sparkColors, NumSparkColors);

      SFXProfiles sound;

      bool isShip = hitObject && isShipType(hitObject->getObjectTypeNumber());

      Ship *ship = NULL;
      if(isShip)
         ship = static_cast<Ship *>(hitObject);

      if(ship && ship->isModulePrimaryActive(ModuleShield))
         sound = SFXBounceShield;
      else if((hitShip || ship))                           // We hit a ship with shields down
      {
         // Railgun goes through ships, give it a different sound
         if(mStyle == ProjectileStyleRailgun)  // mWeaponType doesn't work here for some reason (only server side?)
            sound = GameWeapon::projectileInfo[mStyle].impactSound;
         else
            sound = SFXShipHit;
      }
      else                                                   // We hit something else
         sound = GameWeapon::projectileInfo[mStyle].impactSound;

      getGame()->playSoundEffect(sound, pos, mVelocity);   // Play the sound
   }
#endif
}

BfObject *Projectile::getShooter() const {return mShooter; }


Point Projectile::getRenderVel() const { return mVelocity; }
Point Projectile::getActualVel() const { return mVelocity; }


// TODO: Get rid of this! (currently won't render without it)
void Projectile::render()
{
   renderItem(getPos());
}


bool Projectile::canAddToEditor() { return false; }      // No projectiles in the editor


void Projectile::renderItem(const Point &pos)
{
   if(shouldRender())
   {
      U32 time = getGame()->getCurrentTime() - getCreationTime();

      if(mStyle == ProjectileStyleRailgun)
         renderProjectileRailgun(pos, mVelocity, time);
      else
         renderProjectile(pos, mStyle, time);
   }
}


bool Projectile::shouldRender() const
{
   return mAlive;
}



//// Lua methods
/**
 * @luaclass Projectile
 *
 * @brief Bullet or missile object.
 *
 * @descr Projectile represents most bullets or missile objects in Bitfighter. 
 */
//               Fn name    Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getRad,    ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getWeapon, ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, getVel,    ARRAYDEF({{ END }}), 1 ) \
   METHOD(CLASS, setVel,    ARRAYDEF({{ PT,  END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(Projectile, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Projectile, LUA_METHODS);

#undef LUA_METHODS


const char *Projectile::luaClassName = "Projectile";
REGISTER_LUA_SUBCLASS(Projectile, BfObject);

/**
 * @luafunc num Projectile::getRad()
 *
 * @brief Returns the radius of the projectile. In the current version of
 * Bitfighter, this may not be accurate.
 *
 * @return The radius of the projectile.
 */
S32 Projectile::lua_getRad(lua_State *L)
{ 
   return returnFloat(L, getRadius());
} 


/**
 * @luafunc point Projectile::getVel()
 *
 * @return A point representing the projectile's velocity.
 */
S32 Projectile::lua_getVel(lua_State *L)
{ 
   return returnPoint(L, getActualVel()); 
}


/**
 * @luafunc WeaponEnum Projectile::getWeapon()
 *
 * @brief Returns the index of the weapon used to fire the projectile. See
 * the \ref WeaponEnum enum for valid values. 
 *
 * @return The index of the weapon used to fire the projectile.
 */
S32 Projectile::lua_getWeapon(lua_State *L)
{ 
   return returnWeaponType(L, mWeaponType);
}


/**
 * @luafunc Projectile::setVel(point vel)
 *
 * @brief Sets the projectile's velocity as a point.
 *
 * @param vel A point representing velocity.
 */
S32 Projectile::lua_setVel(lua_State *L)
{
   checkArgList(L, functionArgs, "Projectile", "setVel");
   mVelocity = getPointOrXY(L, 1);

   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Burst);

// Constructor -- used when burst is fired
Burst::Burst(const Point &pos, const Point &vel, BfObject *shooter, F32 radius) : MoveItem(pos, true, radius, BurstMass)
{
   initialize(pos, vel, shooter);
}


// Combined Lua / C++ default constructor -- used in Lua only at the moment
Burst::Burst(lua_State *L)
{
   initialize(Point(0,0), Point(0,0), NULL);
}


void Burst::initialize(const Point &pos, const Point &vel, BfObject *shooter)
{
   mObjectTypeNumber = BurstTypeNumber;
   mWeaponType = WeaponBurst;
   mStyle = BurstStyleNormal;

   mNetFlags.set(Ghostable);

   setActualPos(pos);
   setActualVel(vel);

   updateExtentInDatabase();

   mTimeRemaining = WeaponInfo::getWeaponInfo(WeaponBurst).projLiveTime;
   mExploded = false;

   if(!shooter)
   {
      setTeam(TEAM_HOSTILE);    // Hostile to all, as loose projectiles generally are!
      setOwner(NULL);
   }
   else
   {
      setOwner(shooter->getOwner());
      setTeam(shooter->getTeam());
      mShooter = shooter;
      mKillString = shooter->getKillString();

      // Change style of Burst if a Turret fired it
      if(mShooter->getObjectTypeNumber() == TurretTypeNumber)
         mStyle = BurstStyleTurret;
   }

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
Burst::~Burst()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


// Runs on client and server
void Burst::idle(IdleCallPath path)
{
   bool collisionDisabled = false;
   GameConnection *gc = NULL;

#ifndef ZAP_DEDICATED
   if(isGhost())   // Fix effect of ship getting ahead of burst on laggy client  
   {
      U32 objAge = getGame()->getCurrentTime() - getCreationTime();  // Age of object, in ms

      ClientGame *clientGame = static_cast<ClientGame *>(getGame());
      gc = clientGame->getConnectionToServer();

      collisionDisabled = objAge < 250 && gc && gc->getControlObject();

      if(collisionDisabled) 
         gc->getControlObject()->disableCollision();
   }
#endif

   Parent::idle(path);

   if(collisionDisabled) 
      gc->getControlObject()->enableCollision();

   // Do some drag...  no, not that kind of drag!
   setActualVel(getActualVel() - getActualVel() * (((F32)mCurrentMove.time) / 1000.f));

   if(isGhost())       // Here on down is server only
      return;

   if(!mExploded)
      if(getActualVel().lenSquared() < sq(4.0))
         explode(getActualPos());

   // Update TTL
   S32 deltaT = mCurrentMove.time;
   if(path == ClientIdlingNotLocalShip)
      mTimeRemaining += deltaT;
   else if(!mExploded)
   {
      if(mTimeRemaining <= deltaT)
        explode(getActualPos());
      else
         mTimeRemaining -= deltaT;
   }
}


U32 Burst::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   stream->writeFlag(mExploded);
   if(stream->writeFlag((updateMask & InitialMask) && (getGame()->getCurrentTime() - getCreationTime() < 500)))
      stream->writeEnum(mStyle, BurstStyleCount);

   return ret;
}


void Burst::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   TNLAssert(connection, "Invalid connection to server in Burst//projectile.cpp");

   if(stream->readFlag())
      doExplosion(getActualPos());

   if(stream->readFlag())  // InitialMask
   {
      mStyle = (BurstStyle) stream->readEnum(BurstStyleCount);

      getGame()->playSoundEffect(SFXBurst, getActualPos(), getActualVel());
   }
}


bool Burst::collided(BfObject *hitObject, U32 stateIndex)
{
   if(isGhost())
      return false;

   // If a burst hits a ship (or turret, or ff proj), it should explode immediately.  But we can't have 
   // it explode on contact with the shooter because the way a burst shows down, it is really quite 
   // difficult to use without injuring oneself.  So we must make an exception for the shooter.
   if(isWithHealthType(hitObject->getObjectTypeNumber()) && mShooter != hitObject)
   {
      explode(getActualPos());
      return true;
   }

   return false;
}


void Burst::damageObject(DamageInfo *damageInfo)
{
   // If we're being damaged by another burst, explode...
   if(damageInfo->damageType == DamageTypeArea)
   {
      explode(getActualPos());
      return;
   }

   computeImpulseDirection(damageInfo);

   setMaskBits(PositionMask);
}


void Burst::doExplosion(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(isGhost())
   {
      TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");
      //static_cast<ClientGame *>(getGame())->emitExplosion(getRenderPos(), 0.5, GameWeapon::projectileInfo[ProjectilePhaser].sparkColors, NumSparkColors);      // Original, nancy explosion
      static_cast<ClientGame *>(getGame())->emitBlast(pos, OuterBlastRadius);          // New, manly explosion

      getGame()->playSoundEffect(SFXMineExplode, getActualPos());
   }
#endif
}


// Also used for mines and spybugs
// Server only
void Burst::explode(const Point &pos)
{
   if(mExploded) 
      return;

   // Must set exploded to true immediately here or we risk a stack overflow when two
   // bursts hit each other and call radiusDamage on each other over and over
   mExploded = true;
   setMaskBits(ExplodedMask);

   DamageInfo damageInfo;
   damageInfo.collisionPoint       = pos;    // Location of burst at time of explosion
   damageInfo.damagingObject       = this;
   damageInfo.damageAmount         = WeaponInfo::getWeaponInfo(mWeaponType).damageAmount;
   damageInfo.damageType           = DamageTypeArea;
   damageInfo.damageSelfMultiplier = WeaponInfo::getWeaponInfo(mWeaponType).damageSelfMultiplier;

   S32 hits = radiusDamage(pos, InnerBlastRadius, OuterBlastRadius, (TestFunc)isRadiusDamageAffectableType, damageInfo);

   if(getOwner())
      for(S32 i = 0; i < hits; i++)
         getOwner()->getStatistics()->countHit(mWeaponType);

   disableCollision();
   deleteObject(100);
}


bool Burst::collide(BfObject *otherObj)
{
   return true;
}


BfObject *Burst::getShooter() const {return mShooter; }

bool Burst::canAddToEditor() { return false; }      // No bursts in the editor



void Burst::renderItem(const Point &pos)
{
   if(!shouldRender())
      return;

   F32 initTTL = (F32) WeaponInfo::getWeaponInfo(WeaponBurst).projLiveTime;

   renderGrenade( pos, mStyle, (initTTL - (F32) (getGame()->getCurrentTime() - getCreationTime())) / initTTL);
}


bool Burst::shouldRender() const
{
   return !mExploded;
}


/////
// Lua interface
/**
 * @luafunc Burst::Burst()
 * @luaclass Burst
 * 
 * @brief Grenade-like exploding object.
 * 
 * @descr Note that Bursts explode when their velocity is too low. Be sure to
 * set the Burst's velocity if you don't want it to explode immediately after it
 * is created. 
 */
//               Fn name    Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getWeapon, ARRAYDEF({{ END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(Burst, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Burst, LUA_METHODS);

#undef LUA_METHODS


const char *Burst::luaClassName = "Burst";
REGISTER_LUA_SUBCLASS(Burst, MoveObject);


S32 Burst::lua_getWeapon(lua_State *L) { return returnWeaponType(L, mWeaponType); }


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Mine);


const U32 Mine::FuseDelay = 100;
const S32 Mine::SensorRadius = 50;


// Constructor -- used when mine is planted
Mine::Mine(const Point &pos, BfObject *planter) : Burst(pos, Point(0,0), planter, BurstRadius)
{
   initialize(pos);
}

/**
 * @luafunc Mine::Mine()
 * @luafunc Mine::Mine(point pos)
 */
// Combined Lua / C++ default constructor -- used in Lua and editor
Mine::Mine(lua_State *L) : Burst(Point(0,0), Point(0,0), NULL, BurstRadius)
{
   initialize(Point(0,0));
   
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }}, 2 };
      
      S32 profile = checkArgList(L, constructorArgList, "Mine", "constructor");

      if(profile == 1)
         setPos(L, 1);
   }
}


// Destructor
Mine::~Mine()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void Mine::initialize(const Point &pos)
{
   mObjectTypeNumber = MineTypeNumber;
   mWeaponType = WeaponMine;

   mArmed = false;
   mKillString = "mine";      // Triggers special message when player killed

   mFuseTimer.setPeriod(FuseDelay);

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


Mine *Mine::clone() const
{
   return new Mine(*this);
}


// ProcessArguments() used is the one in item
string Mine::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + geomToLevelCode();
}


void Mine::idle(IdleCallPath path)
{
   // Skip the grenade timing goofiness...
   MoveItem::idle(path);

   if(mExploded || path != BfObject::ServerIdleMainLoop)
      return;

   // If our fuse has gone off, count down until explode
   if(mFuseTimer.update(mCurrentMove.time))
   {
      explode(getActualPos());
      return;
   }

   // And check for enemies in the area...
   Point pos = getActualPos();
   Rect queryRect(pos, pos);
   queryRect.expand(Point(SensorRadius, SensorRadius));

   fillVector.clear();
   findObjects((TestFunc)isMotionTriggerType, fillVector, queryRect);

   // Found something!
   bool foundItem = false;
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *foundObject = static_cast<BfObject *>(fillVector[i]);

      F32 radius;
      Point ipos;
      if(foundObject->getCollisionCircle(ActualState, ipos, radius))
      {
         if((ipos - pos).lenSquared() < sq(radius + SensorRadius))
         {
            bool isMine = foundObject->getObjectTypeNumber() == MineTypeNumber;
            if(!isMine)
            {
               foundItem = true;
               break;
            }
            else if(mArmed && foundObject != this)
            {
               foundItem = true;
               break;
            }
         }
      }
   }
   if(foundItem)
   {     // braces needed
      if(mArmed)
         explode(getActualPos());
   }
   else
   {
      if(!mArmed)
      {
         setMaskBits(ArmedMask);
         mArmed = true;
      }
   }
}


bool Mine::collide(BfObject *otherObj)
{
   if(isGhost())
      return false;  // avoid client side explode, server side don't explode

   if(isProjectileType(otherObj->getObjectTypeNumber()))
      explode(getActualPos());

   return false;
}


void Mine::damageObject(DamageInfo *info)
{
   // Bursts don't explode mines unless they're within the trigger radius
   BfObject *bfObject = info->damagingObject;
   if(bfObject && bfObject->getObjectTypeNumber() == BurstTypeNumber)
   {
      if(getPos().distSquared(bfObject->getPos()) > sq(SensorRadius))
         return;
   }

   // If another mine explosion hit us, delay exploding slightly.  This is so an
   // entire minefield doesn't explode at once.
   if(bfObject && bfObject->getObjectTypeNumber() == MineTypeNumber)
   {
      mFuseTimer.reset();
      return;
   }

   if(info->damageAmount > 0.f && !mExploded)
      explode(getActualPos());
}


// Only runs on server side
U32 Mine::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   if(updateMask & InitialMask)
   {
      writeThisTeam(stream);

      GameConnection *gc = static_cast<GameConnection *>(connection);

      bool isOwner = getOwner() == gc->getClientInfo();

      // This will set mIsOwnedByLocalClient client-side
      stream->write(isOwner);
   }

   stream->writeFlag(mArmed);

   return ret;
}


void Mine::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   Parent::unpackUpdate(connection, stream);

   if(mInitial)     // Initial data
   {
      initial = true;
      readThisTeam(stream);
      stream->read(&mIsOwnedByLocalClient);
   }
   bool wasArmed = mArmed;
   mArmed = stream->readFlag();

   if(initial && !mArmed)
      getGame()->playSoundEffect(SFXMineDeploy, getActualPos());
   else if(!initial && !wasArmed && mArmed)
      getGame()->playSoundEffect(SFXMineArm, getActualPos());
}


void Mine::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(!shouldRender())
      return;

   bool visible = false, armed = false;

   Ship *ship = getGame()->getLocalPlayerShip();

   S32 ourTeam = static_cast<ClientGame*>(getGame())->getCurrentTeamIndex();

   if(ourTeam != TEAM_NEUTRAL)
   {
      armed = mArmed;

      // Can see mine if laid by teammate in team game OR you laid it yourself
      if( (ourTeam == getTeam() && getGame()->isTeamGame()) ||
            mIsOwnedByLocalClient)
         visible = true;

      // If sensor is active and you're within the detection distance
      if(ship && ship->hasModule(ModuleSensor) && (ship->getPos() - getPos()).lenSquared() < sq(ModuleInfo::SensorCloakInnerDetectionDistance))
         visible = true;
   }
   else
   {
      armed = true;
      visible = true;      // We get here in editor when in preview mode
   }

   renderMine(pos, armed, visible);
#endif
}


void Mine::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
   renderMine(getActualPos(), true, true);
}


void Mine::renderDock()
{
#ifndef ZAP_DEDICATED
   Point pos = getActualPos();

   drawCircle(pos, 9, &Colors::gray70);
   drawLetter('M', pos, Colors::gray70, 1);
#endif
}


const char *Mine::getOnScreenName()     { return "Mine";  }
const char *Mine::getOnDockName()       { return "Mine";  }
const char *Mine::getPrettyNamePlural() { return "Mines"; }
const char *Mine::getEditorHelpString() { return "Mines can be prepositioned, and are are \"hostile to all\". [M]"; }


bool Mine::hasTeam()        { return false; }
bool Mine::canBeHostile()   { return false; }
bool Mine::canBeNeutral()   { return false; }

bool Mine::canAddToEditor() { return true; }     



/////
// Lua interface

//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(Mine, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(Mine, LUA_METHODS);

#undef LUA_METHODS


const char *Mine::luaClassName = "Mine";
REGISTER_LUA_SUBCLASS(Mine, Burst);


//////////////////////////////////
//////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(SpyBug);

// Constructor -- used when SpyBug is deployed
SpyBug::SpyBug(const Point &pos, BfObject *planter) : Burst(pos, Point(0,0), planter)
{
   initialize(pos, planter);
}


// Combined Lua / C++ default constructor -- used in Lua and editor
SpyBug::SpyBug(lua_State *L) : Burst(Point(0,0), Point(0,0), NULL)
{
   initialize(Point(0,0), NULL);

   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, TEAM_INDX, END }}, 2 };

      S32 profile = checkArgList(L, constructorArgList, "SpyBug", "constructor");
      
      if(profile == 1)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }
   }
}


void SpyBug::initialize(const Point &pos, BfObject *planter)
{
   mObjectTypeNumber = SpyBugTypeNumber;
   mWeaponType = WeaponSpyBug;

   if(planter == NULL)
      setTeam(TEAM_NEUTRAL);     // Burst will set this to TEAM_HOSTILE

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
SpyBug::~SpyBug()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


SpyBug *SpyBug::clone() const
{
   return new SpyBug(*this);
}


bool SpyBug::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 3)
      return false;

   setTeam(atoi(argv[0]));

   // Strips off first arg from argv, so the parent gets the straight coordinate pair it's expecting
   if(!Parent::processArguments(2, &argv[1], game))    
      return false;

   return true;
}


// ProcessArguments() used is the one in item
string SpyBug::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + itos(getTeam()) + " " + geomToLevelCode();
}


// Spy bugs are always in scope.  This only really matters on pre-positioned spy bugs...
void SpyBug::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);

   if(!isGhost())
      setScopeAlways();
}


void SpyBug::idle(IdleCallPath path)
{
   // Skip the grenade timing goofiness...
   MoveItem::idle(path);

   if(mExploded || path != BfObject::ServerIdleMainLoop)
      return;
}


bool SpyBug::collide(BfObject *otherObj)
{
   if(isGhost())
      return false;  // avoid client side explode, server side don't explode
   if(isProjectileType(otherObj->getObjectTypeNumber()))
      explode(getActualPos());
   return false;
}


void SpyBug::damageObject(DamageInfo *info)
{
   if(info->damageAmount > 0.f && !mExploded)    // Any damage will kill the SpyBug
      explode(getActualPos());
}


U32 SpyBug::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);
   if(stream->writeFlag(updateMask & InitialMask))
   {
      writeThisTeam(stream);

      GameConnection *gc = static_cast<GameConnection *>(connection);

      bool isOwner = getOwner() == gc->getClientInfo();

      stream->write(isOwner);
   }
   return ret;
}


void SpyBug::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
   {
      initial = true;
      readThisTeam(stream);
      stream->read(&mIsOwnedByLocalClient);
   }
   if(initial)
      getGame()->playSoundEffect(SFXSpyBugDeploy, getActualPos());
}


void SpyBug::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(!shouldRender())
      return;

   bool visible = false;

   // During editor preview mode, getGame() is returning NULL; I'm not sure why, and not sure if it's a problem,
   // but we'll add a check here to handle it.
   S32 ourTeam = getGame() ? static_cast<ClientGame*>(getGame())->getCurrentTeamIndex() : TEAM_NEUTRAL;

   if(ourTeam != TEAM_NEUTRAL)
   {
      // Can see bug if laid by teammate in team game OR
      // you laid it yourself OR spyBug is neutral
      if( (ourTeam == getTeam() && getGame()->isTeamGame())   ||
            mIsOwnedByLocalClient || getTeam() == TEAM_NEUTRAL)
         visible = true;

      // If sensor is active and you're within the detection distance
      Ship *ship = getGame()->getLocalPlayerShip();
      if(ship && ship->hasModule(ModuleSensor) &&
            (ship->getPos() - getPos()).lenSquared() < sq(ModuleInfo::SensorCloakInnerDetectionDistance))
         visible = true;
   }
   else    
      visible = true;      // We get here in editor when in preview mode


   renderSpyBug(pos, *getColor(), visible);
#endif
}


void SpyBug::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
   renderSpyBug(getPos(), *getColor(), true);
}


void SpyBug::renderDock()
{
#ifndef ZAP_DEDICATED
   const F32 radius = 9;

   Point pos = getRenderPos();

   renderFilledPolygon(pos, 6, radius, *getColor(), Colors::gray70);

   drawLetter('S', pos, Color(getTeam() < 0 ? .5 : .7), 1);    // Use darker gray for neutral spybugs so S will show up clearer
#endif
}


const char *SpyBug::getOnScreenName()     { return "Spy Bug";  }
const char *SpyBug::getOnDockName()       { return "Bug";      }
const char *SpyBug::getPrettyNamePlural() { return "Spy Bugs"; }
const char *SpyBug::getEditorHelpString() { return "Remote monitoring device that shows enemy ships on the commander's map."; }


bool SpyBug::hasTeam()        { return true;  }
bool SpyBug::canBeHostile()   { return false; }
bool SpyBug::canBeNeutral()   { return true;  }

bool SpyBug::canAddToEditor() { return true;  }  



// Can the player see the spybug?
// client side
bool SpyBug::isVisibleToPlayer(S32 playerTeam, bool isTeamGame)
{
   if(getTeam() == TEAM_NEUTRAL)
      return true;

   if(isTeamGame)
      return getTeam() == playerTeam;
   else
      return mIsOwnedByLocalClient;
}
// server side
bool SpyBug::isVisibleToPlayer(ClientInfo *clientInfo, bool isTeamGame)
{
   if(getTeam() == TEAM_NEUTRAL)
      return true;

   if(isTeamGame)
      return getTeam() == clientInfo->getTeamIndex();
   else
      return getOwner() == clientInfo;
}

/////
// Lua interface
/**
 * @luafunc SpyBug::SpyBug()
 * @luafunc SpyBug::SpyBug(point pos, int team)
 * @luaclass SpyBug
 * 
 * @brief Monitors a section of the map and will show enemy ships there.
 * 
 * @descr Can only be used/created if the Sensor module is selected.  Makes
 * surrounding areas of the commander's map visible to player and teammates.
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(SpyBug, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(SpyBug, LUA_METHODS);

#undef LUA_METHODS



const char *SpyBug::luaClassName = "SpyBug";
REGISTER_LUA_SUBCLASS(SpyBug, Burst);


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Seeker);

// Statics
const F32 Seeker::Radius = 2;
const F32 Seeker::Mass = 0.2;

const U32 Seeker::SpeedIncreasePerSecond = 300;
const U32 Seeker::TargetAcquisitionRadius = 400;
const F32 Seeker::MaximumAngleChangePerSecond = FloatTau / 2;
const F32 Seeker::TargetSearchAngle = FloatTau * .6f;  // Anglular spread in front of ship to search for targets

const S32 Seeker::ReassessTargetTime = 100;  // Milliseconds to reassess target

const S32 Seeker::InnerBlastRadius = 80;
const S32 Seeker::OuterBlastRadius = 120;

// Constructor
Seeker::Seeker(const Point &pos, const Point &vel, F32 angle, BfObject *shooter) : MoveItem(pos, true, Seeker::Radius, Seeker::Mass)
{
   initialize(pos, vel, angle, shooter);
}


// Combined Lua / C++ default constructor
Seeker::Seeker(lua_State *L)
{
   initialize(Point(0,0), Point(0,0), 0, NULL);
}


// Destructor
Seeker::~Seeker()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void Seeker::initialize(const Point &pos, const Point &vel, F32 angle, BfObject *shooter)
{
   mObjectTypeNumber = SeekerTypeNumber;

   mNetFlags.set(Ghostable);

   setPosVelAng(pos, vel, angle);
   mWeaponType = WeaponSeeker;
   mStyle = SeekerStyleNormal;

   updateExtentInDatabase();

   mTimeRemaining = WeaponInfo::getWeaponInfo(WeaponSeeker).projLiveTime;
   mExploded = false;

   if(!shooter)
   {
      setOwner(NULL);
      setTeam(TEAM_HOSTILE);
   }
   else
   {
      setOwner(shooter->getOwner());
      setTeam(shooter->getTeam());
      mShooter = shooter;
      mKillString = shooter->getKillString();

      if(shooter->getObjectTypeNumber() == TurretTypeNumber)
         mStyle = SeekerStyleTurret;  // Forces Seeker to use Turret style
   }
      
   mAcquiredTarget = NULL;
   mReassessTargetTimer = ReassessTargetTime;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


static F32 normalizeAngle(F32 angle)
{
   F32 newAngle = angle;

   while (newAngle <= -FloatPi) newAngle += FloatTau;
   while (newAngle > FloatPi)   newAngle -= FloatTau;

   return newAngle;
}


// Runs on client and server
void Seeker::idle(IdleCallPath path)
{
   TNLAssert(path == ClientIdlingNotLocalShip || path == ServerIdleMainLoop, "Unexpected idle path!");

   Parent::idle(path);

#ifndef ZAP_DEDICATED
   if(path == ClientIdlingNotLocalShip)
   {
      emitMovementSparks();
      mTrail.idle(mCurrentMove.time);
   }
#endif

   if(path != BfObject::ServerIdleMainLoop)      // Server only from now on
      return;

   // Update time-to-live server-side
   S32 deltaT = mCurrentMove.time;
   if(!mExploded)
   {
      if(mTimeRemaining <= deltaT)
         handleCollision(NULL, getActualPos());
      else
         mTimeRemaining -= deltaT;
   }
   // No more processing if we've gone BOOM!
   if(mExploded)  // handleCollision might make it explode
      return;

   // Do we need a target?
   if(!mAcquiredTarget)
      acquireTarget();

   // Do we have a target?
   if(mAcquiredTarget)
   {
      // First, remove target if it is too far away.  Next tick we'll search for a new one.
      Point delta = mAcquiredTarget->getPos() - getActualPos();
      if(delta.lenSquared() > sq(TargetAcquisitionRadius))
         mAcquiredTarget = NULL;

      // Else turn towards target
      else
      {
         // Create a new velocity vector for the seeker to slowly go towards the target.
         // Adjust the vector to always:
         //  - keep a minimum velocity (projectile default)
         //  - only change angle to a maxium amount from the original direction
         //  - increase speed each tick

         // Set velocity vector towards the target for now
         Point newVelocity = delta;

         // Find the angle to the target as well as the current velocity angle
         // atan2 already normalizes these to be between -pi and pi
         F32 angleToTarget = delta.ATAN2();
         F32 currentAngle = getActualAngle();

         // Set our new angle towards the target for now
         F32 newAngle = currentAngle;

         // Find the difference between the target angle and the angle of current travel
         // Normalize it to be between -pi and pi
         F32 difference = normalizeAngle(angleToTarget - currentAngle);

         // This is the maximum change in angle we will allow
         F32 maxTickAngle = MaximumAngleChangePerSecond * F32(deltaT) / 1000.f;

         // If our difference in angles are greater than maximum allowed, reduce to the maximum
         if(fabs(difference) > maxTickAngle)
         {
            if(difference > 0)
            {
               newVelocity.setAngle(currentAngle + maxTickAngle);
               newAngle = (currentAngle + maxTickAngle);
            }
            else
            {
               newVelocity.setAngle(currentAngle - maxTickAngle);
               newAngle = (currentAngle - maxTickAngle);
            }
         }

         // Get current speed
         F32 speed = (F32)WeaponInfo::getWeaponInfo(mWeaponType).projVelocity;

         //F32 speed = getVel().len();
         // Set minimum speed to the default
         //if(speed < WeaponInfo::getWeaponInfo(mWeaponType).projVelocity)
         //   speed = WeaponInfo::getWeaponInfo(mWeaponType).projVelocity;
         // Else, increase or decrease depending on our trajectory to the target
         //else
         //{
         //   F32 tickSpeedIncrease = SpeedIncreasePerSecond * F32(deltaT) / 1000.f;
         //   if(reduceSpeed)
         //      speed -= tickSpeedIncrease;
         //   else
         //      speed += tickSpeedIncrease;
         //}

         newVelocity.normalize(speed);
         setActualVel(newVelocity);
         setActualAngle(newAngle);
      }
   }

   // Force re-acquire to test for closer targets after a short interval
   mReassessTargetTimer -= deltaT;
   if(mReassessTargetTimer < 0)
   {
      mReassessTargetTimer = ReassessTargetTime;
      mAcquiredTarget = NULL;
   }
}


// Here we find a suitable target for the Seeker to home in on
// Will consider targets within TargetAcquisitionRadius in a outward cone with spread TargetSearchAngle
void Seeker::acquireTarget()
{
   F32 ourAngle = getActualAngle();

   // Used for wall detection
   static Vector<DatabaseObject *> localFillVector;

   Rect queryRect(getPos(), TargetAcquisitionRadius);
   fillVector.clear();
   findObjects(isSeekerTarget, fillVector, queryRect);

   F32 closest = F32_MAX;

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      TNLAssert(dynamic_cast<BfObject *>(fillVector[i]), "Not a BfObject");
      BfObject *foundObject = static_cast<BfObject *>(fillVector[i]);

      // Don't target self
      //if(mShooter == foundObject)
      //   continue;

      // Check if this pair of objects can damage one another, this takes care of
      // the team check
      if(!getGame()->objectCanDamageObject(this, foundObject))
         continue;

      Point delta = foundObject->getPos() - getPos();
      F32 distanceSq = delta.lenSquared();

      //// Only acquire an object within a circle radius instead of query rect
      //if(distanceSq > TargetAcquisitionRadius * TargetAcquisitionRadius)
      //   continue;

      // This target is not the closest
      if(distanceSq > closest)
         continue;

      // See if object is within our "cone of vision"
      F32 ang = normalizeAngle(getPos().angleTo(foundObject->getPos()) - ourAngle);
      if(ang > TargetSearchAngle * 0.5f || ang < -TargetSearchAngle * 0.5f)
         continue;

      // Finally make sure there are no collideable objects in the way (like walls, forcefields)
      localFillVector.clear();
      findObjects((TestFunc)isCollideableType, localFillVector, Rect(getPos(), foundObject->getPos()));

      F32 dummy;
      bool wallInTheWay = false;

      for(S32 i = 0; i < localFillVector.size(); i++)
      {
         BfObject *collideObject = static_cast<BfObject *>(localFillVector[i]);

         if(collideObject->collide(this) &&   // Test forcefield up or down
               objectIntersectsSegment(collideObject, getPos(), foundObject->getPos(), dummy))
         {
            wallInTheWay = true;
            break;
         }
      }

      if(wallInTheWay)
         continue;

      closest = distanceSq;
      mAcquiredTarget = foundObject;
   }
}


void Seeker::emitMovementSparks()
{
#ifndef ZAP_DEDICATED

   Point center(-10 + -20 * getActualVel().len() / WeaponInfo::getWeaponInfo(WeaponSeeker).projVelocity, 0);

   F32 th = getActualVel().ATAN2();

   F32 warpInScale = 1;//(Ship::WarpFadeInTime - ((ClientGame *)getGame())->getClientInfo()->getShip()->mWarpInTimer.getCurrent()) / F32(Ship::WarpFadeInTime);

   F32 cosTh = cos(th);
   F32 sinTh = sin(th);
  

   Point emissionPoint(center.x * cosTh + center.y * sinTh,
                       center.y * cosTh + center.x * sinTh);

   emissionPoint *= warpInScale;
 
   mTrail.update(getRenderPos() + emissionPoint, UI::SeekerProfile);

#endif
}


U32 Seeker::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   stream->writeFlag(mExploded);

   if(stream->writeFlag((updateMask & InitialMask) && (getGame()->getCurrentTime() - getCreationTime() < 500)))
      stream->writeEnum(mStyle, SeekerStyleCount);

   if(stream->writeFlag(updateMask & PositionMask))
      stream->writeSignedFloat(getActualAngle() * FloatInversePi, 8);  // 8 bits good enough?
   return ret;
}


void Seeker::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   TNLAssert(connection, "Invalid connection to server in Burst//projectile.cpp");

   bool wasExploded = mExploded;
   mExploded = stream->readFlag();
   if(mExploded && !wasExploded)
   {
      disableCollision();
      doExplosion(getPos());
   }
   else if(!mExploded && !isCollisionEnabled() && getActualVel().lenSquared() != 0)
      enableCollision();

   if(stream->readFlag())     // InitialMask --> seeker was just created
   {
      mStyle = (SeekerStyle) stream->readEnum(SeekerStyleCount);
      getGame()->playSoundEffect(SFXSeekerFire, getPos(), getVel());
   }

   if(stream->readFlag())     // PositionMask --> for angle changes since they are not handled in MoveItem
      setActualAngle(stream->readSignedFloat(8) * FloatPi);
}


void Seeker::damageObject(DamageInfo *theInfo)
{
   // If we're being damaged by a burst or a bullet, explode...
   if(theInfo->damageType == DamageTypeArea || theInfo->damagingObject->getObjectTypeNumber() == BulletTypeNumber)
   {
      handleCollision(theInfo->damagingObject, getPos());
      return;
   }

   computeImpulseDirection(theInfo);

   setMaskBits(PositionMask);
}


void Seeker::doExplosion(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(isGhost())
   {
      TNLAssert(dynamic_cast<ClientGame *>(getGame()) != NULL, "Not a ClientGame");
      static_cast<ClientGame *>(getGame())->emitBlast(pos, 100);          // New, manly explosion

      getGame()->playSoundEffect(SFXMineExplode, getPos());
   }
#endif
}


// Server-side only
void Seeker::handleCollision(BfObject *hitObject, Point collisionPoint)
{
   if(mExploded)  // Rare, but can happen
      return;

   // Must set exploded to true immediately here or we risk a stack overflow when two
   // area-damage objects hit each other and call radiusDamage on each other over and over
   mExploded = true;

   // Damage the object we hit
   if(hitObject)
   {
      DamageInfo damageInfo;

      damageInfo.collisionPoint = collisionPoint;
      damageInfo.damageAmount = WeaponInfo::getWeaponInfo(mWeaponType).damageAmount;
      damageInfo.damageType = DamageTypeArea;
      damageInfo.damagingObject = this;
      damageInfo.damageSelfMultiplier = WeaponInfo::getWeaponInfo(mWeaponType).damageSelfMultiplier;

      S32 hits = radiusDamage(collisionPoint, InnerBlastRadius, OuterBlastRadius, (TestFunc)isRadiusDamageAffectableType, damageInfo, 200);

      if(getOwner())
         for(S32 i = 0; i < hits; i++)
            getOwner()->getStatistics()->countHit(mWeaponType);
   }

   mTimeRemaining = 0;
   setMaskBits(ExplodedMask);

   disableCollision();
   deleteObject(100);
}


bool Seeker::collide(BfObject *otherObj)
{
   if(isShipType(otherObj->getObjectTypeNumber())) // So client-side can predict better and make some sound effect
   {
      TNLAssert(dynamic_cast<Ship *>(otherObj), "Not a ship");
      if(static_cast<Ship *>(otherObj)->isModulePrimaryActive(ModuleShield))
         return true;
   }

   // Don't collide with shooter within first 500 ms of shooting
   if(mShooter.isValid() && mShooter == otherObj && getGame()->getCurrentTime() - getCreationTime() < 500)
      return false;

   return isWeaponCollideableType(otherObj->getObjectTypeNumber());     // Includes bullets... well, includes most everything
}


// Returns true if collision was handled, false if not
bool Seeker::collided(BfObject *otherObj, U32 stateIndex)
{
   // Seeker hits seeker and transfers momentum only (no explosion)
   if(otherObj->getObjectTypeNumber() == SeekerTypeNumber)
      return false;

   if(stateIndex == ActualState)
   {
      if(!isGhost())
         handleCollision(otherObj, getActualPos());
      else if(isCollisionEnabled())
         disableCollision();
   }

   setVel(stateIndex, Point(0,0)); // Might save some CPU telling move() to stop moving.
   return true;
}


BfObject *Seeker::getShooter() const {return mShooter; }


void Seeker::renderItem(const Point &pos)
{
#ifndef ZAP_DEDICATED
   if(!shouldRender())  
      return;

   S32 startLiveTime = WeaponInfo::getWeaponInfo(mWeaponType).projLiveTime;
   renderSeeker(pos, mStyle, getActualAngle(), getActualVel().len(), startLiveTime - (getGame()->getCurrentTime() - getCreationTime()));
#endif
}


bool Seeker::shouldRender() const
{
   return isCollisionEnabled();  // (exploded) always disables collision.
}


bool Seeker::canAddToEditor() { return false; }    // No seekers in the editor!


/////
// Lua interface
/**
 * @luafunc Seeker::Seeker()
 * @luaclass Seeker
 * 
 * @brief Guided projectile that homes in on enemy players.
 */
//               Fn name    Param profiles  Profile count
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getWeapon, ARRAYDEF({{ END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(Seeker, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Seeker, LUA_METHODS);

#undef LUA_METHODS


const char *Seeker::luaClassName = "Seeker";
REGISTER_LUA_SUBCLASS(Seeker, MoveObject);

/**
 * @luafunc Seeker::getWeapon()
 * 
 * @brief Returns the index of the weapon used to fire the projectile. See the
 * \ref WeaponEnum enum for valid values. 
 * 
 * @return int The index of the weapon used to fire the projectile.
 */
S32 Seeker::lua_getWeapon(lua_State *L) { return returnWeaponType(L, mWeaponType); }


};

