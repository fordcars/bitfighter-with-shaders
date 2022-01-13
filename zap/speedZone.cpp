//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "speedZone.h"
#include "game.h"
#include "BfObject.h"
#include "gameType.h"
#include "gameNetInterface.h"
#include "gameObjectRender.h"
#include "GeomUtils.h"
#include "ship.h"
#include "SoundSystem.h"
#include "stringUtils.h"
#include "gameConnection.h"
#include "Colors.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#  include "UIEditorMenus.h"    // For GoFastEditorAttributeMenuUI def
#  include "UI.h"
#endif


namespace Zap
{

using namespace LuaArgs;

// Needed on OS X, but cause link errors in VC++
TNL_IMPLEMENT_NETOBJECT(SpeedZone);

// Statics:
const U16 SpeedZone::minSpeed = 500;
const U16 SpeedZone::maxSpeed = 5000;
const U16 SpeedZone::defaultSpeed = 2000;
const F32 SpeedZone::SpeedMultiplier = 1.5;  // Nobody knows why this is used

#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *SpeedZone::mAttributeMenuUI = NULL;
#endif


// Combined C++/Lua constructor
SpeedZone::SpeedZone(lua_State *L)
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = SpeedZoneTypeNumber;

   mSpeed = defaultSpeed;
   mSnapLocation = false;     // Don't snap unless specified
   mRotateSpeed = 0;
   mUnpackInit = 0;           // Some form of counter, to know that it is a rotating speed zone

   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { SIMPLE_LINE, END }, { SIMPLE_LINE, NUM, END }}, 3 };
      S32 profile = checkArgList(L, constructorArgList, "SpeedZone", "constructor");

      if(profile == 1)
         setGeom(L, 1);

      else if(profile == 2)
      {
         setGeom(L, 1);
         setSpeed(U16(getInt(L, 2)));
      }
   }

   preparePoints();           // If this is constructed by Lua, we need to have some default geometry in place

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
SpeedZone::~SpeedZone()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


U16 SpeedZone::getSpeed()
{
   return mSpeed;
}


void SpeedZone::setSpeed(U16 speed)
{
   mSpeed = max(minSpeed, min(maxSpeed, speed));
}


bool SpeedZone::getSnapping()
{
   return mSnapLocation;
}


void SpeedZone::setSnapping(bool snapping)
{
   mSnapLocation = snapping;
}


SpeedZone *SpeedZone::clone() const
{
   return new SpeedZone(*this);
}


// Take our basic inputs, pos and dir, and expand them into a three element
// vector (the three points of our triangle graphic), and compute its extent
void SpeedZone::preparePoints()
{
   Point vert1 = getVert(1);

   if(mRotateSpeed != 0)
   {
      vert1 = getVert(1) - getVert(0);
      F32 angle = vert1.ATAN2();

      // Adjust angle
      angle += mRotateSpeed * ((getGame() && getGame()->getGameType()) ? getGame()->getGameType()->getTotalGamePlayedInMs() : 0) * 0.001f;
      vert1.setPolar(vert1.len(), angle);

      // Set new end point
      vert1 += getVert(0);
   }

   generatePoints(getVert(0), vert1, mPolyBounds, mOutline);

   computeExtent();
}


// static method
void SpeedZone::generatePoints(const Point &start, const Point &end, Vector<Point> &points, Vector<Point> &outline)
{
   const S32 inset = 3;
   const F32 halfWidth = SpeedZone::halfWidth;
   const F32 height = SpeedZone::height;

   // Presize to reduce allocation overhead
   points.resize(12);
   outline.resize(5);

   Point parallel(end - start);
   parallel.normalize();

   const F32 chevronThickness = height / 3;
   const F32 chevronDepth = halfWidth - inset;

   Point tip = start + parallel * height;
   Point perpendic(start.y - tip.y, tip.x - start.x);
   perpendic.normalize();

   
   S32 index = 0;

   for(S32 i = 0; i < 2; i++)
   {
      F32 offset = halfWidth * 2 * i - (i * 4);

      // Red chevron -- iterate twice to generate pair
      points[index++] = start + parallel *  (chevronThickness + offset);  // 0
      points[index++] = start + perpendic * (halfWidth-2*inset) + parallel * (inset + offset);   // 1                      //  1   2
      points[index++] = start + perpendic * (halfWidth-2*inset) + parallel * (chevronThickness + inset + offset);  // 2    //    0    3
      points[index++] = start + parallel *  (chevronDepth + chevronThickness + inset + offset);  // 3                      //  5    4
      points[index++] = start - perpendic * (halfWidth-2*inset) + parallel * (chevronThickness + inset + offset);  // 4
      points[index++] = start - perpendic * (halfWidth-2*inset) + parallel * (inset + offset);  // 5
   }

   // Pick a few selected points from those generated above to create an outline shape -- note that we need to reverse the winding
   // in order to make buffering work.  We use buffering for generating the outlines shown in the inline help.
   outline[4] = points[1];
   outline[3] = points[8];
   outline[2] = points[9];    // Front tip
   outline[1] = points[10];
   outline[0] = points[5];
}


