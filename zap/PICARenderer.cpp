//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifdef BF_PLATFORM_3DS

#include "GL2Renderer.h"
#include "Color.h"
#include "Point.h"
#include "tnlVector.h"
#include "MathUtils.h"
#include "glad/glad.h"
#include "SDL.h"

#include <memory>
#include <cstddef> // For size_t

namespace Zap
{

GL2Renderer::GL2Renderer()
   : mStaticShader("static", "static.v.glsl", "static.f.glsl")
   , mDynamicShader("dynamic", "dynamic.v.glsl", "dynamic.f.glsl")
   , mTexturedShader("textured", "textured.v.glsl", "textured.f.glsl")
   , mColoredTextureShader("coloredTexture", "coloredTexture.v.glsl", "coloredTexture.f.glsl")
   , mTextureEnabled(false)
   , mAlpha(1.0f)
   , mPointSize(1.0f)
   , mCurrentShaderId(0)
   , mUsingAndStencilTest(false)
   , mMatrixMode(MatrixType::ModelView)
{
#ifndef BF_USE_LEGACY_GL
#  ifdef BF_USE_GLES
   bool success = gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress);
#  else
   bool success = gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
#  endif

   TNLAssert(success, "Unable to load GL functions!");
#endif

   glDepthFunc(GL_LESS);
   glDepthMask(true);   // Always enable writing to depth buffer, needed for glClearing depth buffer

   glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
   glStencilMask(0xFF); // Always enable writing to stencil buffer, needed for glClearing stencil buffer

