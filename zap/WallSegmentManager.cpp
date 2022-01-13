//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "WallSegmentManager.h"

#include "barrier.h"
#include "EngineeredItem.h"      // For forcefieldprojector def ==> probably should not be here

#include "GeomUtils.h"

using namespace TNL;

namespace Zap
{

// Statics
bool WallSegmentManager::mBatchUpdatingGeom = false;


// Constructor
WallSegmentManager::WallSegmentManager()
{
   // These deleted in the destructor
   mWallSegmentDatabase = new GridDatabase(false);      
   mWallEdgeDatabase    = new GridDatabase(false);
}


// Destructor
WallSegmentManager::~WallSegmentManager()
{
   delete mWallSegmentDatabase;
   mWallSegmentDatabase = NULL;

   delete mWallEdgeDatabase;
   mWallEdgeDatabase = NULL;
}


GridDatabase *WallSegmentManager::getWallSegmentDatabase() const
{
   return mWallSegmentDatabase;
}


GridDatabase *WallSegmentManager::getWallEdgeDatabase() const
{
   return mWallEdgeDatabase;
}


void WallSegmentManager::beginBatchGeomUpdate()
{
   mBatchUpdatingGeom = true;
}


void WallSegmentManager::endBatchGeomUpdate(GridDatabase *database, bool modifiedWalls)      // static method
{
   if(modifiedWalls)
      database->getWallSegmentManager()->finishedChangingWalls(database);

   mBatchUpdatingGeom = false;
}


// Find the associated segment(s) and mark them as selected (or not)
void WallSegmentManager::onWallGeomChanged(GridDatabase *editorDatabase, BfObject *wall, bool selected, S32 serialNumber)
{
   computeWallSegmentIntersections(editorDatabase, wall);
   setSelected(serialNumber, selected);     // Make sure newly generated segments retain selection state of parent wall

   if(!mBatchUpdatingGeom)
      finishedChangingWalls(editorDatabase, serialNumber);
}


// This variant only resnaps engineered items that were attached to a segment that moved
void WallSegmentManager::finishedChangingWalls(GridDatabase *editorObjectDatabase, S32 changedWallSerialNumber)
{
   rebuildEdges();         // Rebuild all edges for all walls

   // This block is a modified version of updateAllMountedItems that homes in on a particular segment
   // First, find any items directly mounted on our wall, and update their location.  Because we don't know where the wall _was_, we 
   // will need to search through all the engineered items, and query each to find which ones where attached to the wall that moved.
   fillVector.clear();
   editorObjectDatabase->findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrItem = static_cast<EngineeredItem *>(fillVector[i]);

      // Remount any engr items that were either not attached to any wall, or were attached to any segments on the modified wall
      if(engrItem->getMountSegment() == NULL || engrItem->getMountSegment()->getOwner() == changedWallSerialNumber)
         engrItem->mountToWall(engrItem->getVert(0), editorObjectDatabase->getWallSegmentManager(), NULL);

      // Calculate where all ffs land -- no telling if the segment we moved is or was interfering in its path
      if(engrItem->getObjectTypeNumber() == ForceFieldProjectorTypeNumber)
      {
         ForceFieldProjector *ffp = static_cast<ForceFieldProjector *>(engrItem);
         ffp->findForceFieldEnd();
      }
   }

   rebuildSelectedOutline();
}


void WallSegmentManager::finishedChangingWalls(GridDatabase *editorDatabase)
{
   rebuildEdges();         // Rebuild all edges for all walls
   updateAllMountedItems(editorDatabase);
   rebuildSelectedOutline();
}


// This function clears the WallSegment database, and refills it with the output of clipper
void WallSegmentManager::recomputeAllWallGeometry(GridDatabase *gameDatabase)
{
   buildAllWallSegmentEdgesAndPoints(gameDatabase);
   rebuildEdges();

   rebuildSelectedOutline();
}


// Take geometry from all wall segments, and run them through clipper to generate new edge geometry.  Then use the results to create
// a bunch of WallEdge objects, which will be stored in mWallEdgeDatabase for future reference.  The two key things to understand here
// are that 1) it's all-or-nothing: all edges need to be recomputed at once; there is no way to do a partial rebuild.  And 2) the edges
// cannot be associated with their source segment, so we'll need to rely on other tricks to find an associated wall when needed.
void WallSegmentManager::rebuildEdges()
{
   // Data flow in this method: wallSegments -> wallEdgePoints -> wallEdges

   mWallEdgePoints.clear();

   // Run clipper --> fills mWallEdgePoints from mWallSegments
   clipAllWallEdges(mWallSegmentDatabase->findObjects_fast(), mWallEdgePoints);    
   mWallEdgeDatabase->removeEverythingFromDatabase();  //XXXX <---- THIS CAUSES THE CRASH

   // Create a WallEdge object from the clipped wall geometry.  We'll add it to the WallEdgeDatabase, which will 
   // delete the object when it is ulitmately removed.
   for(S32 i = 0; i < mWallEdgePoints.size(); i+=2)
   {
      WallEdge *newEdge = new WallEdge(mWallEdgePoints[i], mWallEdgePoints[i+1]);   // Create the edge object
      newEdge->addToDatabase(mWallEdgeDatabase);                                    // And add it to the database
   }
}


