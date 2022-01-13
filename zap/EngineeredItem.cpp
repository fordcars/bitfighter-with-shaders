//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "EngineeredItem.h"

#include "gameWeapons.h"
#include "gameObjectRender.h"
#include "WallSegmentManager.h"
#include "Teleporter.h"
#include "gameType.h"

#include "projectile.h"

#include "ServerGame.h"

#include "Colors.h"
#include "stringUtils.h"
#include "MathUtils.h"           // For findLowestRootIninterval()
#include "GeomUtils.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"        // for accessing client's spark manager
#  include "UIEditorMenus.h"
#endif


namespace Zap
{

static bool forceFieldEdgesIntersectPoints(const Vector<Point> *points, const Vector<Point> forceField)
{
   return polygonIntersectsSegment(*points, forceField[0], forceField[1]) || polygonIntersectsSegment(*points, forceField[2], forceField[3]);
}


// Constructor
Engineerable::Engineerable()
{
   mEngineered = false;
}

// Destructor
Engineerable::~Engineerable()
{
   // Do nothing
}


void Engineerable::setEngineered(bool isEngineered)
{
   mEngineered = isEngineered;
}


bool Engineerable::isEngineered()
{
   // If the engineered item has a resource attached, then it was engineered by a player
   return mEngineered;
}


void Engineerable::setResource(MountableItem *resource)
{
   mResource = resource;
   mResource->removeFromDatabase(false);     // Don't want to delete this item -- we'll need it later in releaseResource()

   TNLAssert(dynamic_cast<ServerGame*>(mResource->getGame()), "Null ServerGame");
   static_cast<ServerGame*>(mResource->getGame())->onObjectRemoved(mResource);
}


void Engineerable::releaseResource(const Point &releasePos, GridDatabase *database)
{
   if(!mResource) {
      return;
   }
   mResource->addToDatabase(database);
   mResource->setPosVelAng(releasePos, Point(), 0);               // Reset velocity of resource item to 0,0

   TNLAssert(dynamic_cast<ServerGame*>(mResource->getGame()), "Null ServerGame");
   static_cast<ServerGame*>(mResource->getGame())->onObjectAdded(mResource);
}


////////////////////////////////////////
////////////////////////////////////////

// Returns true if deploy point is valid, false otherwise.  deployPosition and deployNormal are populated if successful.
bool EngineerModuleDeployer::findDeployPoint(const Ship *ship, U32 objectType, Point &deployPosition, Point &deployNormal)
{
   if(objectType == EngineeredTurret || objectType == EngineeredForceField)
   {
         // Ship must be within Ship::MaxEngineerDistance of a wall, pointing at where the object should be placed
         Point startPoint = ship->getActualPos();
         Point endPoint = startPoint + ship->getAimVector() * Ship::MaxEngineerDistance;

         F32 collisionTime;

         // Computes collisionTime and deployNormal -- deployNormal will have been normalized to length of 1
         BfObject *hitObject = ship->findObjectLOS((TestFunc)isWallType, ActualState, startPoint, endPoint,
                                                     collisionTime, deployNormal);

         if(!hitObject)    // No appropriate walls found, can't deploy, sorry!
            return false;


         if(deployNormal.dot(ship->getAimVector()) > 0)
            deployNormal = -deployNormal;      // This is to fix deploy at wrong side of barrier.

         // Set deploy point, and move one unit away from the wall (this is a tiny amount, keeps linework from overlapping with wall)
         deployPosition.set(startPoint + (endPoint - startPoint) * collisionTime + deployNormal);
   }
   else if(objectType == EngineeredTeleporterEntrance || objectType == EngineeredTeleporterExit)
      deployPosition.set(ship->getActualPos() + (ship->getAimVector() * (Ship::CollisionRadius + Teleporter::TELEPORTER_RADIUS)));

   return true;
}


// Check for sufficient energy and resources; return empty string if everything is ok
string EngineerModuleDeployer::checkResourcesAndEnergy(const Ship *ship)
{
   if(!ship->isCarryingItem(ResourceItemTypeNumber))
      return "!!! Need resource item to use Engineer module";

   if(ship->getEnergy() < ModuleInfo::getModuleInfo(ModuleEngineer)->getPrimaryPerUseCost())
      return "!!! Not enough energy to engineer an object";

   return "";
}


// Returns "" if location is OK, otherwise returns an error message
// Runs on client and server
bool EngineerModuleDeployer::canCreateObjectAtLocation(const GridDatabase *gameObjectDatabase, const Ship *ship, U32 objectType)
{
   string msg;

   // Everything needs energy and a resource, except the teleport exit
   if(objectType != EngineeredTeleporterExit)
      mErrorMessage = checkResourcesAndEnergy(ship);

   if(mErrorMessage != "")
      return false;

   if(!findDeployPoint(ship, objectType, mDeployPosition, mDeployNormal))     // Computes mDeployPosition and mDeployNormal
   {
      mErrorMessage = "!!! Could not find a suitable wall for mounting the item";
      return false;
   }

   Vector<Point> bounds;
   bool goodDeploymentPosition = false;

   // Seems inefficient to construct these just for the purpose of bounds checking...
   switch(objectType)
   {
      case EngineeredTurret:
         bounds = Turret::getTurretGeometry(mDeployPosition, mDeployNormal);   
         goodDeploymentPosition = EngineeredItem::checkDeploymentPosition(bounds, gameObjectDatabase);
         break;
      case EngineeredForceField:
         bounds = ForceFieldProjector::getForceFieldProjectorGeometry(mDeployPosition, mDeployNormal);
         goodDeploymentPosition = EngineeredItem::checkDeploymentPosition(bounds, gameObjectDatabase);
         break;
      case EngineeredTeleporterEntrance:
      case EngineeredTeleporterExit:
         goodDeploymentPosition = Teleporter::checkDeploymentPosition(mDeployPosition, gameObjectDatabase, ship);
         break;
      default:    // will never happen
         TNLAssert(false, "Bad objectType");
         return false;
   }

   if(!goodDeploymentPosition)
   {
      mErrorMessage = "!!! Cannot deploy item at this location";
      return false;
   }

   // If this is anything but a forcefield, then we're good to go!
   if(objectType != EngineeredForceField)
      return true;


   // Forcefields only from here on down; we've got miles to go before we sleep

   /// Part ONE
   // We need to ensure forcefield doesn't cross another; doing so can create an impossible situation
   // Forcefield starts at the end of the projector.  Need to know where that is.
   Point forceFieldStart = mDeployPosition;

   // Now we can find the point where the forcefield would end if this were a valid position
   Point forceFieldEnd;
   DatabaseObject *terminatingWallObject;
   ForceField::findForceFieldEnd(gameObjectDatabase, forceFieldStart, mDeployNormal, forceFieldEnd, &terminatingWallObject);

   bool collision = false;

   // Check for collisions with existing projectors
   Rect queryRect(forceFieldStart, forceFieldEnd);
   queryRect.expand(Point(5,5));    // Just a touch bigger than the bare minimum

   Vector<Point> candidateForceFieldGeom = ForceField::computeGeom(forceFieldStart, forceFieldEnd);

   fillVector.clear();
   gameObjectDatabase->findObjects(ForceFieldProjectorTypeNumber, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      ForceFieldProjector *ffp = static_cast<ForceFieldProjector *>(fillVector[i]);

      if(forceFieldEdgesIntersectPoints(ffp->getCollisionPoly(), candidateForceFieldGeom))
      {
         collision = true;
         break;
      }
   }
   
   if(!collision)
   {
      // Check for collision with forcefields that could be projected from those projectors.
      // Projectors up to two forcefield lengths away must be considered because the end of 
      // one could intersect the end of the other.
      fillVector.clear();
      queryRect.expand(Point(ForceField::MAX_FORCEFIELD_LENGTH, ForceField::MAX_FORCEFIELD_LENGTH));
      gameObjectDatabase->findObjects(ForceFieldProjectorTypeNumber, fillVector, queryRect);

      // Reusable containers for holding geom of any forcefields we might need to check for intersection with our candidate
      Point start, end;

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         ForceFieldProjector *proj = static_cast<ForceFieldProjector *>(fillVector[i]);

         proj->getForceFieldStartAndEndPoints(start, end);

         if(forceFieldEdgesIntersectPoints(&candidateForceFieldGeom, ForceField::computeGeom(start, end)))
         {
            collision = true;
            break;
         }
      }
   }

   if(collision)
   {
      mErrorMessage = "!!! Cannot deploy forcefield where it could cross another.";
      return false;
   }


   /// Part TWO - preventative abuse measures

   // First thing first, is abusive engineer allowed?  If so, let's get out of here
   if(ship->getGame()->getGameType()->isEngineerUnrestrictedEnabled())
      return true;

   // Continuing on..  let's check to make sure that forcefield doesn't come within a ship's
   // width of a wall; this should really squelch the forcefield abuse
   bool wallTooClose = false;
   fillVector.clear();

