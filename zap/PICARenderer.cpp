//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifdef BF_PLATFORM_3DS

#include "PICARenderer.h"
#include "Color.h"
#include "SDL/SDL.h"
#include "MathUtils.h"
#include "ScreenInfo.h"

#undef BIT
#include <3ds.h>
#include <citro3d.h>
#include <memory>
#include <cstddef> // For size_t

// Shaders
#include "static_triangles_shbin.h"
#include "static_points_shbin.h"

// From https://github.com/devkitPro/3ds-examples/blob/master/graphics/gpu/textured_cube/source/main.c
#define DISPLAY_TRANSFER_FLAGS \
   (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
   GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
   GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

namespace Zap
{

U32 colorToHex(const Color &color, F32 alpha)
{
   U32 r = color.r * 255;
   U32 g = color.g * 255;
   U32 b = color.b * 255;
   U32 a = alpha * 255;

   return (
      r << 24 |
      g << 16 |
      b << 8  |
      a
   );
}

PICARenderer::PICARenderer()
   : mTarget(0)
   //, mDynamicShader("dynamic", "dynamic.v.glsl", "dynamic.f.glsl")
   //, mTexturedShader("textured", "textured.v.glsl", "textured.f.glsl")
   //, mColoredTextureShader("coloredTexture", "coloredTexture.v.glsl", "coloredTexture.f.glsl")
   , mTextureEnabled(false)
   , mClearColor(0.0f, 0.0f, 0.0f)
   , mClearAlpha(1.0f)
   , mAlpha(1.0f)
   , mPointSize(0.0f)
   , mLineWidth(1.0f)
   , mCurrentShader(0)
   , mUsingAndStencilTest(false)
   , mViewportPos(0.0f, 0.0f)
   , mViewportSize(400.0f, 240.0f)
   , mScissorPos(0.0f, 0.0f)
   , mScissorSize(400.0f, 240.0f)
   , mScissorEnabled(false)
   , mMatrixMode(MatrixType::ModelView)
{
   //glPixelStorei(GL_PACK_ALIGNMENT, 1);
   //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
   mTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
   if(!mTarget)
      printf("Could not create C3D target!\n");

   C3D_RenderTargetSetOutput(mTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
   C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE);

   mStaticTrianglesShader.init("static_triangles", (U32 *)static_triangles_shbin, static_triangles_shbin_size, 0);
   mStaticPointsShader.init("static_points", (U32 *)static_points_shbin, static_points_shbin_size, 2);
   mVertexBuffer.init();

   // Give each stack an identity matrix
   mModelViewMatrixStack.push(Matrix4());
   mProjectionMatrixStack.push(Matrix4());

   // Setup texture environment (needed for proper rendering)
   C3D_TexEnv *env = C3D_GetTexEnv(0);
   C3D_TexEnvInit(env);
   C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
   C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

   // Other initial initializations
   setPointSize(1.0f);
   initRenderer();
}

PICARenderer::~PICARenderer()
{
	// Do nothing
}

void PICARenderer::useShader(const PICAShader &shader)
{
   if(mCurrentShader != &shader)
      shader.bind();
}

// Static
void PICARenderer::create()
{
   setInstance(std::unique_ptr<Renderer>(new PICARenderer));
}

// Uses static shader
template<typename T>
void PICARenderer::renderGenericVertexArray(DataType dataType, const T verts[], U32 vertCount, RenderType type,
	U32 start, U32 stride, U32 vertDimension)
{
   useShader(mStaticPointsShader);

   Matrix4 MVP = mProjectionMatrixStack.top().multiplyAndTranspose(mModelViewMatrixStack.top());
   mStaticPointsShader.setMVP(MVP);
   mStaticPointsShader.setColor(mColor, mAlpha);
   mStaticPointsShader.setPointSize(mPointSize);
   mStaticPointsShader.setTime(static_cast<unsigned>(SDL_GetTicks())); // Give time, it's always useful!

	// Give position data to the shader, and deal with stride
   // Positions
   U32 bytesPerCoord = sizeof(T) * vertDimension;
   if(stride == 0)
      stride = bytesPerCoord;
   else if(stride > bytesPerCoord)
      bytesPerCoord = stride;

   mVertexBuffer.insertAttribData(
      (U8 *)verts + (start * bytesPerCoord), // data
      bytesPerCoord * vertCount,             // size
      stride,
      1,
      0x0
   );

	// Draw!
   C3D_DrawArrays(GPU_GEOMETRY_PRIM, 0, vertCount);
}

U32 PICARenderer::getRenderType(RenderType type) const
{
   /*switch(type)
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
   }*/

   return 0;
}

U32 PICARenderer::getTextureFormat(TextureFormat format) const
{
   /*switch(format)
   {
   case TextureFormat::RGB:
      return GL_RGB;

   case TextureFormat::RGBA:
      return GL_RGBA;

   case TextureFormat::Alpha:
      return GL_ALPHA;

   default:
      return 0;
   }*/

   return 0;
}

U32 PICARenderer::getDataType(DataType type) const
{
   /*switch(type)
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
   }*/
   return 0;
}

void PICARenderer::frameBegin()
{
   C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void PICARenderer::frameEnd()
{
   C3D_FrameEnd(0);
}

void PICARenderer::clear()
{
   C3D_RenderTargetClear(mTarget, C3D_CLEAR_ALL, colorToHex(mClearColor, mClearAlpha), 0);
   C3D_FrameDrawOn(mTarget);
}

void PICARenderer::clearStencil()
{
   // How can we clear stencil buffer?? Does this do anything?
   C3D_RenderTargetClear(mTarget, C3D_CLEAR_DEPTH, 0x00000000, 0);
}

void PICARenderer::clearDepth()
{
   C3D_RenderTargetClear(mTarget, C3D_CLEAR_DEPTH, 0x00000000, 0);
}

void PICARenderer::setClearColor(F32 r, F32 g, F32 b, F32 alpha)
{
   mClearColor = Color(r, g, b);
   mClearAlpha = alpha;
}

void PICARenderer::setColor(F32 r, F32 g, F32 b, F32 alpha)
{
	mColor = Color(r, g, b);
	mAlpha = alpha;
}

void PICARenderer::setPointSize(F32 size)
{
   // Convert point size (pixels) to normalized [-1, 1] space.
   // Was found with trial and error.
   F32 sizeFator = 3.6f/ScreenInfo::PHYSICAL_HEIGHT;
   mPointSize = size * sizeFator;
}

void PICARenderer::setLineWidth(F32 width)
{
   mLineWidth = width;
}

void PICARenderer::enableAntialiasing()
{
   //glEnable(GL_LINE_SMOOTH);
}

void PICARenderer::disableAntialiasing()
{
   //glDisable(GL_LINE_SMOOTH);
}

void PICARenderer::enableBlending()
{
   useDefaultBlending();
}

void PICARenderer::disableBlending()
{
   // C3D_AlphaBlend(colorEq        alphaEq     srcClr   dstClr    srcAlpha  dstAlpha)
   //C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

// Any black pixel will become fully transparent
void PICARenderer::useTransparentBlackBlending()
{
   //glBlendFunc(GL_ONE, GL_ONE);
}

void PICARenderer::useSpyBugBlending()
{
   // This blending works like this, source(SRC) * GL_ONE_MINUS_DST_COLOR + destination(DST) * GL_ONE
   //glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
   // C3D_AlphaBlend(colorEq        alphaEq             srcClr            dstClr   srcAlpha  dstAlpha)
   //C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE_MINUS_DST_COLOR, GPU_ONE, GPU_ZERO, GPU_ONE);
}

void PICARenderer::useDefaultBlending()
{
   //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   // C3D_AlphaBlend(colorEq        alphaEq         srcClr               dstClr         srcAlpha  dstAlpha)
   //C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
}

void PICARenderer::enableDepthTest()
{
   //C3D_DepthTest(true, GPU_LESS, GPU_WRITE_DEPTH);
}

void PICARenderer::disableDepthTest()
{
   //C3D_DepthTest(false, GPU_LESS, GPU_WRITE_DEPTH);
}

/// Stencils
void PICARenderer::enableStencil()
{
   // Do nothing
}

void PICARenderer::disableStencil()
{
   //C3D_StencilTest(false, GPU_EQUAL, 1, 0xFFFFFFFF, 0x00000000);
   //C3D_StencilTest(false, GPU_NOTEQUAL, 1, 0xFFFFFFFF, 0x00000000);
}

void PICARenderer::useAndStencilTest()
{
   // Render if stencil value == 1
   //C3D_StencilTest(true, GPU_EQUAL, 1, 0xFFFFFFFF, 0x00000000);
   //mUsingAndStencilTest = true;
}

void PICARenderer::useNotStencilTest()
{
   // Render if stencil value != 1
   //C3D_StencilTest(true, GPU_NOTEQUAL, 1, 0xFFFFFFFF, 0x00000000);
   //mUsingAndStencilTest = false;
}

void PICARenderer::enableStencilDrawOnly()
{
   //if(mUsingAndStencilTest)
   //   C3D_StencilTest(true, GPU_EQUAL, 1, 0xFFFFFFFF, 0xFFFFFFFF);
   //else
   //   C3D_StencilTest(true, GPU_NOTEQUAL, 1, 0xFFFFFFFF, 0xFFFFFFFF);
}

// Temporarily disable drawing to stencil
void PICARenderer::disableStencilDraw()
{
   //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // Feel free to draw in the color buffer tho!
   // Restore stencil test, it was probably modified by enableStencilDrawOnly()
   //if(mUsingAndStencilTest)
   //   C3D_StencilTest(true, GPU_EQUAL, 1, 0xFFFFFFFF, 0x00000000);
   //else
   //   C3D_StencilTest(true, GPU_NOTEQUAL, 1, 0xFFFFFFFF, 0x00000000);
}

void PICARenderer::setViewport(S32 x, S32 y, S32 width, S32 height)
{
   //C3D_SetViewport((u32)x, (u32)y, (u32)width, (u32)height);
   mViewportPos = Point(x, y);
   mViewportSize = Point(width, height);
}

Point PICARenderer::getViewportPos()
{
   return mViewportPos;
}

Point PICARenderer::getViewportSize()
{
   return mViewportSize;
}

void PICARenderer::enableScissor()
{
   //C3D_SetScissor(
   //   GPU_SCISSOR_NORMAL,
   //   (u32)mScissorPos.x,
   //   (u32)mScissorPos.y,
   //   (u32)(mScissorPos.x + mScissorSize.x),
   //   (u32)(mScissorPos.y + mScissorSize.y)
   //);

   mScissorEnabled = true;
}

void PICARenderer::disableScissor()
{
   //C3D_SetScissor(
   //   GPU_SCISSOR_DISABLE,
   //   (u32)mScissorPos.x,
   //   (u32)mScissorPos.y,
   //   (u32)(mScissorPos.x + mScissorSize.x),
   //   (u32)(mScissorPos.y + mScissorSize.y)
   //);

   mScissorEnabled = false;
}

bool PICARenderer::isScissorEnabled()
{
   return mScissorEnabled;
}

void PICARenderer::setScissor(S32 x, S32 y, S32 width, S32 height)
{
   mScissorPos = Point(x, y);
   mScissorSize = Point(width, height);
}

Point PICARenderer::getScissorPos()
{
   return mScissorPos;
}

Point PICARenderer::getScissorSize()
{
   return mScissorSize;
}

void PICARenderer::scale(F32 x, F32 y, F32 z)
{
	// Choose correct stack
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	Matrix4 newMatrix = stack.top().scale(x, y, z);
	stack.pop();
	stack.push(newMatrix);
}

void PICARenderer::translate(F32 x, F32 y, F32 z)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	Matrix4 newMatrix = stack.top().translate(x, y, z);
	stack.pop();
	stack.push(newMatrix);
}

void PICARenderer::rotate(F32 degAngle, F32 x, F32 y, F32 z)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;

	Matrix4 newMatrix = stack.top().rotate(degreesToRadians(degAngle), x, y, z);
	stack.pop();
	stack.push(newMatrix);
}

void PICARenderer::setMatrixMode(MatrixType type)
{
	mMatrixMode = type;
}

void PICARenderer::getMatrix(MatrixType type, F32 *matrix)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	const F32 *sourceMatrix = stack.top().getData();

	for(int i = 0; i < 16; ++i)
		matrix[i] = sourceMatrix[i];
}

void PICARenderer::pushMatrix()
{
	// Duplicate the top matrix on top of the stack
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.push(stack.top());
}

void PICARenderer::popMatrix()
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
}