   glPixelStorei(GL_PACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   // Give each stack an identity matrix
   mModelViewMatrixStack.push(Matrix4());
   mProjectionMatrixStack.push(Matrix4());
	initRenderer();
}

GL2Renderer::~GL2Renderer()
{
	// Do nothing
}

void GL2Renderer::useShader(const Shader &shader)
{
   if(mCurrentShaderId != shader.getId())
      glUseProgram(shader.getId());
}

// Static
void GL2Renderer::create()
{
   setInstance(std::unique_ptr<Renderer>(new GL2Renderer));
}

// Uses static shader
template<typename T>
void GL2Renderer::renderGenericVertexArray(DataType dataType, const T verts[], U32 vertCount, RenderType type,
	U32 start, U32 stride, U32 vertDimension)
{
   useShader(mStaticShader);

	Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   mStaticShader.setMVP(MVP);
   mStaticShader.setColor(mColor, mAlpha);
   mStaticShader.setPointSize(mPointSize);
   mStaticShader.setTime(static_cast<GLuint>(SDL_GetTicks())); // Give time, it's always useful!

	// Get the position attribute location in the shader
	GLint attribLocation = mStaticShader.getAttributeLocation(AttributeName::VertexPosition);

	// Give position data to the shader, and deal with stride
   // Positions
   U32 bytesPerCoord = sizeof(T) * vertDimension;
   if(stride > bytesPerCoord) // Should never be less than
      bytesPerCoord = stride;

   mPositionBuffer.bind();
   std::size_t positionOffset = mPositionBuffer.insertData((U8 *)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		attribLocation,	       // Attribute index
		vertDimension,				 // Number of values per vertex
		getGLDataType(dataType), // Data type
		GL_FALSE,			       // Normalized?
		stride,				       // Stride
		(void *)positionOffset	 // Array buffer offset
	);

	// Draw!
	glDrawArrays(getGLRenderType(type), 0, vertCount);
}

U32 GL2Renderer::getGLRenderType(RenderType type) const
{
   switch(type)
   {
   case RenderType::Points:
      return GL_POINTS;

   case RenderType::Lines:
      return GL_LINES;

   case RenderType::LineStrip:
      return GL_LINE_STRIP;

   case RenderType::LineLoop:
      return GL_LINE_LOOP;

   case RenderType::Triangles:
      return GL_TRIANGLES;

   case RenderType::TriangleStrip:
      return GL_TRIANGLE_STRIP;

   case RenderType::TriangleFan:
      return GL_TRIANGLE_FAN;

   default:
      return 0;
   }
}

U32 GL2Renderer::getGLTextureFormat(TextureFormat format) const
{
   switch(format)
   {
   case TextureFormat::RGB:
      return GL_RGB;

   case TextureFormat::RGBA:
      return GL_RGBA;

   case TextureFormat::Alpha:
      return GL_ALPHA;

   default:
      return 0;
   }
}

U32 GL2Renderer::getGLDataType(DataType type) const
{
   switch(type)
   {
   case DataType::UnsignedByte:
      return GL_UNSIGNED_BYTE;

   case DataType::Byte:
      return GL_BYTE;

   case DataType::UnsignedShort:
      return GL_UNSIGNED_SHORT;

   case DataType::Short:
      return GL_SHORT;

   case DataType::UnsignedInt:
      return GL_UNSIGNED_INT;

   case DataType::Int:
      return GL_INT;

   case DataType::Float:
      return GL_FLOAT;

   default:
      return 0;
   }
}

void GL2Renderer::clear()
{
   glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GL2Renderer::clearStencil()
{
   glClear(GL_STENCIL_BUFFER_BIT);
}

void GL2Renderer::clearDepth()
{
   glClear(GL_DEPTH_BUFFER_BIT);
}

void GL2Renderer::setClearColor(F32 r, F32 g, F32 b, F32 alpha)
{
   glClearColor(r, g, b, alpha);
}

void GL2Renderer::setColor(F32 r, F32 g, F32 b, F32 alpha)
{
	mColor = Color(r, g, b);
	mAlpha = alpha;
}

void GL2Renderer::setPointSize(F32 size)
{
   mPointSize = size;

#ifndef BF_USE_GLES
   // GL2 does not support vertex shader gl_PointSize, while GLES2 does not suport glPointSize().
   glPointSize(size);
#endif
}

void GL2Renderer::setLineWidth(F32 width)
{
   glLineWidth(width);
}

void GL2Renderer::enableAntialiasing()
{
#ifndef BF_USE_GLES
   glEnable(GL_LINE_SMOOTH);
#endif
   // glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void GL2Renderer::disableAntialiasing()
{
#ifndef BF_USE_GLES
   glDisable(GL_LINE_SMOOTH);
#endif
}

void GL2Renderer::enableBlending()
{
   glEnable(GL_BLEND);
}

void GL2Renderer::disableBlending()
{
   glDisable(GL_BLEND);
}

// Any black pixel will become fully transparent
void GL2Renderer::useTransparentBlackBlending()
{
   glBlendFunc(GL_ONE, GL_ONE);
}

void GL2Renderer::useSpyBugBlending()
{
   // This blending works like this, source(SRC) * GL_ONE_MINUS_DST_COLOR + destination(DST) * GL_ONE
   glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
}

void GL2Renderer::useDefaultBlending()
{
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GL2Renderer::enableDepthTest()
{
   glEnable(GL_DEPTH_TEST);
}

void GL2Renderer::disableDepthTest()
{
   glDisable(GL_DEPTH_TEST);
}

/// Stencils
void GL2Renderer::enableStencil()
{
   glEnable(GL_STENCIL_TEST);
}

void GL2Renderer::disableStencil()
{
   // Enable writing to stencil in case we disabled it, needed for clearing buffer
   glStencilMask(0xFF);
   glDisable(GL_STENCIL_TEST);
}

void GL2Renderer::useAndStencilTest()
{
   // Render if stencil value == 1
   glStencilFunc(GL_EQUAL, 1, 0xFF);
   mUsingAndStencilTest = true;
}

void GL2Renderer::useNotStencilTest()
{
   // Render if stencil value != 1
   glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
   mUsingAndStencilTest = false;
}

void GL2Renderer::enableStencilDrawOnly()
{
   // Always draw to stencil buffer; we don't care what what's in there already
   glStencilFunc(GL_ALWAYS, 1, 0xFF);
   glStencilMask(0xFF);                                 // Draw 1s everywhere in stencil buffer
   glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // Don't draw to color buffer!
}

// Temporarily disable drawing to stencil
void GL2Renderer::disableStencilDraw()
{
   glStencilMask(0x00);                             // Don't draw anything in the stencil buffer
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // Feel free to draw in the color buffer tho!

   // Restore stencil test, it was probably modified by enableStencilDrawOnly()
   if(mUsingAndStencilTest)
      useAndStencilTest();
   else
      useNotStencilTest();
}

void GL2Renderer::setViewport(S32 x, S32 y, S32 width, S32 height)
{
   glViewport(x, y, width, height);
}

Point GL2Renderer::getViewportPos()
{
   GLint viewport[4];
   glGetIntegerv(GL_VIEWPORT, viewport);

   return Point(viewport[0], viewport[1]);
}

Point GL2Renderer::getViewportSize()
{
   GLint viewport[4];
   glGetIntegerv(GL_VIEWPORT, viewport);

   return Point(viewport[2], viewport[3]);
}

void GL2Renderer::enableScissor()
{
   glEnable(GL_SCISSOR_TEST);
}

void GL2Renderer::disableScissor()
{
   glDisable(GL_SCISSOR_TEST);
}

bool GL2Renderer::isScissorEnabled()
{
   GLboolean scissorEnabled;
   glGetBooleanv(GL_SCISSOR_TEST, &scissorEnabled);

   return scissorEnabled;
}

void GL2Renderer::setScissor(S32 x, S32 y, S32 width, S32 height)
{
   glScissor(x, y, width, height);
}

Point GL2Renderer::getScissorPos()
{
   GLint scissor[4];
   glGetIntegerv(GL_SCISSOR_BOX, scissor);

   return Point(scissor[0], scissor[1]);
}

Point GL2Renderer::getScissorSize()
{
   GLint scissor[4];
   glGetIntegerv(GL_SCISSOR_BOX, scissor);

   return Point(scissor[2], scissor[3]);
}

void GL2Renderer::scale(F32 x, F32 y, F32 z)
{
	// Choose correct stack
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	Matrix4 newMatrix = stack.top().scale(x, y, z);
	stack.pop();
	stack.push(newMatrix);
}

void GL2Renderer::translate(F32 x, F32 y, F32 z)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	Matrix4 newMatrix = stack.top().translate(x, y, z);
	stack.pop();
	stack.push(newMatrix);
}

void GL2Renderer::rotate(F32 degAngle, F32 x, F32 y, F32 z)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;

	Matrix4 newMatrix = stack.top().rotate(degreesToRadians(degAngle), x, y, z);
	stack.pop();
	stack.push(newMatrix);
}

void GL2Renderer::setMatrixMode(MatrixType type)
{
	mMatrixMode = type;
}

void GL2Renderer::getMatrix(MatrixType type, F32 *matrix)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	const F32 *sourceMatrix = stack.top().getData();

	for(int i = 0; i < 16; ++i)
		matrix[i] = sourceMatrix[i];
}

void GL2Renderer::pushMatrix()
{
	// Duplicate the top matrix on top of the stack
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.push(stack.top());
}

void GL2Renderer::popMatrix()
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
}

