//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GRIDDB_H_
#define _GRIDDB_H_

#include "GeomObject.h"    // Base class

#include "tnlTypes.h"
#include "tnlDataChunker.h"
#include "tnlVector.h"

#include "Rect.h"


using namespace TNL;

namespace Zap
{

typedef bool (*TestFunc)(U8);

// Interface for dealing with objects that can be in our spatial database.
class GridDatabase;
class EditorObjectDatabase;
struct DatabaseBucketEntry;
class DatabaseObject;

struct DatabaseBucketEntryBase
{
   DatabaseBucketEntry *nextInBucket;
};
struct DatabaseBucketEntry : public DatabaseBucketEntryBase
{
   DatabaseObject *theObject;
   DatabaseBucketEntryBase *prevInBucket;
   DatabaseBucketEntry *nextInBucketForThisObject;
};


class DatabaseObject : public GeomObject
{

   typedef GeomObject Parent;

   friend class GridDatabase;
   friend class EditorObjectDatabase;


private:
   U32 mLastQueryId;
   Rect mExtent;
   bool mExtentSet;     // A flag to mark whether extent has been set on this object
   GridDatabase *mDatabase;
   DatabaseBucketEntry *mBucketList;

protected:
   U8 mObjectTypeNumber;

public:
   DatabaseObject();                            // Constructor
   DatabaseObject(const DatabaseObject &t);     // Copy constructor
   virtual ~DatabaseObject();                   // Destructor

   void initialize();

   GridDatabase *getDatabase() const;           // Returns the database in which this object is stored, NULL if not in any database

   Rect getExtent() const;
   bool getExtentSet() const;

   void setExtent(const Rect &extentRect);
   

   virtual const Vector<Point> *getCollisionPoly() const;
   virtual bool getCollisionCircle(U32 stateIndex, Point &point, float &radius) const;

   virtual bool isCollisionEnabled() const;

   bool isInDatabase();
   bool isDeleted();

   void addToDatabase(GridDatabase *database);

   void removeFromDatabase(bool deleteObject);

   U8 getObjectTypeNumber() const;

   virtual bool isDatabasable();    // Can this item actually be inserted into a database?

   virtual DatabaseObject *clone() const;
};


////////////////////////////////////////
////////////////////////////////////////

class WallSegmentManager;
class GoalZone;
class BfObject;

class GridDatabase
{
private:
   U32 mDatabaseId;
   static U32 mQueryId;
   static U32 mCountGridDatabase;      // Reference counter for destruction of mChunker

   WallSegmentManager *mWallSegmentManager;

   Vector<DatabaseObject *> mAllObjects;
   Vector<DatabaseObject *> mGoalZones;
   Vector<DatabaseObject *> mFlags;
   Vector<DatabaseObject *> mSpyBugs;

   void findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector, const Rect *extents, const IntRect *bins) const;
   void findObjects(Vector<U8> typeNumbers, Vector<DatabaseObject *> &fillVector, const Rect *extents, const IntRect *bins) const;
   void findObjects(TestFunc testFunc, Vector<DatabaseObject *> &fillVector, const Rect *extents, const IntRect *bins, bool sameQuery = false) const;

   void fillBins(const Rect &extents, IntRect &bins) const;    // Helper function -- translates extents into bins to search

public:
   enum {
      BucketRowCount = 16,    // Number of buckets per grid row, and number of rows; should be power of 2
      BucketMask = BucketRowCount - 1,
   };

   static ClassChunker<DatabaseBucketEntry> *mChunker;

   DatabaseBucketEntryBase mBuckets[BucketRowCount][BucketRowCount];

   explicit GridDatabase(bool createWallSegmentManager = true);   // Constructor
   // GridDatabase::GridDatabase(const GridDatabase &source);
   virtual ~GridDatabase();                                       // Destructor


   static const S32 BucketWidthBitShift = 8;    // Width/height of each bucket in pixels, in a form of 2 ^ n, 8 is 256 pixels

   DatabaseObject *findObjectLOS(U8 typeNumber, U32 stateIndex, bool format, const Point &rayStart, const Point &rayEnd,
                                 float &collisionTime, Point &surfaceNormal) const;
   DatabaseObject *findObjectLOS(U8 typeNumber, U32 stateIndex, const Point &rayStart, const Point &rayEnd,
                                 float &collisionTime, Point &surfaceNormal) const;
   DatabaseObject *findObjectLOS(TestFunc testFunc, U32 stateIndex, bool format, const Point &rayStart, const Point &rayEnd,
                                 float &collisionTime, Point &surfaceNormal) const;
   DatabaseObject *findObjectLOS(TestFunc testFunc, U32 stateIndex, const Point &rayStart, const Point &rayEnd,
                                 float &collisionTime, Point &surfaceNormal) const;

   bool pointCanSeePoint(const Point &point1, const Point &point2);
   void computeSelectionMinMax(Point &min, Point &max);

   void findObjects(Vector<DatabaseObject *> &fillVector) const;     // Returns all objects in the database
   const Vector<DatabaseObject *> *findObjects_fast() const;         // Faster than above, but results can't be modified
   const Vector<DatabaseObject *> *findObjects_fast(U8 typeNumber) const;   // Currently only works with goalZones, may be expanded in the future

   void findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector) const;
   void findObjects(U8 typeNumber, Vector<DatabaseObject *> &fillVector, const Rect &extents) const;

   void findObjects(TestFunc testFunc, Vector<DatabaseObject *> &fillVector) const;
   void findObjects(TestFunc testFunc, Vector<DatabaseObject *> &fillVector, const Rect &extents, bool sameQuery = false) const;

   void findObjects(const Vector<U8> &types, Vector<DatabaseObject *> &fillVector) const;
   void findObjects(const Vector<U8> &types, Vector<DatabaseObject *> &fillVector, const Rect &extents) const;

   BfObject *findObjectById(S32 id) const;

   void copyObjects(const GridDatabase *source);


   bool testTypes(const Vector<U8> &types, U8 objectType) const;


   void dumpObjects();     // For debugging purposes

   
   Rect getExtents();      // Get the combined extents of every object in the database

   WallSegmentManager *getWallSegmentManager() const;      

   void addToDatabase(DatabaseObject *databaseObject);
   void addToDatabase(const Vector<DatabaseObject *> &objects);


   virtual void removeFromDatabase(DatabaseObject *theObject, bool deleteObject);
   virtual void removeEverythingFromDatabase();

   S32 getObjectCount() const;                          // Return the number of objects currently in the database
   S32 getObjectCount(U8 typeNumber) const;             // Return the number of objects currently in the database of specified type
   bool hasObjectOfType(U8 typeNumber) const;
   DatabaseObject *getObjectByIndex(S32 index) const;   // Kind of hacky, kind of useful
};


};


// Reusable container for searching gridDatabases
// putting it outside of Zap namespace seems to help with visual C++ debugging showing whats inside fillVector  (debugger forgets to add Zap::)
extern Vector<Zap::DatabaseObject *> fillVector;
extern Vector<Zap::DatabaseObject *> fillVector2;

#endif