   // Build collision poly from forcefield and ship's width
   // Similar to expanding a barrier spine
   Vector<Point> collisionPoly;
   Point dir = forceFieldEnd - forceFieldStart;

   Point crossVec(dir.y, -dir.x);
   crossVec.normalize(2 * Ship::CollisionRadius + ForceField::ForceFieldHalfWidth);

   collisionPoly.push_back(forceFieldStart + crossVec);
   collisionPoly.push_back(forceFieldEnd + crossVec);
   collisionPoly.push_back(forceFieldEnd - crossVec);
   collisionPoly.push_back(forceFieldStart - crossVec);

   // Reset query rect
   queryRect = Rect(collisionPoly);

   // Search for wall segments within query
   gameObjectDatabase->findObjects(isWallType, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      // Exclude the end segment from our search
      if(terminatingWallObject && terminatingWallObject == fillVector[i])
         continue;

      if(polygonsIntersect(*fillVector[i]->getCollisionPoly(), collisionPoly))
      {
         wallTooClose = true;
         break;
      }
   }

   if(wallTooClose)
   {
      mErrorMessage = "!!! Cannot deploy forcefield where it will pass too close to a wall";
      return false;
   }


   /// Part THREE
   // Now we should check for any turrets that may be in the way using the same geometry as in
   // part two.  We can excluded engineered turrets because they can be destroyed
   bool turretInTheWay = false;
   fillVector.clear();
   gameObjectDatabase->findObjects(TurretTypeNumber, fillVector, queryRect);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Turret *turret = static_cast<Turret *>(fillVector[i]);

      // We don't care about engineered turrets because they can be destroyed
      if(turret->isEngineered())
         continue;

      if(polygonsIntersect(*turret->getCollisionPoly(), collisionPoly))
      {
         turretInTheWay = true;
         break;
      }
   }

   if(turretInTheWay)
   {
      mErrorMessage = "!!! Cannot deploy forcefield over a non-destructible turret";
      return false;
   }

   return true;     // We've run the gammut -- this location is OK
}


// Runs on server
// Only run after canCreateObjectAtLocation, which checks for errors and sets mDeployPosition
// Return true if everything went well, false otherwise.  Caller will manage energy credits and debits.
bool EngineerModuleDeployer::deployEngineeredItem(ClientInfo *clientInfo, U32 objectType)
{
   // Do some basic crash-proofing sanity checks first
   Ship *ship = clientInfo->getShip();
   if(!ship)
      return false;

   BfObject *deployedObject = NULL;

   // Create the new engineered item here
   // These will be deleted when destroyed using deleteObject(); or, if not destroyed by end
   // of game, Game::cleanUp() will get rid of them
   switch(objectType)
   {
      case EngineeredTurret:
         deployedObject = new Turret(ship->getTeam(), mDeployPosition, mDeployNormal);    // Deploy pos/norm calc'ed in canCreateObjectAtLocation()
         break;

      case EngineeredForceField:
         deployedObject = new ForceFieldProjector(ship->getTeam(), mDeployPosition, mDeployNormal);
         break;

      case EngineeredTeleporterEntrance:
         deployedObject = new Teleporter(mDeployPosition, mDeployPosition, ship);
         ship->setEngineeredTeleporter(static_cast<Teleporter*>(deployedObject));

         clientInfo->sDisableShipSystems(true);
         clientInfo->setEngineeringTeleporter(true);
         break;

      case EngineeredTeleporterExit:
         if(ship->getEngineeredTeleporter() && !ship->getEngineeredTeleporter()->hasAnyDests())
         {
            // Set the telport endpoint
            ship->getEngineeredTeleporter()->setEndpoint(mDeployPosition);

            // Clean-up
            clientInfo->sTeleporterCleanup();
         }
         else   // Something went wrong
            return false;

         return true;
         break;

      default:
         return false;
   }
   
   Engineerable *engineerable = dynamic_cast<Engineerable *>(deployedObject);

   if((!deployedObject || !engineerable) && !clientInfo->isRobot())  // Something went wrong
   {
      GameConnection *conn = clientInfo->getConnection();
      conn->s2cDisplayErrorMessage("Error deploying object.");
      delete deployedObject;
      return false;
   }

   // It worked!  Object depolyed!
   engineerable->computeExtent();      // Recomputes extents

   deployedObject->setOwner(clientInfo);
   deployedObject->addToGame(ship->getGame(), ship->getGame()->getGameObjDatabase());

   MountableItem *resource = ship->dismountFirst(ResourceItemTypeNumber);
   ship->resetFastRecharge();

   engineerable->setResource(resource);
   engineerable->onConstructed();
   engineerable->setEngineered(true);

   return true;
}


string EngineerModuleDeployer::getErrorMessage()
{
   return mErrorMessage;
}


////////////////////////////////////////
////////////////////////////////////////

const F32 EngineeredItem::EngineeredItemRadius = 7.f;
const F32 EngineeredItem::DamageReductionFactor = 0.25f;
const F32 EngineeredItem::DisabledLevel = 0.25f;

// Constructor
EngineeredItem::EngineeredItem(S32 team, const Point &anchorPoint, const Point &anchorNormal) : Parent(EngineeredItemRadius), Engineerable(), mAnchorNormal(anchorNormal)
{
   mHealth = 1.0f;
   setTeam(team);
   mOriginalTeam = team;
   mIsDestroyed = false;
   mHealRate = 0;
   mMountSeg = NULL;
   mSnapped = false;

   Parent::setPos(anchorPoint);  // Must be parent, or else... TNLAssert!!!

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
EngineeredItem::~EngineeredItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


bool EngineeredItem::processArguments(S32 argc, const char **argv, Game *game)
{
   if(argc < 3)
      return false;

   setTeam(atoi(argv[0]));
   mOriginalTeam = getTeam();
   if(mOriginalTeam == TEAM_NEUTRAL)      // Neutral object starts with no health and can be repaired and claimed by anyone
      mHealth = 0;
   
   Point pos;
   pos.read(argv + 1);
   pos *= game->getLegacyGridSize();

   if(argc >= 4)
   {
      setHealRate(atoi(argv[3]));
   }

   findMountPoint(game, pos);

   return true;
}


void EngineeredItem::computeObjectGeometry()
{
   mCollisionPolyPoints = getObjectGeometry(getPos(), mAnchorNormal);
}


F32 EngineeredItem::getSelectionOffsetMagnitude()
{
   TNLAssert(false, "Not implemented");
   return 0;
}


void EngineeredItem::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);

   computeObjectGeometry();

   if(mHealth != 0)
      onEnabled();
}


string EngineeredItem::toLevelCode() const
{
   return string(appendId(getClassName())) + " " + itos(getTeam()) + " " + geomToLevelCode() + " " + itos(mHealRate);
}


void EngineeredItem::onGeomChanged()
{
   mCollisionPolyPoints = getObjectGeometry(getPos(), mAnchorNormal);     // Recompute collision poly
   Parent::onGeomChanged();
}


#ifndef ZAP_DEDICATED
Point EngineeredItem::getEditorSelectionOffset(F32 currentScale)
{
   if(!mSnapped)
      return Parent::getEditorSelectionOffset(currentScale);

   F32 m = getSelectionOffsetMagnitude();

   Point cross(mAnchorNormal.y, -mAnchorNormal.x);
   F32 ang = cross.ATAN2();

   F32 x = -m * sin(ang);
   F32 y =  m * cos(ang);

   return Point(x,y);
}

#endif


// Render some attributes when item is selected but not being edited
void EngineeredItem::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   keys.push_back("10% Heal");   values.push_back((mHealRate == 0 ? "Disabled" : itos(mHealRate) + " sec" + (mHealRate != 1 ? "s" : "")));
}


// This is used for both positioning items in-game and for snapping them to walls in the editor --> static method
// Polulates anchor and normal
DatabaseObject *EngineeredItem::findAnchorPointAndNormal(GridDatabase *wallEdgeDatabase, const Point &pos, F32 snapDist, 
                                                         const Vector<S32> *excludedWallList,
                                                         bool format, Point &anchor, Point &normal)
{
   return findAnchorPointAndNormal(wallEdgeDatabase, pos, snapDist, excludedWallList, format, (TestFunc)isWallType, anchor, normal);
}


