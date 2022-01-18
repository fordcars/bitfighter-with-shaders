//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _PICARINGBUFFER_H_
#define _PICARINGBUFFER_H_

#include "tnlTypes.h"
#include <cstddef> // For size_t

using namespace TNL;

namespace Zap
{

class PICARingBuffer
{
private:
   void *mBufferInfo;
   void *mData;
   std::size_t mCurrentOffset;

public:
   PICARingBuffer();
   ~PICARingBuffer();

   void init();
   void reset();
   std::size_t insertData(const void* data, U32 size);
};

}

#endif // _PICARINGBUFFER_H_ 
