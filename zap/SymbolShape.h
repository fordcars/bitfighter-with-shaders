//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _SYMBOL_SHAPE_H_
#define _SYMBOL_SHAPE_H_

#include "FontContextEnum.h"
#include "InputCodeEnum.h"

#include "Joystick.h"      // For ButtonSymbol enum

#include "Color.h"
#include "Point.h"
#include "tnlVector.h"
#include "tnlTypes.h"

#include <memory>

using namespace TNL;

namespace Zap { 
   
class InputCodeManager;

namespace UI {


enum Alignment {
   AlignmentLeft,
   AlignmentCenter,
   AlignmentRight,
   AlignmentNone     // Unspecified alignment
};


////////////////////////////////////////
////////////////////////////////////////

// Parent for various Shape classes below
class SymbolShape 
{
protected:
   S32 mWidth, mHeight;
   Point mLabelOffset;
   S32 mLabelSizeAdjustor;
   bool mHasColor;
   Color mColor;

public:
   SymbolShape(S32 width = 0, S32 height = 0, const Color *color = NULL);
   virtual ~SymbolShape();

   void render(S32 x, S32 y, Alignment alignment) const;
   void render(F32 x, F32 y, Alignment alignment) const;

   void render(F32 x, F32 y) const;
   virtual void render(const Point &pos) const = 0;

   virtual S32 getWidth() const;
   virtual S32 getHeight() const;
   virtual bool getHasGap() const;  // Returns true if we automatically render a vertical blank space after this item
   virtual Point getLabelOffset(const string &label, S32 labelSize) const;
   virtual S32 getLabelSizeAdjustor(const string &label, S32 labelSize) const;
};


////////////////////////////////////////
////////////////////////////////////////


typedef shared_ptr<SymbolShape> SymbolShapePtr;


////////////////////////////////////////
////////////////////////////////////////

class SymbolBlank : public SymbolShape
{
   typedef SymbolShape Parent;

public:
   using Parent::render;      // http://stackoverflow.com/questions/72010/c-overload-resolution

