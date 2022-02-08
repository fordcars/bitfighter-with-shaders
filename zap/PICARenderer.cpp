//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifdef BF_PLATFORM_3DS

#include "PICARenderer.h"
#include "PICAUtils.h"
#include "Color.h"
#include "SDL/SDL.h"
#include "MathUtils.h"
#include "ScreenInfo.h"
#include "sparkManager.h"

#undef BIT
#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <memory>
#include <cstddef> // For size_t

/* Shaders */
// Static
#include "static_triangles_shbin.h"
#include "static_points_shbin.h"
#include "static_lines_shbin.h"

// Colored
#include "colored_triangles_shbin.h"
#include "colored_points_shbin.h"
#include "colored_lines_shbin.h"

// Textured
#include "textured_triangles_shbin.h"

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
   , mNextTextureId(1)
   , mBoundTexture(0)
   , mUsedTextureLast(false)
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
   C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
   mTarget = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
   if(!mTarget)
      printf("Could not create C3D target!\n");

   // Initial settings
   C3D_RenderTargetSetOutput(mTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
   C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE);
   C3D_CullFace(GPU_CULL_NONE);
   C3D_DepthTest(false, GPU_LESS, GPU_WRITE_ALL);
   C3D_AlphaTest(false, GPU_ALWAYS, 0x00);

   // Setup texture environment (needed for proper rendering)
   C3D_TexEnv *env = C3D_GetTexEnv(0);
   C3D_TexEnvInit(env);
   mUsedTextureLast = true;
   useDefaultTexEnv();

   // Static shaders
   mStaticTrianglesShader.init("static_triangles", (U32 *)static_triangles_shbin, static_triangles_shbin_size);
   mStaticPointsShader.init("static_points", (U32 *)static_points_shbin, static_points_shbin_size, false, false, 2);
   mStaticLinesShader.init("static_lines", (U32 *)static_lines_shbin, static_lines_shbin_size);

   // Colored shaders
   mColoredTrianglesShader.init("colored_triangles", (U32 *)colored_triangles_shbin, colored_triangles_shbin_size, true);
   mColoredPointsShader.init("colored_points", (U32 *)colored_points_shbin, colored_points_shbin_size, true, false, 2);
   mColoredLinesShader.init("colored_lines", (U32 *)colored_lines_shbin, colored_lines_shbin_size, true);

   // Textured shaders
   mTexturedTrianglesShader.init("textured_triangles", (U32 *)textured_triangles_shbin, textured_triangles_shbin_size, false, true);

   // Init buffers
   mVertPositionBuffer.init();
   mVertColorBuffer.init();
   mVertUVBuffer.init();
   mIndexBuffer.init();

   // Give each stack an identity matrix
   mModelViewMatrixStack.push(Matrix4());
   mProjectionMatrixStack.push(Matrix4());

   // Other initial initializations
   setPointSize(1.0f);
   setLineWidth(1.0f);
   initRenderer();
}

PICARenderer::~PICARenderer()
{
   // Delete all textures
   for(auto const &texture : mTextures)
   {
      C3D_Tex *tex = (C3D_Tex *)(texture.second);
      C3D_TexDelete(tex);
      tex = nullptr;
   }

   // Shutdown C3D
   C3D_Fini();
}

// Static
void PICARenderer::create()
{
   setInstance(std::unique_ptr<Renderer>(new PICARenderer));
}

// Static
U32 PICARenderer::getTextureFormat(TextureFormat format)
{
   switch(format)
   {
   case TextureFormat::RGB:
      return GPU_RGB8;

   case TextureFormat::RGBA:
      return GPU_RGBA8;

   case TextureFormat::Alpha:
      return GPU_A8;
   }

   return 0;
}