// m is column-major
void PICARenderer::loadMatrix(const F32 *m)
{
	// Replace top matrix
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
	stack.push(Matrix4(m));
}

// Results in loss of precision!
void PICARenderer::loadMatrix(const F64 *m)
{
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
	stack.push(Matrix4(m));
}

void PICARenderer::loadIdentity()
{
	// Replace the top matrix with an identity matrix
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	stack.pop();
	stack.push(Matrix4());
}

void PICARenderer::projectOrtho(F32 left, F32 right, F32 bottom, F32 top, F32 nearZ, F32 farZ)
{
	// Multiply the top matrix with an ortho matrix
   MatrixStack &stack = (mMatrixMode == MatrixType::ModelView) ? mModelViewMatrixStack : mProjectionMatrixStack;
	Matrix4 topMatrix = stack.top();
   Matrix4 ortho = Matrix4::getOrthoProjection(left, right, bottom, top, nearZ, farZ);

	stack.pop();
	stack.push(ortho * topMatrix);
}

// Uses "nearest pixel" filtering when useLinearFiltering is false
U32 PICARenderer::generateTexture(bool useLinearFiltering)
{
   //GLuint textureHandle;
   //glGenTextures(1, &textureHandle);

   //// Set filtering
   //GLint currentBinding;
   //glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentBinding);
   //glBindTexture(GL_TEXTURE_2D, textureHandle);

   //if(useLinearFiltering)
   //{
   //   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   //   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   //}
   //else
   //{
   //   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   //   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   //}

   //glBindTexture(GL_TEXTURE_2D, currentBinding); // Restore previous binding

   //return textureHandle;

   return 1;
}