// Static function
DatabaseObject *EngineeredItem::findAnchorPointAndNormal(GridDatabase *wallEdgeDatabase, const Point &pos, F32 snapDist, 
                                                         const Vector<S32> *excludedWallList,
                                                         bool format, TestFunc testFunc, Point &anchor, Point &normal)
{
   F32 minDist = F32_MAX;
   DatabaseObject *closestWall = NULL;

   Point n;    // Reused in loop below
   F32 t;

   // Start with a sweep of the area
   //
   // The smaller the increment, the closer to finding an accurate line perpendicular to the wall; however
   // we will trade accuracy for performance here and follow up with finding the exact normal and anchor
   // below this loop
   //
   // Start at any angle other than 0.  Search at angle 0 seems to return the wrong wall sometimes
   F32 increment = Float2Pi * 0.0625f;
   for(F32 theta = increment; theta < Float2Pi + increment; theta += increment)
   {
      Point dir(cos(theta), sin(theta));
      dir *= snapDist;
      Point mountPos = pos - dir * 0.001f;  // Offsetting slightly prevents spazzy behavior in editor

      // Look for walls
      DatabaseObject *wall = wallEdgeDatabase->findObjectLOS(testFunc, ActualState, format, mountPos, mountPos + dir, t, n);

      if(wall == NULL)
         continue;

      if(t >= minDist)
         continue;

      if(excludedWallList && excludedWallList->contains(static_cast<WallSegment *>(wall)->getOwner()))
         continue;

      anchor.set(mountPos + dir * t);
      normal.set(n);
      minDist = t;
      closestWall = wall;
   }

   // Re-adjust our anchor to a segment built from the anchor and normal vector found above.
   // This is because the anchor may be slightly off due to the inaccurate sweep angles
   //
   // The algorithm here is to concoct a small segment through the anchor detected in the sweep, and
   // make it perpendicular to the normal vector that was also detected in the sweep (so parallel to
   // the wall edge).  Then find the new normal point to this segment and make that the anchor.
   //
   // 10 point length parallel segment should be plenty
   Point normalNormal(normal.y, -normal.x);
   Point p1 = Point(anchor.x + (5 * normalNormal.x), anchor.y + (5 * normalNormal.y));
   Point p2 = Point(anchor.x - (5 * normalNormal.x), anchor.y - (5 * normalNormal.y));

   // Now find our new anchor
   findNormalPoint(pos, p1, p2, anchor);

   return closestWall;
}


void EngineeredItem::setAnchorNormal(const Point &nrml)
{
   mAnchorNormal = nrml;
}


WallSegment *EngineeredItem::getMountSegment()
{
   return mMountSeg;
}


void EngineeredItem::setMountSegment(WallSegment *mountSeg)
{
   mMountSeg = mountSeg;
}


WallSegment *EngineeredItem::getEndSegment()
{
   return NULL;
}


void EngineeredItem::setEndSegment(WallSegment *endSegment)
{
   // Do nothing
}


// setSnapped() / isSnapped() only called from editor
void EngineeredItem::setSnapped(bool snapped)
{
   mSnapped = snapped;
}


bool EngineeredItem::isSnapped() const
{
   return mSnapped;
}


bool EngineeredItem::isEnabled()
{
   return mHealth >= DisabledLevel;
}


void EngineeredItem::damageObject(DamageInfo *di)
{
   // Don't do self damage.  This is more complicated than it should probably be..
   BfObject *damagingObject = di->damagingObject;

   U8 damagingObjectType = UnknownTypeNumber;
   if(damagingObject != NULL)
      damagingObjectType = damagingObject->getObjectTypeNumber();

   if(isProjectileType(damagingObjectType))
   {
      BfObject *shooter = WeaponInfo::getWeaponShooterFromObject(damagingObject);

      // We have a shooter that is another engineered object (turret)
      if(shooter != NULL && isEngineeredType(shooter->getObjectTypeNumber()))
      {
         EngineeredItem* engShooter = static_cast<EngineeredItem*>(shooter);

         // Don't do self damage or damage to a team-turret
         if(engShooter == this || engShooter->getTeam() == this->getTeam())
            return;
      }
   }

   F32 prevHealth = mHealth;

   if(di->damageAmount > 0)
      mHealth -= di->damageAmount * DamageReductionFactor;
   else
      mHealth -= di->damageAmount;

   checkHealthBounds();

   mHealTimer.reset();     // Restart healing timer...

   setMaskBits(HealthMask);

   // Check if turret just died
   if(prevHealth >= DisabledLevel && mHealth < DisabledLevel)        // Turret just died
   {
      // Revert team to neutral if this was a repaired turret
      if(getTeam() != mOriginalTeam)
      {
         setTeam(mOriginalTeam);
         setMaskBits(TeamMask);
      }
      onDisabled();

      // Handle scoring
      if(damagingObject && damagingObject->getOwner())
      {
         ClientInfo *player = damagingObject->getOwner();

         if(mObjectTypeNumber == TurretTypeNumber)
         {
            GameType *gt = getGame()->getGameType();

            if(gt->isTeamGame() && player->getTeamIndex() == getTeam())
               gt->updateScore(player, KillOwnTurret);
            else
               gt->updateScore(player, KillEnemyTurret);

            player->getStatistics()->mTurretsKilled++;
         }
         else if(mObjectTypeNumber == ForceFieldProjectorTypeNumber)
            player->getStatistics()->mFFsKilled++;
      }
   }
   else if(prevHealth < DisabledLevel && mHealth >= DisabledLevel)   // Turret was just repaired or healed
   {
      if(getTeam() == TEAM_NEUTRAL)                   // Neutral objects...
      {
         if(damagingObject)
         {
            setTeam(damagingObject->getTeam());   // ...join the team of their repairer
            setMaskBits(TeamMask);                    // Broadcast new team status
         }
      }
      onEnabled();
   }

   if(mHealth == 0 && mEngineered)
   {
      mIsDestroyed = true;
      onDestroyed();

      if(mResource.isValid())
      {
         releaseResource(getPos() + mAnchorNormal * mResource->getRadius(), getGame()->getGameObjDatabase());
      }

      deleteObject(500);
   }
}


void EngineeredItem::checkHealthBounds()
{
   if(mHealth < 0)
      mHealth = 0;
   else if(mHealth > 1)
      mHealth = 1;
}


bool EngineeredItem::collide(BfObject *hitObject)
{
   return true;
}


F32 EngineeredItem::getHealth() const
{
   return mHealth;
}


void EngineeredItem::computeExtent()
{
   const Vector<Point> *p = getCollisionPoly();
   setExtent(Rect(*p));
}


void EngineeredItem::onConstructed()
{
   onEnabled();      // Does something useful with ForceFieldProjectors!
}


void EngineeredItem::onDestroyed()
{
   // Do nothing
}


void EngineeredItem::onDisabled()
{
   // Do nothing
}


void EngineeredItem::onEnabled()
{
   // Do nothing
}


Vector<Point> EngineeredItem::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   TNLAssert(false, "function not implemented!");

   Vector<Point> dummy;
   return dummy;
}


// Function needed to provide this signature at this level
void EngineeredItem::setPos(lua_State *L, S32 stackIndex)
{
   Parent::setPos(L, stackIndex);
   findMountPoint(Game::getAddTarget(), getPos());
}


void EngineeredItem::setPos(const Point &p)
{
   Parent::setPos(p);

   computeObjectGeometry();
   computeExtent();           // Sets extent based on actual geometry of object
}


void EngineeredItem::explode()
{
#ifndef ZAP_DEDICATED
   const S32 EXPLOSION_COLOR_COUNT = 12;

   static Color ExplosionColors[EXPLOSION_COLOR_COUNT] = {
      Colors::red,
      Color(0.9, 0.5, 0),
      Colors::white,
      Colors::yellow,
      Colors::red,
      Color(0.8, 1.0, 0),
      Colors::orange50,
      Colors::white,
      Colors::red,
      Color(0.9, 0.5, 0),
      Colors::white,
      Colors::yellow,
   };

   getGame()->playSoundEffect(SFXShipExplode, getPos());

   F32 a = TNL::Random::readF() * 0.4f + 0.5f;
   F32 b = TNL::Random::readF() * 0.2f + 0.9f;
   F32 c = TNL::Random::readF() * 0.15f + 0.125f;
   F32 d = TNL::Random::readF() * 0.2f + 0.9f;

   ClientGame *game = static_cast<ClientGame *>(getGame());

   Point pos = getPos();

   game->emitExplosion(pos, 0.65f, ExplosionColors, EXPLOSION_COLOR_COUNT);
   game->emitBurst(pos, Point(a,c) * 0.6f, Color(1, 1, 0.25), Colors::red);
   game->emitBurst(pos, Point(b,d) * 0.6f, Colors::yellow, Colors::yellow);

   disableCollision();
#endif
}


bool EngineeredItem::isDestroyed()
{
   return mIsDestroyed;
}


// Make sure position looks good when player deploys item with Engineer module -- make sure we're not deploying on top of
// a wall or another engineered item
// static method
bool EngineeredItem::checkDeploymentPosition(const Vector<Point> &thisBounds, const GridDatabase *gb)
{
   Vector<DatabaseObject *> foundObjects;
   Rect queryRect(thisBounds);
   gb->findObjects((TestFunc) isForceFieldCollideableType, foundObjects, queryRect);

   for(S32 i = 0; i < foundObjects.size(); i++)
   {
      if( polygonsIntersect(thisBounds, *static_cast<BfObject *>(foundObjects[i])->getCollisionPoly()) )     // Do they intersect?
         return false;     // Bad location
   }
   return true;            // Good location
}