PICAShader &PICARenderer::getShaderForRenderType(RenderType type, bool colored)
{
   if(colored)
   {
      switch(type)
      {
      case RenderType::Lines:
      case RenderType::LineStrip:
      case RenderType::LineLoop:
         return mColoredLinesShader;

      case RenderType::Points:
         return mColoredPointsShader;

      case RenderType::Triangles:
      case RenderType::TriangleStrip:
      case RenderType::TriangleFan:
         return mColoredTrianglesShader;
      }
   }
   else
   {
      switch(type)
      {
      case RenderType::Lines:
      case RenderType::LineStrip:
      case RenderType::LineLoop:
         return mStaticLinesShader;

      case RenderType::Points:
         return mStaticPointsShader;

      case RenderType::Triangles:
      case RenderType::TriangleStrip:
      case RenderType::TriangleFan:
         return mStaticTrianglesShader;
      }
   }

   return mStaticTrianglesShader;
}

void PICARenderer::useShader(PICAShader &shader)
{
   if(mCurrentShader != &shader)
   {
      shader.bind();
      mCurrentShader = &shader;
   }

   Matrix4 MVP = mProjectionMatrixStack.top().multiplyAndTranspose(mModelViewMatrixStack.top());
   shader.setMVP(MVP);
   shader.setColor(mColor, mAlpha);
   shader.setPointSize(mPointSize);
   shader.setLineWidth(mLineWidth);
   // shader.setTime(static_cast<unsigned>(SDL_GetTicks())); // Give time, it's always useful!
}

// C3D_GetCmdBufUsage() only updates on C3D_FrameEnd(). This alternative
// always gets live command buffer usage (I hope).
F32 PICARenderer::getCmdBufferUsage()
{
   u32 size, offset;
   GPUCMD_GetBuffer(nullptr, &size, &offset);

   return (F32)offset / size;
}

bool PICARenderer::cmdBufferIsFull()
{
   if(getCmdBufferUsage() > 0.95)
      return true;

   return false;
}

