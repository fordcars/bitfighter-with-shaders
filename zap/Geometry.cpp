//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "Geometry.h"
#include "GeomUtils.h"              // For polygon triangulation

#include "tnlBitStream.h"
#include "tnlLog.h"

#ifdef TNL_OS_WIN32
#  include <windows.h>   // For ARRAYSIZE
#endif

#include <math.h>


using namespace TNL;

namespace Zap
{

// Constructor
Geometry::Geometry()
{
   // Do nothing
}


// Destructor
Geometry::~Geometry()
{
   // Do nothing
}


GeomType Geometry::getGeomType() const
{
   TNLAssert(false, "Not implemented");
   return geomNone;
}


void Geometry::onPointsChanged()
{
   // Do nothing
}


void Geometry::disableTriangulation()
{
   TNLAssert(false, "Not implemented");
}


Point Geometry::getVert(S32 index) const
{
   TNLAssert(false, "Not implemented");
   return Point();
}


void Geometry::setVert(const Point &pos, S32 index)
{
   TNLAssert(false, "Not implemented");
}


S32 Geometry::getVertCount() const
{
   TNLAssert(false, "Not implemented");
   return 0;
}


S32 Geometry::getMinVertCount() const
{
   TNLAssert(false, "Not implemented");
   return 0;
}


void Geometry::clearVerts()
{
   TNLAssert(false, "Not implemented");
}


bool Geometry::addVert(const Point &point, bool ignoreMaxPointsLimit)
{
   TNLAssert(false, "Not implemented");
   return false;
}


bool Geometry::addVertFront(Point vert)
{
   TNLAssert(false, "Not implemented");
   return false;
}


bool Geometry::deleteVert(S32 vertIndex)
{
   TNLAssert(false, "Not implemented");
   return false;
}


bool Geometry::insertVert(Point vertex, S32 vertIndex)
{
   TNLAssert(false, "Not implemented");
   return false;
}


bool Geometry::anyVertsSelected()
{
   TNLAssert(false, "Not implemented");
   return false;
}


void Geometry::selectVert(S32 vertIndex)
{
   TNLAssert(false, "Not implemented");
}


void Geometry::aselectVert(S32 vertIndex)
{
   TNLAssert(false, "Not implemented");
}


void Geometry::unselectVert(S32 vertIndex)
{
   TNLAssert(false, "Not implemented");
}


void Geometry::unselectVerts()
{
   TNLAssert(false, "Not implemented");
}


bool Geometry::vertSelected(S32 vertIndex)
{
   TNLAssert(false, "Not implemented");
   return false;
}


const Vector<Point> *Geometry::getOutline() const
{
   TNLAssert(false, "Not implemented");
   return NULL;
}


const Vector<Point> *Geometry::getFill() const
{
   TNLAssert(false, "Not implemented");
   return NULL;
}


Point Geometry::getCentroid() const
{
   TNLAssert(false, "Not implemented");
   return Point();
}


F32 Geometry::getLabelAngle() const
{
   TNLAssert(false, "Not implemented");
   return 0;
}


void Geometry::packGeom(GhostConnection *connection, BitStream *stream)
{
   TNLAssert(false, "Not implemented");
}


void Geometry::unpackGeom(GhostConnection *connection, BitStream *stream)
{
   TNLAssert(false, "Not implemented");
}


void Geometry::setGeom(const Vector<Point> &points)
{
   TNLAssert(false, "Not implemented");
}


string Geometry::geomToLevelCode() const
{
   TNLAssert(false, "Not implemented");
   return string();
}


void Geometry::readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize)
{
   TNLAssert(false, "Not implemented");
}


Rect Geometry::calcExtents()
{
   TNLAssert(false, "Not implemented");
   return Rect();
}


void Geometry::rotateAboutPoint(const Point &center, F32 angle)
{
   F32 rad = angle * DoubleTau / 360.0f;

   F32 sinTheta = sinf(rad);
   F32 cosTheta = cosf(rad);

   for(S32 j = 0; j < getVertCount(); j++)
   {
      Point v = getVert(j) - center;
      Point n(v.x * cosTheta + v.y * sinTheta, v.y * cosTheta - v.x * sinTheta);

      setVert(n + center, j);
   }
}


void Geometry::flip(F32 center, bool isHoriz)
{
   S32 count = getVertCount();

   for(S32 i = 0; i < count; i++)
   {
      Point p = getVert(i);
      
      if(isHoriz)
         p.x = center * 2 - p.x;
      else
         p.y = center * 2 - p.y;

      setVert(p, i);
   }
}


