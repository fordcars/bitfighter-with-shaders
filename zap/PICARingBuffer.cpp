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

// A larger ring buffer will prevent overwriting data prematurely.
static const U32 RING_BUFFER_SIZE = 100000U;

namespace Zap
{

void *PICARingBuffer::mBufferInfo = 0;

PICARingBuffer::PICARingBuffer()
   : mData(0)
   , mCurrentOffset(0)
{
   // Do nothing
}

PICARingBuffer::~PICARingBuffer()
{
   linearFree(mData);
}

// Static
// Call before inserting data for each mesh
void PICARingBuffer::initForRendering()
{
   mBufferInfo = C3D_GetBufInfo();
   BufInfo_Init((C3D_BufInfo *)mBufferInfo);
}

void PICARingBuffer::init()
{
   // Allocate initial memory
   mData = linearAlloc(RING_BUFFER_SIZE);
   if(!mData)
      printf("Could not allocate buffer memory!\n");

   // Setup initial bufferInfo value to avoid accidental segfaults
   mBufferInfo = C3D_GetBufInfo();
}

// Inserts data in buffer, and adds to buffer info
void PICARingBuffer::insertAttribData(const void *data, U32 size, U32 stride, U32 attribPerVert, U64 permutation)
{
   void *insertedData = insertData(data, size);

   // Add to buffer info. This is equivalent to a VertexAttribPointer.
   BufInfo_Add((C3D_BufInfo *)mBufferInfo, insertedData, stride, attribPerVert, permutation);
}

// Returns pointer to inseted data
void *PICARingBuffer::insertData(const void *data, U32 size)
{
   void *memory = allocate(size);
   memcpy(memory, data, size);
   return memory;
}

// Allocates memory without writing anything.
// Returns pointer to allocated memory.
void *PICARingBuffer::allocate(U32 size)
{
   if(mCurrentOffset + size >= RING_BUFFER_SIZE)
      mCurrentOffset = 0;

   void *memory = (U8 *)mData + mCurrentOffset;
   mCurrentOffset += size + (4 - size % 4); // 4-byte align next block

   return memory;
}

}

#endif // BF_PLATFORM_3DS