void PICARenderer::useDefaultTexEnv()
{
   if(mUsedTextureLast)
   {
      C3D_TexEnv *env = C3D_GetTexEnv(0);
      C3D_TexEnvSrc(env, C3D_RGB, GPU_PRIMARY_COLOR, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
      C3D_TexEnvSrc(env, C3D_Alpha, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, (GPU_TEVSRC)0);
      C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
      C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
      mUsedTextureLast = false;
   }
}

void PICARenderer::useAlphaTextureTexEnv()
{
   if(!mUsedTextureLast)
   {
      C3D_TexEnv *env = C3D_GetTexEnv(0);
      C3D_TexEnvSrc(env, C3D_Alpha, GPU_PRIMARY_COLOR, GPU_TEXTURE0, (GPU_TEVSRC)0);
      C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
      mUsedTextureLast = true;
   }
}

// Uses static shader
template<typename T>
void PICARenderer::renderGenericVertexArray(DataType dataType, const T verts[], U32 vertCount, RenderType type,
	U32 start, U32 stride, U32 vertDimension)
{
   if(vertCount == 0) return;
   if(cmdBufferIsFull()) return;

   PICAShader &shader = getShaderForRenderType(type);
   useShader(shader);
   useDefaultTexEnv();

	// Give position data to the shader, and deal with stride
   // Positions
   U32 bytesPerCoord = sizeof(T) * vertDimension;
   if(stride == 0)
      stride = bytesPerCoord;
   else if(stride > bytesPerCoord)
      bytesPerCoord = stride;

   PICARingBuffer::initForRendering();
   mVertPositionBuffer.insertAttribData(
      (U8 *)verts + (start * bytesPerCoord), // data
      bytesPerCoord * vertCount,             // size
      stride,
      1,
      0x0
   );

   renderVerts(type, vertCount);
}

void PICARenderer::renderVerts(RenderType type, U32 vertCount)
{
   switch(type)
   {
   case RenderType::Lines:
   {
      // Make sure we have an even number of verts
      if(vertCount < 2) return;
      if(vertCount % 2 == 1)
         vertCount--;

      U16 *indexArray = (U16 *)mIndexBuffer.allocate(vertCount * sizeof(U16));
      for(U16 i = 0; i < vertCount; ++i)
         indexArray[i] = i;

      C3D_DrawElements(GPU_GEOMETRY_PRIM, vertCount, C3D_UNSIGNED_SHORT, indexArray);
      break;
   }

   case RenderType::LineStrip:
   {
      if(vertCount < 2) return;
      U32 lineVertCount = 2 * (vertCount - 1);
      U16 *indexArray = (U16 *)mIndexBuffer.allocate(lineVertCount * sizeof(U16));

      U32 writeIndex = 0;
      for(U16 i = 1; i < vertCount; ++i)
      {
         writeIndex = 2 * i - 1;
         indexArray[writeIndex - 1] = i - 1; // Previous vert
         indexArray[writeIndex] = i;         // Current vert
      }

      C3D_DrawElements(GPU_GEOMETRY_PRIM, lineVertCount, C3D_UNSIGNED_SHORT, indexArray);
      break;
   }

   case RenderType::LineLoop:
   {
      if(vertCount < 2) return;
      U32 lineVertCount = 2 * (vertCount);
      U16 *indexArray = (U16 *)mIndexBuffer.allocate(lineVertCount * sizeof(U16));

      U32 writeIndex = 0;
      for(U16 i = 1; i < vertCount; ++i)
      {
         writeIndex = 2 * i - 1;
         indexArray[writeIndex - 1] = i - 1; // Previous vert
         indexArray[writeIndex] = i;         // Current vert
      }

      // Add first vertex as last vertex to close loop
      indexArray[lineVertCount - 2] = indexArray[lineVertCount - 3];
      indexArray[lineVertCount - 1] = 0;

      C3D_DrawElements(GPU_GEOMETRY_PRIM, lineVertCount, C3D_UNSIGNED_SHORT, indexArray);
      break;
   }

   case RenderType::Points:
      C3D_DrawArrays(GPU_GEOMETRY_PRIM, 0, vertCount);
      break;

   case RenderType::Triangles:
   {
      if(vertCount < 3) return;
      if(vertCount % 3 == 1)
         vertCount--;
      else if(vertCount % 3 == 2)
         vertCount -= 2;

      U16 *indexArray = (U16 *)mIndexBuffer.allocate(vertCount * sizeof(U16));
      for(U16 i = 0; i < vertCount; ++i)
         indexArray[i] = i;

      C3D_DrawElements(GPU_GEOMETRY_PRIM, vertCount, C3D_UNSIGNED_SHORT, indexArray);
      break;
   }

   case RenderType::TriangleStrip:
   {
      // Generate CCW triangles
      if(vertCount < 3) return;
      U32 triangleVertCount = 3 * (vertCount - 2);
      U16 *indexArray = (U16 *)mIndexBuffer.allocate(triangleVertCount * sizeof(U16));

      U32 writeIndex = 0;
      for(U16 i = 1; i < vertCount - 1; ++i)
      {
         writeIndex = 3 * (i - 1);
         indexArray[writeIndex] = i - 1;      // Previous vert
         indexArray[writeIndex + 1] = i + 1;  // Next vert
         indexArray[writeIndex + 2] = i;      // Current vert
      }

      C3D_DrawElements(GPU_GEOMETRY_PRIM, triangleVertCount, C3D_UNSIGNED_SHORT, indexArray);
      break;
   }

   case RenderType::TriangleFan:
   {
      // Generate CCW triangles with index 0 as fan center
      if(vertCount < 3) return;
      U32 triangleVertCount = 3 * (vertCount - 2);
      U16 *indexArray = (U16 *)mIndexBuffer.allocate(triangleVertCount * sizeof(U16));

      U32 writeIndex = 0;
      for(U16 i = 2; i < vertCount; ++i)
      {
         writeIndex = 3 * (i - 2);
         indexArray[writeIndex] = 0;          // Fan center
         indexArray[writeIndex + 1] = i;      // Current vert
         indexArray[writeIndex + 2] = i - 1;  // Previous vert
      }

      C3D_DrawElements(GPU_GEOMETRY_PRIM, triangleVertCount, C3D_UNSIGNED_SHORT, indexArray);
      break;
   }

   default:
      break;
   }
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
   // .5f for better visuals. Found by trial and error.
   F32 sizeFactor = 1.5/ScreenInfo::PHYSICAL_HEIGHT;
   mPointSize = size * sizeFactor;
}

void PICARenderer::setLineWidth(F32 width)
{
   // Convert width (pixels) to normalized [-1, 1] space.
   F32 sizeFactor = 1.0f / ScreenInfo::PHYSICAL_HEIGHT;
   mLineWidth = width * sizeFactor;
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
   //                colorEq        alphaEq         srcClr               dstClr         srcAlpha             dstAlpha
   C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
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

// Linear filtering is always on
U32 PICARenderer::generateTexture(bool useLinearFiltering)
{
   C3D_Tex *newTex = new C3D_Tex;
   U32 newTexId = mNextTextureId;
   ++mNextTextureId;

   mTextures.insert(std::pair<U32, void *>(newTexId, newTex));
   return newTexId;
}

void PICARenderer::bindTexture(U32 textureHandle)
{
   auto foundIt = mTextures.find(textureHandle);
   if(foundIt != mTextures.end())
   {
      C3D_TexBind(0, (C3D_Tex *)(foundIt->second));
      mBoundTexture = textureHandle;
   }
}

bool PICARenderer::isTexture(U32 textureHandle)
{
   return mTextures.find(textureHandle) != mTextures.end();
}

void PICARenderer::deleteTexture(U32 textureHandle)
{
   auto foundIt = mTextures.find(textureHandle);
   if(foundIt != mTextures.end())
   {
      C3D_Tex *tex = (C3D_Tex *)(foundIt->second);
      C3D_TexDelete(tex);
      mTextures.erase(foundIt);
      delete tex; // We used new for the struct creation
   }
}

// We currently only support UnsignedByte textures
void PICARenderer::setTextureData(TextureFormat format, DataType dataType, U32 width, U32 height, const void *data)
{
   // Get bound texture
   auto foundIt = mTextures.find(mBoundTexture);
   if(foundIt == mTextures.end())
      return;
   C3D_Tex *tex = (C3D_Tex *)(foundIt->second);
 
   TNLAssert(dataType == DataType::UnsignedByte, "Texture data type is unsupported!");

   C3D_TexInitParams params;
   params.width = width;
   params.height = height;
   params.maxLevel = 0;
   params.format = static_cast<GPU_TEXCOLOR>(getTextureFormat(format));
   params.type = GPU_TEX_2D;
   params.onVram = false; // Must be false for setSubTextureData()

   if(!C3D_TexInitWithParams(tex, nullptr, params))
      printf("Could not initialize texture!\n");

   C3D_TexLoadImage(tex, data, GPU_TEXFACE_2D, 0);
   C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
}

void PICARenderer::setSubTextureData(TextureFormat format, DataType dataType, S32 xOffset, S32 yOffset,
   U32 width, U32 height, const void *data)
{
   // Get bound texture
   auto foundIt = mTextures.find(mBoundTexture);
   if(foundIt == mTextures.end())
      return;
   C3D_Tex *tex = (C3D_Tex *)(foundIt->second);

   // Get texture data pointer
   u32 size;
   U8 *texData = static_cast<U8*>(C3D_Tex2DGetImagePtr(tex, 0, &size));
   
   // Copy data
   U32 inIndex = 0;
   for(U32 y = yOffset; y < yOffset + height; ++y)
   {
      for(U32 x = xOffset; x < xOffset + width; ++x)
      {
         U32 offset = screenCoordsToTexOffset(x, y, tex->height);
         texData[offset] = ((U8 *)data)[inIndex];
         ++inIndex;
      }
   }
}

// Probably won't implement this, it's only for screenshots
void PICARenderer::readFramebufferPixels(TextureFormat format, DataType dataType, S32 x, S32 y, S32 width, S32 height, void *data)
{
   //glReadPixels(
   //   x, y, width, height,
   //   getTextureFormat(format),
   //   getDataType(dataType),
   //   data);
}

void PICARenderer::renderVertexArray(const F32 verts[], U32 vertCount, RenderType type,
   U32 start, U32 stride, U32 vertDimension)
{
	renderGenericVertexArray(DataType::Float, verts, vertCount, type, start, stride, vertDimension);
}

void PICARenderer::renderColored(const F32 verts[], const F32 colors[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   if(vertCount == 0) return;
   if(cmdBufferIsFull()) return;

   PICAShader &shader = getShaderForRenderType(type, true);
   useShader(shader);
   useDefaultTexEnv();

   // Positions
   U32 posStride = stride;
   U32 bytesPerCoord = sizeof(F32) * vertDimension;

   if(posStride == 0)
      posStride = bytesPerCoord;
   else if(posStride > bytesPerCoord)
      bytesPerCoord = posStride;

   PICARingBuffer::initForRendering();
   mVertPositionBuffer.insertAttribData(
      (U8 *)verts + (start * bytesPerCoord), // data
      bytesPerCoord * vertCount,             // size
      posStride,
      1,
      0x0
   );

   // Colors
   U32 colorStride = stride;
   bytesPerCoord = sizeof(F32) * 4;
   if(colorStride == 0)
      colorStride = bytesPerCoord;
   else if(colorStride > bytesPerCoord)
      bytesPerCoord = colorStride;

   mVertColorBuffer.insertAttribData(
      (U8 *)colors + (start * bytesPerCoord), // data
      bytesPerCoord * vertCount,              // size
      colorStride,                            // stride
      1,                                      // Attribs per vert
      0x1
   );

   renderVerts(type, vertCount);
}

void PICARenderer::renderTextured(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   // Do we even use this?
}

// Render a texture colored by the current color:
void PICARenderer::renderColoredTexture(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension, bool isAlphaTexture)
{
   if(vertCount == 0) return;
   if(cmdBufferIsFull()) return;

   useShader(mTexturedTrianglesShader);
   useAlphaTextureTexEnv();

   // Positions
   U32 posStride = stride;
   U32 bytesPerCoord = sizeof(F32) * vertDimension;

   if(posStride == 0)
      posStride = bytesPerCoord;
   else if(posStride > bytesPerCoord)
      bytesPerCoord = posStride;

   PICARingBuffer::initForRendering();
   mVertPositionBuffer.insertAttribData(
      (U8 *)verts + (start * bytesPerCoord), // data
      bytesPerCoord * vertCount,             // size
      posStride,
      1,
      0x0
   );

   // UVs
   U32 UVStride = stride;
   bytesPerCoord = sizeof(F32) * 2;
   if(UVStride == 0)
      UVStride = bytesPerCoord;
   else if(UVStride > bytesPerCoord)
      bytesPerCoord = UVStride;

   mVertUVBuffer.insertAttribData(
      (U8 *)UVs + (start * bytesPerCoord), // data
      bytesPerCoord * vertCount,           // size
      UVStride,                            // stride
      1,                                   // Attribs per vert
      0x1
   );

   renderVerts(type, vertCount);
}

// The existence of this function is to work around the BufInfo_Add stride bug
void PICARenderer::renderSparks(const Spark *sparks, U32 count, RenderType type)
{
   if(count == 0) return;
   if(cmdBufferIsFull()) return;

   PICAShader &shader = getShaderForRenderType(type, true);
   useShader(shader);
   useDefaultTexEnv();

   // Interweave positions and color in a single buffer
   U32 dataSize = count * sizeof(F32) * 6;
   F32* data = (F32 *)mVertPositionBuffer.allocate(dataSize);

   for(U32 i = 0; i < count; ++i)
   {
      U32 wi = i * 6;
      data[wi]   = sparks[i].pos.x;
      data[wi+1] = sparks[i].pos.y;
      data[wi+2] = sparks[i].color.r;
      data[wi+3] = sparks[i].color.g;
      data[wi+4] = sparks[i].color.b;
      data[wi+5] = sparks[i].alpha;
   }

   C3D_BufInfo *bufInfo = C3D_GetBufInfo();
   BufInfo_Init(bufInfo);
   BufInfo_Add(bufInfo, data, dataSize / count, 2, 0x10); // Stride should work now

   renderVerts(type, count);
}

} // namespace Zap

#endif // BF_PLATFORM_3DS