U32 EngineeredItem::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitialMask))
   {
      Point pos = getPos();

      stream->write(pos.x);
      stream->write(pos.y);
      stream->write(mAnchorNormal.x);
      stream->write(mAnchorNormal.y);
      stream->writeFlag(mEngineered);
   }

   if(stream->writeFlag(updateMask & TeamMask))
      writeThisTeam(stream);

   if(stream->writeFlag(updateMask & HealthMask))
   {
      if(stream->writeFlag(isEnabled()))
         stream->writeFloat((mHealth - DisabledLevel) / (1 - DisabledLevel), 5);
      else
         stream->writeFloat(mHealth / DisabledLevel, 5);

      stream->writeFlag(mIsDestroyed);
   }

   if(stream->writeFlag(updateMask & HealRateMask))
   {
      stream->writeInt(mHealRate, 16);
   }
   return 0;
}


void EngineeredItem::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;

   if(stream->readFlag())
   {
      Point pos;
      initial = true;
      stream->read(&pos.x);
      stream->read(&pos.y);
      stream->read(&mAnchorNormal.x);
      stream->read(&mAnchorNormal.y);
      mEngineered = stream->readFlag();
      setPos(pos);
   }


   if(stream->readFlag())
      readThisTeam(stream);

   if(stream->readFlag())
   {
      if(stream->readFlag())
         mHealth = stream->readFloat(5) * (1 - DisabledLevel) + DisabledLevel; // enabled
      else
         mHealth = stream->readFloat(5) * (DisabledLevel * 0.99f); // disabled, make sure (mHealth < disabledLevel)

      bool wasDestroyed = mIsDestroyed;
      mIsDestroyed = stream->readFlag();

      if(mIsDestroyed && !wasDestroyed && !initial)
         explode();
   }

   if(stream->readFlag())
   {
      mHealRate = stream->readInt(16);
   }

   if(initial)
   {
      computeObjectGeometry();
      computeExtent();
   }
}

void EngineeredItem::setHealRate(S32 rate)
{
   setMaskBits(HealRateMask);
   mHealRate = rate;
   mHealTimer.setPeriod(mHealRate * 1000);
}


S32 EngineeredItem::getHealRate() const
{
   return mHealRate;
}


void EngineeredItem::healObject(S32 time)
{
   if(mHealRate == 0 || getTeam() == TEAM_NEUTRAL)      // Neutral items don't heal!
      return;

   F32 prevHealth = mHealth;

   if(mHealTimer.update(time))
   {
      mHealth += .1f;
      setMaskBits(HealthMask);

      if(mHealth >= 1)
         mHealth = 1;
      else
         mHealTimer.reset();

      if(prevHealth < DisabledLevel && mHealth >= DisabledLevel)
         onEnabled();
   }
}


// Server only
void EngineeredItem::getBufferForBotZone(F32 bufferRadius, Vector<Point> &points) const
{
   // Fill zonePoints
   offsetPolygon(getCollisionPoly(), points, bufferRadius);
}


static const F32 MAX_SNAP_DISTANCE = 100.0f;    // Max distance to look for a mount point

// Figure out where to mount this item during construction; mountToWall() is similar, but used in editor.  
// findDeployPoint() is version used during deployment of engineerered item.
void EngineeredItem::findMountPoint(Game *game, const Point &pos)
{
   Point normal, anchor;

   // Anchor objects to the correct point
   if(!findAnchorPointAndNormal(game->getGameObjDatabase(), pos, MAX_SNAP_DISTANCE, NULL, true, anchor, normal))
   {
      setPos(pos);               // Found no mount point, but for editor, needs to set the position
      mAnchorNormal.set(1,0);
   }
   else
   {
      setPos(anchor + normal);
      mAnchorNormal.set(normal);
   }
   
   computeObjectGeometry();                                    // Fills mCollisionPolyPoints 
   computeExtent();                                            // Uses mCollisionPolyPoints
}


// Find mount point or turret or forcefield closest to pos; used in editor.  See findMountPoint() for in-game version.
Point EngineeredItem::mountToWall(const Point &pos, const WallSegmentManager *wallSegmentManager, const Vector<S32> *excludedWallList)
{  
   Point anchor, nrml;
   DatabaseObject *mountSeg = NULL;

   mountSeg = findAnchorPointAndNormal(wallSegmentManager->getWallSegmentDatabase(), pos,    // <== Note different database than above!
                                       MAX_SNAP_DISTANCE, excludedWallList,
                                       true, (TestFunc)isWallType, anchor, nrml);

   // It is possible to find an edge but not a segment while a wall is being dragged -- the edge remains in it's original location 
   // while the segment is being dragged around, some distance away
   if(mountSeg)   // Found a segment we can mount to
   {
      setPos(anchor);
      setAnchorNormal(nrml);

      TNLAssert(dynamic_cast<WallSegment *>(mountSeg), "Not a WallSegment");
      setMountSegment(static_cast<WallSegment *>(mountSeg));

      mSnapped = true;
      onGeomChanged();

      return anchor;
   }
   else           // No suitable segments found
   {
      mSnapped = false;
      setPos(pos);
      onGeomChanged();

      return pos;
   }
}


/////
// Lua interface
/**
 * @luaclass EngineeredItem
 * 
 * @brief Parent class representing mountable items such as Turret and
 * ForceFieldProjector.
 * 
 * @descr EngineeredItem is a container class for wall-mountable items.
 * Currently, all EngineeredItems can be constructed with the Engineering
 * module, can be destroyed by enemy fire, and can be healed (and sometimes
 * captured) with the Repair module.  All EngineeredItems have a health value
 * that ranges from 0 to 1, where 0 is completely dead and 1 is fully healthy.
 * When health falls below a certain threshold (see getDisabledThrehold()), the
 * item becomes inactive and must be repaired or regenerate itself to be
 * functional again.
 * 
 * If an EngineeredItem has a heal rate greater than zero, it will slowly repair
 * damage to iteself. For more info see setHealRate()
 */
//               Fn name              Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, isActive,             ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, getMountAngle,        ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, getHealth,            ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, setHealth,            ARRAYDEF({{ NUM,  END }}), 1 ) \
   METHOD(CLASS, getDisabledThreshold, ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, getHealRate,          ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, setHealRate,          ARRAYDEF({{ INT,  END }}), 1 ) \
   METHOD(CLASS, getEngineered,        ARRAYDEF({{       END }}), 1 ) \
   METHOD(CLASS, setEngineered,        ARRAYDEF({{ BOOL, END }}), 1 ) \

