//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifdef BF_PLATFORM_3DS

#include "PICAShader.h"
#undef BIT
#include <3ds.h>
#include <citro3d.h>

namespace Zap
{

PICAShader::PICAShader()
   : mLastAlpha(0)
   , mLastPointSize(0)
   , mLastLineWidth(0)
   , mLastTime(0)
{
}

PICAShader::~PICAShader()
{
   shaderProgramFree((shaderProgram_s *)&mProgram);
   DVLB_Free((DVLB_s *)mDVLB);
}

void PICAShader::init(const std::string &name, U32 *shbinData, U32 shbinSize,
                      bool hasColors, bool hasUVs, U32 geometryStride)
{
   mName = name;

   buildProgram(shbinData, shbinSize, geometryStride);
   bind();
   registerUniforms();
   addAttributeInfo(hasColors, hasUVs);

   // Set initial uniform values
   setMVP(Matrix4());
   setColor(Color(), 1.0f);
   setPointSize(1.0f);
   setLineWidth(1.0f);
   setTime(0);
}

void PICAShader::buildProgram(U32 *shbinData, U32 shbinSize, U32 geometryStride)
{
   // Load the vertex shader, create a shader program and bind it
   mDVLB = DVLB_ParseFile((u32*)shbinData, shbinSize);
   if(!mDVLB)
      printf("Could not parse '%s' shader file!\n", mName.c_str());

   DVLB_s *shaderDVLB = (DVLB_s *)mDVLB;
   shaderProgram_s *program = (shaderProgram_s *)&mProgram;
   shaderProgramInit(program);
   shaderProgramSetVsh(program, &shaderDVLB->DVLE[0]);

   if(shaderDVLB->numDVLE > 1)
      shaderProgramSetGsh(program, &shaderDVLB->DVLE[1], geometryStride);

   if(!program)
      printf("Could not initialize '%s' shader!\n", mName.c_str());
}

void PICAShader::registerUniforms()
{
   shaderProgram_s *program = (shaderProgram_s *)&mProgram;

   // -1 if uniform was not found
   // Vertex shader
   mUniformLocations[static_cast<unsigned>(UniformName::MVP)] = shaderInstanceGetUniformLocation(program->vertexShader, "MVP");
   mUniformLocations[static_cast<unsigned>(UniformName::Color)] = shaderInstanceGetUniformLocation(program->vertexShader, "vertColor");
   mUniformLocations[static_cast<unsigned>(UniformName::Time)] = shaderInstanceGetUniformLocation(program->vertexShader, "time");

   // Geometry shaders
   mUniformLocations[static_cast<unsigned>(UniformName::PointSize)] = shaderInstanceGetUniformLocation(program->geometryShader, "pointSize");
   mUniformLocations[static_cast<unsigned>(UniformName::LineWidth)] = shaderInstanceGetUniformLocation(program->geometryShader, "lineWidth");
}

void PICAShader::addAttributeInfo(bool hasColors, bool hasUVs)
{
   U32 location = 0;
   C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
   AttrInfo_Init(attrInfo);
   AttrInfo_AddLoader(attrInfo, location, GPU_FLOAT, 2);
   ++location;

   if(hasColors)
   {
      AttrInfo_AddLoader(attrInfo, location, GPU_FLOAT, 4);
      ++location;
   }

   if(hasUVs)
   {
      AttrInfo_AddLoader(attrInfo, location, GPU_FLOAT, 2);
      ++location;
   }
}

std::string PICAShader::getName() const
{
   return mName;
}

S32 PICAShader::getUniformLocation(UniformName uniformName) const
{
   return mUniformLocations[static_cast<unsigned>(uniformName)];
}

void PICAShader::bind() const
{
   C3D_BindProgram((shaderProgram_s *)&mProgram);
}

void PICAShader::setMVP(const Matrix4 &MVP)
{
   S32 loc = getUniformLocation(UniformName::MVP);
   if(loc != -1)
      C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, loc, (C3D_Mtx *)MVP.getData());
}

void PICAShader::setColor(const Color &color, F32 alpha)
{
   S32 loc = getUniformLocation(UniformName::Color);
   if(loc != -1) // This doesn't work, since uniforms are cleared when shaders are switched: && (color != mLastColor || alpha != mLastAlpha))
   {
      C3D_FVUnifSet(GPU_VERTEX_SHADER, loc, color.r, color.g, color.b, alpha);
      mLastColor = color;
      mLastAlpha = alpha;
   }
}

void PICAShader::setPointSize(F32 size)
{
   S32 loc = getUniformLocation(UniformName::PointSize);
   if(loc != -1)// && size != mLastPointSize)
   {
      C3D_FVUnifSet(GPU_GEOMETRY_SHADER, loc, size, 0.0f, 0.0f, 0.0f);
      mLastPointSize = size;
   }
}

void PICAShader::setLineWidth(F32 width)
{
   S32 loc = getUniformLocation(UniformName::LineWidth);
   if(loc != -1)// && width != mLastLineWidth)
   {
      C3D_FVUnifSet(GPU_GEOMETRY_SHADER, loc, width, 0.0f, 0.0f, 0.0f);
      mLastLineWidth = width;
   }
}

void PICAShader::setTime(U32 time)
{
   S32 loc = getUniformLocation(UniformName::Time);
   if(loc != -1)// && time != mLastTime)
   {
      C3D_IVUnifSet(GPU_VERTEX_SHADER, loc, time, 0, 0, 0);
      mLastTime = time;
   }
}

}

#endif // BF_PLATFORM_3DS