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

// Returns the offset of inserted data within the buffer
std::size_t PICARingBuffer::insertData(const void *data, U32 size)
{
   if(mCurrentOffset + size >= RING_BUFFER_SIZE)
   {
      // Orphan current data and allocate new memory. Any old data still being used by OpenGL will
      // continue to exist until it doesn't need it anymore.
      //glBufferData(GL_ARRAY_BUFFER, RING_BUFFER_SIZE, nullptr, GL_DYNAMIC_DRAW);
      mCurrentOffset = 0;
   }

   // Copy data
   memcpy(mData, data, size);

   std::size_t oldPosition = mCurrentOffset;
   mCurrentOffset += size + (4 - size%4); // Make sure we are 4-byte aligned
   return oldPosition;
}

}

#endif // BF_PLATFORM_3DS