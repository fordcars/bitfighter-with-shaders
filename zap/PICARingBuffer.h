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
   void *mData;
   std::size_t mCurrentOffset;

public:
   PICARingBuffer();
   ~PICARingBuffer();

   void init();
   void reset();
   void insertData(const void *data, U32 size, U32 stride, U32 attribPerVert, U64 permutation);
};

}

#endif // _PICARINGBUFFER_H_ 