   SymbolBlank(S32 width = -1, S32 height = -1);   // Constructor
   virtual ~SymbolBlank();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolHorizLine : public SymbolShape
{
   typedef SymbolShape Parent;

private:
   S32 mVertOffset;

public:
   using Parent::render;

   SymbolHorizLine(S32 width, S32 height, const Color *color);                   // Constructor
   SymbolHorizLine(S32 width, S32 vertOffset, S32 height, const Color *color);   // Constructor
   virtual ~SymbolHorizLine();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolRoundedRect : public SymbolShape
{
   typedef SymbolShape Parent;

protected:
   S32 mRadius;

public:
   using Parent::render;

   SymbolRoundedRect(S32 width, S32 height, S32 radius, const Color *color);   // Constructor
   virtual ~SymbolRoundedRect();

   virtual void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

// As above, but with slightly different rendering
class SymbolSmallRoundedRect : public SymbolRoundedRect
{
   typedef SymbolRoundedRect Parent;

public:
   using Parent::render;

   SymbolSmallRoundedRect(S32 width, S32 height, S32 radius, const Color *color);   // Constructor
   virtual ~SymbolSmallRoundedRect();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolHorizEllipse : public SymbolShape
{
   typedef SymbolShape Parent;

public:
   using Parent::render;

   SymbolHorizEllipse(S32 width, S32 height, const Color *color); // Constructor
   virtual ~SymbolHorizEllipse();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolRightTriangle : public SymbolShape
{
   typedef SymbolShape Parent;

public:
   using Parent::render;

   SymbolRightTriangle(S32 width, const Color *color); // Constructor
   virtual ~SymbolRightTriangle();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolDPadArrow : public SymbolShape
{
   typedef SymbolShape Parent;

private:
   Joystick::ButtonShape mButtonShape;

public:
   using Parent::render;

   SymbolDPadArrow(Joystick::ButtonShape buttonShape, const Color *color); // Constructor
   virtual ~SymbolDPadArrow();

   void render(const Point &center) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolCircle : public SymbolShape
{
   typedef SymbolShape Parent;

public:
   using Parent::render;

   SymbolCircle(S32 radius, const Color *color); // Constructor
   virtual ~SymbolCircle();

   virtual void render(const Point &pos) const;
   S32 getLabelSizeAdjustor(const string &label, S32 labelSize) const;
   Point getLabelOffset(const string &label, S32 labelSize) const;
};


////////////////////////////////////////
////////////////////////////////////////

// Small glyphs for rendering on joystick buttons
class SymbolButtonSymbol : public SymbolShape
{
   typedef SymbolShape Parent;

private:
   Joystick::ButtonSymbol mGlyph;

public:
   using Parent::render;

   SymbolButtonSymbol(Joystick::ButtonSymbol glyph);
   ~SymbolButtonSymbol();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolGear : public SymbolCircle
{
   typedef SymbolCircle Parent;

public:
   using Parent::render;

   SymbolGear(S32 fontSize);  // Constructor, fontSize is size of surrounding text
   virtual ~SymbolGear();

   virtual void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolGoal : public SymbolGear
{
   typedef SymbolGear Parent;

public:
   using Parent::render;

   SymbolGoal(S32 fontSize);  // Constructor, fontSize is size of surrounding text
   virtual ~SymbolGoal();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolNexus : public SymbolGear
{
   typedef SymbolGear Parent;

public:
   using Parent::render;

   SymbolNexus(S32 fontSize);  // Constructor, fontSize is size of surrounding text
   virtual ~SymbolNexus();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolSpinner : public SymbolCircle
{
   typedef SymbolCircle Parent;

public:
   using Parent::render;

   SymbolSpinner(S32 fontSize, const Color *color = NULL);  // Constructor, fontSize is size of surrounding text
   virtual ~SymbolSpinner();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolBullet : public SymbolShape
{
   typedef SymbolShape Parent;

public:
   using Parent::render;

   SymbolBullet();            // Constructor
   virtual ~SymbolBullet();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolText : public SymbolShape
{
   typedef SymbolShape Parent;

protected:
   string mText;
   FontContext mFontContext;
   S32 mFontSize;

public:
   using Parent::render;

   SymbolText(const string &text, S32 fontSize, FontContext context, const Color *color = NULL);
   SymbolText(const string &text, S32 fontSize, FontContext context, const Point &labelOffset, const Color *color = NULL);
   virtual ~SymbolText();

   virtual void render(const Point &pos) const;
   S32 getHeight() const;

   bool getHasGap() const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolKey : public SymbolText
{
   typedef SymbolText Parent;

public:
   using Parent::render;

   SymbolKey(const string &text, const Color *color = NULL);
   virtual ~SymbolKey();

   void render(const Point &pos) const;
};


////////////////////////////////////////
////////////////////////////////////////

// Symbol to be used when we don't know what symbol to use
class SymbolUnknown : public SymbolKey
{
   typedef SymbolKey Parent;

public:
   using Parent::render;

   SymbolUnknown(const Color *color);
   virtual ~SymbolUnknown();
};


////////////////////////////////////////
////////////////////////////////////////


class SymbolString : public SymbolShape      // So a symbol string can hold other symbol strings
{
   typedef SymbolShape Parent;

protected:
   Alignment mAlignment;

   Vector<shared_ptr<SymbolShape> > mSymbols;

public:
   SymbolString(const Vector<shared_ptr<SymbolShape> > &symbols, Alignment alignment = AlignmentNone);
   SymbolString(const        shared_ptr<SymbolShape>   &symbol,  Alignment alignment = AlignmentNone);
   SymbolString(const string &str, const InputCodeManager *inputCodeManager, FontContext context, 
                S32 textSize, bool blockMode, Alignment alignment = AlignmentNone);
   SymbolString();                     // Constructor (can't use until you've setSymbols)
   virtual ~SymbolString();            // Destructor

   void setSymbols(const Vector<shared_ptr<SymbolShape> > &symbols);
   void setSymbolsFromString(const string &string, const InputCodeManager *inputCodeManager,
                             FontContext fontContext, S32 textSize, const Color *color);
   void clear();

   // Dimensions
   virtual S32 getWidth() const;
   S32 getHeight() const;

   // Drawing
   S32 render(S32 x, S32 y, Alignment alignment, S32 blockWidth = -1) const;
   virtual S32 render(F32 x, F32 y, Alignment alignment, S32 blockWidth = -1) const;
   void render(const Point &center, Alignment alignment) const;
   void render(const Point &pos) const;
   void render(S32 x, S32 y) const;

   bool getHasGap() const;

   // Statics to make creating things a bit easier
   static SymbolShapePtr getControlSymbol(InputCode inputCode, const Color *color = NULL);
   static SymbolShapePtr getModifiedKeySymbol(InputCode inputCode, const Vector<string> &modifiers, const Color *color = NULL);
   static SymbolShapePtr getModifiedKeySymbol(const string &symbolName, const Color *color);
   static SymbolShapePtr getSymbolGear(S32 fontSize);
   static SymbolShapePtr getSymbolGoal(S32 fontSize);
   static SymbolShapePtr getSymbolNexus(S32 fontSize);
   static SymbolShapePtr getSymbolSpinner(S32 fontSize, const Color *color);
   static SymbolShapePtr getBullet();
   static SymbolShapePtr getSymbolText(const string &text, S32 fontSize, FontContext context, const Color *color = NULL);
   static SymbolShapePtr getBlankSymbol(S32 width = -1, S32 height = -1);
   static SymbolShapePtr getHorizLine(S32 length, S32 height, const Color *color);
   static SymbolShapePtr getHorizLine(S32 length, S32 vertOffset, S32 height, const Color *color);

   //
   static void symbolParse(const InputCodeManager *inputCodeManager, const string &str, Vector<SymbolShapePtr> &symbols,
                           FontContext fontContext, S32 fontSize, bool block, const Color *textColor = NULL, const Color *symColor = NULL);
};


////////////////////////////////////////
////////////////////////////////////////

// As above, but all sumbols are layered atop one another, to create compound symbols like controller buttons
class LayeredSymbolString : public SymbolString
{
   typedef SymbolString Parent;

public:
   LayeredSymbolString(const Vector<shared_ptr<SymbolShape> > &symbols);  // Constructor
   virtual ~LayeredSymbolString();                                               // Destructor

   S32 render(F32 x, F32 y, Alignment alignment, S32 blockWidth = -1) const;
};


////////////////////////////////////////
////////////////////////////////////////

class SymbolStringSet 
{
private:
   S32 mGap;

   Vector<SymbolString> mSymbolStrings;

public:
   SymbolStringSet(S32 gap);
   void clear();
   void add(const SymbolString &symbolString);
   S32 getHeight() const;
   S32 getWidth() const;
   S32 getItemCount() const;
   S32 render(F32 x, F32 y, Alignment alignment, S32 blockWidth = -1) const;
   S32 render(S32 x, S32 y, Alignment alignment, S32 blockWidth = -1) const;
   S32 renderLine(S32 line, S32 x, S32 y, Alignment alignment) const;
};


////////////////////////////////////////
////////////////////////////////////////


class SymbolStringSetCollection
{
private:
   Vector<SymbolStringSet> mSymbolSet;
   Vector<Alignment> mAlignment;
   Vector<S32> mXPos;

public:
   void clear();
   void addSymbolStringSet(const SymbolStringSet &set, Alignment alignment, S32 xpos);
   S32 render(S32 yPos) const;
};



} } // Nested namespace



#endif
