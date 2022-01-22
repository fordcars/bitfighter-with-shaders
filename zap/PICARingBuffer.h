//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _PICARINGBUFFER_H_
#define _PICARINGBUFFER_H_

#include "tnlTypes.h"

using namespace TNL;

namespace Zap
{

class PICARingBuffer
{
private:
   static void *mBufferInfo; // All buffers share same buffer info
   void *mData;
   std::size_t mCurrentOffset;

public:
   PICARingBuffer();
   ~PICARingBuffer();

   static void initForRendering();

   void init();
   void reset();
   void insertAttribData(const void *data, U32 size, U32 stride, U32 attribPerVert, U64 permutation);
   void *insertData(const void *data, U32 size);

   void *allocate(U32 size);
};

}

#endif // _PICARINGBUFFER_H_ 
