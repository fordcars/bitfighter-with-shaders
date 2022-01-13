//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifdef BF_USE_LEGACY_GL

#include "GLLegacyRenderer.h"
#include <memory>

#ifdef BF_USE_GLES
#  include "SDL/SDL_opengles.h"
#else
#  include "SDL/SDL_opengl.h"
#endif

namespace Zap
{

GLLegacyRenderer::GLLegacyRenderer()
{
   initRenderer();
}

GLLegacyRenderer::~GLLegacyRenderer()
{
   // Do nothing
}

// Static
void GLLegacyRenderer::create()
{
   setInstance(std::unique_ptr<Renderer>(new GLLegacyRenderer));
}

void GLLegacyRenderer::setColor(F32 r, F32 g, F32 b, F32 alpha)
{
   glColor4f(r, g, b, alpha);
}

void GLLegacyRenderer::setPointSize(F32 size)
{
   glPointSize(size);
}

void GLLegacyRenderer::scale(F32 x, F32 y, F32 z)
{
   glScalef(x, y, z);
}

void GLLegacyRenderer::translate(F32 x, F32 y, F32 z)
{
   glTranslatef(x, y, z);
}

void GLLegacyRenderer::rotate(F32 degAngle, F32 x, F32 y, F32 z)
{
   glRotatef(degAngle, x, y, z);
}

void GLLegacyRenderer::setMatrixMode(MatrixType type)
{
   switch(type)
   {
   case MatrixType::ModelView:
      glMatrixMode(GL_MODELVIEW);
      break;

   case MatrixType::Projection:
      glMatrixMode(GL_PROJECTION);
      break;

   default:
      break;
   }
}

void GLLegacyRenderer::getMatrix(MatrixType type, F32 *matrix)
{
   switch(type)
   {
   case MatrixType::ModelView:
      glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
      break;

   case MatrixType::Projection:
      glGetFloatv(GL_PROJECTION_MATRIX, matrix);
      break;

   default:
      break;
   }
}

void GLLegacyRenderer::pushMatrix()
{
   glPushMatrix();
}

void GLLegacyRenderer::popMatrix()
{
   glPopMatrix();
}

void GLLegacyRenderer::loadMatrix(const F32 *m)
{
   glLoadMatrixf(m);
}

void GLLegacyRenderer::loadMatrix(const F64 *m)
{
   glLoadMatrixd(m);
}

void GLLegacyRenderer::loadIdentity()
{
   glLoadIdentity();
}

void GLLegacyRenderer::projectOrtho(F32 left, F32 right, F32 bottom, F32 top, F32 nearZ, F32 farZ)
{
   glOrtho((F32)left, (F32)right, (F32)bottom, (F32)top, (F32)nearZ, (F32)farZ);
}

void GLLegacyRenderer::renderVertexArray(const S8 verts[], U32 vertCount, RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   glEnableClientState(GL_VERTEX_ARRAY);

   glVertexPointer(vertDimension, GL_BYTE, stride, verts);
   glDrawArrays(getGLRenderType(type), start, vertCount);

   glDisableClientState(GL_VERTEX_ARRAY);
}

void GLLegacyRenderer::renderVertexArray(const S16 verts[], U32 vertCount, RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   glEnableClientState(GL_VERTEX_ARRAY);

   glVertexPointer(vertDimension, GL_SHORT, stride, verts);
   glDrawArrays(getGLRenderType(type), start, vertCount);

   glDisableClientState(GL_VERTEX_ARRAY);
}

void GLLegacyRenderer::renderVertexArray(const F32 verts[], U32 vertCount, RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   glEnableClientState(GL_VERTEX_ARRAY);

   glVertexPointer(vertDimension, GL_FLOAT, stride, verts);
   glDrawArrays(getGLRenderType(type), start, vertCount);

   glDisableClientState(GL_VERTEX_ARRAY);
}

void GLLegacyRenderer::renderColored(const F32 verts[], const F32 colors[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_COLOR_ARRAY);

   glVertexPointer(vertDimension, GL_FLOAT, stride, verts);
   glColorPointer(4, GL_FLOAT, stride, colors);
   glDrawArrays(getGLRenderType(type), start, vertCount);

   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_VERTEX_ARRAY);
}

void GLLegacyRenderer::renderTextured(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension)
{
   // !Todo properly!
   glEnable(GL_TEXTURE_2D);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glVertexPointer(vertDimension, GL_FLOAT, stride, verts);
   glTexCoordPointer(2, GL_FLOAT, stride, UVs);
   glDrawArrays(getGLRenderType(type), start, vertCount);

   glDisable(GL_TEXTURE_2D);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void GLLegacyRenderer::renderColoredTexture(const F32 verts[], const F32 UVs[], U32 vertCount,
   RenderType type, U32 start, U32 stride, U32 vertDimension, bool _isAlphaTexture)
{
   // !Todo properly!
   glEnable(GL_TEXTURE_2D);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glVertexPointer(vertDimension, GL_FLOAT, stride, verts);
   glTexCoordPointer(2, GL_FLOAT, stride, UVs);
   glDrawArrays(getGLRenderType(type), start, vertCount);

   glDisable(GL_TEXTURE_2D);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

}

#endif // BF_USE_LEGACY_GL