// Could probably be more clever about this, but only used when merging polygons in the editor, so speed not critical
void Geometry::reverseWinding()
{
   S32 count = getVertCount();
   Vector<Point> temp(count);

   for(S32 i = 0; i < count; i++)
      temp.push_back(getVert(i));

   for(S32 i = 0; i < count; i++)
      setVert(temp[i], count - i - 1);
}


// Make object bigger or smaller
void Geometry::scale(const Point &center, F32 scale) 
{
   S32 count = getVertCount();

   for(S32 j = 0; j < count; j++)
      setVert((getVert(j) - center) * scale + center, j);
}


// Move object to location, specifying (optional) vertex to be positioned at pos
void Geometry::moveTo(const Point &pos, S32 vertexIndexToBePositionedAtPos)
{
   offset(pos - getVert(vertexIndexToBePositionedAtPos));
}


void Geometry::offset(const Point &offset)
{
   S32 count = getVertCount();

   for(S32 i = 0; i < count; i++)
      setVert(getVert(i) + offset, i);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PointGeometry::PointGeometry(F32 radius)
{
   mPosIsSelected = false;
   mRadius = radius;
}


// Destructor
PointGeometry::~PointGeometry()
{
   // Do nothing
}


GeomType PointGeometry::getGeomType() const
{
   return geomPoint;
}


Point PointGeometry::getVert(S32 index) const
{
   return mPoint;
}


S32 PointGeometry::getVertCount() const
{
   return 1;
}


S32 PointGeometry::getMinVertCount() const
{
   return 1;
}


void PointGeometry::clearVerts()
{
   // Do nothing
}


bool PointGeometry::addVert(const Point &point, bool ignoreMaxPointsLimit)
{
   return false;
}


bool PointGeometry::addVertFront(Point vert)
{
   return false;
}


bool PointGeometry::deleteVert(S32 vertIndex)
{
   return false;
}


bool PointGeometry::insertVert(Point vertex, S32 vertIndex)
{
   return false;
}


bool PointGeometry::anyVertsSelected()
{
   return mPosIsSelected;
}


void PointGeometry::selectVert(S32 vertIndex)
{
   mPosIsSelected = true;
}


void PointGeometry::aselectVert(S32 vertIndex)
{
   mPosIsSelected = true;
}


void PointGeometry::unselectVert(S32 vertIndex)
{
   mPosIsSelected = false;
}


void PointGeometry::unselectVerts()
{
   mPosIsSelected = false;
}


bool PointGeometry::vertSelected(S32 vertIndex)
{
   return mPosIsSelected;
}


const Vector<Point> *PointGeometry::getOutline() const
{
   TNLAssert(false, "Points do not have an inherent outline -- if you need an outline for this object, "
                    "please implement an override for getOutline() in the object itself.");
   return NULL;
}


const Vector<Point> *PointGeometry::getFill() const
{
   TNLAssert(false, "Points do not have fill!");
   return NULL;
}


Point PointGeometry::getCentroid() const
{
   return mPoint;
}


F32 PointGeometry::getLabelAngle() const
{
   return 0;
}


void PointGeometry::setVert(const Point &pos, S32 index)
{
	mPoint = pos;
}


void PointGeometry::packGeom(GhostConnection *connection, BitStream *stream)
{
   mPoint.write(stream);
}


void PointGeometry::unpackGeom(GhostConnection *connection, BitStream *stream)
{
   Point p;
   p.read(stream);
   mPoint = p;
}


void PointGeometry::setGeom(const Vector<Point> &points)
{
   if(points.size() >= 1)
      mPoint = points[0];
}


Rect PointGeometry::calcExtents()
{
   return Rect(mPoint, mRadius);
}


string PointGeometry::geomToLevelCode() const
{
   Point pos = getVert(0); 
   return pos.toLevelCode();
}


void PointGeometry::readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize)
{
   TNLAssert(false, "Haven't figured this one out yet!");
}


////////////////////////////////////////
////////////////////////////////////////

static Vector<Point> outlinePoints(2);    // Reusable container

// Constructor
SimpleLineGeometry::SimpleLineGeometry()
{
   mFromSelected = false;
   mToSelected = false;
}


// Destructor
SimpleLineGeometry::~SimpleLineGeometry()
{
   // Do nothing
}


