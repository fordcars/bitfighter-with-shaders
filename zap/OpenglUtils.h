//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

/*
 *  Most of this was taken directly from freeglut sources
 */

#ifndef OPENGLUTILS_H_
#define OPENGLUTILS_H_

#include "FontContextEnum.h"

#include "inclGL.h" // Important to leave this here! Makes it easier for me.


namespace TNL {
   template<class T> class Vector;
};

using namespace TNL;

namespace Zap {

extern void setFont(FontId fontId); // Not defined here
}

#endif /* OPENGLUTILS_H_ */