// m is column-major
void GL2Renderer::loadMatrix(const F32 *m)
{
	// Replace top matrix
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
	stack.push(Matrix4(m));
}

// Results in loss of precision!
void GL2Renderer::loadMatrix(const F64 *m)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
	stack.push(Matrix4(m));
}

void GL2Renderer::loadIdentity()
{
	// Replace the top matrix with an identity matrix
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
	stack.push(Matrix4());
}

void GL2Renderer::projectOrtho(F32 left, F32 right, F32 bottom, F32 top, F32 nearZ, F32 farZ)
{
	// Multiply the top matrix with an ortho matrix
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	Matrix4 topMatrix = stack.top();
	Matrix4 ortho = Matrix4::getOrthoProjection(left, right, bottom, top, nearZ, farZ);

	stack.pop();
	stack.push(ortho * topMatrix);
}

// Uses "nearest pixel" filtering when useLinearFiltering is false
U32 GL2Renderer::generateTexture(bool useLinearFiltering)
{
   GLuint textureHandle;
   glGenTextures(1, &textureHandle);

   // Set filtering
   GLint currentBinding;
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentBinding);
   glBindTexture(GL_TEXTURE_2D, textureHandle);

   if(useLinearFiltering)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   }
   else
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   }

   glBindTexture(GL_TEXTURE_2D, currentBinding); // Restore previous binding

   return textureHandle;
}