GENERATE_LUA_METHODS_TABLE(EngineeredItem, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(EngineeredItem, LUA_METHODS);

#undef LUA_METHODS

const char *EngineeredItem::luaClassName = "EngineeredItem";
REGISTER_LUA_SUBCLASS(EngineeredItem, Item);


/**
 * @luafunc bool EngineeredItem::isActive()
 * 
 * @brief Determine if the item is active (i.e. its health is above the
 * disbaledThreshold).
 * 
 * @descr A player can activate an inactive item by repairing it. To set whether
 * an EngineeredItem as active or disabled, use setHealth()
 * 
 * @return Returns `true` if the item is active, or `false` if it is disabled
 */
S32 EngineeredItem::lua_isActive(lua_State *L)
{ 
   return returnBool(L, isEnabled()); 
}


/**
 * @luafunc num EngineeredItem::getMountAngle()
 * 
 * @brief Gets the angle (in radians) at which the item is mounted.
 * 
 * @return Returns the mount angle, in radians.
 */
S32 EngineeredItem::lua_getMountAngle(lua_State *L)
{ 
   return returnFloat(L, mAnchorNormal.ATAN2()); 
}


/**
 * @luafunc num EngineeredItem::getHealth()
 * 
 * @brief Returns health of the item. 
 * 
 * @descr Health is specified as a number between 0 and 1 where 0 is completely
 * dead and 1 is totally healthy.
 * 
 * @return Returns a value between 0 and 1 indicating the health of the item.
 */
S32 EngineeredItem::lua_getHealth(lua_State *L)
{ 
   return returnFloat(L, mHealth);     
}


/**
 * @luafunc EngineeredItem::setHealth(num health)
 * 
 * @brief Set the current health of the item. 
 * 
 * @descr Health is specified as a number between 0 and 1 where 0 is completely
 * dead and 1 is totally healthy.  Values outside this range will be clamped to
 * the valid range.
 * 
 * @param health The item's new health, between 0 and 1.
 */
S32 EngineeredItem::lua_setHealth(lua_State *L)
{ 
   checkArgList(L, functionArgs, "EngineeredItem", "setHealth");
   F32 newHealth = getFloat(L, 1);
   checkHealthBounds();

   // Just 'damage' the engineered item to take care of all of the disabling/mask/etc.
   DamageInfo di;
   di.damagingObject = NULL;

   F32 healthDifference = mHealth - newHealth;
   if(healthDifference > 0)
      di.damageAmount = 4.0f * healthDifference;
   else
      di.damageAmount = healthDifference;

   damageObject(&di);

   return 0;
}


/**
 * @luafunc num EngineeredItem::getDisabledThreshold()
 * 
 * @brief Gets the health threshold below which the item becomes disabled. 
 * 
 * @descr The value will always be between 0 and 1. This value is constant and
 * will be the same for all \link EngineeredItem EngineeredItems\endlink.
 * 
 * @return Health threshold below which the item will be disabled.
 */
S32 EngineeredItem::lua_getDisabledThreshold(lua_State *L)
{
   return returnFloat(L, DisabledLevel);
}


/**
 * @luafunc int EngineeredItem::getHealRate()
 * 
 * @brief Gets the item's healRate. 
 * 
 * @descr The specified heal rate will be the time, in seconds, it takes for the
 * item to repair itself by 10.  If an EngineeredItem is assigned to the neutral
 * team, it will not heal itself.
 * 
 * @return The item's heal rate
 */
S32 EngineeredItem::lua_getHealRate(lua_State *L)
{
   return returnInt(L, mHealRate);
}


/**
 * @luafunc EngineeredItem::setHealRate(int healRate)
 * 
 * @brief Sets the item's heal rate. 
 * 
 * @descr The specified `healRate` will be the time, in seconds, it takes for
 * the item to repair itself by 10. In practice, a heal rate of 1 makes an item
 * effectively unkillable. If the item is assigned to the neutral team, it will
 * not heal itself.  Passing a negative value will generate an error.
 * 
 * @param healRate The new heal rate. Specify 0 to disable healing.
 */
S32 EngineeredItem::lua_setHealRate(lua_State *L)
{
   checkArgList(L, functionArgs, "EngineeredItem", "setHealRate");

   S32 healRate = S32(getInt(L, 1));

   if(healRate < 0)
      THROW_LUA_EXCEPTION(L, "Specified healRate is negative, and that just makes me crazy!");

   setHealRate(healRate);

   return returnInt(L, mHealRate);
}


/**
 * @luafunc bool EngineeredItem::getEngineered()
 * 
 * @brief Get whether the item can be totally destroyed
 * 
 * @return `true` if the item can be destroyed.
 */
S32 EngineeredItem::lua_getEngineered(lua_State *L)
{
   return returnBool(L, mEngineered);
}


/**
 * @luafunc EngineeredItem::setEngineered(bool engineered)
 * 
 * @brief Sets whether the item can be destroyed when its health reaches zero.
 * 
 * @param engineered `true` to make the item destructible, `false` to make it
 * permanent
 */
S32 EngineeredItem::lua_setEngineered(lua_State *L)
{
   checkArgList(L, functionArgs, "EngineeredItem", "setEngineered");

   mEngineered = getBool(L, 1);
   setMaskBits(InitialMask);

   return returnBool(L, mEngineered);
}


// Override some methods
S32 EngineeredItem::lua_setGeom(lua_State *L)
{
   S32 retVal = Parent::lua_setGeom(L);

   findMountPoint(Game::getAddTarget(), getPos());

   return retVal;
}


S32 EngineeredItem::lua_setPos(lua_State *L)
{
   S32 retVal = Parent::lua_setPos(L);

   // This re-triggers all the position information on the client
   setMaskBits(InitialMask);

   return retVal;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ForceFieldProjector);

/**
 * @luafunc ForceFieldProjector::ForceFieldProjector()
 * @luafunc ForceFieldProjector::ForceFieldProjector(point pos)
 * @luafunc ForceFieldProjector::ForceFieldProjector(point pos, int teamIndex)
 */
// Combined Lua / C++ default constructor
ForceFieldProjector::ForceFieldProjector(lua_State *L) : Parent(TEAM_NEUTRAL, Point(0,0), Point(1,0))
{
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, TEAM_INDX, END }}, 3 };

      S32 profile = checkArgList(L, constructorArgList, "ForceFieldProjector", "constructor");

      if(profile == 1 )
      {
         setPos(L, 1);
         setTeam(TEAM_NEUTRAL);
      }
      if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }

      findMountPoint(Game::getAddTarget(), getPos());
   }

   initialize();
}


// Constructor for when projector is built with engineer
ForceFieldProjector::ForceFieldProjector(S32 team, const Point &anchorPoint, const Point &anchorNormal) : Parent(team, anchorPoint, anchorNormal)
{
   initialize();
}


// Destructor
ForceFieldProjector::~ForceFieldProjector()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void ForceFieldProjector::initialize()
{
   mNetFlags.set(Ghostable);
   mObjectTypeNumber = ForceFieldProjectorTypeNumber;
   onGeomChanged();     // Can't be placed on parent, as parent constructor must initalized first

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


ForceFieldProjector *ForceFieldProjector::clone() const
{
   return new ForceFieldProjector(*this);
}


void ForceFieldProjector::onDisabled()
{
   if(mField.isValid())
      mField->deleteObject(0);
}


void ForceFieldProjector::idle(BfObject::IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   healObject(mCurrentMove.time);
}


U32 ForceFieldProjector::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);

   // Update field health
   if(mField.isValid() && isEnabled())
   {
      // Recalculate FF health based on the enabled portion of the FFP health
      // i.e. 0.25 to 1 for the FFP becomes 0 to 1 for the FF
      F32 ffHealth = (mHealth - DisabledLevel) / (1 - DisabledLevel);

      mField->setHealth(ffHealth);
   }

   return ret;
}


void ForceFieldProjector::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);
}


static const S32 PROJECTOR_OFFSET = 15;      // Distance from wall to projector tip; thickness, if you will

F32 ForceFieldProjector::getSelectionOffsetMagnitude()
{
   return PROJECTOR_OFFSET / 3;     // Centroid of a triangle is at 1/3 its height
}


Vector<Point> ForceFieldProjector::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   return getForceFieldProjectorGeometry(anchor, normal);
}


// static method
Vector<Point> ForceFieldProjector::getForceFieldProjectorGeometry(const Point &anchor, const Point &normal)
{
   static const S32 PROJECTOR_HALF_WIDTH = 12;  // Half the width of base of the projector, along the wall

   Vector<Point> geom;
   geom.reserve(3);

   Point cross(normal.y, -normal.x);
   cross.normalize((F32)PROJECTOR_HALF_WIDTH);

   geom.push_back(getForceFieldStartPoint(anchor, normal));
   geom.push_back(anchor - cross);
   geom.push_back(anchor + cross);

   TNLAssert(!isWoundClockwise(geom), "Go the other way!");

   return geom;
}


// Get the point where the forcefield actually starts, as it leaves the projector; i.e. the tip of the projector.  Static method.
Point ForceFieldProjector::getForceFieldStartPoint(const Point &anchor, const Point &normal)
{
   return Point(anchor.x + normal.x * PROJECTOR_OFFSET,
                anchor.y + normal.y * PROJECTOR_OFFSET);
}


void ForceFieldProjector::getForceFieldStartAndEndPoints(Point &start, Point &end)
{
   Point pos = getPos();

   start = getForceFieldStartPoint(pos, mAnchorNormal);

   DatabaseObject *collObj;
   ForceField::findForceFieldEnd(getDatabase(), getForceFieldStartPoint(pos, mAnchorNormal), mAnchorNormal, end, &collObj);
}


WallSegment *ForceFieldProjector::getEndSegment()
{
   return mForceFieldEndSegment;
}


void ForceFieldProjector::setEndSegment(WallSegment *endSegment)
{
   mForceFieldEndSegment = endSegment;
}


// Forcefield projector has been turned on some how; either at the beginning of
// a level, or via repairing, or deploying.
// Runs on server
void ForceFieldProjector::onEnabled()
{
   // Database can be NULL here if adding a forcefield from the editor:  The editor will
   // add a new game object *without* adding it to a grid database in order to optimize
   // adding large groups of objects with copy/paste/undo/redo
   if(!getDatabase())
      return;

   // Server only
   if(isGhost())
      return;

   if(mField.isNull())  // Add mField only when we don't have any
   {
      Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
      Point end;
      DatabaseObject *collObj;

      ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end, &collObj);

      mField = new ForceField(getTeam(), start, end);
      mField->addToGame(getGame(), getGame()->getGameObjDatabase());
   }
}


const Vector<Point> *ForceFieldProjector::getCollisionPoly() const
{
   TNLAssert(mCollisionPolyPoints.size() != 0, "mCollisionPolyPoints.size() shouldn't be zero");
   return &mCollisionPolyPoints;
}


void ForceFieldProjector::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);
}


void ForceFieldProjector::render()
{
#ifndef ZAP_DEDICATED
   // We're not in editor (connected to game)
   if(static_cast<ClientGame*>(getGame())->isConnectedToServer())
      renderForceFieldProjector(&mCollisionPolyPoints, getPos(), getColor(), isEnabled(), mHealth, mHealRate);
   else
      renderEditor(0, false);
#endif
}


void ForceFieldProjector::renderDock()
{
   renderSquareItem(getPos(), getColor(), 1, &Colors::white, '>');
}