void SpeedZone::render()
{
   renderSpeedZone(mPolyBounds, getGame()->getCurrentTime());
}


Color SpeedZone::getEditorRenderColor()
{
   return Colors::red;
}


void SpeedZone::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
   Parent::renderEditor(currentScale, snappingToWallCornersEnabled);
   render();
}


void SpeedZone::onAttrsChanging()
{
   // Do nothing
}


void SpeedZone::onGeomChanging()
{
   onGeomChanged();
}


void SpeedZone::onGeomChanged()   
{  
   generatePoints(getVert(0), getVert(1), mPolyBounds, mOutline);
   Parent::onGeomChanged();
}


// Server only
void SpeedZone::getBufferForBotZone(F32 bufferRadius, Vector<Point> &outputPoly) const
{
   // Expand polygons
   offsetPolygon(&mOutline, outputPoly, bufferRadius, ClipperLib::jtMiter);
}


// This object should be drawn above polygons
S32 SpeedZone::getRenderSortValue()
{
   return 0;
}


// Runs on server and client
void SpeedZone::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   if(!isGhost())
      setScopeAlways();    // Runs on server
}


// Bounding box for quick collision-possibility elimination
void SpeedZone::computeExtent()
{
   setExtent(Rect(mPolyBounds));
}


const Vector<Point> *SpeedZone::getOutline() const
{
   return &mOutline;
}


const Vector<Point> *SpeedZone::getEditorHitPoly() const
{
   return Parent::getOutline();
}


// More precise boundary for more precise collision detection
const Vector<Point> *SpeedZone::getCollisionPoly() const
{
   return &mOutline;
}


// Create objects from parameters stored in level file
bool SpeedZone::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc = 0;
   const char *argv[8];                // 8 is ok, SpeedZone only supports 4 numbered args

   for(S32 i = 0; i < argc2; i++)      // The idea here is to allow optional R3.5 for rotate at speed of 3.5
   {
      char firstChar = argv2[i][0];    // First character of arg

      if(isAlpha(firstChar))
      {
         if(!strnicmp(argv2[i], "Rotate=", 7))   // 016, same as 'R', better name
            mRotateSpeed = (F32)atof(&argv2[i][7]);   // "Rotate=3.4" or "Rotate=-1.7"
         else if(!stricmp(argv2[i], "SnapEnabled"))
            mSnapLocation = true;
         else if(firstChar == 'R') // 015a
            mRotateSpeed = (F32)atof(&argv2[i][1]);   // using second char to handle number, "R3.4" or "R-1.7"
      }
      else
      {
         if(argc < 8)
         {  
            argv[argc] = argv2[i];
            argc++;
         }
      }
   }

   // All "special" args have been processed, now we process the standard args

   if(argc < 4)      // Need two points at a minimum, with an optional speed item
      return false;

   Point start, end;

   start.read(argv);
   start *= game->getLegacyGridSize();

   end.read(argv + 2);
   end *= game->getLegacyGridSize();

   // Save the points we read into our geometry
   setVert(start, 0);
   setVert(end, 1);

   if(argc >= 5)
      setSpeed((U16)(atoi(argv[4])));

   preparePoints();

   return true;
}


