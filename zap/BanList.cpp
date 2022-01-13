//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "BanList.h"
#include "config.h"
#include "stringUtils.h"

#include "tnlLog.h"

#include <chrono>
#include <ctime>

namespace Zap
{

using namespace chrono;

// Constructor
BanList::BanList(const string &iniDir)
{
   banListTokenDelimiter = "|";
   banListWildcardCharater = "*";

   defaultBanDurationMinutes = 60;
   kickDurationMilliseconds = 30 * 1000;     // 30 seconds is a good breather
}


// Destructor
BanList::~BanList()
{
   // Do nothing
}


string addressToString(const Address &address)
{
   // Build proper IP Address string
   char addressBuffer[16];
   dSprintf(addressBuffer, 16, "%d.%d.%d.%d", U8(address.netNum[0] >> 24 ),
         U8 (address.netNum[0] >> 16 ), U8 (address.netNum[0] >> 8), U8(address.netNum[0]));

   return string(addressBuffer);
}


// Return the current time
string timeNowToISOString()
{
   // Get current time
   time_t now = system_clock::to_time_t(system_clock::now());

   // Convert to ISO string
   char buf[sizeof "11111111T111111"];
   strftime(buf, sizeof buf, "%Y%m%dT%H%M%S", localtime(&now));

   return string(buf);
}


// Convert our ISO time string into a time_t object
time_t ISOStringToTime(const string &timeString)
{
   int year, month, day, hh, mm, ss;
   struct tm when = {0};

   // Parse string that matches timeNowToISOString()
   sscanf(timeString.c_str(), "%04d%02d%02dT%02d%02d%02d", &year, &month, &day, &hh, &mm, &ss);

   when.tm_year = year-1900;  // Funny quirk with <ctime>
   when.tm_mon = month-1;     // Another quirk
   when.tm_mday = day;
   when.tm_hour = hh;
   when.tm_min = mm;
   when.tm_sec = ss;

   time_t converted = mktime(&when);

   return converted;
}


void BanList::addToBanList(const Address &address, S32 durationMinutes, bool nonAuthenticatedOnly)
{
   BanItem banItem;
   banItem.durationMinutes = itos(durationMinutes);
   banItem.address = addressToString(address);
   banItem.nickname = nonAuthenticatedOnly ? "*NonAuthenticated" : "*";
   banItem.startDateTime = timeNowToISOString();

   serverBanList.push_back(banItem);
}

void BanList::addPlayerNameToBanList(const char *playerName, S32 durationMinutes)
{
   BanItem banItem;
   banItem.durationMinutes = itos(durationMinutes);
   banItem.address = "*";
   banItem.nickname = playerName;
   banItem.startDateTime = timeNowToISOString();

   serverBanList.push_back(banItem);
}


void BanList::removeFromBanList(const Address &address)
{
   // TODO call this from an admin command?
   return;
}


bool BanList::processBanListLine(const string &line)
{
   // Tokenize the line
   Vector<string> words;
   parseString(line.c_str(), words, banListTokenDelimiter[0]);

   // Check for incorrect number of tokens => 4, which is the member count of the BanItem struct
   if (words.size() < 4)
      return false;

   // Check to make sure there is at lease one character in each token
   for (S32 i = 0; i < 4; i++)
      if(words[i].length() < 1)
         return false;

   // IP, nickname, startTime, duration <- in this order
   string address = words[0];
   string nickname = words[1];
   string startDateTime = words[2];
   string durationMinutes = words[3];

   // Validate IP address string
   if (!(Address(address.c_str()).isValid()) && address.compare(banListWildcardCharater) != 0)
      return false;

   // nickname could be anything...

   // Validate date
   time_t tempDateTime = 0;

   // If exception is thrown, then date didn't parse correctly
   try
   {
      tempDateTime = ISOStringToTime(startDateTime);
   }
   catch (...)
   {
      return false;
   }
   // If date time ends up equaling empty time_t, then it didn't parse right either
   if (tempDateTime == 0)
      return false;

   // Validate duration
   if(atoi(durationMinutes.c_str()) <= 0)
      return false;

   // Now finally add to banList
   BanItem banItem;
   banItem.address = address;
   banItem.nickname = nickname;
   banItem.startDateTime = startDateTime;
   banItem.durationMinutes = durationMinutes;

   serverBanList.push_back(banItem);

   // Phoew! we made it..
   return true;
}


string BanList::banItemToString(BanItem *banItem)
{
   // IP, nickname, startTime, duration     <- in this order

   return
         banItem->address + banListTokenDelimiter +
         banItem->nickname + banListTokenDelimiter +
         banItem->startDateTime + banListTokenDelimiter +
         banItem->durationMinutes;
}


bool BanList::isBanned(const Address &address, const string &nickname, bool isAuthenticated)
{
   string addressString = addressToString(address);
   auto currentTime = system_clock::now();

   for (S32 i = 0; i < serverBanList.size(); i++)
   {
      // Check IP
      if (addressString.compare(serverBanList[i].address) != 0 && serverBanList[i].address.compare("*") != 0)
         continue;

      // Check if authenticated
      if (serverBanList[i].nickname.compare("*NonAuthenticated") == 0 && isAuthenticated)
         continue;

      // Check nickname
      else if (nickname.compare(serverBanList[i].nickname) != 0 && serverBanList[i].nickname.compare("*") != 0)
         continue;

      // Check time
      time_t banTime = ISOStringToTime(serverBanList[i].startDateTime);
      auto chronoBanTime = system_clock::from_time_t(banTime);
      auto timeDifference = duration_cast<minutes>(currentTime - chronoBanTime).count();

      int banDurationMinutes = atoi(serverBanList[i].durationMinutes.c_str());

      // See if ban has run out
      if (timeDifference < banDurationMinutes)
         continue;

      // If we get here, that means nickname and IP address matched and we are still in the
      // ban allotted time period
      return true;
   }

   return false;
}


string BanList::getDelimiter()
{
   return banListTokenDelimiter;
}


string BanList::getWildcard()
{
   return banListWildcardCharater;
}


S32 BanList::getKickDuration()
{
   return kickDurationMilliseconds;
}


S32 BanList::getDefaultBanDuration()
{
   return defaultBanDurationMinutes;
}


Vector<string> BanList::banListToString()
{
   Vector<string> banList;
   for(S32 i = 0; i < serverBanList.size(); i++)
      banList.push_back(banItemToString(&serverBanList[i]));

   return banList;
}


void BanList::loadBanList(const Vector<string> &banItemList)
{
   serverBanList.clear();  // Clear old list for /loadini command.
   for(S32 i = 0; i < banItemList.size(); i++)
      if(!processBanListLine(banItemList[i]))
         logprintf("Ban list item on line %d is malformed: %s", i+1, banItemList[i].c_str());
      else
         logprintf("Loading ban: %s", banItemList[i].c_str());
}


void BanList::kickHost(const Address &address)
{
   KickedHost h;
   h.address = address;
   h.kickTimeRemaining = kickDurationMilliseconds;
   serverKickList.push_back(h);
}


bool BanList::isAddressKicked(const Address &address)
{
   for(S32 i = 0; i < serverKickList.size(); i++)
      if(address.isEqualAddress(serverKickList[i].address))
         return true;

   return false;
}


void BanList::updateKickList(U32 timeElapsed)
{
   for(S32 i = 0; i < serverKickList.size(); )
   {
      if(serverKickList[i].kickTimeRemaining < timeElapsed)
         serverKickList.erase_fast(i);
      else
      {
         serverKickList[i].kickTimeRemaining -= timeElapsed;
         i++;
      }
   }
}


} /* namespace Zap */