GeomType SimpleLineGeometry::getGeomType() const
{
   return geomSimpleLine;
}


Point SimpleLineGeometry::getVert(S32 index) const
{
   return (index == 1) ? mToPos : mFromPos;
}


void SimpleLineGeometry::setVert(const Point &pos, S32 index)
{
   if(index == 1)
      mToPos = pos;
   else
      mFromPos = pos;
}


S32 SimpleLineGeometry::getVertCount() const
{
   return 2;
}


S32 SimpleLineGeometry::getMinVertCount() const
{
   return 2;
}


void SimpleLineGeometry::clearVerts()
{
   // Do nothing
}


bool SimpleLineGeometry::addVert(const Point &point, bool ignoreMaxPointsLimit)
{
   return false;
}


bool SimpleLineGeometry::addVertFront(Point vert)
{
   return false;
}


bool SimpleLineGeometry::deleteVert(S32 vertIndex)
{
   return false;
}


bool SimpleLineGeometry::insertVert(Point vertex, S32 vertIndex)
{
   return false;
}


bool SimpleLineGeometry::anyVertsSelected()
{
   return mFromSelected | mToSelected;
}


void SimpleLineGeometry::selectVert(S32 vertIndex)
{
   unselectVerts();
   if(vertIndex == 1)
      mToSelected = true;
   else
      mFromSelected = true;
}


void SimpleLineGeometry::aselectVert(S32 vertIndex)
{
   if(vertIndex == 1)
      mToSelected = true;
   else
      mFromSelected = true;
}


void SimpleLineGeometry::unselectVert(S32 vertIndex)
{
   if(vertIndex == 1)
      mToSelected = false;
   else
      mFromSelected = false;
}


void SimpleLineGeometry::unselectVerts()
{
   mFromSelected = false;
   mToSelected = false;
}


bool SimpleLineGeometry::vertSelected(S32 vertIndex)
{
   return (vertIndex == 1) ? mToSelected : mFromSelected;
}


const Vector<Point> *SimpleLineGeometry::getFill() const
{
   TNLAssert(false, "SimpleLines do not have fill!");
   return NULL;
}


Point SimpleLineGeometry::getCentroid() const
{
   return (mFromPos + mToPos) * 0.5f; // Returns midpoint of line
}


F32 SimpleLineGeometry::getLabelAngle() const
{
   return mFromPos.angleTo(mToPos);
}


const Vector<Point> *SimpleLineGeometry::getOutline() const
{
   outlinePoints.resize(2);
   outlinePoints[0] = mFromPos;
   outlinePoints[1] = mToPos;

   return &outlinePoints;
}


void SimpleLineGeometry::packGeom(GhostConnection *connection, BitStream *stream)
{
   mFromPos.write(stream);
   mToPos.write(stream);
}


void SimpleLineGeometry::unpackGeom(GhostConnection *connection, BitStream *stream)
{
   mFromPos.read(stream);
   mToPos.read(stream);
}


void SimpleLineGeometry::setGeom(const Vector<Point> &points)
{
   if(points.size() >= 2)
   {
      mFromPos = points[0];
      mToPos   = points[1];
   }
}


Rect SimpleLineGeometry::calcExtents()
{
   return Rect(mFromPos, mToPos);
}


string SimpleLineGeometry::geomToLevelCode() const
{
   Point fromPos = mFromPos;
   Point toPos = mToPos;

   return fromPos.toLevelCode() + " " + toPos.toLevelCode();
}


