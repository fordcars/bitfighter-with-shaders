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

PICAShader::PICAShader(const std::string &name, U32 *shbinData, U32 shbinSize)
   : mName(name)
   , mUniformLocations()
   , mLastAlpha(0)
   , mLastPointSize(0)
   , mLastTime(0)
{

   buildProgram(shbinData, shbinSize);
   bind();
   registerUniforms();
   registerAttributes();

   // Set initial uniform values
   setMVP(Matrix4());
   setColor(Color(), 1.0f);
   setPointSize(1.0f);
   setTime(0);
}

PICAShader::~PICAShader()
{
   shaderProgramFree((shaderProgram_s *)&mProgram);
   DVLB_Free((DVLB_s *)mDVLB);
}

void PICAShader::buildProgram(U32 *shbinData, U32 shbinSize)
{
   // Load the vertex shader, create a shader program and bind it
   mDVLB = DVLB_ParseFile((u32*)shbinData, shbinSize);
   if(!mDVLB)
      printf("Could not parse '%s' shader file!\n", mName.c_str());

   shaderProgram_s *program = (shaderProgram_s *)&mProgram;
   shaderProgramInit(program);
   shaderProgramSetVsh(program, &(((DVLB_s*)mDVLB)->DVLE[0]));

   if(!program)
      printf("Could not initialize '%s' shader!\n", mName.c_str());
}

void PICAShader::registerUniforms()
{
   shaderProgram_s *program = (shaderProgram_s *)&mProgram;

   // -1 if uniform was not found
   mUniformLocations[static_cast<unsigned>(UniformName::MVP)] = shaderInstanceGetUniformLocation(program->vertexShader, "MVP");
   mUniformLocations[static_cast<unsigned>(UniformName::Color)] = shaderInstanceGetUniformLocation(program->vertexShader, "color");
   mUniformLocations[static_cast<unsigned>(UniformName::PointSize)] = shaderInstanceGetUniformLocation(program->vertexShader, "pointSize");
   mUniformLocations[static_cast<unsigned>(UniformName::Time)] = shaderInstanceGetUniformLocation(program->vertexShader, "time");
}

void PICAShader::registerAttributes()
{
   // glGetAttribLocation returns -1 if attribute was not found
   mAttributeLocations[static_cast<unsigned>(AttributeName::VertexPosition)] = 0;
   mAttributeLocations[static_cast<unsigned>(AttributeName::VertexColor)] = 1;
   mAttributeLocations[static_cast<unsigned>(AttributeName::VertexUV)] = 2;

   C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
   AttrInfo_Init(attrInfo);
   if(mName == "static")
      AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
}

std::string PICAShader::getName() const
{
   return mName;
}

S32 PICAShader::getUniformLocation(UniformName uniformName) const
{
   return mUniformLocations[static_cast<unsigned>(uniformName)];
}

S32 PICAShader::getAttributeLocation(AttributeName attributeName) const
{
   return mAttributeLocations[static_cast<unsigned>(attributeName)];
}

void PICAShader::bind() const
{
   C3D_BindProgram((shaderProgram_s *)&mProgram);
}

void PICAShader::setMVP(const Matrix4 &MVP)
{
   S32 loc = getUniformLocation(UniformName::MVP);
   if(loc != -1)
      C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, loc, (C3D_Mtx*)MVP.getData());
}

void PICAShader::setColor(const Color &color, F32 alpha)
{
   S32 loc = getUniformLocation(UniformName::Color);
   if(loc != -1 && (color != mLastColor || alpha != mLastAlpha))
   {
      C3D_FVUnifSet(GPU_VERTEX_SHADER, loc, color.r, color.g, color.b, alpha);
      mLastColor = color;
      mLastAlpha = alpha;
   }
}

void PICAShader::setPointSize(F32 size)
{
   S32 loc = getUniformLocation(UniformName::PointSize);
   if(loc != -1 && size != mLastPointSize)
   {
      C3D_FVUnifSet(GPU_VERTEX_SHADER, loc, size, 0.0f, 0.0f, 0.0f);
      mLastPointSize = size;
   }
}

void PICAShader::setTime(U32 time)
{
   S32 loc = getUniformLocation(UniformName::Time);
   if(loc != -1 && time != mLastTime)
   {
      C3D_IVUnifSet(GPU_VERTEX_SHADER, loc, time, 0, 0, 0);
      mLastTime = time;
   }
}

}

#endif // BF_PLATFORM_3DS