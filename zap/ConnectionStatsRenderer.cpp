//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "ConnectionStatsRenderer.h"

#include "DisplayManager.h"
#include "Renderer.h"
#include "ClientGame.h"
#include "FontManager.h"

#include "Colors.h"

#include "RenderUtils.h"

namespace Zap { 

namespace UI {


// Constructor
ConnectionStatsRenderer::ConnectionStatsRenderer()
{
   mVisible = false;
   mGraphVisible = false;
   mTime = 0;
   mCurrentIndex = 0;
   zeromem(mSendSize, sizeof(mSendSize));
   zeromem(mRecvSize, sizeof(mRecvSize));
}

// Destructor
ConnectionStatsRenderer::~ConnectionStatsRenderer()
{
   // Do nothing
}

void ConnectionStatsRenderer::reset()
{
   mTime = 0;
   zeromem(mSendSize, sizeof(mSendSize));
   zeromem(mRecvSize, sizeof(mRecvSize));
}


void ConnectionStatsRenderer::idle(U32 timeDelta, GameConnection *conn)
{
   // Graph connection speed?


   mTime += timeDelta;
   if(mTime >= 1000 && conn)
   {
      mTime = 0;

      mSendSize[mCurrentIndex] = conn->mPacketSendBytesTotal;
      mRecvSize[mCurrentIndex] = conn->mPacketRecvBytesTotal;

      mCurrentIndex++;
      if(mCurrentIndex >= ArraySize)
         mCurrentIndex = 0;
   }
}


void ConnectionStatsRenderer::render(GameConnection *conn) const
{
   Renderer& r = Renderer::get();

   S32 y = 0;
   if(mVisible && conn)
   {
      const S32 x1 = 550;
      const S32 x2 = 640;
      const S32 x3 = 700;
      const S32 y_space = 12;
      const S32 size = 10;

      if(mGraphVisible)
         r.setColor(Colors::red);
      else
         r.setColor(Colors::white);
      drawStringr (x2,           y, size, "Send");

      if(mGraphVisible)
         r.setColor(Colors::green);
      drawStringr (x3,           y, size, "Recv");

      r.setColor(Colors::white);
      drawString  (x1, y_space  +y, size, "Count");
      drawStringfr(x2, y_space  +y, size, "%i", conn->mPacketSendCount);
      drawStringfr(x3, y_space  +y, size, "%i", conn->mPacketRecvCount);
      drawString  (x1, y_space*2+y, size, "Drop");
      drawStringfr(x2, y_space*2+y, size, "%i", conn->mPacketSendDropped);
      drawStringfr(x3, y_space*2+y, size, "%i", conn->mPacketRecvDropped);
      drawString  (x1, y_space*3+y, size, "Size");
      drawStringfr(x2, y_space*3+y, size, "%i", conn->mPacketSendBytesLast);
      drawStringfr(x3, y_space*3+y, size, "%i", conn->mPacketRecvBytesLast);
      drawString  (x1, y_space*4+y, size, "Total");
      drawStringfr(x2, y_space*4+y, size, "%i", conn->mPacketSendBytesTotal);
      drawStringfr(x3, y_space*4+y, size, "%i", conn->mPacketRecvBytesTotal);

      y += y_space*5;
   }


   if(mGraphVisible)
   {
      const S32 x1 = 550;
      const S32 x2 = 700;
      const S32 y_size = 150;

      r.setColor(Colors::white);
      drawRect(x1, y, x2, y + y_size, RenderType::LineLoop);

      const U32 ArraySizeGraph = ArraySize - 1;

      F32 graphs[ArraySizeGraph * 2];
      for(U32 i = 0; i < ArraySizeGraph; i++)
         graphs[i * 2] = F32(i * (x2 - x1)) / (ArraySizeGraph - 1) + x1;

      U32 i1 = mCurrentIndex;
      U32 i2 = i1 + 1 >= ArraySize ? 0 : i1 + 1;
      U32 max = 0;
      for(U32 x = 0; x < ArraySizeGraph; x++)
      {
         if(mSendSize[i2] - mSendSize[i1] > max)
            max = mSendSize[i2] - mSendSize[i1];
         if(mRecvSize[i2] - mRecvSize[i1] > max)
            max = mRecvSize[i2] - mRecvSize[i1];
         i1 = i2;
         i2 = i2 + 1 >= ArraySize ? 0 : i2 + 1;
      }

      i1 = mCurrentIndex;
      i2 = i1 + 1 >= ArraySize ? 0 : i1 + 1;
      for(U32 x = 0; x < ArraySizeGraph; x++)
      {
         graphs[x * 2 + 1] = (y + y_size) - F32(mSendSize[i2] - mSendSize[i1]) * y_size / max;
         i1 = i2;
         i2 = i2 + 1 >= ArraySize ? 0 : i2 + 1;
      }

      drawStringf(x1 + 2, y, 10, "%1.1f kbps", max * (1/128.f));
      r.setColor(Colors::red);
      r.renderVertexArray(graphs, ArraySizeGraph, RenderType::LineStrip);

      i1 = mCurrentIndex;
      i2 = i1+1 >= ArraySize ? 0 : i1+1;
      for(U32 x = 0; x < ArraySizeGraph; x++)
      {
         graphs[x * 2 + 1] = (y + y_size) - F32(mRecvSize[i2] - mRecvSize[i1]) * y_size / max;
         i1 = i2;
         i2 = i2 + 1 >= ArraySize ? 0 : i2 + 1;
      }
      r.setColor(Colors::green);
      r.renderVertexArray(graphs, ArraySizeGraph, RenderType::LineStrip);
      y += y_size;
   }
}


void ConnectionStatsRenderer::toggleVisibility()
{
   if(mVisible)
      mGraphVisible = !mGraphVisible;
   mVisible = !mVisible;
}


} }      // Nested namespaces