void SimpleLineGeometry::readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize)
{
   TNLAssert(false, "Haven't figured this one out yet!");
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PolylineGeometry::PolylineGeometry()
{
   mAnyVertsSelected = false;
}


// Destructor
PolylineGeometry::~PolylineGeometry()
{
   // Do nothing
}


GeomType PolylineGeometry::getGeomType() const
{
   return geomPolyLine;
}


Point PolylineGeometry::getVert(S32 index)  const
{ 
   return mPolyBounds[index]; 
}


void PolylineGeometry::setVert(const Point &point, S32 index) 
{ 
   mPolyBounds[index] = point; 
}


S32 PolylineGeometry::getVertCount() const 
{ 
   return mPolyBounds.size(); 
}


S32 PolylineGeometry::getMinVertCount() const
{
   return 2;
}


void PolylineGeometry::clearVerts() 
{ 
   mPolyBounds.clear(); 
   mVertSelected.clear(); 
   mAnyVertsSelected = false;
}


bool PolylineGeometry::addVert(const Point &point, bool ignoreMaxPointsLimit) 
{ 
   if(mPolyBounds.size() >= Geometry::MAX_POLY_POINTS && !ignoreMaxPointsLimit)
      return false;

   mPolyBounds.push_back(point); 
   mVertSelected.push_back(false); 

   return true;
}


bool PolylineGeometry::addVertFront(Point vert) 
{ 
   if(mPolyBounds.size() >= Geometry::MAX_POLY_POINTS)
      return false;

   mPolyBounds.push_front(vert); 
   mVertSelected.push_front(false); 

   return true;
}


bool PolylineGeometry::deleteVert(S32 vertIndex) 
{ 
   TNLAssert(vertIndex < S32(mVertSelected.size()), "Index out of bounds!");

   if(vertIndex >= mPolyBounds.size())
      return false;

   mPolyBounds.erase(vertIndex); 
   mVertSelected.erase(vertIndex); 
   checkIfAnyVertsSelected();

   return true;
}


bool PolylineGeometry::insertVert(Point vertex, S32 vertIndex) 
{ 
   if(mPolyBounds.size() >= Geometry::MAX_POLY_POINTS)
      return false;

   mPolyBounds.insert(vertIndex, vertex);                                                   
   mVertSelected.insert(vertIndex, false); 

   return true;
}


bool PolylineGeometry::anyVertsSelected()
{
   return mAnyVertsSelected;
}


void PolylineGeometry::selectVert(S32 vertIndex)
{
   unselectVerts();
   aselectVert(vertIndex);
}


void PolylineGeometry::aselectVert(S32 vertIndex)
{
   TNLAssert(vertIndex < S32(mVertSelected.size()), "Index out of bounds!");
   mVertSelected[vertIndex] = true;
   mAnyVertsSelected = true;
}


void PolylineGeometry::unselectVert(S32 vertIndex)
{
   TNLAssert(vertIndex < S32(mVertSelected.size()), "Index out of bounds!");
   mVertSelected[vertIndex] = false;
   checkIfAnyVertsSelected();
}


void PolylineGeometry::unselectVerts()
{
   for(S32 i = 0; i < mVertSelected.size(); i++)
      mVertSelected[i] = false;

   mAnyVertsSelected = false;
}


bool PolylineGeometry::vertSelected(S32 vertIndex)
{
   TNLAssert(vertIndex < S32(mVertSelected.size()), "Index out of bounds!");
   return mVertSelected[vertIndex];
}


void PolylineGeometry::checkIfAnyVertsSelected()
{
   mAnyVertsSelected = false;

   for(S32 i = 0; i < mVertSelected.size(); i++)
      if(mVertSelected[i])
      {
         mAnyVertsSelected = true;
         break;
      }
}


const Vector<Point> *PolylineGeometry::getOutline() const
{
   return &mPolyBounds;
}


const Vector<Point> *PolylineGeometry::getFill() const
{
   TNLAssert(false, "Polylines don't have fill!");
   return NULL;
}


Point PolylineGeometry::getCentroid() const
{
   return mCentroid;
}


F32 PolylineGeometry::getLabelAngle() const
{
   return 0;
}


void PolylineGeometry::packGeom(GhostConnection *connection, BitStream *stream)
{
   TNLAssert(mPolyBounds.size() > 0, "Invalid geometry!");

   // - 1 because writeEnum ranges from 0 to n-1; mPolyBounds.size() ranges from 1 to n
   stream->writeEnum(mPolyBounds.size() - 1, Geometry::MAX_POLY_POINTS);  
   for(S32 i = 0; i < mPolyBounds.size(); i++)
      mPolyBounds[i].write(stream);
}


// Client only, I think.  Why would a server be unpacking?
void PolylineGeometry::unpackGeom(GhostConnection *connection, BitStream *stream)
{
   U32 size = stream->readEnum(Geometry::MAX_POLY_POINTS) + 1;

   mPolyBounds.resize(size);
   mVertSelected.resize(size);
   
   for(U32 i = 0; i < size; i++)
      mPolyBounds[i].read(stream);

   // If we are getting this from a packet, we can reverse the order of the points without consequence.
   // Putting them in CCW order makes it easier for Clipper to use these points as input.
   if(isWoundClockwise(mPolyBounds))
      mPolyBounds.reverse();
}


void PolylineGeometry::setGeom(const Vector<Point> &points)
{
   S32 size = points.size();

   mPolyBounds.clear();
   mPolyBounds.reserve(size);

   for(S32 i = 0; i < size; i++)
   {
      // Filter out points with NaN values
      if(points[i] == points[i])
         mPolyBounds.push_back(points[i]);
   }

   mVertSelected.resize(mPolyBounds.size());
}


Rect PolylineGeometry::calcExtents()
{
   return Rect(mPolyBounds);
}


string PolylineGeometry::geomToLevelCode() const
{
   string bounds = "";
   S32 size = mPolyBounds.size();

   Point p;
   for(S32 i = 0; i < size; i++)
   {
      p = mPolyBounds[i];
      bounds += p.toLevelCode() + (i < size - 1 ? " " : "");
   }

   return bounds;
}


// Fills bounds with points from argv starting at firstCoord; also resizes selected to match the size of bounds
static void readPolyBounds(S32 argc, const char **argv, S32 firstCoord, F32 gridSize, 
                           bool allowFirstAndLastPointToBeEqual, Vector<Point> &bounds, Vector<bool> &selected)
{
   Point p, lastP;
   
   bool isTwoPointLine = (argc - firstCoord) / 2 == 2;
   
   bounds.clear();

   // Make sure we don't crash with firstCoord = 0; argc = 7; or some uneven number
   for(S32 i = firstCoord; i < argc - 1; i += 2)
   {
      // If we are loading legacy levels (earlier than 019), then they used a gridsize multiplier.
      if(gridSize != 1.f)
         p.set( (F32) (atof(argv[i]) * gridSize), (F32) (atof(argv[i+1]) * gridSize ) );
      else
         p.set( (F32) atof(argv[i]), (F32) atof(argv[i+1]));

      // Normally, we'll want to filter out adjacent points that are identical.  But we also
      // need to handle the situation where the user has created a 2-pt 0-length line.
      // Because the users demand it.  We will deliver.
      if(i == firstCoord || p != lastP || isTwoPointLine)
         bounds.push_back(p);

      lastP.set(p);
   }

   // Check if last point was same as first; if so, scrap it
   if(!allowFirstAndLastPointToBeEqual && bounds.first() == bounds.last())
      bounds.erase(bounds.size() - 1);

   selected.resize(bounds.size());
}


// For walls at least, this is client (i.e. editor) only; walls processed in ServerGame::processPseudoItem() on server
void PolylineGeometry::readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize)
{
    readPolyBounds(argc, argv, firstCoord, gridSize, true, mPolyBounds, mVertSelected);      // Fills mPolyBounds
}