void ForceFieldProjector::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
#ifndef ZAP_DEDICATED
   const Color *color = getColor();

   if(mSnapped)
   {
      Point forceFieldStart = getForceFieldStartPoint(getPos(), mAnchorNormal);

      renderForceFieldProjector(&mCollisionPolyPoints, getPos(), color, true, 1.0, mHealRate);
      renderForceField(forceFieldStart, forceFieldEnd, color, true);
   }
   else
      renderDock();
#endif
}


const char *ForceFieldProjector::getOnScreenName()     { return "ForceFld"; }
const char *ForceFieldProjector::getOnDockName()       { return "ForceFld"; }
const char *ForceFieldProjector::getPrettyNamePlural() { return "Force Field Projectors"; }
const char *ForceFieldProjector::getEditorHelpString() { return "Creates a force field that lets only team members pass. [F]"; }


bool ForceFieldProjector::hasTeam() { return true; }
bool ForceFieldProjector::canBeHostile() { return true; }
bool ForceFieldProjector::canBeNeutral() { return true; }


// Determine on which segment forcefield lands -- only used in the editor, wraps ForceField::findForceFieldEnd()
void ForceFieldProjector::findForceFieldEnd()
{
   // Load the corner points of a maximum-length forcefield into geom
   DatabaseObject *collObj;

   Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);

   // Pass in database containing WallSegments, returns object in collObj
   if(ForceField::findForceFieldEnd(getDatabase()->getWallSegmentManager()->getWallSegmentDatabase(), 
                                    start, mAnchorNormal, forceFieldEnd, &collObj))
   {
      setEndSegment(dynamic_cast<WallSegment *>(collObj));
   }
   else
      setEndSegment(NULL);

   setExtent(Rect(ForceField::computeGeom(start, forceFieldEnd)));
}


void ForceFieldProjector::onGeomChanged()
{
   if(mSnapped)
      findForceFieldEnd();

   Parent::onGeomChanged();
}


/////
// Lua interface

// No custom ForceFieldProjector methods
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(ForceFieldProjector, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(ForceFieldProjector, LUA_METHODS);

#undef LUA_METHODS

const char *ForceFieldProjector::luaClassName = "ForceFieldProjector";
REGISTER_LUA_SUBCLASS(ForceFieldProjector, EngineeredItem);

// LuaItem methods -- override method in parent class
S32 ForceFieldProjector::lua_getPos(lua_State *L)
{
   return returnPoint(L, getPos() + mAnchorNormal * getRadius() );
}


S32 ForceFieldProjector::lua_setPos(lua_State *L)
{
   S32 retVal = Parent::lua_setPos(L);

   // Re-find start/end points of FF.
   //
   // Can't just do onEnabled()/onDisabled() because it would reset the FF health
   Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
   Point end;
   DatabaseObject *collObj;

   ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end, &collObj);

   if(mField.isValid())
   {
      mField->setEndPoints(start, end);
      // This will update the client
      mField->setMaskBits(ForceField::InitialMask);
   }

   return retVal;
}


S32 ForceFieldProjector::lua_removeFromGame(lua_State *L)
{
   // Remove field
   onDisabled();

   return Parent::lua_removeFromGame(L);
}


S32 ForceFieldProjector::lua_setTeam(lua_State *L)
{
   // Save old team
   S32 prevTeam = getTeam();

   // Change to new team
   Parent::lua_setTeam(L);

   // We need to set the mOriginalTeam team as the just-set team because of conflicts with
   // projector-disabled logic due to the fact that they can start as neutral
   mOriginalTeam = getTeam();

   // Only re-add a forcefield if the team has changed and if it isn't disabled
   //
   // We're duplicating a lot of logic in the onEnabled() method because calling onEnabled()
   // doesn't seem to work right after calling onDisabled().  Probably because of slow deletion?
   if(mOriginalTeam != prevTeam && isEnabled() && getGame())
   {
      onDisabled();

      Point start = getForceFieldStartPoint(getPos(), mAnchorNormal);
      Point end;
      DatabaseObject *collObj;

      ForceField::findForceFieldEnd(getDatabase(), start, mAnchorNormal, end, &collObj);

      mField = new ForceField(getTeam(), start, end);
      mField->addToGame(getGame(), getGame()->getGameObjDatabase());
   }

   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(ForceField);

// This is only created server-side
ForceField::ForceField(S32 team, Point start, Point end) 
{
   setTeam(team);

   // These are sent to the client
   mStart = start;
   mEnd = end;
   mFieldUp = true;
   mHealth = 0;

   updateGeomAndExtents();

   mObjectTypeNumber = ForceFieldTypeNumber;
   mNetFlags.set(Ghostable);
}

// Destructor
ForceField::~ForceField()
{
   // Do nothing
}


bool ForceField::collide(BfObject *hitObject)
{
   if(!mFieldUp)
      return false;

   // If it's a ship that collides with this forcefield, check team to allow it through
   if(isShipType(hitObject->getObjectTypeNumber()))
   {
      if(hitObject->getTeam() == getTeam())     // Ship and force field are same team
      {
         if(!isGhost())
         {
            mFieldUp = false;
            mDownTimer.reset(FieldDownTime);
            setMaskBits(StatusMask);
         }
         return false;
      }
   }
   // If it's a flag that collides with this forcefield and we're hostile, let it through
   else if(hitObject->getObjectTypeNumber() == FlagTypeNumber)
   {
      if(getTeam() == TEAM_HOSTILE)
         return false;
      else
         return true;
   }

   return true;
}


// Returns true if two forcefields intersect
bool ForceField::intersects(ForceField *forceField)
{
   return polygonsIntersect(mOutline, *forceField->getOutline());
}


const Vector<Point> *ForceField::getOutline() const
{
   return &mOutline;
}


void ForceField::onAddedToGame(Game *game)
{
   Parent::onAddedToGame(game);
}


void ForceField::idle(BfObject::IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   if(mDownTimer.update(mCurrentMove.time))
   {
      // do an LOS test to see if anything is in the field:
      F32 t;
      Point n;
      if(!findObjectLOS((TestFunc)isForceFieldDeactivatingType, ActualState, mStart, mEnd, t, n))
      {
         mFieldUp = true;
         setMaskBits(StatusMask);
      }
      else
         mDownTimer.reset(10);
   }
}


U32 ForceField::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   if(stream->writeFlag(updateMask & InitialMask))
   {
      stream->write(mStart.x);
      stream->write(mStart.y);
      stream->write(mEnd.x);
      stream->write(mEnd.y);
      writeThisTeam(stream);
   }

   if(stream->writeFlag(updateMask & HealthMask))
      stream->writeFloat(mHealth, 5);

   stream->writeFlag(mFieldUp);
   return 0;
}


void ForceField::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   bool initial = false;
   if(stream->readFlag())
   {
      initial = true;
      stream->read(&mStart.x);
      stream->read(&mStart.y);
      stream->read(&mEnd.x);
      stream->read(&mEnd.y);
      readThisTeam(stream);

      updateGeomAndExtents();
   }

   if(stream->readFlag())
      mHealth = stream->readFloat(5);

   bool wasUp = mFieldUp;
   mFieldUp = stream->readFlag();

   if(initial || (wasUp != mFieldUp))
      getGame()->playSoundEffect(mFieldUp ? SFXForceFieldUp : SFXForceFieldDown, mStart);
}


// ForceField health is the portion of health of the ForceFieldProjector above
// the disabled amount
void ForceField::setHealth(F32 health)
{
   // Update FF health if it has changed
   if(health != mHealth)
   {
      mHealth = health;
      setMaskBits(HealthMask);
   }
}


void ForceField::setEndPoints(const Point &start, const Point &end)
{
   // Update the end points of the ForceField and adjust the geom/extents
   mStart = start;
   mEnd = end;

   updateGeomAndExtents();
}


void ForceField::updateGeomAndExtents()
{
   mOutline = computeGeom(mStart, mEnd);

   Rect extent(mStart, mEnd);
   extent.expand(Point(5,5));
   setExtent(extent);
}


const F32 ForceField::ForceFieldHalfWidth = 2.5;

// static
Vector<Point> ForceField::computeGeom(const Point &start, const Point &end)
{
   Vector<Point> geom;
   geom.reserve(4);

   Point normal(end.y - start.y, start.x - end.x);
   normal.normalize(ForceFieldHalfWidth);

   geom.push_back(start + normal);
   geom.push_back(end + normal);
   geom.push_back(end - normal);
   geom.push_back(start - normal);

   return geom;
}


// Pass in a database containing walls or wallsegments
bool ForceField::findForceFieldEnd(const GridDatabase *db, const Point &start, const Point &normal,  
                                   Point &end, DatabaseObject **collObj)
{
   F32 time;
   Point n;

   end.set(start.x + normal.x * MAX_FORCEFIELD_LENGTH, start.y + normal.y * MAX_FORCEFIELD_LENGTH);

   *collObj = db->findObjectLOS((TestFunc)isWallType, ActualState, start, end, time, n);

   if(*collObj)
   {
      end.set(start + (end - start) * time); 
      return true;
   }

   return false;
}


