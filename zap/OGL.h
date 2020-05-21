//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

// If you include this file in a translation unit,
// you shouldn't include an additional OpenGL header!
// So, DON'T INCLUDE OpenGL headers IN HEADER FILES.
// When using this file, remember to enter the GLOPT namespace!

#ifndef OGL_H_
#define OGL_H_

#ifdef ZAP_DEDICATED
#  error "OGL.h should not be included in dedicated build"
#endif

#include "tnlTypes.h"
#include "tnlVector.h"
#include "Color.h"

// Don't include glad here!
#include "Shader.hpp"
#include "glm/glm.hpp"

#include <stack> // For matrix stacks
#include <stddef.h> // For std::size_t

using namespace TNL;

namespace Zap
{

// Here we abstract all GL_* options so no class will require the OpenGL
// headers.
// What does GLOPT mean??
// ALL GL things are in GLOPT!
class Point;
namespace GLOPT {
	// GL types
	using GLint = S32;
	using GLuint = U32;
	using GLfloat = F32;
	using GLboolean = bool;

	// GL functions
	void glColor(const Color &c, float alpha = 1.0);
	void glColor(const Color *c, float alpha = 1.0);
	void glColor(F32 c, float alpha = 1.0);
	void glColor(F32 r, F32 g, F32 b);
	void glColor(F32 r, F32 g, F32 b, F32 alpha);
	void glColor(F64 r, F64 g, F64 b, F64 alpha);
	void glColor4f(F32 r, F32 g, F32 b, F32 alpha);
	void glColor4d(F64 r, F64 g, F64 b, F64 alpha);

	void glLoadMatrixf(const F32 *m);
	void glLoadMatrixd(const F64 *m);

	void glScale(const Point &scaleFactor);
	void glScale(F32 scaleFactor);
	void glScale(F32 xScaleFactor, F32 yScaleFactor);
	void glScale(F32 xScaleFactor, F32 yScaleFactor, F32 zScaleFactor);
	void glScale(F64 xScaleFactor, F64 yScaleFactor, F64 zScaleFactor);
	void glScalef(F32 xScaleFactor, F32 yScaleFactor, F32 zScaleFactor);
	void glScaled(F64 xScaleFactor, F64 yScaleFactor, F64 zScaleFactor);

	void glTranslate(const Point &pos);
	void glTranslate(F32 x, F32 y);
	void glTranslate(F32 x, F32 y, F32 z);
	void glTranslate(F64 x, F64 y, F64 z);
	void glTranslatef(F32 x, F32 y, F32 z);
	void glTranslated(F64 x, F64 y, F64 z);

	void glRotate(F32 angle);
	void glRotate(F32 angle, F32 x, F32 y, F32 z);
	void glRotatef(F32 angle, F32 x, F32 y, F32 z);

	void glLineWidth(F32 width);
	void glViewport(S32 x, S32 y, S32 width, S32 height);
	void glViewport(S32 x, S32 y, U32 width, U32 height);
	void glScissor(S32 x, S32 y, S32 width, S32 height);
	void glPointSize(F32 size);
	void glLoadIdentity();
	void glOrtho(F64 left, F64 right, F64 bottom, F64 top, F64 near, F64 far);
	void glClear(U32 mask);
	void glClearColor(F32 red, F32 green, F32 blue, F32 alpha);
	void glReadBuffer(U32 mode);
	void glPixelStore(U32 name, S32 param);
	void glPixelStorei(U32 name, S32 param);
	void glReadPixels(S32 x, S32 y, U32 width, U32 height, U32 format, U32 type, void *data);

	void glBlendFunc(U32 sourceFactor, U32 destFactor);
	void glDepthFunc(U32 func);

	bool glIsTexture(U32 texture);
	void glActiveTexture(U32 texture);
	void glBindTexture(U32 target, U32 texture);

	void glTexImage2D(U32 target, S32 level, S32 internalformat, U32 width,
		U32 height, S32 border, U32 format, U32 type, const void *data);
	void glTexSubImage2D(U32 target, S32 level, S32 xoffset, S32 yoffset, U32 width,
		U32 height, U32 format, U32 type, const void *pixels);
	void glGenTextures(U32 n, U32 *textures);
	void glDeleteTextures(U32 n, const U32 *textures);

	void glTexParameteri(U32 target, U32 pname, S32 param);

	void glGetValue(U32 name, bool *fill);
	void glGetValue(U32 name, U8 *fill);
	void glGetValue(U32 name, S32 *fill);
	void glGetValue(U32 name, F32 *fill);
	void glGetValue(U32 name, F64 *fill);
	void glGetBooleanv(U32 name, bool *fill);
	void glGetIntegerv(U32 name, S32 *fill);
	void glGetFloatv(U32 name, F32 *fill);
	void glGetDoublev(U32 name, F64 *fill);
	const U8* glGetString(U32 name);
	U32 glGetError();