void PolylineGeometry::onPointsChanged()
{
   Parent::onPointsChanged();

   if(mPolyBounds.size() == 2)
      mCentroid = (mPolyBounds[0] + mPolyBounds[1]) * 0.5f;
   else
      mCentroid = findCentroid(mPolyBounds);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PolygonGeometry::PolygonGeometry() : PolylineGeometry()
{
   mLabelAngle = 0;
   mTriangluationDisabled = false;
}

// Destructor
PolygonGeometry::~PolygonGeometry()
{
   // Do nothing
}


GeomType PolygonGeometry::getGeomType() const
{
   return geomPolygon;
}


const Vector<Point> *PolygonGeometry::getFill() const
{
   TNLAssert(!mTriangluationDisabled, "Triangluation disabled!");
   return &mPolyFill;
}


Point PolygonGeometry::getCentroid() const
{
   TNLAssert(!mTriangluationDisabled, "Triangluation disabled!");
   return Parent::getCentroid();
}


F32 PolygonGeometry::getLabelAngle() const
{
   TNLAssert(!mTriangluationDisabled, "Triangluation disabled!");
   return mLabelAngle;
}


void PolygonGeometry::readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize)
{
   readPolyBounds(argc, argv, firstCoord, gridSize, false, mPolyBounds, mVertSelected);
}


void PolygonGeometry::onPointsChanged()
{
   if(mTriangluationDisabled)
      return;

   Parent::onPointsChanged();

   Triangulate::Process(mPolyBounds, mPolyFill);        // Resizes and fills mPolyFill from data in mPolyBounds
   mLabelAngle = angleOfLongestSide(mPolyBounds);
}


void PolygonGeometry::disableTriangulation()
{
   mTriangluationDisabled = true;
}


S32 PolygonGeometry::getMinVertCount() const
{
   return 3;
}

};