const Vector<Point> *ForceField::getCollisionPoly() const
{
   return &mOutline;
}


void ForceField::render()
{
   renderForceField(mStart, mEnd, getColor(), mFieldUp, mHealth, getGame()->getGameType()->getTotalGamePlayedInMs());
}


S32 ForceField::getRenderSortValue()
{
   return 0;
}


void ForceField::getForceFieldStartAndEndPoints(Point &start, Point &end)
{
   start = mStart;
   end = mEnd;
}


////////////////////////////////////////
////////////////////////////////////////

TNL_IMPLEMENT_NETOBJECT(Turret);

#ifndef ZAP_DEDICATED
   EditorAttributeMenuUI *Turret::mAttributeMenuUI = NULL;
#endif

// Combined Lua / C++ default constructor
/**
 * @luafunc Turret::Turret()
 * @luafunc Turret::Turret(point, team)
 */
Turret::Turret(lua_State *L) : Parent(TEAM_NEUTRAL, Point(0,0), Point(1,0))
{
   if(L)
   {
      static LuaFunctionArgList constructorArgList = { {{ END }, { PT, END }, { PT, TEAM_INDX, END }}, 2 };
      S32 profile = checkArgList(L, constructorArgList, "Turret", "constructor");
      
      if(profile == 1 )
      {
         setPos(L, 1);
         setTeam(TEAM_NEUTRAL);
      }
      if(profile == 2)
      {
         setPos(L, 1);
         setTeam(L, 2);
      }
   }

   initialize();
}


// Constructor for when turret is built with engineer
Turret::Turret(S32 team, const Point &anchorPoint, const Point &anchorNormal) : Parent(team, anchorPoint, anchorNormal)
{
   initialize();
}


// Destructor
Turret::~Turret()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void Turret::initialize()
{
   mObjectTypeNumber = TurretTypeNumber;

   mWeaponFireType = WeaponTurret;
   mNetFlags.set(Ghostable);

   onGeomChanged();

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


Turret *Turret::clone() const
{
   return new Turret(*this);
}


bool Turret::processArguments(S32 argc2, const char **argv2, Game *game)
{
   S32 argc1 = 0;
   const char *argv1[32];
   for(S32 i = 0; i < argc2; i++)
   {
      const char *token = argv2[i];
      unsigned char firstChar = token[0];
      if(isAlpha(firstChar))  // starts with a letter
      {
         if(!strncmp(token, "W=", 2))  // W= is in 015a
         {
            WeaponType w = WeaponInfo::getWeaponTypeFromString(&token[2]);

            if(w < WeaponCount)
            {
               mWeaponFireType = WeaponType(w);
               logprintf(LogConsumer::LogLevelError, "'W=' weapon construct in "
                     "level file is deprecated and will be removed in the future. Instead, remove the 'W='");
            }
         }
         // Proper way to declare a Turret Weapon (since 021), no 'W='
         else
         {
            WeaponType w = WeaponInfo::getWeaponTypeFromString(token);

            if(w < WeaponCount)
               mWeaponFireType = WeaponType(w);
         }

         // Constrain weapon types to a useful subset
         if(mWeaponFireType != WeaponTurret && mWeaponFireType != WeaponBurst &&
               mWeaponFireType != WeaponSeeker && mWeaponFireType != WeaponTriple)
            mWeaponFireType = WeaponTurret;  // Default (no phaser for you)
      }
      else
      {
         if(argc1 < 32)
         {
            argv1[argc1] = token;
            argc1++;
         }
      }
      
   }

   bool returnBool = EngineeredItem::processArguments(argc1, argv1, game);
   mCurrentAngle = mAnchorNormal.ATAN2();
   return returnBool;
}


string Turret::toLevelCode() const
{
   string out = Parent::toLevelCode();

   if(mWeaponFireType != WeaponTurret)
      out = out + " " + writeLevelString(WeaponInfo::getWeaponInfo(mWeaponFireType).name.getString());

   return out;
}


Vector<Point> Turret::getObjectGeometry(const Point &anchor, const Point &normal) const
{
   return getTurretGeometry(anchor, normal);
}


// static method
Vector<Point> Turret::getTurretGeometry(const Point &anchor, const Point &normal)
{
   Point cross(normal.y, -normal.x);

   Vector<Point> polyPoints;
   polyPoints.reserve(4);

   polyPoints.push_back(anchor + cross * 25);
   polyPoints.push_back(anchor + cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 10 + Point(normal) * 45);
   polyPoints.push_back(anchor - cross * 25);

   TNLAssert(!isWoundClockwise(polyPoints), "Go the other way!");

   return polyPoints;
}


const Vector<Point> *Turret::getCollisionPoly() const
{
   return &mCollisionPolyPoints;
}


const Vector<Point> *Turret::getOutline() const
{
   return getCollisionPoly();
}


F32 Turret::getEditorRadius(F32 currentScale)
{
   if(mSnapped)
      return 25 * currentScale;
   else 
      return Parent::getEditorRadius(currentScale);
}


F32 Turret::getSelectionOffsetMagnitude()
{
   return 20;
}


void Turret::onAddedToGame(Game *theGame)
{
   Parent::onAddedToGame(theGame);
   mCurrentAngle = mAnchorNormal.ATAN2();
}


void Turret::render()
{
   renderTurret(*(getColor()), getHealthBarColor(), getPos(), mAnchorNormal, isEnabled(), mHealth, mCurrentAngle, mHealRate);
}


void Turret::renderDock()
{
   renderTurretIcon(getPos(), 1, getColor());
}


void Turret::renderEditor(F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices)
{
   if(mSnapped)
   {
      // We render the turret with/without health if it is neutral or not (as it
      // starts in the game)
      S32 team = getTeam();
      bool enabled = team != TEAM_NEUTRAL;
      F32 health = team == TEAM_NEUTRAL ? 0.0f : 1.0f;

      renderTurret(*(getColor()), getHealthBarColor(), getPos(), mAnchorNormal, enabled, health, mCurrentAngle, mHealRate);
   }
   else
      renderTurretIcon(getPos(), 1/currentScale, getColor());
}


U32 Turret::packUpdate(GhostConnection *connection, U32 updateMask, BitStream *stream)
{
   U32 ret = Parent::packUpdate(connection, updateMask, stream);
   if(stream->writeFlag(updateMask & AimMask))
      stream->write(mCurrentAngle);

   return ret;
}


void Turret::unpackUpdate(GhostConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection, stream);

   if(stream->readFlag())
      stream->read(&mCurrentAngle);
}


