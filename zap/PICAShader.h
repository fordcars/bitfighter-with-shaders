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

// Ditto
typedef struct
{
   U32 flags[2];
   U64 permutation;
   int attrCount;
} AttrInfoStub;

namespace Zap
{

enum class UniformName
{
   MVP = 0,
   Color,
   PointSize,
   LineWidth,
   Time,
   UniformName_LAST // Keep this at the end
};

class PICAShader
{
private:
   std::string mName;
   shaderProgramStub mProgram;
   AttrInfoStub mAttrInfo;
   void *mDVLB;

   S32 mUniformLocations[static_cast<unsigned>(UniformName::UniformName_LAST)];

   Color mLastColor;
   F32 mLastAlpha;
   F32 mLastPointSize;
   F32 mLastLineWidth;
   U32 mLastTime;

   void buildProgram(U32 *shbinData, U32 shbinSize, U32 geometryStride);
   void registerUniforms();
   void addAttributeInfo(bool hasColors, bool hasUVs);

public:
   PICAShader();
   ~PICAShader();

   // geometryStride: number of vec4s per primitive
   void init(const std::string &name, U32 *shbinData, U32 shbinSize, bool hasColors = false, bool hasUVs = false, U32 geometryStride = 0);
   std::string getName() const;
   S32 getUniformLocation(UniformName uniformName) const;
   void bind();

   // Defined for all shaders, even if unused.
   // Shader must be active when called!
   void setMVP(const Matrix4 &MVP);
   void setColor(const Color &color, F32 alpha);
   void setPointSize(F32 size);
   void setLineWidth(F32 width);
   void setTime(U32 time);
};

}

#endif // _PICASHADER_H_