// Editor
string SpeedZone::toLevelCode() const
{
   string out = string(appendId(getClassName())) + " " + geomToLevelCode() + " " + itos(mSpeed);
   if(mSnapLocation)
      out += " SnapEnabled";
   if(mRotateSpeed != 0)
      out += " Rotate=" + ftos(mRotateSpeed, 4);
   return out;
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *SpeedZone::getAttributeMenu()
{
   // Lazily initialize this -- if we're in the game, we'll never need this to be instantiated
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = static_cast<ClientGame *>(getGame());

      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      mAttributeMenuUI->addMenuItem(new CounterMenuItem("Speed:", 999, 100, minSpeed, maxSpeed, "", "Really slow", ""));
      mAttributeMenuUI->addMenuItem(new YesNoMenuItem("Snapping:", true, ""));

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void SpeedZone::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(mSpeed);
   attributeMenu->getMenuItem(1)->setIntValue(mSnapLocation ? 1 : 0);
}


// Retrieve the values we need from the menu
void SpeedZone::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   mSpeed        = attributeMenu->getMenuItem(0)->getIntValue();
   mSnapLocation = attributeMenu->getMenuItem(1)->getIntValue();    // Returns 0 or 1
}


// Render some attributes when item is selected but not being edited
void SpeedZone::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("Speed");   values.push_back(itos(mSpeed));
   keys.push_back("Snap");    values.push_back(mSnapLocation ? "Yes" : "No");
}

#endif


static bool ignoreThisCollision = false;

// Checks collisions with a SpeedZone
bool SpeedZone::collide(BfObject *hitObject)
{
   if(ignoreThisCollision)
      return false;

   // This is run on both server and client side to reduce teleport lag effect.
   if(isShipType(hitObject->getObjectTypeNumber()))     // Only ships & robots collide
   {
#ifndef ZAP_DEDICATED
      if(isGhost()) // On client, don't process speedZone on all moveObjects except the controlling one
      {
         ClientGame *client = static_cast<ClientGame *>(getGame());
         GameConnection *gc = client->getConnectionToServer();
         if(gc && gc->getControlObject() != hitObject) 
            return false;
      }
#endif
      return true;
   }
   return false;
}


// Handles collisions with a SpeedZone
bool SpeedZone::collided(BfObject *hitObject, U32 stateIndex)
{
   TNLAssert(dynamic_cast<MoveObject *>(hitObject), "Not a MoveObject");
   MoveObject *ship = static_cast<MoveObject *>(hitObject);

   static Point start, end, impulse, newVel;      // Reusable containers
   start = getVert(0);
   end   = getVert(1);

   if(mRotateSpeed != 0)
   {
      end = end - start;
      F32 angle = end.ATAN2();

      // Adjust angle
      angle += mRotateSpeed * ((getGame() && getGame()->getGameType()) ? getGame()->getGameType()->getTotalGamePlayedInMs() : 0) * 0.001f;
      end.setPolar(end.len(), angle);

      // Set new end point
      end += start;
   }

   impulse = end - start;                 // Gives us direction
   impulse.normalize(mSpeed);             // Gives us the magnitude of speed
   Point shipNormal = ship->getVel(stateIndex);
   shipNormal.normalize(mSpeed);
   F32 angleSpeed = mSpeed * 0.5f;

   // Using mUnpackInit, as client does not know that mRotateSpeed is not zero.
   if(mSnapLocation && mRotateSpeed == 0 && mUnpackInit < 3)
      angleSpeed *= 0.01f;
   if(shipNormal.distanceTo(impulse) < angleSpeed && ship->getVel(stateIndex).len() > mSpeed)
      return true;

   // This following line will cause ships entering the speedzone to have their location set to the same point
   // within the zone so that their path out will be very predictable.
   if(mSnapLocation)
   {
      static Point diffpos, thisAngle, newPos, oldPos, oldVel, collisionPoint, p;    // Reusable points

      diffpos = ship->getPos(stateIndex) - start;
      thisAngle = end - start;
      thisAngle.normalize();
      newPos = thisAngle * diffpos.dot(thisAngle) + start + impulse * 0.001f;

      oldPos = ship->getPos(stateIndex);
      oldVel = ship->getVel(stateIndex);

      ignoreThisCollision = true;  // Seem to need it to ignore collide to SpeedZone during a findFirstCollision
      ship->setVel(stateIndex, newPos - oldPos);

      F32 collisionTime = 1;
      ship->findFirstCollision(stateIndex, collisionTime, collisionPoint);

      p = ship->getPos(stateIndex) + ship->getVel(stateIndex) * collisionTime;    // x = x + vt
      ship->setPos(stateIndex, p);

      ignoreThisCollision = false;

      if(collisionTime != 1)     // Don't allow using speed zone when could not line up due to going into wall?
      {
         ship->setPos(stateIndex, oldPos);
         ship->setVel(stateIndex, oldVel);
         return true;
      }
      newVel = impulse * SpeedMultiplier;    // Why have a multiplier?
   }
   else
   {
      if(shipNormal.distanceTo(impulse) < mSpeed && ship->getVel(stateIndex).len() > mSpeed * 0.8)
         return true;

      newVel = ship->getVel(stateIndex) + impulse * SpeedMultiplier; // Why have a multiplier?
   }

   ship->setVel(stateIndex, newVel);


   if(!ship->isGhost() && stateIndex == ActualState)        // Only server needs to send information
   {
      setMaskBits(HitMask);

      // Trigger a sound on the player's machine: They're going to be so far away they'll never hear the sound emitted by the gofast itself...
      if(ship->getControllingClient() && ship->getControllingClient().isValid())
         ship->getControllingClient()->s2cDisplayMessage(0, SFXGoFastInside, "");
   }
   return true;
}


