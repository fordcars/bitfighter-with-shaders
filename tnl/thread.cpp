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

#include "tnlThread.h"
#include "tnlLog.h"

#ifndef TNL_OS_WIN32
#include "stdint.h"
#endif

namespace TNL
{

#if defined(TNL_NO_THREADS)

Semaphore::Semaphore(U32 initialCount, U32 maximumCount) {}
Semaphore::~Semaphore() {}
void Semaphore::wait() {}
void Semaphore::increment(U32 count) {}

Mutex::Mutex() {}
Mutex::~Mutex() {}
void Mutex::lock() {}
void Mutex::unlock() {}
bool Mutex::tryLock() { return false; }

ThreadStorage::ThreadStorage() {}
ThreadStorage::~ThreadStorage() {}
void *ThreadStorage::get() { return NULL; }
void ThreadStorage::set(void *value) {}

Thread::Thread() {}
Thread::~Thread() {}

bool Thread::start()
{
   run();
   return true;
}

#elif defined(TNL_OS_WIN32)
Semaphore::Semaphore(U32 initialCount, U32 maximumCount)
{
   mSemaphore = CreateSemaphore(NULL, initialCount, maximumCount, NULL);
}

Semaphore::~Semaphore()
{
   CloseHandle(mSemaphore);
}

void Semaphore::wait()
{
   WaitForSingleObject(mSemaphore, INFINITE);
}

void Semaphore::increment(U32 count)
{
   ReleaseSemaphore(mSemaphore, count, NULL);
}

Mutex::Mutex()
{
   InitializeCriticalSection(&mLock);
}

Mutex::~Mutex()
{
   DeleteCriticalSection(&mLock);
}

void Mutex::lock()
{
   EnterCriticalSection(&mLock);
}

void Mutex::unlock()
{
   LeaveCriticalSection(&mLock);
}

bool Mutex::tryLock()
{
   return false;//   return TryEnterCriticalSection(&mLock);
}

ThreadStorage::ThreadStorage()
{
   mTlsIndex = TlsAlloc();
}

ThreadStorage::~ThreadStorage()
{
   TlsFree(mTlsIndex);
}

void *ThreadStorage::get()
{
   return TlsGetValue(mTlsIndex);
}

void ThreadStorage::set(void *value)
{
   TlsSetValue(mTlsIndex, value);
}

DWORD WINAPI ThreadProc( LPVOID lpParameter )
{
   return ((Thread *) lpParameter)->run();
}

U32 Thread::run()
{
   return 0;
}

bool Thread::start()
{
   HANDLE thread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
   if(thread == NULL)
      return false;
   CloseHandle(thread); // This does not kill the thread or cause any problems, it is to prevent memory leak
   return true;
}

Thread::Thread()
{
}

Thread::~Thread()
{
}

#else

Semaphore::Semaphore(U32 initialCount, U32 maximumCount)
{
   sem_init(&mSemaphore, 0, initialCount);
}

Semaphore::~Semaphore()
{
   sem_destroy(&mSemaphore);
}

void Semaphore::wait()
{
   sem_wait(&mSemaphore);
}

void Semaphore::increment(U32 count)
{
   for(U32 i = 0; i < count; i++)
      sem_post(&mSemaphore);
}


// Workaround for missing definition on some systems
#if !defined(PTHREAD_MUTEX_RECURSIVE) && (defined(TNL_OS_LINUX) || defined (TNL_OS_ANDROID))
#  define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

Mutex::Mutex()
{
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&mMutex, &attr);
   pthread_mutexattr_destroy(&attr);
}

Mutex::~Mutex()
{
   pthread_mutex_destroy(&mMutex);
}

void Mutex::lock()
{
   pthread_mutex_lock(&mMutex);
}

void Mutex::unlock()
{
   pthread_mutex_unlock(&mMutex);
}

bool Mutex::tryLock()
{
   return false;//   return TryEnterCriticalSection(&mLock);
}

ThreadStorage::ThreadStorage()
{
   pthread_key_create(&mThreadKey, NULL);
}

ThreadStorage::~ThreadStorage()
{
   pthread_key_delete(mThreadKey);
}

void *ThreadStorage::get()
{
   return pthread_getspecific(mThreadKey);
}

void ThreadStorage::set(void *value)
{
   pthread_setspecific(mThreadKey, value);
}

void *ThreadProc(void *lpParameter)
{
   return (void *) (uintptr_t) ((Thread *) lpParameter)->run();
}

Thread::Thread()
{
}

bool Thread::start()
{
   pthread_t thread;
   if(pthread_create(&thread, NULL, ThreadProc, this) != 0)
      return false;
   pthread_detach(thread);  // Declare that this thread clean itself up after finishing
   return true;
}

Thread::~Thread()
{
}

#endif

ThreadQueue::ThreadQueueThread::ThreadQueueThread(ThreadQueue *q)
{
   mThreadQueue = q;
}

U32 ThreadQueue::ThreadQueueThread::run()
{
   mThreadQueue->threadStart();

   mThreadQueue->lock();
   ThreadStorage &sto = mThreadQueue->getStorage();
   sto.set((void *) 0);
   mThreadQueue->unlock();

   for(;;)
      mThreadQueue->dispatchNextCall();
   return 0;
}

ThreadQueue::ThreadQueue(U32 threadCount)
{
   mStorage.set((void *) 1);
   for(U32 i = 0; i < threadCount; i++)
   {
      Thread *theThread = new ThreadQueueThread(this);
      mThreads.push_back(theThread);
      theThread->start();
   }
}

ThreadQueue::~ThreadQueue()
{
}

void ThreadQueue::dispatchNextCall()
{
   mSemaphore.wait();
   lock();
   if(mThreadCalls.size() == 0)
   {
      unlock();
      return;
   }
   Functor *c = mThreadCalls.first();
   mThreadCalls.pop_front();
   unlock();
   c->dispatch(this);
   delete c;
}

void ThreadQueue::postCall(Functor *theCall)
{
   lock();
   if(isMainThread())
   {
      mThreadCalls.push_back(theCall);
      unlock();
      mSemaphore.increment();
   }
   else
   {
      mResponseCalls.push_back(theCall);
      unlock();
   }
}

void ThreadQueue::dispatchResponseCalls()
{
   lock();
   for(S32 i = 0; i < mResponseCalls.size(); i++)
   {
      Functor *c = mResponseCalls[i];
      c->dispatch(this);
      delete c;
   }
   mResponseCalls.clear();
   unlock();
}

};
