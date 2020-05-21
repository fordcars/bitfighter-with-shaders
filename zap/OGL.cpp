//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "OGL.h" // Include this first!

#include "glad/glad.h" // This should to be the only place we include this!

#define MAX_NUMBER_OF_VERTICES 300000

#include "Point.h"

#include "tnlTypes.h"
#include "tnlLog.h"

#include "Utils.hpp" // For testing

#include "glm/gtc/matrix_transform.hpp" // For matrix transformations
#include "glm/gtc/type_ptr.hpp" // For GL vertex array to glm::vec conversions

#include <string> // For debug messages
#include <type_traits> // For is_same
#include <cstddef> // For size_t

// TEST
#include "SDL.h" // To get time

// Remove names we will use, since GLAD #defines a bunch of them (which causes bad stuff).
#undef glColor
#undef glColor4f
#undef glColor4d

#undef glLoadMatrixf
#undef glLoadMatrixd

#undef glScale
#undef glScalef
#undef glScaled

#undef glTranslate
#undef glTranslatef
#undef glTranslated

#undef glRotate
#undef glRotatef

#undef glLineWidth
#undef glViewport
#undef glScissor
#undef glPointSize
#undef glLoadIdentity
#undef glOrtho

#undef glClear
#undef glClearColor
#undef glReadBuffer
#undef glPixelStore
#undef glPixelStorei
#undef glReadPixels
#undef glBlendFunc
#undef glDepthFunc

#undef glIsTexture
#undef glActiveTexture
#undef glBindTexture

#undef glTexImage2D
#undef glTexSubImage2D
#undef glGenTextures
#undef glDeleteTextures

#undef glTexParameteri

#undef glGetValue
#undef glGetBooleanv
#undef glGetIntegerv
#undef glGetFloatv
#undef glGetDoublev
#undef glGetString
#undef glGetError

#undef glGetFloatv
#undef glPushMatrix
#undef glPopMatrix
#undef glMatrixMode

#undef glEnable
#undef glDisable
#undef glIsEnabled

namespace Zap
{

namespace GLOPT
{
	// GL functions
	// (Functions have external linkage by default).
	void glColor(const Color &c, float alpha){GLWrap::getGL()->glColor(c, alpha);}
	void glColor(const Color *c, float alpha){GLWrap::getGL()->glColor(c, alpha);}
	void glColor(F32 c, float alpha){GLWrap::getGL()->glColor(c, alpha);}
	void glColor(F32 r, F32 g, F32 b){GLWrap::getGL()->glColor(r, g, b);}
	void glColor(F32 r, F32 g, F32 b, F32 alpha){GLWrap::getGL()->glColor(r, g, b, alpha);}
	void glColor(F64 r, F64 g, F64 b, F64 alpha){GLWrap::getGL()->glColor(r, g, b, alpha);}
	void glColor4f(F32 r, F32 g, F32 b, F32 alpha){GLWrap::getGL()->glColor(r, g, b, alpha);}
	void glColor4d(F64 r, F64 g, F64 b, F64 alpha){GLWrap::getGL()->glColor(r, g, b, alpha);}

	void glLoadMatrixf(const F32 *m){GLWrap::getGL()->glLoadMatrixf(m);}
	void glLoadMatrixd(const F64 *m){GLWrap::getGL()->glLoadMatrixd(m);}

	void glScale(const Point &scaleFactor){GLWrap::getGL()->glScale(scaleFactor);}
	void glScale(F32 scaleFactor){GLWrap::getGL()->glScale(scaleFactor);}
	void glScale(F32 xScaleFactor, F32 yScaleFactor){GLWrap::getGL()->glScale(xScaleFactor, yScaleFactor);}
	void glScale(F32 xScaleFactor, F32 yScaleFactor, F32 zScaleFactor){
			GLWrap::getGL()->glScale(xScaleFactor, yScaleFactor, zScaleFactor);}
	void glScale(F64 xScaleFactor, F64 yScaleFactor, F64 zScaleFactor){
			GLWrap::getGL()->glScale(xScaleFactor, yScaleFactor, zScaleFactor);}
	void glScalef(F32 xScaleFactor, F32 yScaleFactor, F32 zScaleFactor){
			GLWrap::getGL()->glScale(xScaleFactor, yScaleFactor, zScaleFactor);}
	void glScaled(F64 xScaleFactor, F64 yScaleFactor, F64 zScaleFactor){
			GLWrap::getGL()->glScale(xScaleFactor, yScaleFactor, zScaleFactor);}

	void glTranslate(const Point &pos){GLWrap::getGL()->glTranslate(pos);}
	void glTranslate(F32 x, F32 y){GLWrap::getGL()->glTranslate(x, y);}
	void glTranslate(F32 x, F32 y, F32 z){GLWrap::getGL()->glTranslate(x, y, z);}
	void glTranslate(F64 x, F64 y, F64 z){GLWrap::getGL()->glTranslate(x, y, z);}
	void glTranslatef(F32 x, F32 y, F32 z){GLWrap::getGL()->glTranslate(x, y, z);}
	void glTranslated(F64 x, F64 y, F64 z){GLWrap::getGL()->glTranslate(x, y, z);}