// Delete all segments, then find all walls and build a new set of segments
void WallSegmentManager::buildAllWallSegmentEdgesAndPoints(GridDatabase *database)
{
   mWallSegmentDatabase->removeEverythingFromDatabase();

   fillVector.clear();
   database->findObjects((TestFunc)isWallType, fillVector);

   Vector<DatabaseObject *> engrObjects;
   database->findObjects((TestFunc)isEngineeredType, engrObjects);   // All engineered objects

   // Iterate over all our wall objects
   for(S32 i = 0; i < fillVector.size(); i++)
      buildWallSegmentEdgesAndPoints(database, fillVector[i], engrObjects);
}


// Given a wall, build all the segments and related geometry; also manage any affected mounted items
// Operates only on passed wall segment -- does not alter others
void WallSegmentManager::buildWallSegmentEdgesAndPoints(GridDatabase *database, DatabaseObject *wallDbObject, 
                                                        const Vector<DatabaseObject *> &engrObjects)
{
#ifndef ZAP_DEDICATED
   // Find any engineered objects that terminate on this wall, and mark them for resnapping later

   Vector<EngineeredItem *> toBeRemounted;    // A list of engr objects terminating on the wall segment that we'll be deleting

   BfObject *wall = static_cast<BfObject *>(wallDbObject);     // Wall we're deleting and rebuilding

   S32 count = mWallSegmentDatabase->getObjectCount();

   // Loop through all the walls, and, for each, see if any of the engineered objects we were given are mounted to it
   for(S32 i = 0; i < count; i++)
   {
      WallSegment *wallSegment = static_cast<WallSegment *>(mWallSegmentDatabase->getObjectByIndex(i));
      if(wallSegment->getOwner() == wall->getSerialNumber())       // Segment belongs to wall
         for(S32 j = 0; j < engrObjects.size(); j++)               // Loop through all engineered objects checking the mount seg
         {
            EngineeredItem *engrObj = static_cast<EngineeredItem *>(engrObjects[j]);

            // Does FF start or end on this segment?
            if(engrObj->getMountSegment() == wallSegment || engrObj->getEndSegment() == wallSegment)
               toBeRemounted.push_back(engrObj);
         }
   }

   // Get rid of any segments that correspond to our wall; we'll be building new ones
   deleteSegments(wall->getSerialNumber());

   Rect allSegExtent;

   // Polywalls will have one segment; it will have the same geometry as the polywall itself.
   // The WallSegment constructor will add it to the specified database.
   if(wall->getObjectTypeNumber() == PolyWallTypeNumber)
      new WallSegment(mWallSegmentDatabase, *wall->getOutline(), wall->getSerialNumber());

   // Traditional walls will be represented by a series of rectangles, each representing a "puffed out" pair of sequential vertices
   else     
   {
      TNLAssert(dynamic_cast<WallItem *>(wall), "Expected an WallItem!");
      WallItem *wallItem = static_cast<WallItem *>(wall);

      // Build out segment data for this line
      Vector<Vector<Point> > segmentData;
      barrierLineToSegmentData(*(wallItem->getOutline()), segmentData);

      for(S32 i = 0; i < segmentData.size(); i++)
      {
         // Create the segment; the WallSegment constructor will add it to the specified database
         WallSegment *newSegment = new WallSegment(mWallSegmentDatabase, segmentData[i],
                                                   (F32)wallItem->getWidth(), wallItem->getSerialNumber());

         // Build up extents of the whole WallItem
         if(i == 0)
            allSegExtent.set(newSegment->getExtent());
         else
            allSegExtent.unionRect(newSegment->getExtent());
      }

      wall->setExtent(allSegExtent);      // A wall's extent is the union of the extents of all its segments.  Makes sense, right?
   }

   // Remount all turrets & forcefields mounted on or terminating on any of the wall segments we deleted and potentially recreated
   for(S32 i = 0; i < toBeRemounted.size(); i++)  
      toBeRemounted[i]->mountToWall(toBeRemounted[i]->getVert(0), database->getWallSegmentManager(), NULL);

#endif
}