	void glPushMatrix();
	void glPopMatrix();
	void glMatrixMode(U32 mode);

	void glEnable(U32 option);
	void glDisable(U32 option);
	bool glIsEnabled(U32 option);

	// Custom functions
	void setDefaultBlendFunction();

	void renderColorVertexArray(const F32 vertices[], const F32 colors[], U32 vertCount,
		U32 geomType, U32 start = 0, U32 stride = 0);
	void renderTexturedVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount,
		U32 geomType, U32 start = 0, U32 stride = 0);
	void renderColoredTextureVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount,
		U32 geomType, U32 start = 0, U32 stride = 0);

	void renderVertexArray(const S8 verts[], U32 vertCount, U32 geomType,
		U32 start = 0, U32 stride = 0);
	void renderVertexArray(const S16 verts[], U32 vertCount, U32 geomType,
		U32 start = 0, U32 stride = 0);
	void renderVertexArray(const F32 verts[], U32 vertCount, U32 geomType,
		U32 start = 0, U32 stride = 0);

	void renderPointArray(const Point points[], U32 pointCount, U32 geomType,
		U32 start = 0, U32 stride = 0);

	void renderPointVector(const Vector<Point> *points, U32 geomType);
	// Same, but with points offset some distance
	void renderPointVector(const Vector<Point> *points, const Point &offset, U32 geomType);
	void renderPointVector(const Vector<Point> *points, U32 start, U32 vertCount, U32 geomType);

	void renderLine(const Vector<Point> *points);

	// GLAD
	int gladLoadGL();

	// z for Zap!
	extern const U32 zGL_VENDOR;
	extern const U32 zGL_RENDERER;
	extern const U32 zGL_VERSION;

	extern const U32 zGL_NO_ERROR;

	extern const U32 zGL_FRONT;
	extern const U32 zGL_BACK;
	extern const U32 zGL_BLEND;
	extern const U32 zGL_COLOR_BUFFER_BIT;
	extern const U32 zGL_DEPTH_BUFFER_BIT;
	extern const U32 zGL_DEPTH_TEST;
	extern const U32 zGL_DEPTH_WRITEMASK;

	extern const U32 zGL_UNSIGNED_BYTE;
	extern const U32 zGL_FLOAT;
	extern const U32 zGL_LESS;

	extern const U32 zGL_LINE_SMOOTH;
	extern const U32 zGL_POLYGON_SMOOTH;

	extern const U32 zGL_LINE_LOOP;
	extern const U32 zGL_LINE_STRIP;
	extern const U32 zGL_LINES;
	extern const U32 zGL_POINTS;

	extern const U32 zGL_VIEWPORT;
	extern const U32 zGL_MODELVIEW;
	extern const U32 zGL_MODELVIEW_MATRIX;
	extern const U32 zGL_PROJECTION;
	extern const U32 zGL_PROJECTION_MATRIX;

	extern const U32 zGL_NEAREST;
	extern const U32 zGL_FARTHEST;
	extern const U32 zGL_ONE;
	extern const U32 zGL_ONE_MINUS_DST_COLOR;
	extern const U32 zGL_PACK_ALIGNMENT;;

	extern const U32 zGL_TEXTURE_2D;
	extern const U32 zGL_TEXTURE_MIN_FILTER;
	extern const U32 zGL_TEXTURE_MAG_FILTER;
	extern const U32 zGL_UNPACK_ALIGNMENT;
	extern const U32 zGL_LINEAR;

	extern const U32 zGL_RGB;
	extern const U32 zGL_ALPHA;
	extern const U32 zGL_SCISSOR_BOX;
	extern const U32 zGL_SCISSOR_TEST;
	extern const U32 zGL_SHORT;
	extern const U32 zGL_TRIANGLE_FAN;
	extern const U32 zGL_TRIANGLE_STRIP;
	extern const U32 zGL_TRIANGLES;

	extern const U32 zGL_SRC_ALPHA;
	extern const U32 zGL_ONE_MINUS_SRC_ALPHA;
	
} /* namespace GLOPT */


class GL;
class GLWrap
{
protected:
   static GL *mGL;

public:
   GLWrap();
   virtual ~GLWrap();

   static void init();
   static void shutdown();

   static GL *getGL();
};


// This implementation is for using the OpenGL ES 1.1 API (which is a subset
// of desktop OpenGL 1.1 compatible [a subset]).
class GL
{
private:
	Shader mStaticShader;
	Shader mDynamicShader;
	Shader mTexturedShader;
	Shader mColoredTextureShader;

	// Reusable buffers holding vertex data
	U32 mPositionBuffer;
	// A buffer made for colors, like this we can access positions and colors at the same time
	U32 mColorBuffer;
	// UV-coord buffer (tex coords)
	U32 mUVBuffer;

	bool mTextureEnabled;
	Color mColor;
	float mAlpha;