	void glRotate(F32 angle){GLWrap::getGL()->glRotate(angle);}
	void glRotate(F32 angle, F32 x, F32 y, F32 z){GLWrap::getGL()->glRotate(angle, x, y, z);}
	void glRotatef(F32 angle, F32 x, F32 y, F32 z){GLWrap::getGL()->glRotate(angle, x, y, z);}

	void glLineWidth(F32 width){GLWrap::getGL()->glLineWidth(width);}
	void glViewport(S32 x, S32 y, S32 width, S32 height){GLWrap::getGL()->glViewport(x, y, width, height);}
	void glViewport(S32 x, S32 y, U32 width, U32 height){GLWrap::getGL()->glViewport(x, y, width, height);}
	void glScissor(S32 x, S32 y, S32 width, S32 height){GLWrap::getGL()->glScissor(x, y, width, height);}
	void glPointSize(F32 size){GLWrap::getGL()->glPointSize(size);}
	void glLoadIdentity(){GLWrap::getGL()->glLoadIdentity();}
	void glOrtho(F64 left, F64 right, F64 bottom, F64 top, F64 near, F64 far){
			GLWrap::getGL()->glOrtho(left, right, bottom, top, near, far);}

	void glClear(U32 mask){GLWrap::getGL()->glClear(mask);}
	void glClearColor(F32 red, F32 green, F32 blue, F32 alpha){
			GLWrap::getGL()->glClearColor(red, green, blue, alpha);}

	void glReadBuffer(U32 mode){GLWrap::getGL()->glReadBuffer(mode);}
	void glPixelStore(U32 name, S32 param){GLWrap::getGL()->glPixelStore(name, param);}
	void glPixelStorei(U32 name, S32 param){GLWrap::getGL()->glPixelStore(name, param);}
	void glReadPixels(S32 x, S32 y, U32 width, U32 height, U32 format, U32 type, void *data){
			GLWrap::getGL()->glReadPixels(x, y, width, height, format, type, data);}

	void glBlendFunc(U32 sourceFactor, U32 destFactor){GLWrap::getGL()->glBlendFunc(sourceFactor, destFactor);}
	void glDepthFunc(U32 func){GLWrap::getGL()->glDepthFunc(func);}

	bool glIsTexture(U32 texture){return GLWrap::getGL()->glIsTexture(texture);}
	void glActiveTexture(U32 texture){GLWrap::getGL()->glActiveTexture(texture);}
	void glBindTexture(U32 target, U32 texture){GLWrap::getGL()->glBindTexture(target, texture);}

	void glTexImage2D(U32 target, S32 level, S32 internalformat, U32 width,
		U32 height, S32 border, U32 format, U32 type, const void *data){
			GLWrap::getGL()->glTexImage2D(target, level, internalformat, width, height, border, format, type, data);}

	void glTexSubImage2D(U32 target, S32 level, S32 xoffset, S32 yoffset, U32 width,
		U32 height, U32 format, U32 type, const void *pixels){
			GLWrap::getGL()->glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);}

	void glGenTextures(U32 n, U32 *textures){GLWrap::getGL()->glGenTextures(n, textures);}
	void glDeleteTextures(U32 n, const U32 *textures){GLWrap::getGL()->glDeleteTextures(n, textures);}

	void glTexParameteri(U32 target, U32 pname, S32 param){GLWrap::getGL()->glTexParameteri(target, pname, param);}