void GL2Renderer::bindTexture(U32 textureHandle)
{
   glBindTexture(GL_TEXTURE_2D, textureHandle);
}

bool GL2Renderer::isTexture(U32 textureHandle)
{
   return glIsTexture(textureHandle);
}

void GL2Renderer::deleteTexture(U32 textureHandle)
{
   glDeleteTextures(1, &textureHandle);
}

void GL2Renderer::setTextureData(TextureFormat format, DataType dataType, U32 width, U32 height, const void *data)
{
   U32 textureFormat = getGLTextureFormat(format);

   glTexImage2D(
      GL_TEXTURE_2D, 0, textureFormat,
      width, height, 0,
      textureFormat, getGLDataType(dataType), data);
}

void GL2Renderer::setSubTextureData(TextureFormat format, DataType dataType, S32 xOffset, S32 yOffset,
   U32 width, U32 height, const void *data)
{
   glTexSubImage2D(
      GL_TEXTURE_2D, 0,
      xOffset, yOffset,
      width, height,
      getGLTextureFormat(format),
      getGLDataType(dataType), data);
}

// Fairly slow operation
void GL2Renderer::readFramebufferPixels(TextureFormat format, DataType dataType, S32 x, S32 y, S32 width, S32 height, void *data)
{
   glReadPixels(
      x, y, width, height,
      getGLTextureFormat(format),
      getGLDataType(dataType),
      data);
}

void GL2Renderer::renderVertexArray(const S8 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	renderGenericVertexArray(DataType::Byte, verts, vertCount, type, start, stride, vertDimension);
}

void GL2Renderer::renderVertexArray(const S16 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	renderGenericVertexArray(DataType::Short, verts, vertCount, type, start, stride, vertDimension);
}

void GL2Renderer::renderVertexArray(const F32 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	renderGenericVertexArray(DataType::Float, verts, vertCount, type, start, stride, vertDimension);
}

