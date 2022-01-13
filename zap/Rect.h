//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _RECT_H_
#define _RECT_H_

#include "Point.h"
#include "tnlTypes.h"

namespace TNL {
   template<class T> class Vector;
};


using namespace TNL;

namespace Zap
{

class IntRect
{
public:
   S32 minx, miny, maxx, maxy;
   IntRect();
   IntRect(S32 x1, S32 y1, S32 x2, S32 y2);

   void set(S32 x1, S32 y1, S32 x2, S32 y2);
};


////////////////////////////////////////
////////////////////////////////////////

class Rect
{

public:
   typedef float member_type;
   Point min, max;

   Rect();                                      // Constuctor
   Rect(const Point &p1, const Point &p2);      // Constuctor
   Rect(F32 x1, F32 y1, F32 x2, F32 y2);        // Constuctor
   // Try a templatized constructor as an alternative to casting casting casting
   template <typename T>
   Rect(const Point &p, T radius)             // Constuctor, takes centerpoint and "radius"
   {
      set(p, (F32)radius);
   }

   explicit Rect(const Rect *rect);             // Constructor, takes pointer to another rectangle

   explicit Rect(const Vector<Point> &p);       // Construct as a bounding box around multiple points

   Point getCenter() const;

   void set(const Point &p1, const Point &p2);

   void set(const Vector<Point> &p);     // Set to bounding box around multiple points

   void set(const Rect &r);
   void set(const Rect *r);

   void set(const Point &p, F32 radius);   // Takes point and "radius"

   bool contains(const Point &p) const;          // Returns true if rect contains p

   void unionPoint(const Point &p);

   void unionRect(const Rect &r);

   // Does rect interset rect r?
   bool intersects(const Rect &r);
   
   // Does rect interset or border on rect r?
   bool intersectsOrBorders(const Rect &r);

   // Does rect intersect line defined by p1 and p2?
   bool intersects(const Point &p1, const Point &p2) const;
   bool intersects(const Point &p1, const Point &p2, member_type &collisionTime) const;     // Sets collisionTime to where intersection occurs

   // Check for intersection with circle
   bool intersects(const Point &center, F32 radius) const;

   void expand(const Point &delta);
   void expandToInt(const Point &delta);

   void offset(const Point &offset);

   // Return a polygon from this Rect
   void toPoly(Vector<Point> &polyPoints);

   F32 getWidth() const;
   F32 getHeight() const;

   Point getExtents() const;

   string toString() const;

   // inlines must stay in headers
   inline Rect& operator=(const Rect &r)  // Performance equivalent to set()
   {
      set(r);
      return *this;
   }

   inline bool operator==(const Rect &r) const
   {
      return min == r.min && max == r.max;
   }

};

};	// namespace

#endif // _RECT_H_