	std::stack<glm::mat4> mModelViewMatrixStack;
	std::stack<glm::mat4> mProjectionMatrixStack;
	U32 mMatrixMode;

public:
   GL();          // Constructor
   virtual ~GL(); // Destructor

   // GL methods
   void glColor(const Color &c, float alpha = 1.0);
   void glColor(const Color *c, float alpha = 1.0);
   void glColor(F32 c, float alpha = 1.0);
   void glColor(F32 r, F32 g, F32 b);
   void glColor(F32 r, F32 g, F32 b, F32 alpha);
   void glColor(F64 r, F64 g, F64 b, F64 alpha);

   void glLoadMatrixf(const F32 *m);
   void glLoadMatrixd(const F64 *m);

   void glScale(const Point &scaleFactor);
   void glScale(F32 scaleFactor);
   void glScale(F32 xScaleFactor, F32 yScaleFactor);
   void glScale(F32 xScaleFactor, F32 yScaleFactor, F32 zScaleFactor);
   void glScale(F64 xScaleFactor, F64 yScaleFactor, F64 zScaleFactor);

   void glTranslate(const Point &pos);
   void glTranslate(F32 x, F32 y);
   void glTranslate(F32 x, F32 y, F32 z);
   void glTranslate(F64 x, F64 y, F64 z);

   void glRotate(F32 angle);
   void glRotate(F32 angle, F32 x, F32 y, F32 z);

   void glLineWidth(F32 width);
   void glViewport(S32 x, S32 y, S32 width, S32 height);
   void glViewport(S32 x, S32 y, U32 width, U32 height);
   void glScissor(S32 x, S32 y, S32 width, S32 height);
   void glPointSize(F32 size);
   void glLoadIdentity();
   void glOrtho(F64 left, F64 right, F64 bottom, F64 top, F64 near, F64 far);
   void glClear(U32 mask);
   void glClearColor(F32 red, F32 green, F32 blue, F32 alpha);
   void glReadBuffer(U32 mode);
   void glPixelStore(U32 name, S32 param);
   void glReadPixels(S32 x, S32 y, U32 width, U32 height, U32 format, U32 type, void *data);

   void glBlendFunc(U32 sourceFactor, U32 destFactor);
   void glDepthFunc(U32 func);

   bool glIsTexture(U32 texture);
   void glActiveTexture(U32 texture);
   void glBindTexture(U32 target, U32 texture);

   void glTexImage2D(U32 target, S32 level, S32 internalformat, U32 width,
         U32 height, S32 border, U32 format, U32 type, const void *data);
   void glTexSubImage2D(U32 target, S32 level, S32 xoffset, S32 yoffset, U32 width,
         U32 height, U32 format, U32 type, const void *pixels);
   void glGenTextures(U32 n, U32 *textures);
   void glDeleteTextures(U32 n, const U32 *textures);

   void glTexParameteri(U32 target, U32 pname, S32 param);

   void glGetValue(U32 name, bool *fill);
   void glGetValue(U32 name, S32 *fill);
   void glGetValue(U32 name, F32 *fill);
   void glGetValue(U32 name, F64 *fill);
   const U8* glGetString(U32 name);
   U32 glGetError();

   void glPushMatrix();
   void glPopMatrix();
   void glMatrixMode(U32 mode);

   void glEnable(U32 option);
   void glDisable(U32 option);
   bool glIsEnabled(U32 option);

   // Custom methods
   void setDefaultBlendFunction();

   // Templated since we always need this.
   template<typename T>
   void renderGL2VertexArray(U32 dataType, const T verts[], U32 vertCount, U32 geomType,
         U32 start = 0, U32 stride = 0);

   void renderColorVertexArray(const F32 vertices[], const F32 colors[], U32 vertCount,
         U32 geomType, U32 start = 0, U32 stride = 0);
   void renderTexturedVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount,
         U32 geomType, U32 start = 0, U32 stride = 0);
   void renderColoredTextureVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount,
         U32 geomType, U32 start = 0, U32 stride = 0);

   void renderVertexArray(const S8 verts[], U32 vertCount, U32 geomType,
         U32 start = 0, U32 stride = 0);
   void renderVertexArray(const S16 verts[], U32 vertCount, U32 geomType,
         U32 start = 0, U32 stride = 0);
   void renderVertexArray(const F32 verts[], U32 vertCount, U32 geomType,
         U32 start = 0, U32 stride = 0);

   void renderPointArray(const Point points[], U32 pointCount, U32 geomType,
         U32 start = 0, U32 stride = 0);

   void renderPointVector(const Vector<Point> *points, U32 geomType);
   // Same, but with points offset some distance
   void renderPointVector(const Vector<Point> *points, const Point &offset, U32 geomType);
   void renderPointVector(const Vector<Point> *points, U32 start, U32 vertCount, U32 geomType);

   void renderLine(const Vector<Point> *points);
};


} /* namespace Zap */

#endif /* OGL_H_ */