void GL2Renderer::renderColored(const F32 verts[], const F32 colors[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   useShader(mDynamicShader);

	Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   mDynamicShader.setMVP(MVP);
   mDynamicShader.setPointSize(mPointSize);
   mDynamicShader.setTime(static_cast<GLuint>(SDL_GetTicks()));

	// Attribute locations
	GLint vertexPositionAttrib = mDynamicShader.getAttributeLocation(AttributeName::VertexPosition);
	GLint colorAttrib = mDynamicShader.getAttributeLocation(AttributeName::VertexColor);

	// Positions
	U32 bytesPerCoord = sizeof(F32) * vertDimension;
   if(stride > bytesPerCoord) // Should never be less than
      bytesPerCoord = stride;

   mPositionBuffer.bind();
   std::size_t positionOffset = mPositionBuffer.insertData((U8 *)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		vertexPositionAttrib,  // Attribute index
		vertDimension,			  // Number of values per vertex
		GL_FLOAT,			     // Data type
		GL_FALSE,			     // Normalized?
		stride,				     // Stride
		(void *)positionOffset // Array buffer offset
	);

	// Colors
	bytesPerCoord = sizeof(F32) * 4;
   if(stride > bytesPerCoord) // Should never be less than
      bytesPerCoord = stride;

   mColorBuffer.bind();
   std::size_t colorOffset = mColorBuffer.insertData((U8 *)colors + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		colorAttrib,          // Attribute index
		4,				          // Number of values per color
		GL_FLOAT,				 // Data type
		GL_FALSE,				 // Normalized?
		stride,					 // Stride
		(void *)colorOffset	 // Array buffer offset
	);

	// Draw!
	glDrawArrays(getGLRenderType(type), 0, vertCount);
}

void GL2Renderer::renderTextured(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   useShader(mTexturedShader);

	Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   mTexturedShader.setMVP(MVP);
   mTexturedShader.setTextureSampler(0); // Default texture unit
   mTexturedShader.setTime(static_cast<GLuint>(SDL_GetTicks()));

	// Attribute locations
	GLint vertexPositionAttrib = mTexturedShader.getAttributeLocation(AttributeName::VertexPosition);
	GLint UVAttrib = mTexturedShader.getAttributeLocation(AttributeName::VertexUV);

   // Positions
   U32 bytesPerCoord = sizeof(F32) * vertDimension;
   if(stride > bytesPerCoord)
      bytesPerCoord = stride;

   mPositionBuffer.bind();
   std::size_t positionOffset = mPositionBuffer.insertData((U8 *)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		vertexPositionAttrib,  // Attribute index
		vertDimension,			  // Number of values per vertex
		GL_FLOAT,			     // Data type
		GL_FALSE,			     // Normalized?
		stride,				     // Stride
		(void *)positionOffset // Array buffer offset
	);

	// UV-coords
   bytesPerCoord = sizeof(F32) * 2;
   if(stride > bytesPerCoord)
      bytesPerCoord = stride;

   mUVBuffer.bind();
   std::size_t UVOffset = mUVBuffer.insertData((U8 *)UVs + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		UVAttrib,			    // Attribute index
		2,				          // Number of values per coord
		GL_FLOAT,			    // Data type
		GL_FALSE,			    // Normalized?
		stride,				    // Stride
		(void *)UVOffset	    // Array buffer offset
	);

	// Draw!
	glDrawArrays(getGLRenderType(type), 0, vertCount);
}

// Render a texture colored by the current color:
void GL2Renderer::renderColoredTexture(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension, bool isAlphaTexture)
{
   useShader(mColoredTextureShader);

	// Uniforms
	Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   mColoredTextureShader.setMVP(MVP);
   mColoredTextureShader.setColor(mColor, mAlpha);
   mColoredTextureShader.setIsAlphaTexture(isAlphaTexture);
   mColoredTextureShader.setTextureSampler(0); // Default texture unit
   mColoredTextureShader.setTime(static_cast<GLuint>(SDL_GetTicks()));

	// Attribute locations
	GLint vertexPositionAttrib = mColoredTextureShader.getAttributeLocation(AttributeName::VertexPosition);
	GLint UVAttrib = mColoredTextureShader.getAttributeLocation(AttributeName::VertexUV);

	// Positions
   U32 bytesPerCoord = sizeof(F32) * vertDimension;
   if(stride > bytesPerCoord)
      bytesPerCoord = stride;

   mPositionBuffer.bind();
   std::size_t positionOffset = mPositionBuffer.insertData((U8*)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		vertexPositionAttrib,  // Attribute index
		vertDimension,			  // Number of values per vertex
		GL_FLOAT,			     // Data type
		GL_FALSE,			     // Normalized?
		stride,				     // Stride
		(void *)positionOffset // Array buffer offset
	);

   // UV-coords
   bytesPerCoord = sizeof(F32) * 2;
   if(stride > bytesPerCoord)
      bytesPerCoord = stride;

   mUVBuffer.bind();
   std::size_t UVOffset = mUVBuffer.insertData((U8 *)UVs + (start * bytesPerCoord), bytesPerCoord * vertCount);

	glVertexAttribPointer(
		UVAttrib,			    // Attribute index
		2,				          // Number of values per coord
		GL_FLOAT,			    // Data type
		GL_FALSE,			    // Normalized?
		stride,				    // Stride
		(void *)UVOffset		 // Array buffer offset
	);

	// Draw!
	glDrawArrays(getGLRenderType(type), 0, vertCount);
}

}

#endif // BF_PLATFORM_3DS