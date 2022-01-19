//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _PICASHADER_H_
#define _PICASHADER_H_

#include "Matrix4.h"
#include "Color.h"
#include "tnlTypes.h"
#include <string>
#include <stdint.h>

using namespace TNL;

// Declare this here to avoid including citro3D.h here.
typedef struct
{
   void *vertexShader;   ///< Vertex shader.
   void *geometryShader; ///< Geometry shader.
   uint32_t geoShaderInputPermutation[2]; ///< Geometry shader input permutation.
   uint8_t geoShaderInputStride;          ///< Geometry shader input stride.
} shaderProgramStub;

namespace Zap
{

enum class UniformName
{
   MVP = 0,
   Color,
   PointSize,
   Time,
   UniformName_LAST // Keep this at the end
};

enum class AttributeName
{
   VertexPosition = 0,
   VertexColor,
   VertexUV,
   AttributeName_LAST // Keep this at the end
};

class PICAShader
{
private:
   std::string mName;
   shaderProgramStub mProgram;
   void *mDVLB;

   S32 mUniformLocations[static_cast<unsigned>(UniformName::UniformName_LAST)];
   S32 mAttributeLocations[static_cast<unsigned>(AttributeName::AttributeName_LAST)];

   Color mLastColor;
   F32 mLastAlpha;
   F32 mLastPointSize;
   U32 mLastTime;

   void buildProgram(U32 *shbinData, U32 shbinSize);
   void registerUniforms();
   void registerAttributes();

public:
   PICAShader();
   ~PICAShader();

   void init(const std::string &name, U32 *shbinData, U32 shbinSize);
   std::string getName() const;
   S32 getUniformLocation(UniformName uniformName) const;
   S32 getAttributeLocation(AttributeName attributeName) const;
   void bind() const;

   // Defined for all shaders, even if unused.
   // Shader must be active when called!
   void setMVP(const Matrix4 &MVP);
   void setColor(const Color &color, F32 alpha);
   void setPointSize(F32 size);
   void setTime(U32 time);
};

}

#endif // _PICASHADER_H_