// Used above and from instructions
void WallSegmentManager::clipAllWallEdges(const Vector<DatabaseObject *> *wallSegments, Vector<Point> &wallEdges) const
{
   Vector<const Vector<Point> *> inputPolygons;
   Vector<Vector<Point> > solution;

   S32 count = wallSegments->size();

   for(S32 i = 0; i < count; i++)
   {
      WallSegment *wallSegment = static_cast<WallSegment *>(wallSegments->get(i));
      inputPolygons.push_back(wallSegment->getCorners());
   }

   mergePolys(inputPolygons, solution);      // Merged wall segments are placed in solution

   unpackPolygons(solution, wallEdges);
}


// Called by WallItems and PolyWalls when their geom changes
void WallSegmentManager::updateAllMountedItems(GridDatabase *database)
{
   // First, find any items directly mounted on our wall, and update their location.  Because we don't know where the wall _was_, we 
   // will need to search through all the engineered items, and query each to find which ones where attached to the wall that moved.
   fillVector.clear();
   database->findObjects((TestFunc)isEngineeredType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      EngineeredItem *engrItem = static_cast<EngineeredItem *>(fillVector[i]);
      engrItem->mountToWall(engrItem->getVert(0), database->getWallSegmentManager(), NULL);
   }
}


// Called when a wall segment has somehow changed.  All current and previously intersecting segments 
// need to be recomputed.  This only operates on the specified item.  rebuildEdges() will need to be run separately.
void WallSegmentManager::computeWallSegmentIntersections(GridDatabase *gameObjDatabase, BfObject *item)
{
   Vector<DatabaseObject *> engrObjects;
   gameObjDatabase->findObjects((TestFunc)isEngineeredType, engrObjects);   // All engineered objects

   buildWallSegmentEdgesAndPoints(gameObjDatabase, item, engrObjects);
}


void WallSegmentManager::clear()
{
   mWallEdgeDatabase->removeEverythingFromDatabase();
   mWallSegmentDatabase->removeEverythingFromDatabase();

   mWallEdgePoints.clear();
}


void WallSegmentManager::clearSelected()
{
   S32 count = mWallSegmentDatabase->getObjectCount();

   for(S32 i = 0; i < count; i++)
   {
      WallSegment *wallSegment = static_cast<WallSegment *>(mWallSegmentDatabase->getObjectByIndex(i));
      wallSegment->setSelected(false);
   }
}


const Vector<Point> *WallSegmentManager::getWallEdgePoints() const
{
   return &mWallEdgePoints;
}


const Vector<Point> *WallSegmentManager::getSelectedWallEdgePoints() const
{
   return &mSelectedWallEdgePoints;
}



void WallSegmentManager::setSelected(S32 owner, bool selected)
{
   S32 count = mWallSegmentDatabase->getObjectCount();

   for(S32 i = 0; i < count; i++)
   {
      WallSegment *wallSegment = static_cast<WallSegment *>(mWallSegmentDatabase->getObjectByIndex(i));
      if(wallSegment->getOwner() == owner)
         wallSegment->setSelected(selected);
   }
}


void WallSegmentManager::rebuildSelectedOutline()
{
   Vector<DatabaseObject *> selectedSegments;      // Use DatabaseObject here to match the args for clipAllWallEdges()

   S32 count = mWallSegmentDatabase->getObjectCount();

   for(S32 i = 0; i < count; i++)
   {
      WallSegment *wallSegment = static_cast<WallSegment *>(mWallSegmentDatabase->getObjectByIndex(i));
      if(wallSegment->isSelected())
         selectedSegments.push_back(wallSegment);
   }

   // If no walls are selected we can skip a lot of work, butx removing this check will not change the result
   if(selectedSegments.size() == 0)          
      mSelectedWallEdgePoints.clear();    
   else
      clipAllWallEdges(&selectedSegments, mSelectedWallEdgePoints);    // Populate edgePoints from segments
}


// Delete all wall segments owned by specified owner
void WallSegmentManager::deleteSegments(S32 owner)
{
   S32 count = mWallSegmentDatabase->getObjectCount();

   Vector<DatabaseObject *> toBeDeleted;     // Use DatabaseObject to match args for removeFromDatabase

   for(S32 i = 0; i < count; i++)
   {
      WallSegment *wallSegment = static_cast<WallSegment *>(mWallSegmentDatabase->getObjectByIndex(i));
      if(wallSegment->getOwner() == owner)
         toBeDeleted.push_back(wallSegment);
   }

   for(S32 i = 0; i < toBeDeleted.size(); i++)
      mWallSegmentDatabase->removeFromDatabase(toBeDeleted[i], true);
}



};