void PICARenderer::bindTexture(U32 textureHandle)
{
   //glBindTexture(GL_TEXTURE_2D, textureHandle);
}

bool PICARenderer::isTexture(U32 textureHandle)
{
   //return glIsTexture(textureHandle);
   return true;
}

void PICARenderer::deleteTexture(U32 textureHandle)
{
   //glDeleteTextures(1, &textureHandle);
}

void PICARenderer::setTextureData(TextureFormat format, DataType dataType, U32 width, U32 height, const void *data)
{
   //U32 textureFormat = getTextureFormat(format);

   //glTexImage2D(
   //   GL_TEXTURE_2D, 0, textureFormat,
   //   width, height, 0,
   //   textureFormat, getDataType(dataType), data);
}

void PICARenderer::setSubTextureData(TextureFormat format, DataType dataType, S32 xOffset, S32 yOffset,
   U32 width, U32 height, const void *data)
{
   //glTexSubImage2D(
   //   GL_TEXTURE_2D, 0,
   //   xOffset, yOffset,
   //   width, height,
   //   getTextureFormat(format),
   //   getDataType(dataType), data);
}

// Fairly slow operation
void PICARenderer::readFramebufferPixels(TextureFormat format, DataType dataType, S32 x, S32 y, S32 width, S32 height, void *data)
{
   //glReadPixels(
   //   x, y, width, height,
   //   getTextureFormat(format),
   //   getDataType(dataType),
   //   data);
}

