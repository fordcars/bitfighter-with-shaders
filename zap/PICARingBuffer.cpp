//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

// Ring buffer for avoid collisions when OpenGL tries to render our data
// while we are writing new data.
// Data should be 4-byte aligned.
// For more information: https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming

#ifdef BF_PLATFORM_3DS

#include "PICARingBuffer.h"
#undef BIT
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

// A larger ring buffer will allow us to reallocate buffer memory less often
static const U32 RING_BUFFER_SIZE = 5 * 1000000U;

namespace Zap
{

PICARingBuffer::PICARingBuffer()
   : mBufferInfo(0)
   , mData(0)
   , mCurrentOffset(0)
{
   // Do nothing
}

PICARingBuffer::~PICARingBuffer()
{
   linearFree(mData);
}

void PICARingBuffer::init()
{
   // Generate buffer and allocate initial memory
   mBufferInfo = C3D_GetBufInfo();
   if(!mBufferInfo)
      printf("Could not get buffer info!\n");

   mData = linearAlloc(RING_BUFFER_SIZE);
   if(!mData)
      printf("Could not allocate buffer memory!\n");

   reset();
}

void PICARingBuffer::reset()
{
   BufInfo_Init((C3D_BufInfo *)mBufferInfo);
}

// Inserts data in buffer, and adds to buffer info
void PICARingBuffer::insertData(const void *data, U32 size, U32 stride, U32 attribPerVert, U64 permutation)
{
   if(mCurrentOffset + size >= RING_BUFFER_SIZE)
   {
      // Orphan current data and allocate new memory. Any old data still being used by OpenGL will
      // continue to exist until it doesn't need it anymore.
      //glBufferData(GL_ARRAY_BUFFER, RING_BUFFER_SIZE, nullptr, GL_DYNAMIC_DRAW);
      mCurrentOffset = 0;
   }

   // Copy data
   memcpy((U8 *)mData + mCurrentOffset, data, size);

   // Add to buffer info
   BufInfo_Add((C3D_BufInfo *)mBufferInfo, (U8*)mData + mCurrentOffset, stride, attribPerVert, permutation);
   mCurrentOffset += size + (4 - size % 4); // Make sure we are 4-byte aligned
}

}

#endif // BF_PLATFORM_3DS