void SpeedZone::idle(BfObject::IdleCallPath path)
{
   if(mRotateSpeed != 0)
   {
      preparePoints();     // Updates rotating position
   }
}


U32 SpeedZone::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitMask))    
   {
      Point pos = getVert(0);
      Point dir = getVert(1);

      pos.write(stream);
      dir.write(stream);

      stream->writeInt(mSpeed, 16);
      stream->writeFlag(mSnapLocation);

      stream->write(mRotateSpeed);
   }

   stream->writeFlag((updateMask & HitMask) && updateMask != 0xFFFFFFFF);

   return 0;
}


void SpeedZone::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   if(stream->readFlag())     // InitMask
   {
      Point pos, dir;

      mUnpackInit++;

      pos.read(stream);
      dir.read(stream);

      setVert(pos, 0);
      setVert(dir, 1);

      mSpeed = stream->readInt(16);
      mSnapLocation = stream->readFlag();

      stream->read(&mRotateSpeed);

      preparePoints();
   }

   if(stream->readFlag()) 
      SoundSystem::playSoundEffect(SFXGoFastOutside, getVert(0), getVert(0));
}


// Some properties about the item that will be needed in the editor
const char *SpeedZone::getOnScreenName()     { return "GoFast";  }
const char *SpeedZone::getOnDockName()       { return "GoFast";  }
const char *SpeedZone::getPrettyNamePlural() { return "GoFasts"; }
const char *SpeedZone::getEditorHelpString() { return "Makes ships go fast in direction of arrow. [P]"; }

bool SpeedZone::hasTeam()      { return false; }
bool SpeedZone::canBeHostile() { return false; }
bool SpeedZone::canBeNeutral() { return false; }


//// Lua methods

/**
 * @luafunc SpeedZone::SpeedZone()
 * @luafunc SpeedZone::SpeedZone(Geom lineGeom)
 * @luafunc SpeedZone::SpeedZone(Geom lineGeom, int speed)
 * 
 * @luaclass SpeedZone
 * 
 * @brief Propels ships at high speed.
 * 
 * @descr SpeedZones are game objects that propel ships around a level. Each
 * SpeedZone has a direction point that is only used for aiming the SpeedZone.
 * The speed at which ships are flung can be set with the setSpeed() method.
 * SpeedZones also have a snapping parameter which, when `true`, will first move
 * the ship to the SpeedZone's center before propelling them. This allows level
 * designers to control the exact path a ship will take, which can be useful if
 * there is a target that the ships should hit.
 * 
 * Note that a SpeedZone's setGeom() method will take two points. The first will
 * be the SpeedZone's location, the second represents its direction. The
 * distance between the two points is not important; only the angle between them
 * matters.
 */