// Choose target, aim, and, if possible, fire
void Turret::idle(IdleCallPath path)
{
   if(path != ServerIdleMainLoop)
      return;

   // Server only!

   healObject(mCurrentMove.time);

   if(!isEnabled())
      return;

   mFireTimer.update(mCurrentMove.time);

   // Choose best target:
   Point aimPos = getPos() + mAnchorNormal * TURRET_OFFSET;
   Point cross(mAnchorNormal.y, -mAnchorNormal.x);

   Rect queryRect(aimPos, aimPos);
   queryRect.unionPoint(aimPos + cross * TurretPerceptionDistance);
   queryRect.unionPoint(aimPos - cross * TurretPerceptionDistance);
   queryRect.unionPoint(aimPos + mAnchorNormal * TurretPerceptionDistance);
   fillVector.clear();
   findObjects((TestFunc)isTurretTargetType, fillVector, queryRect);    // Get all potential targets

   WeaponInfo weaponInfo = WeaponInfo::getWeaponInfo(mWeaponFireType);

   BfObject *bestTarget = NULL;
   F32 bestRange = F32_MAX;
   Point bestDelta;

   Point delta;
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      if(isShipType(fillVector[i]->getObjectTypeNumber()))
      {
         Ship *potential = static_cast<Ship *>(fillVector[i]);

         // Is it dead or cloaked?  Carrying objects makes ship visible, except in nexus game
         if(!potential->isVisible(false) || potential->mHasExploded)
            continue;
      }

      // Don't target mounted items (like resourceItems and flagItems)
      if(isMountableItemType(fillVector[i]->getObjectTypeNumber()))
         if(static_cast<MountableItem *>(fillVector[i])->isMounted())
            continue;
      
      BfObject *potential = static_cast<BfObject *>(fillVector[i]);
      if(potential->getTeam() == getTeam())     // Is target on our team?
         continue;                              // ...if so, skip it!

      // Calculate where we have to shoot to hit this...
      Point Vs = potential->getVel();
      F32 S = (F32)weaponInfo.projVelocity;
      Point d = potential->getPos() - aimPos;

// This could possibly be combined with Robot's getFiringSolution, as it's essentially the same thing
      F32 t;      // t is set in next statement
      if(!findLowestRootInInterval(Vs.dot(Vs) - S * S, 2 * Vs.dot(d), d.dot(d), weaponInfo.projLiveTime * 0.001f, t))
         continue;

      Point leadPos = potential->getPos() + Vs * t;

      // Calculate distance
      delta = (leadPos - aimPos);

      Point angleCheck = delta;
      angleCheck.normalize();

      // Check that we're facing it...
      if(angleCheck.dot(mAnchorNormal) <= -0.1f)
         continue;

      // See if we can see it...
      Point n;
      if(findObjectLOS((TestFunc)isWallType, ActualState, aimPos, potential->getPos(), t, n))
         continue;

      // See if we're gonna clobber our own stuff...
      disableCollision();
      Point delta2 = delta;
      delta2.normalize(weaponInfo.projLiveTime * (F32)weaponInfo.projVelocity / 1000.f);
      BfObject *hitObject = findObjectLOS((TestFunc) isWithHealthType, 0, aimPos, aimPos + delta2, t, n);
      enableCollision();

      // Skip this target if there's a friendly object in the way
      if(hitObject && hitObject->getTeam() == getTeam() &&
        (hitObject->getPos() - aimPos).lenSquared() < delta.lenSquared())         
         continue;

      F32 dist = delta.len();

      if(dist < bestRange)
      {
         bestDelta  = delta;
         bestRange  = dist;
         bestTarget = potential;
      }
   }

   if(!bestTarget)      // No target, nothing to do
      return;
 
   // Aim towards the best target.  Note that if the turret is at one extreme of its range, and the target is at the other,
   // then the turret will rotate the wrong-way around to aim at the target.  If we were to detect that condition here, and
   // constrain our turret to turning the correct direction, that would be great!!
   F32 destAngle = bestDelta.ATAN2();

   F32 angleDelta = destAngle - mCurrentAngle;

   if(angleDelta > FloatPi)
      angleDelta -= Float2Pi;
   else if(angleDelta < -FloatPi)
      angleDelta += Float2Pi;

   F32 maxTurn = TurretTurnRate * mCurrentMove.time * 0.001f;

   if(angleDelta != 0)
      setMaskBits(AimMask);

   if(angleDelta > maxTurn)
      mCurrentAngle += maxTurn;
   else if(angleDelta < -maxTurn)
      mCurrentAngle -= maxTurn;
   else
   {
      mCurrentAngle = destAngle;

      if(mFireTimer.getCurrent() == 0)
      {
         bestDelta.normalize();
         Point velocity;
         
         // String handling in C++ is such a mess!!!
         string killer = string("got blasted by ") + getGame()->getTeamName(getTeam()).getString() + " turret";
         mKillString = killer.c_str();

         F32 shooterRadius = mWeaponFireType == WeaponBurst ? 45.f : 35.f;
         GameWeapon::createWeaponProjectiles(mWeaponFireType, bestDelta, aimPos, velocity, 0, shooterRadius, this);

         mFireTimer.reset(weaponInfo.fireDelay);
      }
   }
}


#ifndef ZAP_DEDICATED

EditorAttributeMenuUI *Turret::getAttributeMenu()
{
   // Lazily initialize
   if(!mAttributeMenuUI)
   {
      ClientGame *clientGame = static_cast<ClientGame *>(mGame);

      mAttributeMenuUI = new EditorAttributeMenuUI(clientGame);

      // Heal rate
      // Value doesn't matter (set to 99 here), as it will be clobbered when startEditingAttrs() is called
      CounterMenuItem *menuItem = new CounterMenuItem("10% Heal:", 99, 1, 0, 100, "secs", "Disabled",
                                                      "Time for this item to heal itself 10%");
      mAttributeMenuUI->addMenuItem(menuItem);

      // Weapon Type
      Vector<string> opts;
      opts.push_back(WeaponInfo::getWeaponName(WeaponTurret));
      opts.push_back(WeaponInfo::getWeaponName(WeaponTriple));
      opts.push_back(WeaponInfo::getWeaponName(WeaponBurst));
      opts.push_back(WeaponInfo::getWeaponName(WeaponSeeker));

      U32 curOption = 0;
      if(mWeaponFireType == WeaponTriple)
         curOption = 1;
      else if(mWeaponFireType == WeaponBurst)
         curOption = 2;
      else if(mWeaponFireType == WeaponSeeker)
         curOption = 3;

      mAttributeMenuUI->addMenuItem(new ToggleMenuItem("Weapon: ", opts, curOption, false,
                                           NULL, "Select the turret weapon type"));

      // Add our standard save and exit option to the menu
      mAttributeMenuUI->addSaveAndQuitMenuItem();
   }

   return mAttributeMenuUI;
}


// Get the menu looking like what we want
void Turret::startEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   attributeMenu->getMenuItem(0)->setIntValue(getHealRate());


   U32 curOption = 0;
   if(mWeaponFireType == WeaponTriple)
      curOption = 1;
   else if(mWeaponFireType == WeaponBurst)
      curOption = 2;
   else if(mWeaponFireType == WeaponSeeker)
      curOption = 3;

   attributeMenu->getMenuItem(1)->setIntValue(curOption);
}


// Retrieve the values we need from the menu
void Turret::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu)
{
   setHealRate(attributeMenu->getMenuItem(0)->getIntValue());

   string weaponValue = attributeMenu->getMenuItem(1)->getValue();
   mWeaponFireType = WeaponInfo::getWeaponTypeFromString(weaponValue.c_str());
}


// Render some attributes when item is selected but not being edited
void Turret::fillAttributesVectors(Vector<string> &keys, Vector<string> &values)
{
   string healValue = (mHealRate == 0 ? "Disabled" : itos(mHealRate) + " sec" + (mHealRate != 1 ? "s" : ""));
   keys.push_back("10% Heal");   values.push_back(healValue);

   // Weapon type attribute
   string weaponValue = WeaponInfo::getWeaponName(mWeaponFireType);
   keys.push_back("Weapon");   values.push_back(weaponValue);
}

#endif


const char *Turret::getOnScreenName()     { return "Turret";  }
const char *Turret::getOnDockName()       { return "Turret";  }
const char *Turret::getPrettyNamePlural() { return "Turrets"; }
const char *Turret::getEditorHelpString() { return "Creates shooting turret.  Can be on a team, neutral, or \"hostile to all\". [Y]"; }


bool Turret::hasTeam()      { return true; }
bool Turret::canBeHostile() { return true; }
bool Turret::canBeNeutral() { return true; }


void Turret::onGeomChanged() 
{ 
   mCurrentAngle = mAnchorNormal.ATAN2();       // Keep turret pointed away from the wall... looks better like that!
   Parent::onGeomChanged();
}


/////
// Lua interface
/**
 * @luaclass Turret
 * 
 * @brief Mounted gun that shoots at enemy ships and other objects.
 */
//               Fn name     Param profiles  Profile count                           
#define LUA_METHODS(CLASS, METHOD) \
   METHOD(CLASS, getAimAngle,  ARRAYDEF({{      END }}), 1 ) \
   METHOD(CLASS, setAimAngle,  ARRAYDEF({{ NUM, END }}), 1 ) \
   METHOD(CLASS, setWeapon,    ARRAYDEF({{ WEAP_ENUM, END }}), 1 ) \


GENERATE_LUA_METHODS_TABLE(Turret, LUA_METHODS);
GENERATE_LUA_FUNARGS_TABLE(Turret, LUA_METHODS);

#undef LUA_METHODS


const char *Turret::luaClassName = "Turret";
REGISTER_LUA_SUBCLASS(Turret, EngineeredItem);


/**
 * @luafunc num Turret::getAimAngle()
 * 
 * @brief Returns the angle (in radians) at which the Turret is aiming.
 * 
 * @return The angle (in radians) at which the Turret is aiming.
 */
S32 Turret::lua_getAimAngle(lua_State *L)
{
   return returnFloat(L, mCurrentAngle);
}


/**
 * @luafunc Turret::setAimAngle(num angle)
 * 
 * @brief Sets the angle (in radians) where the Turret should aim.
 * 
 * @param angle Angle (in radians) where the turret should aim.
 */
S32 Turret::lua_setAimAngle(lua_State *L)
{
   checkArgList(L, functionArgs, "Turret", "setAimAngle");
   mCurrentAngle = getFloat(L, 1);

   return 0;
}


/**
 * @luafunc Turret::setWeapon(Weapon weapon)
 *
 * @brief Sets the weapon for this turret to use.
 *
 * @param weapon Weapon to set on the turret
 *
 * @note This is experimental and may be removed or changed from the game at any time
 */
S32 Turret::lua_setWeapon(lua_State *L)
{
   checkArgList(L, functionArgs, "Turret", "setWeapon");

   mWeaponFireType = getWeaponType(L, 1);

   return 0;
}


// Override some methods
S32 Turret::lua_getRad(lua_State *L)
{
   return returnInt(L, TURRET_OFFSET);
}


S32 Turret::lua_getPos(lua_State *L)
{
   return returnPoint(L, getPos() + mAnchorNormal * TURRET_OFFSET);
}



};