	void glGetValue(U32 name, bool *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetValue(U32 name, U8 *fill){GLWrap::getGL()->glGetValue(name, reinterpret_cast<bool*>(fill));} // Sorry
	void glGetValue(U32 name, S32 *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetValue(U32 name, F32 *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetValue(U32 name, F64 *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetBooleanv(U32 name, bool *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetIntegerv(U32 name, S32 *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetFloatv(U32 name, F32 *fill){GLWrap::getGL()->glGetValue(name, fill);}
	void glGetDoublev(U32 name, F64 *fill){GLWrap::getGL()->glGetValue(name, fill);}
	const U8* glGetString(U32 name){return GLWrap::getGL()->glGetString(name);}
	U32 glGetError(){return GLWrap::getGL()->glGetError();}

	void glPushMatrix(){GLWrap::getGL()->glPushMatrix();}
	void glPopMatrix(){GLWrap::getGL()->glPopMatrix();}
	void glMatrixMode(U32 mode){GLWrap::getGL()->glMatrixMode(mode);}

	void glEnable(U32 option){GLWrap::getGL()->glEnable(option);}
	void glDisable(U32 option){GLWrap::getGL()->glDisable(option);}
	bool glIsEnabled(U32 option){return GLWrap::getGL()->glIsEnabled(option);}

	// Custom functions
	void setDefaultBlendFunction(){GLWrap::getGL()->setDefaultBlendFunction();}

	void renderColorVertexArray(const F32 vertices[], const F32 colors[], U32 vertCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderColorVertexArray(vertices, colors, vertCount, geomType, start, stride);}
	void renderTexturedVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderTexturedVertexArray(vertices, UVs, vertCount, geomType, start, stride);}
	void renderColoredTextureVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderColoredTextureVertexArray(vertices, UVs, vertCount, geomType, start, stride);}

	void renderVertexArray(const S8 verts[], U32 vertCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderVertexArray(verts, vertCount, geomType, start, stride);}
	void renderVertexArray(const S16 verts[], U32 vertCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderVertexArray(verts, vertCount, geomType, start, stride);}
	void renderVertexArray(const F32 verts[], U32 vertCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderVertexArray(verts, vertCount, geomType, start, stride);}

	void renderPointArray(const Point points[], U32 pointCount, U32 geomType, U32 start, U32 stride){
		GLWrap::getGL()->renderPointArray(points, pointCount, geomType, start, stride);}

	void renderPointVector(const Vector<Point> *points, U32 geomType){
		GLWrap::getGL()->renderPointVector(points, geomType);}
	// Same, but with points offset some distance
	void renderPointVector(const Vector<Point> *points, const Point &offset, U32 geomType){
		GLWrap::getGL()->renderPointVector(points, offset, geomType);}
	void renderPointVector(const Vector<Point> *points, U32 start, U32 vertCount, U32 geomType){
		GLWrap::getGL()->renderPointVector(points, start, vertCount, geomType);}

	void renderLine(const Vector<Point> *points){
		GLWrap::getGL()->renderLine(points);}
}

// GLAD
// Out of GLOPT{} to not confuse C++.
int GLOPT::gladLoadGL(){return ::gladLoadGL();}

// z for Zap!
// Macros forced us to change the name of constants.
const U32 GLOPT::zGL_VENDOR = GL_VENDOR;
const U32 GLOPT::zGL_RENDERER = GL_RENDERER;
const U32 GLOPT::zGL_VERSION = GL_VERSION;

const U32 GLOPT::zGL_NO_ERROR = GL_NO_ERROR;

const U32 GLOPT::zGL_FRONT = GL_FRONT;
const U32 GLOPT::zGL_BACK = GL_BACK;
const U32 GLOPT::zGL_BLEND = GL_BLEND;
const U32 GLOPT::zGL_COLOR_BUFFER_BIT = GL_COLOR_BUFFER_BIT;
const U32 GLOPT::zGL_DEPTH_BUFFER_BIT = GL_DEPTH_BUFFER_BIT;
const U32 GLOPT::zGL_DEPTH_TEST = GL_DEPTH_TEST;
const U32 GLOPT::zGL_DEPTH_WRITEMASK = GL_DEPTH_WRITEMASK;

const U32 GLOPT::zGL_UNSIGNED_BYTE = GL_UNSIGNED_BYTE;
const U32 GLOPT::zGL_FLOAT = GL_FLOAT;
const U32 GLOPT::zGL_LESS = GL_LESS;

const U32 GLOPT::zGL_LINE_SMOOTH = GL_LINE_SMOOTH;
const U32 GLOPT::zGL_POLYGON_SMOOTH = GL_POLYGON_SMOOTH;

const U32 GLOPT::zGL_LINE_LOOP = GL_LINE_LOOP;
const U32 GLOPT::zGL_LINE_STRIP = GL_LINE_STRIP;
const U32 GLOPT::zGL_LINES = GL_LINES;
const U32 GLOPT::zGL_POINTS = GL_POINTS;

const U32 GLOPT::zGL_VIEWPORT = GL_VIEWPORT;
const U32 GLOPT::zGL_MODELVIEW = 11111111;
const U32 GLOPT::zGL_MODELVIEW_MATRIX = 2222222;
const U32 GLOPT::zGL_PROJECTION = 33333333;
const U32 GLOPT::zGL_PROJECTION_MATRIX = 44444444;

const U32 GLOPT::zGL_NEAREST = GL_NEAREST;

const U32 GLOPT::zGL_ONE = GL_ONE;
const U32 GLOPT::zGL_ONE_MINUS_DST_COLOR = GL_ONE_MINUS_DST_COLOR;
const U32 GLOPT::zGL_PACK_ALIGNMENT = GL_PACK_ALIGNMENT;

const U32 GLOPT::zGL_TEXTURE_2D = GL_TEXTURE_2D;
const U32 GLOPT::zGL_TEXTURE_MIN_FILTER = GL_TEXTURE_MIN_FILTER;
const U32 GLOPT::zGL_TEXTURE_MAG_FILTER = GL_TEXTURE_MAG_FILTER;
const U32 GLOPT::zGL_UNPACK_ALIGNMENT = GL_UNPACK_ALIGNMENT;
const U32 GLOPT::zGL_LINEAR = GL_LINEAR;

const U32 GLOPT::zGL_RGB = GL_RGB;
const U32 GLOPT::zGL_ALPHA = GL_ALPHA;
const U32 GLOPT::zGL_SCISSOR_BOX = GL_SCISSOR_BOX;
const U32 GLOPT::zGL_SCISSOR_TEST = GL_SCISSOR_TEST;
const U32 GLOPT::zGL_SHORT = GL_SHORT;
const U32 GLOPT::zGL_TRIANGLE_FAN = GL_TRIANGLE_FAN;
const U32 GLOPT::zGL_TRIANGLE_STRIP = GL_TRIANGLE_STRIP;
const U32 GLOPT::zGL_TRIANGLES = GL_TRIANGLES;

const U32 GLOPT::zGL_SRC_ALPHA = GL_SRC_ALPHA;
const U32 GLOPT::zGL_ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA;

GL *GLWrap::mGL = NULL;

GLWrap::GLWrap()
{
   // Do nothing
}


GLWrap::~GLWrap()
{
   // Do nothing
}


void GLWrap::init()
{
   TNLAssert(mGL == NULL, "GL Renderer should only be created once!");
   
   mGL = new GL();
}


void GLWrap::shutdown()
{
   TNLAssert(mGL != NULL, "GL Renderer should have been created; never called GLWrap::init()?");
   delete mGL;
}


GL *GLWrap::getGL()
{
   TNLAssert(mGL != NULL, "GL Renderer should not be NULL!  Run GLWrap::init() before calling this!");
   return mGL;
}

////////////////////////////////////
////////////////////////////////////
/// OpenGL API abstraction
///

// The static shader needs to output to gl_FragData[0]!
GL::GL()
	: mStaticShader("static", "shaders/static.v.glsl", "shaders/static.f.glsl"),
	mDynamicShader("dynamic", "shaders/dynamic.v.glsl", "shaders/dynamic.f.glsl"),
	mTexturedShader("textured", "shaders/textured.v.glsl", "shaders/textured.f.glsl"),
	mColoredTextureShader("coloredTexture", "shaders/coloredTexture.v.glsl", "shaders/coloredTexture.f.glsl")
{
	Utils::LOGPRINT("Starting GL...");

	mPositionBuffer = 0;
	mColorBuffer = 0;
	mUVBuffer = 0;

	mTextureEnabled = false;
	mAlpha = 1.0f;

	// Give each stack one identity matrix
	mModelViewMatrixStack.push(glm::mat4(1.0f));
	mProjectionMatrixStack.push(glm::mat4(1.0f));
	mMatrixMode = GLOPT::zGL_MODELVIEW;

	// Generate vertex buffers

	// Position buffer
	glGenBuffers(1, &mPositionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
	// Create a big reusable buffer, don't give it any data and tell OpenGL we will be modifying the buffer often
	// * 4 * 2 since it is vec2s of 32-bit floats
	glBufferData(GL_ARRAY_BUFFER, MAX_NUMBER_OF_VERTICES * 4 * 2, nullptr, GL_DYNAMIC_DRAW);

	// Color buffer
	glGenBuffers(1, &mColorBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mColorBuffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_NUMBER_OF_VERTICES * 4 * 4, nullptr, GL_DYNAMIC_DRAW); // * 4 for r,g,b,a

	// UV buffer
	glGenBuffers(1, &mUVBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, mUVBuffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_NUMBER_OF_VERTICES * 4 * 2, nullptr, GL_DYNAMIC_DRAW); // * 2 for vec2

	// Check if everything is ok
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		Utils::CRASH("Frame buffer creation failed!");
}


GL::~GL()
{
   // Do nothing
}

// API methods

void GL::glColor(const Color &c, float alpha)
{
	mColor = c;
	mAlpha = alpha;
}


void GL::glColor(const Color *c, float alpha)
{
	mColor = *c;
	mAlpha = alpha;
}


void GL::glColor(F32 c, float alpha)
{
	mColor = Color(c, c, c);
	mAlpha = alpha;
}


void GL::glColor(F32 r, F32 g, F32 b)
{
	mColor = Color(r, g, b);
	mAlpha = 1.0f;
}


void GL::glColor(F32 r, F32 g, F32 b, F32 alpha)
{
	mColor = Color(r, g, b);
	mAlpha = alpha;
}

// Results in loss of precision!
void GL::glColor(F64 r, F64 g, F64 b, F64 alpha)
{
	mColor = Color(static_cast<F32>(r),
			static_cast<F32>(g),
			static_cast<F32>(b));
	mAlpha = static_cast<F32>(alpha);
}

// m is in column-major, but glm deals with it accordingly.
void GL::glLoadMatrixf(const F32 *m)
{
	// Choose the right stack
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;
	// Replace top matrix
	stack.pop();
	stack.push(glm::make_mat4(m));
}

// Results in loss of precision!
void GL::glLoadMatrixd(const F64 *m)
{
	// Choose the right stack
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;
	// Replace top matrix
	stack.pop();
	stack.push(static_cast<glm::fmat4>(glm::make_mat4(m))); // Cast to float matrix
}


void GL::glScale(const Point &scaleFactor)
{
	// Choose the right stack
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	glm::mat4 newMatrix = glm::scale(stack.top(), glm::vec3(scaleFactor.x, scaleFactor.y, 1.0f));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glScale(F32 scaleFactor)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// This is how it is used in Bitfighter: no z scaling!
	glm::mat4 newMatrix = glm::scale(stack.top(), glm::vec3(scaleFactor, scaleFactor, 1.0f));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glScale(F32 xScaleFactor, F32 yScaleFactor)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	glm::mat4 newMatrix = glm::scale(stack.top(), glm::vec3(xScaleFactor, yScaleFactor, 1.0f));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glScale(F32 xScaleFactor, F32 yScaleFactor, F32 zScaleFactor)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	glm::mat4 newMatrix = glm::scale(stack.top(), glm::vec3(xScaleFactor, yScaleFactor, zScaleFactor));
	stack.pop();
	stack.push(newMatrix);
}

// Loss of precision!
void GL::glScale(F64 xScaleFactor, F64 yScaleFactor, F64 zScaleFactor)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// Precision loss
	glm::mat4 newMatrix = glm::scale(stack.top(), glm::vec3(static_cast<F32>(xScaleFactor),
							static_cast<F32>(yScaleFactor),
							static_cast<F32>(zScaleFactor)));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glTranslate(const Point &pos)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	glm::mat4 newMatrix = glm::translate(stack.top(), glm::vec3(pos.x, pos.y, 0.0f));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glTranslate(F32 x, F32 y)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	glm::mat4 newMatrix = glm::translate(stack.top(), glm::vec3(x, y, 0.0f));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glTranslate(F32 x, F32 y, F32 z)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	glm::mat4 newMatrix = glm::translate(stack.top(), glm::vec3(x, y, z));
	stack.pop();
	stack.push(newMatrix);
}

// Loss of precision!
void GL::glTranslate(F64 x, F64 y, F64 z)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// Precision loss
	glm::mat4 newMatrix = glm::translate(stack.top(), glm::vec3(static_cast<F32>(x),
							static_cast<F32>(y),
							static_cast<F32>(z)));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glRotate(F32 angle)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// GLM takes radians
	glm::mat4 newMatrix = glm::rotate(stack.top(), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glRotate(F32 angle, F32 x, F32 y, F32 z)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// GLM takes radians
	// This rotate() function part of GLM_GTX_transform extension.
	glm::mat4 newMatrix = glm::rotate(stack.top(), glm::radians(angle), glm::vec3(x, y, z));
	stack.pop();
	stack.push(newMatrix);
}


void GL::glLineWidth(F32 angle)
{
	glad_glLineWidth(angle); // Use GL macro
}


void GL::glViewport(S32 x, S32 y, S32 width, S32 height)
{
	glad_glViewport(x, y, width, height);
}


void GL::glViewport(S32 x, S32 y, U32 width, U32 height)
{
	glad_glViewport(x, y, width, height);
}


void GL::glScissor(S32 x, S32 y, S32 width, S32 height)
{
	glad_glScissor(x, y, width, height);
}


void GL::glPointSize(F32 size)
{
	glad_glPointSize(size);
}


void GL::glLoadIdentity()
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// Replace the top matrix with an identity matrix
	glm::mat4 newMatrix = glm::mat4(1.0f);
	stack.pop();
	stack.push(newMatrix);
}


void GL::glOrtho(F64 left, F64 right, F64 bottom, F64 top, F64 nearx, F64 farx)
{
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// Multiply the top matrix with an ortho matrix
	glm::mat4 ortho = glm::ortho(left, right, bottom, top, nearx, farx);
	glm::mat4 topMatrix = stack.top();

	stack.pop();
	stack.push(ortho * topMatrix);
}


void GL::glClear(U32 mask)
{
	glad_glClear(mask);
}


void GL::glClearColor(F32 red, F32 green, F32 blue, F32 alpha)
{
	glad_glClearColor(red, green, blue, alpha);
}


void GL::glReadBuffer(U32 mode)
{
	glad_glReadBuffer(mode);
}


void GL::glPixelStore(U32 name, S32 param)
{
	glad_glPixelStorei(name, param);
}


void GL::glReadPixels(S32 x, S32 y, U32 width, U32 height, U32 format, U32 type, void *data)
{
	glad_glReadPixels(x, y, width, height, format, type, data);
}


void GL::glBlendFunc(U32 sourceFactor, U32 destFactor)
{
	glad_glBlendFunc(sourceFactor, destFactor);
}


void GL::glDepthFunc(U32 function)
{
	glad_glDepthFunc(function);
}


bool GL::glIsTexture(U32 texture)
{
	return glad_glIsTexture(texture);
}


void GL::glActiveTexture(U32 texture)
{
	glad_glActiveTexture(texture);
}


void GL::glBindTexture(U32 target, U32 texture)
{
	glad_glBindTexture(target, texture);
}


void GL::glTexImage2D(U32 target, S32 level, S32 internalformat, U32 width,
	U32 height, S32 border, U32 format, U32 type, const void *data)
{
	glad_glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}


void GL::glTexSubImage2D(U32 target, S32 level, S32 xoffset, S32 yoffset, U32 width,
	U32 height, U32 format, U32 type, const void *pixels)
{
	glad_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}


void GL::glGenTextures(U32 n, U32 *textures)
{
	glad_glGenTextures(n, textures);
}


void GL::glDeleteTextures(U32 n, const U32 *textures)
{
	glad_glDeleteTextures(n, textures);
}


void GL::glTexParameteri(U32 target, U32 pname, S32 param)
{
	glad_glTexParameteri(target, pname, param);
}


void GL::glGetValue(U32 name, bool *fill)
{
	glad_glGetBooleanv(name, reinterpret_cast<unsigned char*>(fill)); // Sorry!
}


void GL::glGetValue(U32 name, S32 *fill)
{
	glad_glGetIntegerv(name, fill);
}


void GL::glGetValue(U32 name, F32 *fill)
{
	glad_glGetFloatv(name, fill);
}


void GL::glGetValue(U32 name, F64 *fill)
{
	glad_glGetDoublev(name, fill);
}


const U8* GL::glGetString(U32 name)
{
	return glad_glGetString(name);
}


U32 GL::glGetError()
{
	return glad_glGetError();
}


void GL::glPushMatrix()
{
	// Choose the right stack
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	// Duplicate the top matrix on top of the stack
	glm::mat4 currentMatrix = stack.top();
	stack.push(currentMatrix);
}


void GL::glPopMatrix()
{
	// Choose the right stack
	std::stack<glm::mat4>& stack = (mMatrixMode == GLOPT::zGL_MODELVIEW) ? mModelViewMatrixStack : mProjectionMatrixStack;

	stack.pop(); // Pop the stack, now the top matrix is the next one down
}


void GL::glMatrixMode(U32 mode)
{
	mMatrixMode = mode;
}


void GL::glEnable(U32 option)
{
	if(option == GL_TEXTURE_2D)
		mTextureEnabled = true;
	else
		glad_glEnable(option);
}


void GL::glDisable(U32 option)
{
	if(option == GL_TEXTURE_2D)
		mTextureEnabled = false;
	else
		glad_glDisable(option);
}


bool GL::glIsEnabled(U32 option)
{
	if(option == GL_TEXTURE_2D)
		return mTextureEnabled;
	else
		return glad_glIsEnabled(option);
}

void GL::setDefaultBlendFunction()
{
	glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Templated since we always need this, for different vert types
// Verts are always 2D.
// dataType is an OpenGL data type (example: GLfloat)
// This uses the static shader
// verts holds all vertices, vertexCount is the amount of vertices to draw (not the total amount of vertices)
// and start is a vertex offset to start drawing at.
template<typename T>
void GL::renderGL2VertexArray(U32 dataType, const T verts[], U32 vertCount, U32 geomType,
	U32 start, U32 stride)
{
	GLint shaderID = mStaticShader.getID();

	// Use this shader
	glad_glUseProgram(mStaticShader.getID());

	glm::mat4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
	glad_glUniformMatrix4fv(mStaticShader.findUniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
	glad_glUniform4f(mStaticShader.findUniform("color"), mColor.r, mColor.g, mColor.b, mAlpha);

	// FOR TESTING GIVE TIME
	//glUniform1i(mStaticShader.findUniform("time"), static_cast<int>(SDL_GetTicks()));

	// Get the vertex position attribute location in the shader
	GLint attribLocation = glad_glGetAttribLocation(shaderID, "vertexPosition_modelspace");

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
	// If the verts are points, take into account 2 values per point.
	if(std::is_same<T, Point>::value)
	{
		// 1 point per vertex
		U32 extraBytesPerVert = 0;
		if(stride > sizeof(Point)) // Should never be less than
			extraBytesPerVert = stride - sizeof(Point);

		glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
				(sizeof(Point) * vertCount) + (extraBytesPerVert * vertCount),
				verts + start);
	} else
	{
		// 2 floats per vertex
		U32 extraBytesPerVert = 0;
		if(stride > sizeof(F32)*2) // Should never be less than
			extraBytesPerVert = stride - sizeof(F32)*2;

		glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
				(sizeof(T) * vertCount * 2) + (extraBytesPerVert * vertCount),
				verts + (start * 2));
	}

	glad_glEnableVertexAttribArray(attribLocation); // Enable the attribute variable
	// Set the attribute to point to the buffer data
	glad_glVertexAttribPointer(
		attribLocation,			// Attribute index
		2,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		dataType,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// Draw!
	glad_glDrawArrays(geomType, 0, vertCount);
	glad_glDisableVertexAttribArray(attribLocation);
	
	glad_glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to the screen next time we draw
}

// Colors have an alpha channel!
// This uses the dynamic shader.
// Verts are always 2D.
void GL::renderColorVertexArray(const F32 vertices[], const F32 colors[], U32 vertCount,
      U32 geomType, U32 start, U32 stride)
{
	GLint shaderID = mDynamicShader.getID();

	// Use this shader
	glad_glUseProgram(mDynamicShader.getID());

	glm::mat4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
	glad_glUniformMatrix4fv(mDynamicShader.findUniform("MVP"), 1, GL_FALSE, &MVP[0][0]);

	// FOR TESTING GIVE TIME
	//glUniform1i(mDynamicShader.findUniform("time"), static_cast<int>(SDL_GetTicks()));

	// Attribute locations
	GLint vertexPositionAttrib = glad_glGetAttribLocation(shaderID, "vertexPosition_modelspace");
	GLint colorAttrib = glad_glGetAttribLocation(shaderID, "vertexColor");

	U32 extraBytesPerVert = 0;
	if(stride > sizeof(F32)*2) // Should never be less than
		extraBytesPerVert = stride - sizeof(F32)*2;

	// Positions

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
	glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
			(sizeof(F32) * vertCount * 2) + (extraBytesPerVert * vertCount),
			vertices + (start*2));

	glad_glEnableVertexAttribArray(vertexPositionAttrib); // Enable the attribute variable
	glad_glVertexAttribPointer(
		vertexPositionAttrib,		// Attribute index
		2,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		GL_FLOAT,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// Colors

	U32 extraBytesPerColorVert = 0;
	if(stride > sizeof(F32)*4) // Should never be less than
		extraBytesPerColorVert = stride - sizeof(F32)*4;

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mColorBuffer);
	// 4D colors
	glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
			(sizeof(F32) * vertCount * 4) + (extraBytesPerColorVert * vertCount),
			colors + (start*4));

	glad_glEnableVertexAttribArray(colorAttrib);
	glad_glVertexAttribPointer(
		colorAttrib,			// Attribute index
		4,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		GL_FLOAT,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// Draw!
	glad_glDrawArrays(geomType, 0, vertCount);
	glad_glDisableVertexAttribArray(vertexPositionAttrib);
	glad_glDisableVertexAttribArray(colorAttrib);

	glad_glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to the screen next time we draw
}


// Verts are always 2D.
// Texture must be loaded and be in the active texture unit!
void GL::renderTexturedVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount,
      U32 geomType, U32 start, U32 stride)
{
	GLint shaderID = mTexturedShader.getID();

	// Use textured shader
	glad_glUseProgram(mTexturedShader.getID());

	glm::mat4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
	glad_glUniformMatrix4fv(mTexturedShader.findUniform("MVP"), 1, GL_FALSE, &MVP[0][0]);

	// Set texture sampler in shader to the active texture unit.
	GLint activeTexture = 0;
	glad_glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture); // Get active texture unit
	glad_glUniform1i(mTexturedShader.findUniform("textureSampler"), activeTexture);

	// Attribute locations
	GLint vertexPositionAttrib = glad_glGetAttribLocation(shaderID, "vertexPosition_modelspace");
	GLint UVAttrib = glad_glGetAttribLocation(shaderID, "vertexUV");

	U32 extraBytesPerVert = 0;
	if(stride > sizeof(F32)*2) // Should never be less than
		extraBytesPerVert = stride - sizeof(F32)*2;

	// Positions

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
	glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
			(sizeof(F32) * vertCount * 2) + (extraBytesPerVert * vertCount),
			vertices + (start*2));