//               Fn name     Param profiles       Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, setDir,      ARRAYDEF({{ PT,      END }}), 1 ) \
   METHOD(CLASS, getDir,      ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setSpeed,    ARRAYDEF({{ INT_GE0, END }}), 1 ) \
   METHOD(CLASS, getSpeed,    ARRAYDEF({{          END }}), 1 ) \
   METHOD(CLASS, setSnapping, ARRAYDEF({{ BOOL,    END }}), 1 ) \
   METHOD(CLASS, getSnapping, ARRAYDEF({{          END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(SpeedZone, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(SpeedZone, LUA_METHODS);

#undef LUA_METHODS


const char *SpeedZone::luaClassName = "SpeedZone";
REGISTER_LUA_SUBCLASS(SpeedZone, BfObject);


/** 
 * @luafunc SpeedZone::setDir(point dest)
 *
 * @brief Sets the direction of the SpeedZone.
 *
 * @param dest A point which the speed zone should aim at
 *
 * Example:
 * @code 
 *   s = SpeedZone.new()
 *   s:setDir(100,150)
 *   levelgen:addItem(s)  -- or plugin:addItem(s) in a plugin
 * @endcode
 */
S32 SpeedZone::lua_setDir(lua_State *L)
{
   checkArgList(L, functionArgs, "SpeedZone", "setDir");

   Point point = getPointOrXY(L, 1);
   setVert(point, 1);

   onGeomChanged();

   return 0;
}

/**
 * @luafunc point SpeedZone::getDir()
 *
 * @brief Returns the object's direction.
 *
 * @descr The distance between the returned point and the object's location is
 * not important; only the angle between them matters.
 *
 * @return A point object representing the SpeedZone's direction. 
 */
S32 SpeedZone::lua_getDir(lua_State *L)
{
   // Calculate the direction point
   Point offset(getVert(1) - getVert(0));
   offset.normalize();

   return returnPoint(L, offset);
}


/**
 * @luafunc SpeedZone::setSpeed(int speed)
 *
 * @brief Sets the SpeedZone's speed.
 *
 * @descr Speed must be a positive number, and will be limited to a maximum of
 * 65536. Default speed is 2000.
 *
 * @param speed The speed that the SpeedZone should propel ships.
 */
S32 SpeedZone::lua_setSpeed(lua_State *L)
{
   checkArgList(L, functionArgs, "SpeedZone", "setSpeed");
   U32 speed = U32(getInt(L, 1));
   mSpeed = min(speed, (U32)U16_MAX);    // Speed is a U16 -- respond to larger values in a sane manner

   return 0;
}


/**
 * @luafunc int SpeedZone::getSpeed()
 *
 * @brief Returns the SpeedZone's speed.
 *
 * @return A number representing the SpeedZone's speed. Bigger is faster.
 */
S32 SpeedZone::lua_getSpeed(lua_State *L)
{
   return returnInt(L, mSpeed);
}


/**
 * @luafunc SpeedZone::setSnapping(bool snapping)
 *
 * @brief Sets the SpeedZone's snapping parameter.
 *
 * @descr When a ship hits a SpeedZone, it is flung at speed in the
 * direction the SpeedZone is pointing. Depending on exactly how the ship
 * approaches the SpeedZone, its trajectory may differ slightly. By enabling
 * snapping, the ship will first be moved to the center of the SpeedZone
 * before its velocity is calculated. This will cause the ship to follow an
 * exact and predictable path, which may be important if there is a specific
 * target you want the ship to hit.
 *
 * Snapping is off by default.
 *
 * @param snapping `true` if snapping should be enabled, `false` otherwise.
 */
S32 SpeedZone::lua_setSnapping(lua_State *L)
{
   checkArgList(L, functionArgs, "SpeedZone", "setSnapping");
   mSnapLocation = getBool(L, 1);

   return 0;
}


/**
 * @luafunc bool SpeedZone::getSnapping()
 *
 * @brief Returns the SpeedZone's snapping parameter.
 *
 * @return `true` if snapping is enabled, `false` if not.
 */
S32 SpeedZone::lua_getSnapping(lua_State *L)
{
   return returnBool(L, mSnapLocation);
}


};


