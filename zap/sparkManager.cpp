//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UI.h"
#include "sparkManager.h"
#include "Teleporter.h"
#include "gameObjectRender.h"
#include "Colors.h"
#include "config.h"

#include "Renderer.h"
#include "RenderUtils.h"
#include "MathUtils.h"
#include "FontManager.h"

#include "tnlRandom.h"

#ifdef BF_PLATFORM_3DS
#include "PICARenderer.h"
#endif

using namespace TNL;

namespace Zap { namespace UI {

struct FxManager::TeleporterEffect
{
   Point pos;
   S32 time;
   U32 type;
   TeleporterEffect *nextEffect;
};

FxManager::FxManager()
{
   for(U32 i = 0; i < SparkTypeCount; i++)
   {
      firstFreeIndex[i] = 0;
      lastOverwrittenIndex[i] = 500;
   }
   teleporterEffects = NULL;
}


FxManager::~FxManager()
{
   TeleporterEffect *e = teleporterEffects;
   while(e != NULL)
   {
      TeleporterEffect *e_current = e;
      e = e->nextEffect;
      delete e_current;
   }
}


// Create a new spark.   ttl = Time To Live (milliseconds)
void FxManager::emitSpark(const Point &pos, const Point &vel, const Color &color, S32 ttl, UI::SparkType sparkType)
{
   Spark *s;
   Spark *s2;

   U32 sparkIndex;

   U8 slotsNeeded = (sparkType == SparkTypePoint ? 1 : 2);             // We need a U2 data type here!

   // Make sure we have room for an additional spark.  Regular sparks need one slot, line sparks need two.
   if(firstFreeIndex[sparkType] >= MAX_SPARKS - slotsNeeded)           // Spark list is full... need to overwrite an older spark
   {
      // Out of room for new sparks.  We'll jump elsewhere in our array and overwrite some older spark.
      // Overwrite every nth spark to avoid noticable artifacts by grabbing too many sparks from one place.
      // But make sure we grab a multiple of 2 to avoid wierdness with SparkTypeLine sparks, wich require proper byte alignment.
      // This doesn't matter for point sparks, but neither does it hurt.
      sparkIndex = (lastOverwrittenIndex[sparkType] + 100) % (MAX_SPARKS / 2 - 1) * 2;
      lastOverwrittenIndex[sparkType] = sparkIndex;
      TNLAssert(sparkIndex < MAX_SPARKS - slotsNeeded, "Spark error!");
   }
   else
   {
      sparkIndex = firstFreeIndex[sparkType];
      firstFreeIndex[sparkType] += slotsNeeded;    // Point sparks take 1 slot, line sparks need 2
   }

   s = mSparks[sparkType] + sparkIndex;            // Assign our spark to its slot

   s->pos = pos;
   s->vel = vel;
   s->color = color;

   // Use ttl if it was specified, otherwise pick something random
   s->ttl = ttl > 0 ? ttl : 15 * TNL::Random::readI(0, 1000);  // 0 - 15 seconds

   if(sparkType == SparkTypeLine)                  // Line sparks require two points; add the second here
   {
      s2 = mSparks[sparkType] + sparkIndex + 1;    // Since we know we had room for two, this one should be available
      Point len = vel;
      len.normalize(20);
      s2->pos = (pos - len);
      s2->vel = vel;
      s2->color = Color(color.r * 1, color.g * 0.25, color.b * 0.25);    // Give the trailing edge of this spark a fade effect
      s2->ttl = s->ttl;
   }
}


#define dr(x) degreesToRadians(x)
#define rd(x) radiansToDegrees(x)


void FxManager::DebrisChunk::idle(U32 timeDelta)
{
   F32 dT = F32(timeDelta) * 0.001f;      // Convert timeDelta to seconds

   pos   += vel      * dT;
   angle += rotation * dT;
   ttl   -= timeDelta;
}


void FxManager::DebrisChunk::render() const
{
   Renderer& r = Renderer::get();

   r.pushMatrix();

   r.translate(pos);
   r.rotate(rd(angle), 0, 0, 1);

   F32 alpha = 1;
   if(ttl < 250)
      alpha = ttl / 250.f;

   r.setColor(color, alpha);

   r.renderPointVector(&points, RenderType::LineLoop);

   r.popMatrix();
}


static const F32 MAX_TEXTEFFECT_SIZE = 10.0f;

void FxManager::TextEffect::idle(U32 timeDelta)
{
   F32 dTsecs = F32(timeDelta) * 0.001f;     // Convert timeDelta to seconds

   pos += vel * dTsecs;
   if(size < MAX_TEXTEFFECT_SIZE)
      size += growthRate * dTsecs;

   ttl -= timeDelta;
}


void FxManager::TextEffect::render() const
{
   Renderer& r = Renderer::get();

   F32 alpha = 1;
   if(ttl < 300)
      alpha = F32(ttl) / 300.f;     // Fade as item nears the end of its life
   r.setColor(color, alpha);
   //glLineWidth(size);
   r.pushMatrix();
      r.translate(pos);
      r.scale(size / MAX_TEXTEFFECT_SIZE);  // We'll draw big and scale down
      FontManager::pushFontContext(TextEffectContext);
         drawStringc(0.0f, 0.0f, 120.0f, text.c_str());
      FontManager::popFontContext();
   r.popMatrix();
   //glLineWidth(gDefaultLineWidth);
}


void FxManager::emitDebrisChunk(const Vector<Point> &points, const Color &color, const Point &pos, const Point &vel, S32 ttl, F32 angle, F32 rotation)
{
   DebrisChunk debrisChunk;

   debrisChunk.points   = points;
   debrisChunk.color    = color;
   debrisChunk.pos      = pos;
   debrisChunk.vel      = vel;
   debrisChunk.ttl      = ttl;
   debrisChunk.angle    = angle;
   debrisChunk.rotation = rotation;

   mDebrisChunks.push_back(debrisChunk);
}


void FxManager::emitTextEffect(const string &text, const Color &color, const Point &pos)
{
   TextEffect textEffect;

   textEffect.text  = text;
   textEffect.color = color;
   textEffect.pos   = pos;

   textEffect.vel  = Point(0,-130);
   textEffect.size = 0;
   textEffect.growthRate = 20;
   textEffect.ttl = 1500;

   mTextEffects.push_back(textEffect);
}


void FxManager::emitTeleportInEffect(const Point &pos, U32 type)
{
   TeleporterEffect *e = new TeleporterEffect;

   e->pos = pos;
   e->time = 0;
   e->type = type;
   e->nextEffect = teleporterEffects;
   teleporterEffects = e;
}


void FxManager::idle(U32 timeDelta)
{
   F32 dTsecs = timeDelta * .001f;

   for(U32 j = 0; j < SparkTypeCount; j++)
      for(U32 i = 0; i < firstFreeIndex[j]; )
      {
         Spark *theSpark = mSparks[j] + i;
         if(theSpark->ttl < (S32)timeDelta)
         {                          // Spark is dead -- remove it
            firstFreeIndex[j]--;
            *theSpark = mSparks[j][firstFreeIndex[j]];
         }
         else
         {
            theSpark->ttl -= timeDelta;
            theSpark->pos += theSpark->vel * dTsecs;
            if ((SparkType) j == SparkTypePoint)
            {
               if(theSpark->ttl > 1000)
                  theSpark->alpha = 1;
               else
                  theSpark->alpha = F32(theSpark->ttl) / 1000.f;
            }
            else if ((SparkType) j == SparkTypeLine)
            {
               if(theSpark->ttl > 250)
                  theSpark->alpha = 1;
               else
                  theSpark->alpha = F32(theSpark->ttl) / 250.f;
            }

            i++;
         }
      }


   // Kill off any old debris chunks, advance the others
   for(S32 i = 0; i < mDebrisChunks.size(); i++)
   {
      if(mDebrisChunks[i].ttl < (S32)timeDelta)
      {
         mDebrisChunks.erase_fast(i);
         i--;
      }
      else
         mDebrisChunks[i].idle(timeDelta);
   }


   // Same for our TextEffects
   for(S32 i = 0; i < mTextEffects.size(); i++)
   {
      if(mTextEffects[i].ttl < (S32)timeDelta)
      {
         mTextEffects.erase_fast(i);
         i--;
      }
      else
         mTextEffects[i].idle(timeDelta);
   }


   for(TeleporterEffect **walk = &teleporterEffects; *walk; )
   {
      TeleporterEffect *temp = *walk;
      temp->time += timeDelta;
      if(temp->time > Teleporter::TeleportInExpandTime)
      {
         *walk = temp->nextEffect;
         delete temp;
      }
      else
         walk = &(temp->nextEffect);
   }
}


void FxManager::render(S32 renderPass, F32 commanderZoomFraction) const
{
   Renderer& r = Renderer::get();

   // The teleporter effects should render under the ships and such
   if(renderPass == 0)
   {
      for(TeleporterEffect *walk = teleporterEffects; walk; walk = walk->nextEffect)
      {
         F32 radius = walk->time / F32(Teleporter::TeleportInExpandTime);
         F32 alpha = 1.0;

         if(radius > 0.5)
            alpha = (1 - radius) / 0.5f;

         Vector<Point> dummy;
         
         renderTeleporter(walk->pos, walk->type, false, Teleporter::TeleportInExpandTime - walk->time, commanderZoomFraction,
                          radius, Teleporter::TeleportInRadius, alpha, &dummy);
      }
   }

   else if(renderPass == 1)      // Time for sparks!!
   {
      for(S32 i = SparkTypeCount - 1; i >= 0; i --)     // Loop through our different spark types
      {
         RenderType renderType = (SparkType)i == SparkTypePoint ? RenderType::Points : RenderType::Lines;

         r.setPointSize(gDefaultLineWidth);

#ifdef BF_PLATFORM_3DS
         dynamic_cast<PICARenderer &>(r).renderSparks(mSparks[i], firstFreeIndex[i], renderType);
#else
         r.renderColored(
            (F32*)&mSparks[i][0].pos,
            (F32*)&mSparks[i][0].color,
            firstFreeIndex[i],      // Count
            renderType,             // RenderType
            0,                      // Start
            sizeof(Spark));         // Stride
#endif
      }

      for(S32 i = 0; i < mDebrisChunks.size(); i++)
         mDebrisChunks[i].render();

      for(S32 i = 0; i < mTextEffects.size(); i++)
         mTextEffects[i].render();
   }
}


// Create a circular pattern of long sparks, a-la bomb in Gridwars
void FxManager::emitBlast(const Point &pos, U32 size)
{
   const F32 speed = 800.0f;

   for(F32 i = 0.0f; i < 360.0f; i += 1)
   {
      Point dir = Point(cos(dr(i)), sin(dr(i)));
      // Emit a ring of bright orange sparks, as well as a whole host of yellow ones
      emitSpark(pos + dir * 50, dir * TNL::Random::readF() * 500, Colors::yellow,    TNL::Random::readI(0, U32(1000.f * F32(1000.f / speed))), SparkTypePoint );
      emitSpark(pos + dir * 50, dir * speed,                      Color(1, .8, .45), U32(1000.f * F32(size - 50) / speed),                     SparkTypeLine);
   }
}


void FxManager::emitExplosion(const Point &pos, F32 size, const Color *colorArray, U32 numColors)
{
   for(U32 i = 0; i < (250.0 * size); i++)
   {
      F32 th = TNL::Random::readF() * 2 * 3.14f;
      F32 f = (TNL::Random::readF() * 2 - 1) * 400 * size;
      
      S32 colorIndex = TNL::Random::readI() % numColors;
      S32 ttl        = S32(F32(TNL::Random::readI(0, 1000) + 2000) * size);

      emitSpark(pos, Point(cos(th)*f, sin(th)*f), colorArray[colorIndex], ttl);
   }
}


void FxManager::emitBurst(const Point &pos, const Point &scale, const Color &color1, const Color &color2)
{
   emitBurst(pos, scale, color1, color2, 250);
}


void FxManager::emitBurst(const Point &pos, const Point &scale, const Color &color1, const Color &color2, U32 sparkCount)
{
   F32 size = 1;

   for(U32 i = 0; i < sparkCount; i++)
   {

      F32 th = TNL::Random::readF() * 2 * FloatPi;                // angle
      F32 f = (TNL::Random::readF() * 0.1f + 0.9f) * 200 * size;

      Color color;
      color.interp(TNL::Random::readF(), color1, color2);         // Random blend of color1 and color2

      emitSpark(
            pos + Point(cos(th)*scale.x, sin(th)*scale.y),        // pos
            Point(cos(th)*scale.x*f, sin(th)*scale.y*f),          // vel
            color,                                                // color
            S32(TNL::Random::readI(0, 1000) * scale.len() * 3 + 1000.f * scale.len())  // ttl
      );
   }
}


void FxManager::clearSparks()
{
   // Remove all sparks
   for(U32 j = 0; j < SparkTypeCount; j++)
      for(U32 i = 0; i < firstFreeIndex[j]; )
      {
         Spark *theSpark = mSparks[j] + i;
         firstFreeIndex[j]--;
         *theSpark = mSparks[j][firstFreeIndex[j]];
      }
}


//-----------------------------------------------------------------------------

FxTrail::FxTrail(U32 dropFrequency, U32 len)
{
   mDropFreq = dropFrequency;
   mLength   = len;
   registerTrail();
}


FxTrail::~FxTrail()
{
   unregisterTrail();
}


void FxTrail::update(Point pos, TrailProfile profile)
{
   if(mNodes.size() < mLength)
   {
      TrailNode t;
      t.pos = pos;
      t.ttl = mDropFreq;
      t.profile = profile;

      mNodes.push_front(t);
   }
   else
   {
      mNodes[0].pos = pos;
      mNodes[0].profile = profile;
   }
}


void FxTrail::idle(U32 timeDelta)
{
   if(mNodes.size() == 0)
      return;

   mNodes.last().ttl -= timeDelta;
   if(mNodes.last().ttl < (S32)timeDelta)
      mNodes.pop_back();      // Delete last item
}


void FxTrail::render() const
{
   // Largest node size found was 15; I chose buffer of 32
   static F32 FxTrailVertexArray[64];     // 2 coordinates per node
   static F32 FxTrailColorArray[128];     // 4 colors components per node

   for(S32 i = 0; i < mNodes.size(); i++)
   {
      F32 t = ((F32)i / (F32)mNodes.size());

      F32 r, g, b, a, rFade, gFade, bFade, aFade;

      switch(mNodes[i].profile) {
         case ShipProfile:
            r = 1;      g = 1;      b = 1;      a = 0.7f;
            rFade = 2;  gFade = 2;  bFade = 0;  aFade = 0.7f;
            break;

         case CloakedShipProfile:
            r = 0;      g = 0;      b = 0;      a = 0.0f;
            rFade = 0;  gFade = 0;  bFade = 0;  aFade = 0.0f;
            break;

         case TurboShipProfile:
            r = 1;      g = 1;      b = 0;      a = 1;
            rFade = 1;  gFade = 1;  bFade = 0;  aFade = 1;
            break;

         case SeekerProfile:
            r = 0.5f;       g = 0.5f;    b = 0.5f;   a = 0.4f;
            rFade = 0.5f;   gFade = 1;   bFade = 1;  aFade = 0.2f;
            break;

         case RailgunProfile:
            r = 0.0f;       g = 0.5f;     b = 0.7f;     a = 0.6f;
            rFade = 0.0f;   gFade = 0.8f; bFade = 0.8f; aFade = 0.5f;
            break;

         default:
            TNLAssert(false, "No such profile!");
            break;
      }

      FxTrailColorArray[(4*i) + 0] = r - rFade * t;
      FxTrailColorArray[(4*i) + 1] = g - gFade * t;
      FxTrailColorArray[(4*i) + 2] = b - bFade * t;
      FxTrailColorArray[(4*i) + 3] = a - aFade * t;


      FxTrailVertexArray[(2*i) + 0] = mNodes[i].pos.x;
      FxTrailVertexArray[(2*i) + 1] = mNodes[i].pos.y;
   }

   Renderer::get().renderColored(FxTrailVertexArray, FxTrailColorArray, mNodes.size(), RenderType::LineStrip);
}


void FxTrail::reset()
{
   mNodes.clear();
}


Point FxTrail::getLastPos()
{
   if(mNodes.size())
   {
      return mNodes[0].pos;
   }
   else
      return Point(0,0);
}


FxTrail * FxTrail::mHead = NULL;

void FxTrail::registerTrail()
{
   FxTrail *n = mHead;
   mHead = this;
   mNext = n;
}


void FxTrail::unregisterTrail()
{
   // Find ourselves in the list (lame O(n) solution)
   FxTrail *w = mHead, *p = NULL;
   while(w)
   {
      if(w == this)
      {
         if(p)
         {
            p->mNext = w->mNext;
         }
         else
         {
            mHead = w->mNext;
         }
      }
      p = w;
      w = w->mNext;
   }
}


void FxTrail::renderTrails()
{
   FxTrail *w = mHead;
   while(w)
   {
      w->render();
      w = w->mNext;
   }
}

} } // Nexted namespace


