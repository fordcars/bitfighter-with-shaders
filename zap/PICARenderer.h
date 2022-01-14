//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _PICARENDERER_H_
#define _PICARENDERER_H_

#include "Renderer.h"
#include "PICAShader.h"
#include "PICARingBuffer.h"
#include "Matrix4.h"
#include "Stack.h"
#include "Color.h"

#define STACK_CAPACITY 100

struct C3D_RenderTarget_tag;
namespace Zap
{

class PICARenderer : public Renderer
{
private:
   using MatrixStack = Stack<Matrix4, STACK_CAPACITY>;

   C3D_RenderTarget_tag *mTarget;

   // Shaders
   PICAShader mStaticShader;
   PICAShader mDynamicShader;
   PICAShader mTexturedShader;
   PICAShader mColoredTextureShader;

   // Reusable buffers for vertex data
   PICARingBuffer mPositionBuffer;
   PICARingBuffer mColorBuffer;
   PICARingBuffer mUVBuffer;

   bool mTextureEnabled;
   Color mColor;
   F32 mAlpha;
   F32 mPointSize;
   U32 mCurrentShaderId;
   bool mUsingAndStencilTest;

   MatrixStack mModelViewMatrixStack;
   MatrixStack mProjectionMatrixStack;
   MatrixType mMatrixMode;

   PICARenderer();
   void useShader(const PICAShader &shader);

   template<typename T>
   void renderGenericVertexArray(DataType dataType, const T verts[], U32 vertCount, RenderType type,
      U32 start, U32 stride, U32 vertDimension);

   U32 getRenderType(RenderType type) const;
   U32 getTextureFormat(TextureFormat format) const;
   U32 getDataType(DataType type) const;

public:
   ~PICARenderer() override;
   static void create();

   void clear() override;
   void clearStencil() override;
   void clearDepth() override;
   void setClearColor(F32 r, F32 g, F32 b, F32 alpha = 1.0f) override;
   void setColor(F32 r, F32 g, F32 b, F32 alpha = 1.0f) override;
   void setPointSize(F32 size) override;
   void setLineWidth(F32 width) override;
   void enableAntialiasing() override;
   void disableAntialiasing() override;
   void enableBlending() override;
   void disableBlending() override;
   void useTransparentBlackBlending() override;
   void useSpyBugBlending() override;
   void useDefaultBlending() override;
   void enableDepthTest() override;
   void disableDepthTest() override;

   void enableStencil() override;
   void disableStencil() override;
   void useAndStencilTest() override;
   void useNotStencilTest() override;
   void enableStencilDrawOnly() override;
   void disableStencilDraw() override;

   void setViewport(S32 x, S32 y, S32 width, S32 height) override;
   Point getViewportPos() override;
   Point getViewportSize() override;

   void enableScissor() override;
   void disableScissor() override;
   bool isScissorEnabled() override;
   void setScissor(S32 x, S32 y, S32 width, S32 height) override;
   Point getScissorPos() override;
   Point getScissorSize() override;

   void scale(F32 x, F32 y, F32 z = 1.0f) override;
   void translate(F32 x, F32 y, F32 z = 0.0f) override;
   void rotate(F32 degAngle, F32 x, F32 y, F32 z) override;

   void setMatrixMode(MatrixType type) override;
   void getMatrix(MatrixType type, F32 *matrix) override;
   void pushMatrix() override;
   void popMatrix() override;
   void loadMatrix(const F32 *m) override;
   void loadMatrix(const F64 *m) override;
   void loadIdentity() override;
   void projectOrtho(F32 left, F32 right, F32 bottom, F32 top, F32 nearZ, F32 farZ) override;

   U32 generateTexture(bool useLinearFiltering = true) override;
   void bindTexture(U32 textureHandle) override;
   bool isTexture(U32 textureHandle) override;
   void deleteTexture(U32 textureHandle) override;
   void setTextureData(TextureFormat format, DataType dataType, U32 width, U32 height, const void *data) override;
   void setSubTextureData(TextureFormat format, DataType dataType, S32 xOffset, S32 yOffset,
      U32 width, U32 height, const void *data) override;

   void readFramebufferPixels(TextureFormat format, DataType dataType, S32 x, S32 y, S32 width, S32 height, void *data) override;

   void renderVertexArray(const S8 verts[], U32 vertCount, RenderType type,
      U32 start = 0, U32 stride = 0, U32 vertDimension = 2) override;
   void renderVertexArray(const S16 verts[], U32 vertCount, RenderType type,
      U32 start = 0, U32 stride = 0, U32 vertDimension = 2) override;
   void renderVertexArray(const F32 verts[], U32 vertCount, RenderType type,
      U32 start = 0, U32 stride = 0, U32 vertDimension = 2) override;

   void renderColored(const F32 verts[], const F32 colors[], U32 vertCount,
      RenderType type, U32 start = 0, U32 stride = 0, U32 vertDimension = 2) override;

   void renderTextured(const F32 verts[], const F32 UVs[], U32 vertCount,
      RenderType type, U32 start = 0, U32 stride = 0, U32 vertDimension = 2) override;

   // Render a texture colored by the current color:
   void renderColoredTexture(const F32 verts[], const F32 UVs[], U32 vertCount, RenderType type,
      U32 start = 0, U32 stride = 0, U32 vertDimension = 2, bool isAlphaTexture = false) override;
};

}

#endif // _PICARENDERER_H_