	glad_glEnableVertexAttribArray(vertexPositionAttrib); // Enable the attribute variable
	glad_glVertexAttribPointer(
		vertexPositionAttrib,		// Attribute index
		2,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		GL_FLOAT,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// UV-coords

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mUVBuffer);
	glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
			(sizeof(F32) * vertCount * 2) + (extraBytesPerVert * vertCount),
			UVs + (start*2));

	glad_glEnableVertexAttribArray(UVAttrib);
	glad_glVertexAttribPointer(
		UVAttrib,			// Attribute index
		2,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		GL_FLOAT,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// Draw!
	glad_glDrawArrays(geomType, 0, vertCount);
	glad_glDisableVertexAttribArray(vertexPositionAttrib);
	glad_glDisableVertexAttribArray(UVAttrib);

	glad_glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to the screen next time we draw
}


// Verts are always 2D.
// Texture must be loaded and be in the active texture unit!
// Rendered texture will have the color set by glColor().
void GL::renderColoredTextureVertexArray(const F32 vertices[], const F32 UVs[], U32 vertCount,
      U32 geomType, U32 start, U32 stride)
{
	GLint shaderID = mColoredTextureShader.getID();

	// Use textured shader
	glad_glUseProgram(mColoredTextureShader.getID());

	glm::mat4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
	glad_glUniformMatrix4fv(mColoredTextureShader.findUniform("MVP"), 1, GL_FALSE, &MVP[0][0]);
	glad_glUniform4f(mColoredTextureShader.findUniform("color"), mColor.r, mColor.g, mColor.b, mAlpha);

	// Set texture sampler in shader to the active texture unit.
	GLint activeTexture = 0;
	glad_glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture); // Get active texture unit
	glad_glUniform1i(mColoredTextureShader.findUniform("textureSampler"), activeTexture);

	// Attribute locations
	GLint vertexPositionAttrib = glad_glGetAttribLocation(shaderID, "vertexPosition_modelspace");
	GLint UVAttrib = glad_glGetAttribLocation(shaderID, "vertexUV");

	U32 extraBytesPerVert = 0;
	if(stride > sizeof(F32)*2) // Should never be less than
		extraBytesPerVert = stride - sizeof(F32)*2;

	// Positions

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mPositionBuffer);
	glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
			(sizeof(F32) * vertCount * 2) + (extraBytesPerVert * vertCount),
			vertices + (start*2));

	glad_glEnableVertexAttribArray(vertexPositionAttrib); // Enable the attribute variable
	glad_glVertexAttribPointer(
		vertexPositionAttrib,		// Attribute index
		2,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		GL_FLOAT,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// UV-coords

	// Modify the buffer to give the data to the shader
	glad_glBindBuffer(GL_ARRAY_BUFFER, mUVBuffer);
	glad_glBufferSubData(GL_ARRAY_BUFFER, 0,
			(sizeof(F32) * vertCount * 2) + (extraBytesPerVert * vertCount),
			UVs + (start*2));

	glad_glEnableVertexAttribArray(UVAttrib);
	glad_glVertexAttribPointer(
		UVAttrib,			// Attribute index
		2,				// Size. Number of values per vertex, must be 1, 2, 3 or 4.
		GL_FLOAT,			// Type of data
		GL_FALSE,			// Normalized?
		stride,				// Stride
		(void*)0			// Array buffer offset
		);

	// Draw!
	glad_glDrawArrays(geomType, 0, vertCount);
	glad_glDisableVertexAttribArray(vertexPositionAttrib);
	glad_glDisableVertexAttribArray(UVAttrib);

	glad_glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to the screen next time we draw
}

