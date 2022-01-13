//-----------------------------------------------------------------------------------
//
//   Torque Network Library
//   Copyright (C) 2004 GarageGames.com, Inc.
//   For more information see http://www.opentnl.org
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   For use in products that are not compatible with the terms of the GNU 
//   General Public License, alternative licensing options are available 
//   from GarageGames.com.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//------------------------------------------------------------------------------------

#include "tnl.h"
#include "tnlGhostConnection.h"
#include "tnlBitStream.h"
#include "tnlNetBase.h"
#include "tnlNetObject.h"
#include "tnlNetInterface.h"

namespace TNL {

GhostConnection::GhostConnection()
{
   // ghost management data:
   mScopeObject = NULL;
   mGhostingSequence = 0;
   mGhosting = false;
   mScoping = false;
   mGhostLookupTable = NULL;
   mGhostZeroUpdateIndex = 0;

   mGhostFrom = false;
   mGhostTo = false;
}

GhostConnection::~GhostConnection()
{
   clearAllPacketNotifies();

   // delete any ghosts that may exist for this connection, but aren't added
   clearGhostInfo();
   deleteLocalGhosts();
   delete[] mGhostLookupTable;
}

void GhostConnection::setGhostTo(bool ghostTo)
{
   if(!ghostTo)
   {
      deleteLocalGhosts();
      mGhostTo = false;
   }
   else
      mGhostTo = true;
}

void GhostConnection::setGhostFrom(bool ghostFrom)
{
   if(!ghostFrom)
   {
      clearGhostInfo();
      ghostFrom = false;
   }
   else if(ghostFrom && !mGhostFrom)
   {
      mGhostFrom = true;
      S32 i;
      mGhostFreeIndex = mGhostZeroUpdateIndex = 0;
      if(!mGhostLookupTable)
      {
         mGhostLookupTable = new GhostInfo *[GhostLookupTableSize];
         for(i = 0; i < GhostLookupTableSize; i++)
            mGhostLookupTable[i] = 0;
      }
   }
}

void GhostConnection::packetDropped(PacketNotify *pnotify)
{
   Parent::packetDropped(pnotify);
   GhostPacketNotify *notify = static_cast<GhostPacketNotify *>(pnotify);

   GhostRef *packRef = notify->ghostList;
   // loop through all the packRefs in the packet
 
   while(packRef)
   {
      GhostRef *temp = packRef->nextRef;

      U32 updateFlags = packRef->mask;

      // figure out which flags need to be updated on the object
      for(GhostRef *walk = packRef->updateChain; walk && updateFlags; walk = walk->updateChain)
         updateFlags &= ~walk->mask;
     
      // for any flags we haven't updated since this (dropped) packet
      // or them into the mask so they'll get updated soon

      if(updateFlags)
      {
         if(!packRef->ghost->updateMask)
         {
            packRef->ghost->updateMask = updateFlags;
            ghostPushNonZero(packRef->ghost);
         }
         else
            packRef->ghost->updateMask |= updateFlags;
      }

      // make sure this packRef isn't the last one on the GhostInfo
      if(packRef->ghost->lastUpdateChain == packRef)
         packRef->ghost->lastUpdateChain = NULL;
      
      // if this packet was ghosting an object, set it
      // to re ghost at it's earliest convenience

      if(packRef->ghostInfoFlags & GhostInfo::Ghosting)
      {
         packRef->ghost->flags |= GhostInfo::NotYetGhosted;
         packRef->ghost->flags &= ~GhostInfo::Ghosting;
      }
      
      // otherwise, if it was being deleted,
      // set it to re-delete

      else if(packRef->ghostInfoFlags & GhostInfo::KillingGhost)
      {
         packRef->ghost->flags |= GhostInfo::KillGhost;
         packRef->ghost->flags &= ~GhostInfo::KillingGhost;
      }

      delete packRef;
      packRef = temp;
   }
}

void GhostConnection::packetReceived(PacketNotify *pnotify)
{
   Parent::packetReceived(pnotify);
   GhostPacketNotify *notify = static_cast<GhostPacketNotify *>(pnotify);

   GhostRef *packRef = notify->ghostList;

   // loop through all the notifies in this packet

   while(packRef)
   {
      // Make sure this packRef isn't the last one on the GhostInfo
      if(packRef->ghost->lastUpdateChain == packRef)
         packRef->ghost->lastUpdateChain = NULL;

      GhostRef *temp = packRef->nextRef;      

      // If this object was ghosting, it is now ghosted...
      if(packRef->ghostInfoFlags & GhostInfo::Ghosting)
      {
         packRef->ghost->flags &= ~GhostInfo::Ghosting;
         if(packRef->ghost->obj)
            packRef->ghost->obj->onGhostAvailable(this);
      }
      // ...otherwise, if it was dying, free the ghost

      else if(packRef->ghostInfoFlags & GhostInfo::KillingGhost)
         freeGhostInfo(packRef->ghost);

      delete packRef;
      packRef = temp;
   }
}

static S32 QSORT_CALLBACK UQECompare(const void *a,const void *b)
{
   GhostInfo *ga = *((GhostInfo **) a);
   GhostInfo *gb = *((GhostInfo **) b);

   F32 ret = ga->priority - gb->priority;
   return (ret < 0) ? -1 : ((ret > 0) ? 1 : 0);
} 

void GhostConnection::prepareWritePacket()
{
   Parent::prepareWritePacket();

   if(!doesGhostFrom() && !mGhosting)
      return;

   if(mGhostFreeIndex > MaxGhostCount - 10)  // almost running out of GhostFreeIndex, free some objects not in scope.
   {
      for(S32 i = mGhostZeroUpdateIndex; i < mGhostFreeIndex; i++)
      {
         GhostInfo *walk = mGhostArray[i];
         if(!(walk->flags & GhostInfo::ScopeLocalAlways))
         {
            if(!(walk->flags & GhostInfo::InScope))
               detachObject(walk);
            else
               walk->flags &= ~GhostInfo::InScope;
         }
      }
   }


   // First step is to check all our polled ghosts:

   // 1. Scope query - find if any new objects have come into
   //    scope and if any have gone out.

   // Each packet we loop through all the objects with non-zero masks and
   // mark them as "out of scope" before the scope query runs.
   // If the object has a zero update mask, we wait to remove it until it requests
   // an update.

   for(S32 i = 0; i < mGhostZeroUpdateIndex; i++)
   {
      // Increment the updateSkip for everyone... it's all good
      GhostInfo *walk = mGhostArray[i];
      walk->updateSkipCount++;
      if(!(walk->flags & (GhostInfo::ScopeLocalAlways)))
         walk->flags &= ~GhostInfo::InScope;
   }

   if(mScopeObject)
      mScopeObject->performScopeQuery(this);
}

bool GhostConnection::isDataToTransmit()
{
   // Once we've run the scope query - if there are no objects that need to be updated,
   // we return false
   return Parent::isDataToTransmit() || mGhostZeroUpdateIndex != 0;
}

void GhostConnection::writePacket(BitStream *bstream, PacketNotify *pnotify)
{
   Parent::writePacket(bstream, pnotify);
   GhostPacketNotify *notify = static_cast<GhostPacketNotify *>(pnotify);

   if(mConnectionParameters.mDebugObjectSizes)
      bstream->writeInt(DebugChecksum, 32);

   notify->ghostList = NULL;
   
   if(!doesGhostFrom())
      return;
   
   if(!bstream->writeFlag(mGhosting && mScopeObject.isValid()))
      return;
      
   // fill a packet (or two) with ghosting data

   // 2. call scoped objects' priority functions if the flag set is nonzero
   //    A removed ghost is assumed to have a high priority
   // 3. call updates based on sorted priority until the packet is
   //    full.  set flags to zero for all updated objects

   GhostInfo *walk;

   for(S32 i = mGhostZeroUpdateIndex - 1; i >= 0; i--)
   {
      if(!(mGhostArray[i]->flags & GhostInfo::InScope))
         detachObject(mGhostArray[i]);
   }

   U32 maxIndex = 0;
   for(S32 i = mGhostZeroUpdateIndex - 1; i >= 0; i--)
   {
      walk = mGhostArray[i];
      if(walk->index > maxIndex)
         maxIndex = walk->index;

      // clear out any kill objects that haven't been ghosted yet
      if((walk->flags & GhostInfo::KillGhost) && (walk->flags & GhostInfo::NotYetGhosted))
      {
         freeGhostInfo(walk);
         continue;
      }
      // don't do any ghost processing on objects that are being killed
      // or in the process of ghosting
      else if(!(walk->flags & (GhostInfo::KillingGhost | GhostInfo::Ghosting)))
      {
         if(walk->flags & GhostInfo::KillGhost)
            walk->priority = 10000;
         else
            walk->priority = walk->obj->getUpdatePriority(this, walk->updateMask, walk->updateSkipCount);
      }
      else
         walk->priority = 0;
   }
   GhostRef *updateList = NULL;
   if(mGhostZeroUpdateIndex != 0)
      qsort(&mGhostArray[0], mGhostZeroUpdateIndex, sizeof(GhostInfo *), UQECompare);
   // reset the array indices...
   for(S32 i = mGhostZeroUpdateIndex - 1; i >= 0; i--)
      mGhostArray[i]->arrayIndex = i;

   U8 sendSize = 0;
   while(maxIndex != 0)
   {
      maxIndex >>= 1;
      sendSize++;
   }

   if(sendSize < ID_BIT_OFFSET)
      sendSize = ID_BIT_OFFSET;

   bool BitSizeWritten = false;

   U32 count = 0;
   bool have_something_to_send = bstream->getBitPosition() >= 256;
   for(S32 i = mGhostZeroUpdateIndex - 1; i >= 0 && !bstream->isFull(); i--)
   {
      GhostInfo *walk = mGhostArray[i];
      if(walk->flags & (GhostInfo::KillingGhost | GhostInfo::Ghosting))
         continue;

      U32 updateStart = bstream->getBitPosition();
      U32 updateMask = walk->updateMask;
      U32 retMask = 0;
      ConnectionStringTable::PacketEntry *strEntry = getCurrentWritePacketNotify()->stringList.stringTail;;

      bstream->writeFlag(true);
      if(!BitSizeWritten)
      {
         BitSizeWritten = true;
         TNLAssert(((sendSize - ID_BIT_OFFSET) >> ID_BIT_SIZE) == 0, "invalid range");
         bstream->writeInt(sendSize - ID_BIT_OFFSET, ID_BIT_SIZE);
      }
      bstream->writeInt(walk->index, sendSize);
      if(!bstream->writeFlag(walk->flags & GhostInfo::KillGhost))
      {
         // this is an update of some kind:
         if(mConnectionParameters.mDebugObjectSizes)
            bstream->advanceBitPosition(BitStreamPosBitSize);

         S32 startPos = bstream->getBitPosition();

         if(walk->flags & GhostInfo::NotYetGhosted)
         {
            S32 classId = walk->obj->getClassId(getNetClassGroup());
            TNLAssert(U32(classId) < mGhostClassCount, "classID out of range");
            bstream->writeInt(classId, mGhostClassBitSize);
            NetObject::mIsInitialUpdate = true;
         }
         // update the object
         retMask = walk->obj->packUpdate(this, updateMask, bstream);

         if(NetObject::mIsInitialUpdate)
         {
            NetObject::mIsInitialUpdate = false;
            walk->obj->getClassRep()->addInitialUpdate(bstream->getBitPosition() - startPos);
         }
         else
            walk->obj->getClassRep()->addPartialUpdate(bstream->getBitPosition() - startPos);

         if(mConnectionParameters.mDebugObjectSizes)
            bstream->writeIntAt(bstream->getBitPosition(), BitStreamPosBitSize, startPos - BitStreamPosBitSize);

         logprintf(LogConsumer::LogGhostConnection, "GhostConnection %s GHOST %d", walk->obj->getClassName(), bstream->getBitPosition() - 16 - startPos);

         TNLAssert((retMask & (~updateMask)) == 0, "Cannot set new bits in packUpdate return");
      }

      // check for packet overrun, and rewind this update if there
      // was one:
      if(!bstream->isValid() || bstream->getBitPosition() >= mWriteMaxBitSize)
      {
         mStringTable->packetRewind(&getCurrentWritePacketNotify()->stringList, strEntry);  // we never sent those stuff (TableStringEntry), so let it drop
         TNLAssert(have_something_to_send || bstream->getBitPosition() < mWriteMaxBitSize, "Packet too big to send");
         if(have_something_to_send)
         {
            bstream->setBitPosition(updateStart);
            bstream->clearError();
            break;
         }
      }
      have_something_to_send = true;

      // otherwise, create a record of this ghost update and
      // attach it to the packet.
      GhostRef *upd = new GhostRef;

      upd->nextRef = updateList;
      updateList = upd;

      if(walk->lastUpdateChain)
         walk->lastUpdateChain->updateChain = upd;
      walk->lastUpdateChain = upd;

      upd->ghost = walk;
      upd->ghostInfoFlags = 0;
      upd->updateChain = NULL;

      if(walk->flags & GhostInfo::KillGhost)
      {
         walk->flags &= ~GhostInfo::KillGhost;
         walk->flags |= GhostInfo::KillingGhost;
         walk->updateMask = 0;
         upd->mask = updateMask;
         ghostPushToZero(walk);
         upd->ghostInfoFlags = GhostInfo::KillingGhost;
      }
      else
      {
         if(walk->flags & GhostInfo::NotYetGhosted)
         {
            walk->flags &= ~GhostInfo::NotYetGhosted;
            walk->flags |= GhostInfo::Ghosting;
            upd->ghostInfoFlags = GhostInfo::Ghosting;
         }
         walk->updateMask = retMask;
         if(!retMask)
            ghostPushToZero(walk);
         upd->mask = updateMask & ~retMask;
         walk->updateSkipCount = 0;
         count++;
      }
   }
   // count # of ghosts have been updated,
   // mGhostZeroUpdateIndex # of ghosts remain to be updated.
   // no more objects...
   bstream->writeFlag(false);
   notify->ghostList = updateList;
}

void GhostConnection::readPacket(BitStream *bstream)
{
   Parent::readPacket(bstream);

   if(mConnectionParameters.mDebugObjectSizes)
   {
      U32 USED_EXTERNAL sum = bstream->readInt(32);
      TNLAssert(sum == DebugChecksum, "Invalid checksum.");
   }

   if(!doesGhostTo())
      return;
   if(!bstream->readFlag())
      return;

   U8 idSize = U8_MAX;

   // while there's an object waiting...

   while(bstream->readFlag())
   {
      if(idSize == U8_MAX)
         idSize = (U8)  bstream->readInt( ID_BIT_SIZE ) + ID_BIT_OFFSET;
      //S32 startPos = bstream->getCurPos();
      U32 index = bstream->readInt(idSize);
      if(bstream->readFlag()) // is this ghost being deleted?
      {
         TNLAssert(index >= U32(mLocalGhosts.size()) || mLocalGhosts[index] != NULL, "Error, NULL ghost encountered.");
         if(index < U32(mLocalGhosts.size()) && mLocalGhosts[index])
         {
            mLocalGhosts[index]->onGhostRemove();
            mLocalGhosts[index]->decRef();  // This deletes the object if needed
            mLocalGhosts[index] = NULL;
         }
      }
      else
      {
         U32 endPosition = 0;
         if(mConnectionParameters.mDebugObjectSizes)
            endPosition = bstream->readInt(BitStreamPosBitSize);

         while(U32(mLocalGhosts.size()) <= index)  // Increase vector size when needed
            mLocalGhosts.push_back(NULL);

         if(!mLocalGhosts[index]) // it's a new ghost... cool
         {
            S32 classId = bstream->readInt(mGhostClassBitSize);
            if(U32(classId) >= mGhostClassCount)
            {
               setLastError("Invalid packet.");
               return;
            }

            NetObject *obj = (NetObject *) Object::create(getNetClassGroup(), NetClassTypeObject, classId);
            if(!obj)
            {
               setLastError("Invalid packet.");
               return;
            }
            obj->mOwningConnection = this;
            obj->mNetFlags = NetObject::IsGhost;
            obj->incRef(); // This is to disallow others delete our object

            // object gets initial update before adding to the manager

            obj->mNetIndex = index;
            mLocalGhosts[index] = obj;

            obj->onGhostAddBeforeUpdate(this);

            NetObject::mIsInitialUpdate = true;
            mLocalGhosts[index]->unpackUpdate(this, bstream);
            NetObject::mIsInitialUpdate = false;
            
            if(!obj->onGhostAdd(this))    // Runs addToGame() on some objects
            {
               if(!mErrorBuffer[0])
                  setLastError("Invalid packet.");
               return;
            }
            if(mRemoteConnection)
            {
               GhostConnection *gc = static_cast<GhostConnection *>(mRemoteConnection.getPointer());
               obj->mServerObject = gc->resolveGhostParent(index);
            }
         }
         else
         {
            mLocalGhosts[index]->unpackUpdate(this, bstream);
         }

         if(mConnectionParameters.mDebugObjectSizes)
         {
            TNLAssert(bstream->getBitPosition() == endPosition,
            avar("unpackUpdate did not match packUpdate for object of class %s. Expected %d bits, got %d bits.",
               mLocalGhosts[index]->getClassName(), endPosition, bstream->getBitPosition()) );
         }

         if(mErrorBuffer[0])
            return;
      }
   }
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

void GhostConnection::setScopeObject(NetObject *obj)
{
   if(((NetObject *) mScopeObject) == obj)
      return;
   mScopeObject = obj;
}

void GhostConnection::detachObject(GhostInfo *info)
{
   // mark it for ghost killin'
   info->flags |= GhostInfo::KillGhost;

   // if the mask is in the zero range, we've got to move it up...
   if(!info->updateMask)
   {
      info->updateMask = 0xFFFFFFFF;
      ghostPushNonZero(info);
   }
   if(info->obj)
   {
      if(info->prevObjectRef)
         info->prevObjectRef->nextObjectRef = info->nextObjectRef;
      else
         info->obj->mFirstObjectRef = info->nextObjectRef;
      if(info->nextObjectRef)
         info->nextObjectRef->prevObjectRef = info->prevObjectRef;
      // remove it from the lookup table
      
      U32 id = info->obj->getHashId();
      for(GhostInfo **walk = &mGhostLookupTable[id & GhostLookupTableMask]; *walk; walk = &((*walk)->nextLookupInfo))
      {
         GhostInfo *temp = *walk;
         if(temp == info)
         {
            *walk = temp->nextLookupInfo;
            break;
         }
      }
      info->prevObjectRef = info->nextObjectRef = NULL;
      info->obj = NULL;
   }
}

void GhostConnection::freeGhostInfo(GhostInfo *ghost)
{
   TNLAssert(ghost->arrayIndex < mGhostFreeIndex, "Ghost already freed.");
   if(ghost->arrayIndex < mGhostZeroUpdateIndex)
   {
      TNLAssert(ghost->updateMask != 0, "Invalid ghost mask.");
      ghost->updateMask = 0;
      ghostPushToZero(ghost);
   }
   ghostPushZeroToFree(ghost);
   TNLAssert(ghost->lastUpdateChain == NULL, "Ack!");
}

//-----------------------------------------------------------------------------

void GhostConnection::objectLocalScopeAlways(NetObject *obj)
{
   if(!doesGhostFrom())
      return;
   if(U32(obj->getClassId(getNetClassGroup())) >= U32(mGhostClassCount))
      return; // Not supported from both side of connection
   objectInScope(obj);
   for(GhostInfo *walk = mGhostLookupTable[obj->getHashId() & GhostLookupTableMask]; walk; walk = walk->nextLookupInfo)
   {
      if(walk->obj != obj)
         continue;
      walk->flags |= GhostInfo::ScopeLocalAlways;
      return;
   }
}

void GhostConnection::objectLocalClearAlways(NetObject *obj)
{
   if(!doesGhostFrom())
      return;
   for(GhostInfo *walk = mGhostLookupTable[obj->getHashId() & GhostLookupTableMask]; walk; walk = walk->nextLookupInfo)
   {
      if(walk->obj != obj)
         continue;
      walk->flags &= ~GhostInfo::ScopeLocalAlways;
      return;
   }
}

bool GhostConnection::validateGhostArray()
{
   TNLAssert(mGhostZeroUpdateIndex >= 0 && mGhostZeroUpdateIndex <= mGhostFreeIndex, "Invalid update index range.");
   TNLAssert(mGhostFreeIndex <= MaxGhostCount, "Invalid free index range.");
   S32 i;
   for(i = 0; i < mGhostZeroUpdateIndex; i ++)
   {
      TNLAssert(mGhostArray[i]->arrayIndex == i, "Invalid array index.");
      TNLAssert(mGhostArray[i]->updateMask != 0, "Invalid ghost mask.");
   }
   for(; i < mGhostFreeIndex; i ++)
   {
      TNLAssert(mGhostArray[i]->arrayIndex == i, "Invalid array index.");
      TNLAssert(mGhostArray[i]->updateMask == 0, "Invalid ghost mask.");
   }
   for(; i < mGhostArray.size(); i++)
   {
      TNLAssert(mGhostArray[i]->arrayIndex == i, "Invalid array index.");
   }
   return true;
}


void GhostConnection::objectInScope(NetObject *obj)
{
   if (!mScoping || !doesGhostFrom())     // doesGhostFrom ==>  Does this GhostConnection ghost NetObjects to the remote host?
      return;

   if (!obj->isGhostable() || (obj->isScopeLocal() && !isLocalConnection()))
      return;

   if(U32(obj->getClassId(getNetClassGroup())) >= U32(mGhostClassCount))
      return; // Not supported from both side of connection

   S32 index = obj->getHashId() & GhostLookupTableMask;
   
   // Check if it's already in scope:
   // The object may have been cleared out without the lookupTable being cleared
   // so validate that the object pointers are the same.
   for(GhostInfo *walk = mGhostLookupTable[index]; walk; walk = walk->nextLookupInfo)
   {
      if(walk->obj != obj)
         continue;
      walk->flags |= GhostInfo::InScope;
      return;
   }

   if(mGhostFreeIndex == MaxGhostCount)
      return;

   // create more GhostInfo here if needed
   if(mGhostArray.size() == mGhostFreeIndex)
   {
      S32 i = mGhostArray.size();
      GhostInfo *info = new GhostInfo();
      mGhostArray.push_back(info);
      mGhostRefs.push_back(info);
      info->obj = NULL;
      info->index = i;
      info->arrayIndex = i;
      info->updateMask = 0;
   }

   GhostInfo *giptr = mGhostArray[mGhostFreeIndex];
   ghostPushFreeToZero(giptr);
   giptr->updateMask = 0xFFFFFFFF;
   ghostPushNonZero(giptr);

   giptr->flags = GhostInfo::NotYetGhosted | GhostInfo::InScope;
   
   giptr->obj = obj;
   giptr->lastUpdateChain = NULL;
   giptr->updateSkipCount = 0;

   giptr->connection = this;

   giptr->nextObjectRef = obj->mFirstObjectRef;
   if(obj->mFirstObjectRef)
      obj->mFirstObjectRef->prevObjectRef = giptr;
   giptr->prevObjectRef = NULL;
   obj->mFirstObjectRef = giptr;
   
   giptr->nextLookupInfo = mGhostLookupTable[index];
   mGhostLookupTable[index] = giptr;
   //TNLAssert(validateGhostArray(), "Invalid ghost array!");
}

//-----------------------------------------------------------------------------

void GhostConnection::activateGhosting()
{
   if(!doesGhostFrom())    // Leave if we have no objects to ghost to the remote host
      return;

   mGhostingSequence++;
   logprintf(LogConsumer::LogGhostConnection, "Ghosting activated - %d", mGhostingSequence);

   TNLAssert((mGhostFreeIndex == 0) && (mGhostZeroUpdateIndex == 0), "Error: ghosts in the ghost list before activate.");

   mScoping = true; // so that objectInScope will work

   rpcStartGhosting(mGhostingSequence);
   //TNLAssert(validateGhostArray(), "Invalid ghost array!");
}

TNL_IMPLEMENT_RPC(GhostConnection, rpcStartGhosting, 
                  (U32 sequence), (sequence),
      NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirAny, 0)
{
   logprintf(LogConsumer::LogGhostConnection, "Got GhostingStarting %d", sequence);

   if(!doesGhostTo())
   {
      setLastError("Invalid packet.");
      return;
   }
   onStartGhosting();
   rpcReadyForNormalGhosts(sequence);
}

TNL_IMPLEMENT_RPC(GhostConnection, rpcReadyForNormalGhosts, 
                  (U32 sequence), (sequence),
      NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirAny, 0)
{
   logprintf(LogConsumer::LogGhostConnection, "Got ready for normal ghosts %d %d", sequence, mGhostingSequence);
   if(!doesGhostFrom())
   {
      setLastError("Invalid packet.");
      return;
   }
   if(sequence != mGhostingSequence)
      return;
   mGhosting = true;
}

TNL_IMPLEMENT_RPC(GhostConnection, rpcEndGhosting, (), (),
      NetClassGroupGameMask, RPCGuaranteedOrdered, RPCDirAny, 0)
{
   if(!doesGhostTo())
   {
      setLastError("Invalid packet.");
      return;
   }
   deleteLocalGhosts();
   onEndGhosting();
}

void GhostConnection::deleteLocalGhosts()
{
   if(!mGhostTo)
      return;

   // just delete all the local ghosts,
   // and delete all the ghosts in the current save list
   for(S32 i = 0; i < mLocalGhosts.size(); i++)
   {
      if(mLocalGhosts[i])
      {
         mLocalGhosts[i]->onGhostRemove();
         mLocalGhosts[i]->decRef(); // This deletes the object
         mLocalGhosts[i] = NULL;
      }
   }
}

void GhostConnection::clearGhostInfo()
{
   if(!mGhostFrom)
      return;

   // gotta clear out the ghosts...
   for(PacketNotify *walk = mNotifyQueueHead; walk; walk = walk->nextPacket)
   {
      GhostPacketNotify *note = static_cast<GhostPacketNotify *>(walk);
      GhostRef *delWalk = note->ghostList;
      note->ghostList = NULL;
      while(delWalk)
      {
         GhostRef *next = delWalk->nextRef;
         delete delWalk;
         delWalk = next;
      }
   }
   while(0 < mGhostFreeIndex)
   {
      detachObject(mGhostArray[0]);
      mGhostArray[0]->lastUpdateChain = NULL;
      freeGhostInfo(mGhostArray[0]); // this subtracts mGhostFreeIndex
   }
   TNLAssert((mGhostFreeIndex == 0) && (mGhostZeroUpdateIndex == 0), "Invalid indices.");

   for(U32 j = 0; j < U32(mGhostRefs.size()); j++)
      delete mGhostRefs[j];
   mGhostRefs.clear();
   mGhostArray.clear();
}

void GhostConnection::resetGhosting()
{
   if(!doesGhostFrom())
      return;
   // stop all ghosting activity
   // send a message to the other side notifying of this
   
   mGhosting = false;
   mScoping = false;
   rpcEndGhosting();
   mGhostingSequence++;
   clearGhostInfo();
   //TNLAssert(validateGhostArray(), "Invalid ghost array!");
}

//-----------------------------------------------------------------------------

NetObject *GhostConnection::resolveGhost(S32 id)
{
   if(id <= -1 || id >= mLocalGhosts.size())
      return NULL;

   return mLocalGhosts[id];
}

NetObject *GhostConnection::resolveGhostParent(S32 id)
{
   if(U32(id) >= U32(mGhostRefs.size()))
      return NULL;
   return mGhostRefs[id]->obj;
}

S32 GhostConnection::getGhostIndex(NetObject *obj)
{
   if(!obj)
      return -1;
   if(!doesGhostFrom())
      return obj->mNetIndex;
   S32 index = obj->getHashId() & GhostLookupTableMask;

   for(GhostInfo *gptr = mGhostLookupTable[index]; gptr; gptr = gptr->nextLookupInfo)
   {
      if(gptr->obj == obj && (gptr->flags & (GhostInfo::KillingGhost | GhostInfo::Ghosting | GhostInfo::NotYetGhosted | GhostInfo::KillGhost)) == 0)
         return gptr->index;
   }
   return -1;
}

//-----------------------------------------------------------------------------

void GhostConnection::onStartGhosting()
{
   // Do nothing
}

void GhostConnection::onEndGhosting()
{
   // Do nothing
}


void GhostConnection::writeConnectRequest(BitStream *stream)
{
   Parent::writeConnectRequest(stream);
   stream->writeInt(NetClassRep::getNetClassCount(getNetClassGroup(), NetClassTypeObject), 16);
}

// Reads the NetEvent class count max that the remote host is requesting.
// If this host has MORE NetEvent classes declared, the mGhostClassCount
// is set to the requested count, and is verified to lie on a boundary between versions.
// This gets run when someone is connecting to us
bool GhostConnection::readConnectRequest(BitStream *stream, NetConnection::TerminationReason &reason)
{
   if(!Parent::readConnectRequest(stream, reason))
      return false;

   U32 remoteClassCount = stream->readInt(16);

   U32 localClassCount = NetClassRep::getNetClassCount(getNetClassGroup(), NetClassTypeObject);

   // If remote client has more classes defined than we do, hope/assume they're defined in the same order, so that we at least agree
   // on the available set of RPCs.
   // This implies the client is higher version than the server  
   if(localClassCount <= remoteClassCount)
      mGhostClassCount = localClassCount;    // We're only willing to support as many as we have
   else     // We have more RPCs on the local machine ==> implies server is higher version than client
      mGhostClassCount = remoteClassCount;   // We're willing to support the number of classes the client has

   mGhostClassBitSize = getNextBinLog2(mGhostClassCount);
   return true;
}


void GhostConnection::writeConnectAccept(BitStream *stream)
{
   Parent::writeConnectAccept(stream);
   stream->writeInt(mGhostClassCount, 16);// Tell the client how many RPCs we, the server, are willing to support
                                          // (we may support more... see how this val is calced above)
}


bool GhostConnection::readConnectAccept(BitStream *stream, NetConnection::TerminationReason &reason)
{
   if(!Parent::readConnectAccept(stream, reason))
      return false;

   mGhostClassCount = stream->readInt(16);                                                      // Number of RPCs the remote server is willing to support
   U32 myCount = NetClassRep::getNetClassCount(getNetClassGroup(), NetClassTypeObject);   // Number we, the client, support

   if(mGhostClassCount > myCount)      // Normally, these should be equal.  If the server is not willing to support
   {                                   // as many RPCs as we want to use, then bail.
      logprintf(LogConsumer::LogConnection, "Connection failed due to a disagreement on the number of RPCs supported.");
      return false;
   }

   mGhostClassBitSize = getNextBinLog2(mGhostClassCount);
   return true;
}



};
