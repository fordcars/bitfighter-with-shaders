//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _RENDER_UTILS_H_
#define _RENDER_UTILS_H_

#ifdef ZAP_DEDICATED
#  error "RenderUtils.h shouldn't be included in dedicated build"
#endif

#include "Point.h"
#include "Color.h"
#include "FontContextEnum.h"
#include "DisplayManager.h"

#include "tnlTypes.h"
#include "tnlVector.h"

#include "Renderer.h"

using namespace TNL;


namespace Zap {

void drawFilledRect(S32 x1, S32 y1, S32 x2, S32 y2);
void drawFilledRect(F32 x1, F32 y1, F32 x2, F32 y2);

void drawFilledRect(S32 x1, S32 y1, S32 x2, S32 y2, const Color &fillColor);
void drawFilledRect(S32 x1, S32 y1, S32 x2, S32 y2, const Color &fillColor, const Color &outlineColor);
void drawFilledRect(S32 x1, S32 y1, S32 x2, S32 y2, const Color &fillColor, F32 fillAlpha);
void drawFilledRect(S32 x1, S32 y1, S32 x2, S32 y2, const Color &fillColor, F32 fillAlpha, const Color &outlineColor);

void drawHollowRect(const Point &center, S32 width, S32 height);
void drawHollowRect(const Point &p1, const Point &p2);


void drawRect(S32 x1, S32 y1, S32 x2, S32 y2, RenderType type);
void drawRect(F32 x1, F32 y1, F32 x2, F32 y2, RenderType type);


// Allow any numeric arguments for drawHollowRect... getting sick of casting!
template<typename T, typename U, typename V, typename W>
void drawHollowRect(T x1, U y1, V x2, W y2)
{
   drawRect(static_cast<F32>(x1), static_cast<F32>(y1), static_cast<F32>(x2), static_cast<F32>(y2), RenderType::LineLoop);
}


template<typename T, typename U, typename V, typename W>
void drawHollowRect(T x1, U y1, V x2, W y2, const Color &outlineColor)
{
   Renderer::get().setColor(outlineColor.r, outlineColor.g, outlineColor.b, 1.0);
   drawHollowRect(x1, y1, x2, y2);
}

void drawFancyBox(F32 xLeft, F32 yTop, F32 xRight, F32 yBottom, F32 cornerInset, RenderType mode);

template<typename T, typename U, typename V, typename W, typename X>
void drawFancyBox(T xLeft, U yTop, V xRight, W yBottom, X cornerInset, RenderType mode)
{
   drawFancyBox(F32(xLeft), F32(yTop), F32(xRight), F32(yBottom), F32(cornerInset), mode);
}


void drawHollowFancyBox(S32 xLeft, S32 yTop, S32 xRight, S32 yBottom, S32 cornerInset);
void drawFilledFancyBox(S32 xLeft, S32 yTop, S32 xRight, S32 yBottom, S32 cornerInset, const Color &fillColor, F32 fillAlpha, const Color &borderColor);


void renderUpArrow(const Point &center, S32 size);
void renderDownArrow(const Point &center, S32 size);
void renderLeftArrow(const Point &center, S32 size);
void renderRightArrow(const Point &center, S32 size);


// Draw string at given location (normal and formatted versions)
// Note it is important that x be S32 because for longer strings, they are occasionally drawn starting off-screen
// to the left, and better to have them partially appear than not appear at all, which will happen if they are U32
void drawString(S32 x, S32 y, F32 size, const char *string);
void drawString(F32 x, F32 y, F32 size, const char *string);
void drawString(F32 x, F32 y, S32 size, const char *string);
void drawString(S32 x, S32 y, S32 size, const char *string);
void drawString(const Point &left, S32 size, const char *string);

void drawStringf(S32 x, S32 y, S32 size, const char *format, ...);
void drawStringf(F32 x, F32 y, F32 size, const char *format, ...);
void drawStringf(F32 x, F32 y, S32 size, const char *format, ...);

// Draw strings centered at point
S32 drawStringfc(F32 x, F32 y, F32 size, const char *format, ...);
S32 drawStringc (F32 x, F32 y, F32 size, const char *string);
S32 drawStringc (S32 x, S32 y, S32 size, const char *string);
S32 drawStringc(const Point &cen, F32 size, const char *string);



// Draw strings right-aligned at point
S32 drawStringfr(F32 x, F32 y, F32 size, const char *format, ...);
S32 drawStringfr(S32 x, S32 y, S32 size, const char *format, ...);
S32 drawStringr(S32 x, S32 y, S32 size, const char *string);

// Draw string and get it's width
S32 drawStringAndGetWidth(S32 x, S32 y, S32 size, const char *string);
S32 drawStringAndGetWidth(F32 x, F32 y, S32 size, const char *string);
S32 drawStringAndGetWidthf(S32 x, S32 y, S32 size, const char *format, ...);
S32 drawStringAndGetWidthf(F32 x, F32 y, S32 size, const char *format, ...);


// Original drawAngleString has a bug in positioning, but fixing it everywhere in the app would be a huge pain, so
// we've created a new drawAngleString function without the bug, called xx_fixed.  Actual work now moved to doDrawAngleString,
// which is marked private.  I think all usage of broken function has been removed, and _fixed can be renamed to something better.
void drawAngleString(F32 x, F32 y, F32 size, F32 angle, const char *string);
void drawAngleString_fixed(F32 x, F32 y, F32 size, F32 angle, const char *string);
void drawAngleStringf(F32 x, F32 y, F32 size, F32 angle, const char *format, ...);

// Center text between two points
void drawStringf_2pt(Point p1, Point p2, F32 size, F32 vert_offset, const char *format, ...);

S32 drawCenteredString_fixed(S32 y, S32 size, const char *str);
S32 drawCenteredString(S32 x, S32 y, S32 size, const char *str);
S32 drawCenteredString_fixed(S32 x, S32 y, S32 size, const char *str);
S32 drawCenteredString_fixed(F32 x, F32 y, S32 size, FontContext fontContext, const char *string);

F32 drawCenteredString(F32 x, F32 y, S32 size, const char *str);
F32 drawCenteredString(F32 x, F32 y, F32 size, const char *str);
S32 drawCenteredStringf(S32 y, S32 size, const char *format, ...);
S32 drawCenteredStringf(S32 x, S32 y, S32 size, const char *format, ...);

// Draw text centered on screen (normal and formatted versions)  --> now return starting location
template <typename T, typename U>
F32 drawCenteredString(T y, U size, const char *str)
{
   return drawCenteredString((F32)DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2.0f, (F32)y, (F32)size, str);
}

// Draw text at x,y --> fixes ye olde timee string rendering bug
template <typename T, typename U, typename V>
void drawString_fixed(T x, U y, V size, const char *string)
{
   drawAngleString(F32(x), F32(y), F32(size), 0, string);
}

void drawCenteredString_highlightKeys(S32 y, S32 size, const string &str, const Color &bodyColor, const Color &keyColor);


S32 drawCenteredUnderlinedString(S32 y, S32 size, const char *string);

S32 drawStringPair(S32 xpos, S32 ypos, S32 size, const Color &leftColor, const Color &rightColor, 
                             const char *leftStr, const char *rightStr);

S32 drawStringPair(S32 xpos, S32 ypos, S32 size, FontContext leftContext, FontContext rightContext, const Color &leftColor, const Color &rightColor,
                             const char *leftStr, const char *rightStr);

S32 drawCenteredStringPair(S32 xpos, S32 ypos, S32 size, const Color &leftColor, const Color &rightColor, 
                                    const char *leftStr, const char *rightStr);
S32 drawCenteredStringPair(S32 ypos, S32 size, const Color &leftColor, const Color &rightColor,
                                    const char *leftStr, const char *rightStr);
S32 drawCenteredStringPair(S32 xpos, S32 ypos, S32 size, FontContext leftContext, FontContext rightContext, const Color &leftColor, const Color &rightColor,
                                    const char *leftStr, const char *rightStr);

S32 getStringPairWidth(S32 size, const char *leftStr, const char *rightStr);

// Draw text centered in a left or right column (normal and formatted versions)  --> now return starting location
S32 drawCenteredString2Col(S32 y, S32 size, bool leftCol, const char *str);
S32 drawCenteredString2Colf(S32 y, S32 size, bool leftCol, const char *format, ...);
S32 drawCenteredStringPair2Colf(S32 y, S32 size, bool leftCol, const char *left, const char *right, ...);
S32 drawCenteredStringPair2Colf(S32 y, S32 size, bool leftCol, const Color &leftColor, const Color &rightColor,
      const char *left, const char *right, ...);

S32 drawCenteredStringPair2Col(S32 y, S32 size, bool leftCol, const Color &leftColor, const Color &rightColor,
      const char *left, const char *right);

// Get info about where text will be draw
S32 get2ColStartingPos(bool leftCol);
S32 getCenteredStringStartingPos(S32 size, const char *string);
S32 getCenteredStringStartingPosf(S32 size, const char *format, ...);
S32 getCenteredString2ColStartingPos(S32 size, bool leftCol, const char *string);
S32 getCenteredString2ColStartingPosf(S32 size, bool leftCol, const char *format, ...);

// Draw 4-column left-justified text
void drawString4Col(S32 y, S32 size, U32 col, const char *str);
void drawString4Colf(S32 y, S32 size, U32 col, const char *format, ...);

void drawTime(S32 x, S32 y, S32 size, S32 timeInMs, const char *prefixString = "");

// Return string rendering width (normal and formatted versions)
S32 getStringWidth(FontContext context, S32 size, const char *string);
F32 getStringWidth(FontContext context, F32 size, const char *string);

F32 getStringWidth(F32 size, const char *str);
S32 getStringWidth(S32 size, const char *str);

F32 getStringWidthf(F32 size, const char *format, ...);
S32 getStringWidthf(S32 size, const char *format, ...);

S32 getStringPairWidth(S32 size, FontContext leftContext, FontContext rightContext, const char* leftStr, const char* rightStr);

void wrapString(const string &str, S32 wrapWidth, S32 fontSize, FontContext context, Vector<string> &lines);
Vector<string> wrapString(const string &str, S32 width, S32 fontSize, const string indentPrefix = "");


U32 drawWrapText(const string &msg, S32 xpos, S32 ypos, S32 width, S32 ypos_end,
      S32 lineHeight, S32 fontSize, bool draw = true);

};


#endif