// Always 2D points!
void GL::renderVertexArray(const S8 verts[], U32 vertCount, U32 geomType,
      U32 start, U32 stride)
{

	renderGL2VertexArray(GL_BYTE, verts, vertCount, geomType, start, stride);
}


void GL::renderVertexArray(const S16 verts[], U32 vertCount, U32 geomType,
      U32 start, U32 stride)
{
	renderGL2VertexArray(GL_SHORT, verts, vertCount, geomType, start, stride);
}


void GL::renderVertexArray(const F32 verts[], U32 vertCount, U32 geomType,
      U32 start, U32 stride)
{
	renderGL2VertexArray(GL_FLOAT, verts, vertCount, geomType, start, stride);

	/*glm::vec4 testPoint(verts[0], verts[1], 0, 1);
	testPoint = mProjectionMatrixStack.top() * mModelViewMatrixStack.top() * testPoint;

	Utils::LOGPRINT("");
	Utils::LOGPRINT("x: " + std::to_string(testPoint.x));
	Utils::LOGPRINT("y: " + std::to_string(testPoint.y));
	Utils::LOGPRINT("z: " + std::to_string(testPoint.z));
	Utils::LOGPRINT("w: " + std::to_string(testPoint.w));*/
}


void GL::renderPointArray(const Point points[], U32 pointCount, U32 geomType,
         U32 start, U32 stride)
{
	renderGL2VertexArray(GL_FLOAT, points, pointCount, geomType, start, stride);
}

// geomType: GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP, GL_TRIANGLES, GL_TRIANGLE_FAN, etc.
void GL::renderPointVector(const Vector<Point> *points, U32 geomType)
{
	renderGL2VertexArray(GL_FLOAT, points->address(), points->size(), geomType, 0, sizeof(Point));
}


void GL::renderPointVector(const Vector<Point> *points, const Point &offset, U32 geomType)
{
	// We defined those earlier, awesome!
	glPushMatrix();
		glTranslate(offset);
		renderGL2VertexArray(GL_FLOAT, points->address(), points->size(), geomType, 0, sizeof(Point));
	glPopMatrix();
}


void GL::renderPointVector(const Vector<Point> *points, U32 start, U32 vertCount, U32 geomType)
{
	renderGL2VertexArray(GL_FLOAT, points->address() + start, vertCount, geomType, 0, sizeof(Point));
}


void GL::renderLine(const Vector<Point> *points)
{
	renderGL2VertexArray(GL_FLOAT, points->address(), points->size(), GL_LINE_STRIP, 0, sizeof(Point));
}


} /* namespace Zap */