void PICARenderer::renderVertexArray(const S8 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	//renderGenericVertexArray(DataType::Byte, verts, vertCount, type, start, stride, vertDimension);
}

void PICARenderer::renderVertexArray(const S16 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	//renderGenericVertexArray(DataType::Short, verts, vertCount, type, start, stride, vertDimension);
}

void PICARenderer::renderVertexArray(const F32 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	renderGenericVertexArray(DataType::Float, verts, vertCount, type, start, stride, vertDimension);
}

void PICARenderer::renderColored(const F32 verts[], const F32 colors[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   //renderGenericVertexArray(DataType::Float, verts, vertCount, type, start, stride, vertDimension);
   //useShader(mDynamicShader);

	//Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   //mDynamicShader.setMVP(MVP);
   //mDynamicShader.setPointSize(mPointSize);
   //mDynamicShader.setTime(static_cast<unsigned>(SDL_GetTicks()));

	//// Attribute locations
	//GLint vertexPositionAttrib = mDynamicShader.getAttributeLocation(AttributeName::VertexPosition);
	//GLint colorAttrib = mDynamicShader.getAttributeLocation(AttributeName::VertexColor);

	//// Positions
	//U32 bytesPerCoord = sizeof(F32) * vertDimension;
 //  if(stride > bytesPerCoord) // Should never be less than
 //     bytesPerCoord = stride;

 //  mPositionBuffer.bind();
 //  std::size_t positionOffset = mPositionBuffer.insertData((U8 *)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	//glVertexAttribPointer(
	//	vertexPositionAttrib,  // Attribute index
	//	vertDimension,			  // Number of values per vertex
	//	GL_FLOAT,			     // Data type
	//	GL_FALSE,			     // Normalized?
	//	stride,				     // Stride
	//	(void *)positionOffset // Array buffer offset
	//);

	//// Colors
	//bytesPerCoord = sizeof(F32) * 4;
 //  if(stride > bytesPerCoord) // Should never be less than
 //     bytesPerCoord = stride;

 //  mColorBuffer.bind();
 //  std::size_t colorOffset = mColorBuffer.insertData((U8 *)colors + (start * bytesPerCoord), bytesPerCoord * vertCount);

	//glVertexAttribPointer(
	//	colorAttrib,          // Attribute index
	//	4,				          // Number of values per color
	//	GL_FLOAT,				 // Data type
	//	GL_FALSE,				 // Normalized?
	//	stride,					 // Stride
	//	(void *)colorOffset	 // Array buffer offset
	//);

	//// Draw!
	//glDrawArrays(getRenderType(type), 0, vertCount);
}

void PICARenderer::renderTextured(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   //useShader(mTexturedShader);

	Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   //mTexturedShader.setMVP(MVP);
   //mTexturedShader.setTextureSampler(0); // Default texture unit
   //mTexturedShader.setTime(static_cast<unsigned>(SDL_GetTicks()));

	//// Attribute locations
	//GLint vertexPositionAttrib = mTexturedShader.getAttributeLocation(AttributeName::VertexPosition);
	//GLint UVAttrib = mTexturedShader.getAttributeLocation(AttributeName::VertexUV);

 //  // Positions
 //  U32 bytesPerCoord = sizeof(F32) * vertDimension;
 //  if(stride > bytesPerCoord)
 //     bytesPerCoord = stride;

 //  mPositionBuffer.bind();
 //  std::size_t positionOffset = mPositionBuffer.insertData((U8 *)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	//glVertexAttribPointer(
	//	vertexPositionAttrib,  // Attribute index
	//	vertDimension,			  // Number of values per vertex
	//	GL_FLOAT,			     // Data type
	//	GL_FALSE,			     // Normalized?
	//	stride,				     // Stride
	//	(void *)positionOffset // Array buffer offset
	//);

	//// UV-coords
 //  bytesPerCoord = sizeof(F32) * 2;
 //  if(stride > bytesPerCoord)
 //     bytesPerCoord = stride;

 //  mUVBuffer.bind();
 //  std::size_t UVOffset = mUVBuffer.insertData((U8 *)UVs + (start * bytesPerCoord), bytesPerCoord * vertCount);

	//glVertexAttribPointer(
	//	UVAttrib,			    // Attribute index
	//	2,				          // Number of values per coord
	//	GL_FLOAT,			    // Data type
	//	GL_FALSE,			    // Normalized?
	//	stride,				    // Stride
	//	(void *)UVOffset	    // Array buffer offset
	//);

	//// Draw!
	//glDrawArrays(getRenderType(type), 0, vertCount);
}

// Render a texture colored by the current color:
void PICARenderer::renderColoredTexture(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension, bool isAlphaTexture)
{
   //useShader(mColoredTextureShader);

	// Uniforms
	Matrix4 MVP = mProjectionMatrixStack.top() * mModelViewMatrixStack.top();
   //mColoredTextureShader.setMVP(MVP);
   //mColoredTextureShader.setColor(mColor, mAlpha);
   //mColoredTextureShader.setIsAlphaTexture(isAlphaTexture);
   //mColoredTextureShader.setTextureSampler(0); // Default texture unit
   //mColoredTextureShader.setTime(static_cast<unsigned>(SDL_GetTicks()));

	//// Attribute locations
	//GLint vertexPositionAttrib = mColoredTextureShader.getAttributeLocation(AttributeName::VertexPosition);
	//GLint UVAttrib = mColoredTextureShader.getAttributeLocation(AttributeName::VertexUV);

	//// Positions
 //  U32 bytesPerCoord = sizeof(F32) * vertDimension;
 //  if(stride > bytesPerCoord)
 //     bytesPerCoord = stride;

 //  mPositionBuffer.bind();
 //  std::size_t positionOffset = mPositionBuffer.insertData((U8*)verts + (start * bytesPerCoord), bytesPerCoord * vertCount);

	//glVertexAttribPointer(
	//	vertexPositionAttrib,  // Attribute index
	//	vertDimension,			  // Number of values per vertex
	//	GL_FLOAT,			     // Data type
	//	GL_FALSE,			     // Normalized?
	//	stride,				     // Stride
	//	(void *)positionOffset // Array buffer offset
	//);

 //  // UV-coords
 //  bytesPerCoord = sizeof(F32) * 2;
 //  if(stride > bytesPerCoord)
 //     bytesPerCoord = stride;

 //  mUVBuffer.bind();
 //  std::size_t UVOffset = mUVBuffer.insertData((U8 *)UVs + (start * bytesPerCoord), bytesPerCoord * vertCount);

	//glVertexAttribPointer(
	//	UVAttrib,			    // Attribute index
	//	2,				          // Number of values per coord
	//	GL_FLOAT,			    // Data type
	//	GL_FALSE,			    // Normalized?
	//	stride,				    // Stride
	//	(void *)UVOffset		 // Array buffer offset
	//);

	//// Draw!
	//glDrawArrays(getRenderType(type), 0, vertCount);
}

}

#endif // BF_PLATFORM_3DS