//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "statistics.h"
#include "gameWeapons.h"

#include "MathUtils.h"

namespace Zap
{

// Constructor
Statistics::Statistics()
{
   mTotalKills = 0;      
   mTotalFratricides = 0;
   mTotalDeaths = 0;     
   mTotalSuicides = 0; 
   mGamesPlayed = 0;
   mLongestKillStreak = 0;

   resetStatistics();
}


// Destructor
Statistics::~Statistics()
{
   // Do nothing
}


void Statistics::countShot(WeaponType weaponType)
{
   TNLAssert(weaponType < WeaponCount, "Out of range");
   mShots[(S32) weaponType]++;
}


void Statistics::countHit(WeaponType weaponType)
{
   TNLAssert(weaponType < WeaponCount, "Out of range");
   mHits[(S32) weaponType]++;
}


void Statistics::countHitBy(WeaponType weaponType)
{
   TNLAssert(weaponType < WeaponCount, "Out of range");
   mHitBy[(S32) weaponType]++;
}


S32 Statistics::getShots()
{
   S32 totalShots = 0;

   for(S32 i = 0; i < WeaponCount; i++)
      totalShots += mShots[i];

   return totalShots;
}


S32 Statistics::getShots(WeaponType weaponType)
{
   return mShots[(S32)weaponType];
}


Vector<U32> Statistics::getShotsVector()
{
   Vector<U32>(shots);
   shots.resize(WeaponCount);
   for(S32 i = 0; i < WeaponCount; i++)
      shots[i] = mShots[i];
   return shots;
}


Vector<U32> Statistics::getHitsVector()
{
   Vector<U32>(hits);
   hits.resize(WeaponCount);
   for(S32 i = 0; i < WeaponCount; i++)
      hits[i] = mHits[i];
   return hits;
}


S32 Statistics::getHits()
{
   S32 totalHits = 0;

   for(S32 i = 0; i < WeaponCount; i++)
      totalHits += mHits[i];

   return totalHits;
}


S32 Statistics::getHits(WeaponType weaponType)
{
   return mHits[(S32)weaponType];
}


// Report overall hit rate
F32 Statistics::getHitRate()
{
   return (F32)getHits() / (F32)getShots();
}


// Report hit rate for specified weapon
F32 Statistics::getHitRate(WeaponType weaponType)
{
   return (F32)mHits[(S32)weaponType] / (F32)mShots[(S32)weaponType];
}


S32 Statistics::getHitBy(WeaponType weaponType)
{
   return mHitBy[(S32)weaponType];
}

void Statistics::addModuleUsed(ShipModule module, U32 milliseconds)
{
   TNLAssert(U32(module) < U32(ModuleCount), "ShipModule out of range");
   mModuleUsedTime[(S32)module] += milliseconds;
}


U32 Statistics::getModuleUsed(ShipModule module)
{
   TNLAssert(U32(module) < U32(ModuleCount), "ShipModule out of range");
   return mModuleUsedTime[(S32)module];
}


void Statistics::addGamePlayed()
{
   mGamesPlayed++;
}


// Player killed another player
void Statistics::addKill(U32 killStreak)
{
   mKills++;
   mTotalKills++;
   mLongestKillStreak = MAX(killStreak, mLongestKillStreak);
}


// Report cumulated kills
U32 Statistics::getKills()
{
   return mKills;
}


// Player got killed
void Statistics::addDeath()
{
   mDeaths++;
   mTotalDeaths++;
}


// Report cumulated deaths
U32 Statistics::getDeaths()
{
   return mDeaths;
}


U32 Statistics::getLongestKillStreak() const
{
   return mLongestKillStreak;
}


// Player killed self
void Statistics::addSuicide()
{
   mSuicides++;
   mTotalSuicides++;
}


// Report cumulated suicides
U32 Statistics::getSuicides()
{
   return mSuicides;
}


// Player killed teammate
void Statistics::addFratricide()
{
   mFratricides++;
   mTotalFratricides++;
}


// Report cumulated fratricides
U32 Statistics::getFratricides()
{
   return mFratricides;
}


void Statistics::addLoadout(U32 loadoutHash)
{
   mLoadouts.push_back(loadoutHash);
}


Vector<U32> Statistics::getLoadouts()
{
   return mLoadouts;
}


// Return a measure of a player's strength.
// Right now this is roughly a kill - death / kill + death ratio
// Better might be: https://secure.wikimedia.org/wikipedia/en/wiki/Elo_rating_system
F32 Statistics::getCalculatedRating()
{
   // Total kills = mKills + mFratricides (but we won't count mFratricides)
   // Counted deaths = mDeaths - mSuicides (mSuicides are included in mDeaths and we want to ignore them)
   // Use F32 here so we don't underflow with U32 math; probably not necessary
   F32 deathsDueToEnemyAction   = F32(mTotalDeaths) - F32(mTotalSuicides);
   F32 totalTotalKillsAndDeaths = F32(mTotalKills) + deathsDueToEnemyAction;

   // Initial case: you haven't killed or died -- go out and prove yourself, lad!
   if(totalTotalKillsAndDeaths == 0)
      return 0;

   // Standard case
   else   
      return ((F32)mTotalKills - deathsDueToEnemyAction) / totalTotalKillsAndDeaths;
}


static const U32 DIST_MULTIPLIER = 10000;
static const U32 DIST_RATCHETING_DOWN_FACTOR = 10;    // The higher this is, the slower distance accrues

void Statistics::accumulateDistance(F32 dist)
{
   // We'll track distance as an integer to avoid data loss due to a small float being added to a big one.
   // Not that precision is important; it isn't... but rather to avoid appearance of "going no further" once
   // you've gone a certain distance.
   mDist += U64(dist * DIST_MULTIPLIER);
}


U32 Statistics::getDistanceTraveled()
{
   if(mDist / DIST_MULTIPLIER > (U64)U32_MAX)
      return U32_MAX;
   else
      return U32(mDist / (DIST_MULTIPLIER * DIST_RATCHETING_DOWN_FACTOR));
}


// Gets called at beginning of each game -- stats listed here do not persist
void Statistics::resetStatistics()
{
   mKills = 0;
   mDeaths = 0;
   mSuicides = 0;
   mFratricides = 0;
   mDist = 0;

   for(S32 i = 0; i < WeaponCount; i++)
   {
      mShots[i] = 0;
      mHits[i] = 0;
      mHitBy[i] = 0;
   }

   for(S32 i = 0; i < ModuleCount; i++)
      mModuleUsedTime[i] = 0;

   mLoadouts.clear();

   mFlagPickup = 0;
   mFlagReturn = 0;
   mFlagScore = 0;
   mFlagDrop = 0;
   mTurretsKilled = 0;
   mFFsKilled = 0;
   mAsteroidsKilled = 0;
   mCrashedIntoAsteroid = 0;
   mChangedLoadout = 0;
   mTeleport = 0;
   mPlayTime = 0;
   mTurretsEngineered = 0;
   mFFsEngineered = 0;
   mTeleportersEngineered = 0;
}

}

