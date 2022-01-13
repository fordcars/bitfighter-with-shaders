//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "game.h"

#include "GameManager.h"

#include "gameType.h"
#include "masterConnection.h"
#include "robot.h"
#include "stringUtils.h"
#include "SlipZone.h"  
#include "Teleporter.h"
#include "ServerGame.h"
#include "gameNetInterface.h"
#include "gameLoader.h"          // Parent class

#include "md5wrapper.h"

#include <sstream>

#include "../master/DatabaseAccessThread.h"

using namespace TNL;

namespace Zap
{


////////////////////////////////////////
////////////////////////////////////////

// Constructor
NameToAddressThread::NameToAddressThread(const char *address_string) : mAddress_string(address_string)
{
   mDone = false;
}

// Destructor
NameToAddressThread::~NameToAddressThread()
{
   // Do nothing
}


U32 NameToAddressThread::run()
{
   // This can take a lot of time converting name (such as "bitfighter.org:25955") into IP address.
   mAddress.set(mAddress_string);
   mDone = true;
   return 0;
}


////////////////////////////////////////
////////////////////////////////////////

static Vector<DatabaseObject *> fillVector2;
md5wrapper Game::md5;

////////////////////////////////////
////////////////////////////////////

// Statics
static Game *mObjectAddTarget = NULL;

const U32 Game::CurrentLevelFormat = 2;

// Constructor
Game::Game(const Address &theBindAddress, GameSettingsPtr settings) : mGameObjDatabase(new GridDatabase())  // New database will be deleted by boost
{
   mLegacyGridSize = 1.f;              // Default to 1 unless we detect LevelFormat is missing or there's a GridSize parameter
   mLevelFormat = CurrentLevelFormat;  // Default to current format version
   mHasLevelFormat = false;

   mLevelDatabaseId = 0;
   mSettings = settings;

   mNextMasterTryTime = 0;
   mReadyToConnectToMaster = false;

   mCurrentTime = 0;
   mGameSuspended = false;

   mRobotCount = 0;
   mPlayerCount = 0;

   mTimeUnconnectedToMaster = 0;

   mNetInterface = new GameNetInterface(theBindAddress, this);
   mHaveTriedToConnectToMaster = false;

   mNameToAddressThread = NULL;

   mActiveTeamManager = &mTeamManager;

   mObjectsLoaded = 0;

   mSecondaryThread = new Master::DatabaseAccessThread();
}


// Destructor
Game::~Game()
{
   if(mNameToAddressThread)
      delete mNameToAddressThread;
   delete mSecondaryThread;
}


F32 Game::getLegacyGridSize() const
{
   return mLegacyGridSize;
}


U32 Game::getCurrentTime()
{
   return mCurrentTime;
}


const Vector<SafePtr<BfObject> > &Game::getScopeAlwaysList()
{
   return mScopeAlwaysList;
}


void Game::setScopeAlwaysObject(BfObject *theObject)
{
   mScopeAlwaysList.push_back(theObject);
}


void Game::setAddTarget()
{
   mObjectAddTarget = this;
}


// Clear the addTarget, but only if it's us -- this prevents the ServerGame destructor from wiping this out
// after it has already been set by the editor after testing a level.
void Game::clearAddTarget()
{
   if(mObjectAddTarget == this)
      mObjectAddTarget = NULL;
}


// When we're adding an object and don't know where to put it... put it here!
// Static method
Game *Game::getAddTarget()
{
   return mObjectAddTarget;
}


bool Game::isSuspended() const
{
   return mGameSuspended;
}


GameSettings *Game::getSettings() const
{
   return mSettings.get();
}


GameSettingsPtr Game::getSettingsPtr() const
{
   return mSettings;
}


S32    Game::getBotCount() const                          { TNLAssert(false, "Not implemented for this class!"); return 0; }
Robot *Game::findBot(const char *id)                      { TNLAssert(false, "Not implemented for this class!"); return NULL; }
string Game::addBot(const Vector<const char *> &args, ClientInfo::ClientClass clientClass)     
                                                          { TNLAssert(false, "Not implemented for this class!"); return "";    }

void   Game::kickSingleBotFromLargestTeamWithBots()  { TNLAssert(false, "Not implemented for this class!"); }
void   Game::moreBots()                              { TNLAssert(false, "Not implemented for this class!"); }
void   Game::fewerBots()                             { TNLAssert(false, "Not implemented for this class!"); }
Robot *Game::getBot(S32 index)                       { TNLAssert(false, "Not implemented for this class!"); return NULL; }
void   Game::addBot(Robot *robot)                    { TNLAssert(false, "Not implemented for this class!"); }
void   Game::removeBot(Robot *robot)                 { TNLAssert(false, "Not implemented for this class!"); }
void   Game::deleteBot(const StringTableEntry &name) { TNLAssert(false, "Not implemented for this class!"); }
void   Game::deleteBot(S32 i)                        { TNLAssert(false, "Not implemented for this class!"); }
void   Game::deleteBotFromTeam(S32 teamIndex)        { TNLAssert(false, "Not implemented for this class!"); }
void   Game::deleteAllBots()                         { TNLAssert(false, "Not implemented for this class!"); }
void   Game::balanceTeams()                          { TNLAssert(false, "Not implemented for this class!"); }



void Game::setReadyToConnectToMaster(bool ready)
{
   mReadyToConnectToMaster = ready;
}


Point Game::getScopeRange(bool sensorEquipped)
{
   if(sensorEquipped)
      return Point(PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_HORIZONTAL + PLAYER_SCOPE_MARGIN,
            PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_VERTICAL + PLAYER_SCOPE_MARGIN);

   return Point(PLAYER_VISUAL_DISTANCE_HORIZONTAL + PLAYER_SCOPE_MARGIN,
         PLAYER_VISUAL_DISTANCE_VERTICAL + PLAYER_SCOPE_MARGIN);
}


S32 Game::getClientCount() const
{
   return mClientInfos.size();
}


// Return the number of human players (does not include bots)
S32 Game::getPlayerCount() const
{
   return mPlayerCount;
}


S32 Game::getAuthenticatedPlayerCount() const
{
   S32 count = 0;
   for(S32 i = 0; i < mClientInfos.size(); i++)
      if(!mClientInfos[i]->isRobot() && mClientInfos[i]->isAuthenticated())
         count++;

   return count;
}


S32 Game::getRobotCount() const
{
   return mRobotCount;
}


ClientInfo *Game::getClientInfo(S32 index) const
{ 
   return mClientInfos[index]; 
}


const Vector<RefPtr<ClientInfo> > *Game::getClientInfos()
{
   return &mClientInfos;
}


// ClientInfo will be a RemoteClientInfo in ClientGame and a FullClientInfo in ServerGame
void Game::addToClientList(ClientInfo *clientInfo) 
{ 
   // Adding the same ClientInfo twice is never The Right Thing To Do
   //
   // NOTE - This can happen when a Robot line is found in a level file.  For some reason
   // it tries to get added twice to the game
   for(S32 i = 0; i < mClientInfos.size(); i++)
   {
      if(mClientInfos[i] == clientInfo)
         return;
   }

   mClientInfos.push_back(clientInfo);

   if(clientInfo->isRobot())
      mRobotCount++;
   else
      mPlayerCount++;
}     


// Helper function for other find functions
S32 Game::findClientIndex(const StringTableEntry &name)
{
   for(S32 i = 0; i < mClientInfos.size(); i++)
   {
      if(mClientInfos[i]->getName() == name) 
         return i;
   }

   return -1;     // Not found
}


void Game::removeFromClientList(const StringTableEntry &name)
{
   S32 index = findClientIndex(name);

   if(index >= 0)
   {
      if(mClientInfos[index]->isRobot())
         mRobotCount--;
      else
         mPlayerCount--;

      mClientInfos.erase_fast(index);
   }
}


void Game::removeFromClientList(ClientInfo *clientInfo)
{
   for(S32 i = 0; i < mClientInfos.size(); i++)
      if(mClientInfos[i] == clientInfo)
      {
         if(mClientInfos[i]->isRobot())
            mRobotCount--;
         else
            mPlayerCount--;

         mClientInfos.erase_fast(i);
         return;
      }
}


void Game::clearClientList() 
{ 
   mClientInfos.clear();   // ClientInfos are refPtrs, so this will delete them

   mRobotCount = 0;
   mPlayerCount = 0;
}


// Find clientInfo given a player name
ClientInfo *Game::findClientInfo(const StringTableEntry &name)
{
   S32 index = findClientIndex(name);

   return index >= 0 ? mClientInfos[index] : NULL;
}


// Currently only used on client, for various effects
// Will return NULL if ship is out-of-scope... we have ClientInfos for all players, but not aways their ships
Ship *Game::findShip(const StringTableEntry &clientName)
{
   ClientInfo *clientInfo = findClientInfo(clientName);
   
   if(clientInfo)
      return clientInfo->getShip();
   else
      return NULL;
}


GameNetInterface *Game::getNetInterface()
{
   return mNetInterface;
}


GridDatabase *Game::getGameObjDatabase()
{
   return mGameObjDatabase.get();
}


MasterServerConnection *Game::getConnectionToMaster()
{
   return mConnectionToMaster;
}


S32 Game::getClientId()
{
   if(mConnectionToMaster)
      return mConnectionToMaster->getClientId();

   return 0;
}


// Only used for testing
void Game::setConnectionToMaster(MasterServerConnection *connection)
{
   TNLAssert(mConnectionToMaster.isNull(), "mConnectionToMaster not NULL");
   mConnectionToMaster = connection;
}


// Unused?
//void Game::runAnonymousMasterRequest(MasterConnectionCallback callback)
//{
//   TNLAssert(mAnonymousMasterServerConnection.isNull(), "An anonymous master connection is still open!");
//
//   // Create a new anonymous connection and set a callback method
//   // You need to make sure to remember to put terminateIfAnonymous() into the master
//   // response RPC that this callback calls
//   mAnonymousMasterServerConnection = new AnonymousMasterServerConnection(this);
//   mAnonymousMasterServerConnection->setConnectionCallback(callback);
//}


GameType *Game::getGameType() const
{
   return mGameType;    // This is a safePtr, so it can be NULL, but will never point off into space
}


// There is a bigger need to use StringTableEntry and not const char *
//    mainly to prevent errors on CTF neutral flag and out of range team number.
StringTableEntry Game::getTeamName(S32 teamIndex) const
{
   if(teamIndex >= 0 && teamIndex < getTeamCount())
      return getTeam(teamIndex)->getName();
   else if(teamIndex == TEAM_HOSTILE)
      return StringTableEntry("Hostile");
   else if(teamIndex == TEAM_NEUTRAL)
      return StringTableEntry("Neutral");
   else
      return StringTableEntry("UNKNOWN");
}


// Given a player's name, return his team
S32 Game::getTeamIndex(const StringTableEntry &playerName)
{
   ClientInfo *clientInfo = findClientInfo(playerName);              // Returns NULL if player can't be found
   
   return clientInfo ? clientInfo->getTeamIndex() : TEAM_NEUTRAL;    // If we can't find the team, let's call it neutral
}


// The following just delegate their work to the TeamManager
void Game::removeTeam(S32 teamIndex)                  { mActiveTeamManager->removeTeam(teamIndex);    }
void Game::addTeam(AbstractTeam *team)                { mActiveTeamManager->addTeam(team);            }
void Game::addTeam(AbstractTeam *team, S32 index)     { mActiveTeamManager->addTeam(team, index);     }
void Game::replaceTeam(AbstractTeam *team, S32 index) { mActiveTeamManager->replaceTeam(team, index); }
void Game::clearTeams()                               { mActiveTeamManager->clearTeams();             }
void Game::clearTeamHasFlagList()                     { mActiveTeamManager->clearTeamHasFlagList();   }


bool Game::addPolyWall(BfObject *polyWall, GridDatabase *database)
{
   return polyWall->addToGame(this, database);
}


void Game::addWallItem(BfObject *wallItem, GridDatabase *database)
{
   wallItem->addToGame(this, database);
}


// Pass through to to GameType
bool Game::addWall(const WallRec &barrier) { return mGameType->addWall(barrier, this); }


void Game::setTeamHasFlag(S32 teamIndex, bool hasFlag)
{
   mActiveTeamManager->setTeamHasFlag(teamIndex, hasFlag);
}


// Get slowing factor if we are in a slip zone; could be used if we have go faster zones
F32 Game::getShipAccelModificationFactor(const Ship *ship) const
{
   BfObject *obj = ship->isInZone(SlipZoneTypeNumber);

   if(obj)
   {
      SlipZone *slipzone = static_cast<SlipZone *>(obj);
      return slipzone->slipAmount;
   }

   return 1.0f;
}


void Game::teleporterDestroyed(Teleporter *teleporter)
{
   if(teleporter)
      teleporter->onDestroyed();       
}


S32           Game::getTeamCount()                const { return mActiveTeamManager->getTeamCount();            } 
AbstractTeam *Game::getTeam(S32 team)             const { return mActiveTeamManager->getTeam(team);             }
bool          Game::getTeamHasFlag(S32 teamIndex) const { return mActiveTeamManager->getTeamHasFlag(teamIndex); }


S32 Game::getTeamIndexFromTeamName(const char *teamName) const 
{ 
   for(S32 i = 0; i < mActiveTeamManager->getTeamCount(); i++)
      if(stricmp(teamName, getTeamName(i).getString()) == 0)
         return i;

   if(stricmp(teamName, "Hostile") == 0)
      return TEAM_HOSTILE;
   if(stricmp(teamName, "Neutral") == 0)
      return TEAM_NEUTRAL;

   return NO_TEAM;
}


// Makes sure that the mTeams[] structure has the proper player counts
// Needs to be called manually before accessing the structure
// Bot counts do work on client.  Yay!
// Rating may only work on server... not tested on client
void Game::countTeamPlayers() const
{
   for(S32 i = 0; i < getTeamCount(); i++)
   {
      TNLAssert(dynamic_cast<Team *>(getTeam(i)), "Invalid team");      // Assert for safety
      static_cast<Team *>(getTeam(i))->clearStats();                    // static_cast for speed
   }

   for(S32 i = 0; i < getClientCount(); i++)
   {
      ClientInfo *clientInfo = getClientInfo(i);

      S32 teamIndex = clientInfo->getTeamIndex();

      if(teamIndex >= 0 && teamIndex < getTeamCount())
      { 
         // Robot could be neutral or hostile, skip out-of-range team numbers
         TNLAssert(dynamic_cast<Team *>(getTeam(teamIndex)), "Invalid team");
         Team *team = static_cast<Team *>(getTeam(teamIndex));            

         if(clientInfo->isRobot())
            team->incrementBotCount();
         else
            team->incrementPlayerCount();

         // The following bit won't work on the client... 
         if(isServer())
         {
            const F32 BASE_RATING = .1f;
            team->addToRatingSum(max(clientInfo->getCalculatedRating(), BASE_RATING));    
         }
      }
   }
}


// Finds biggest team that has bots; if two teams are tied for largest, will return index of the first
S32 Game::findLargestTeamWithBots() const
{
   countTeamPlayers();

   S32 largestTeamCount = 0;
   S32 largestTeamIndex = NONE;

   for(S32 i = 0; i < getTeamCount(); i++)
   {
      TNLAssert(dynamic_cast<Team *>(getTeam(i)), "Invalid team");
      Team *team = static_cast<Team *>(getTeam(i));

      // Must have at least one bot to be the largest team with bots!
      if(team->getPlayerBotCount() > largestTeamCount && team->getBotCount() > 0)
      {
         largestTeamCount = team->getPlayerBotCount();
         largestTeamIndex = i;
      }
   }

   return largestTeamIndex;
}


void Game::setGameType(GameType *gameType)
{
   mGameType = gameType;
}


U32 Game::getTimeUnconnectedToMaster()
{
   return mTimeUnconnectedToMaster;
}


// Note: lots of stuff for this method in child classes!
void Game::onConnectedToMaster()
{
   getSettings()->saveMasterAddressListInIniUnlessItCameFromCmdLine();
}


// Called when ServerGame or the editor loads a level
void Game::resetLevelInfo()
{
   // These data need to be reset everytime before a level loads
   mLegacyGridSize = 1.f;
   mLevelFormat = CurrentLevelFormat;
   mHasLevelFormat = false;
   mLevelLoadTriggeredWarnings.clear();
}


// Each line of the file is handled separately by processLevelLoadLine in game.cpp or UIEditor.cpp

void Game::parseLevelLine(const char *line, GridDatabase *database, const string &levelFileName, S32 lineNum)
{
   Vector<string> args = parseString(string(line));
   U32 argc = args.size();
   S32 id = 0;
   const char **argv = new const char *[argc];

   if(argc >= 1)
   {
      std::size_t pos = args[0].find("!");
      if(pos != string::npos)
      {
         id = atoi(args[0].substr(pos + 1, args[0].size() - pos - 1).c_str());
         args[0] = args[0].substr(0, pos);
      }
   }

   for(U32 i = 0; i < argc; i++)
      argv[i] = args[i].c_str();

   try
   {
      processLevelLoadLine(argc, id, (const char **) argv, database, levelFileName, lineNum);
   }
   catch(LevelLoadException &e)
   {
      logprintf("Level Error: Can't parse %s: %s", line, e.what());  // TODO: fix "line" variable having hundreds of level lines
   }

   delete[] argv;
}


void Game::loadLevelFromString(const string &contents, GridDatabase *database, const string &filename)
{
   istringstream iss(contents);
   string line;
   S32 lineNum = 1;
   while(std::getline(iss, line))
   {
      parseLevelLine(line.c_str(), database, filename, lineNum);
      lineNum++;
   }
}


bool Game::loadLevelFromFile(const string &filename, GridDatabase *database)
{
   string contents = readFile(filename);
   if(contents == "")
      return false;

   loadLevelFromString(contents, database, filename);

#ifdef SAM_ONLY
   // In case the level crash the game trying to load, want to know which file is the problem. 
   logprintf("Loading %s", filename.c_str());
#endif

   return true;
}


// Process a single line of a level file, loaded in gameLoader.cpp
// argc is the number of parameters on the line, argv is the params themselves
// Used by ServerGame and the editor
void Game::processLevelLoadLine(U32 argc, S32 id, const char **argv, GridDatabase *database, const string &levelFileName, S32 lineNum)
{
   if(argc == 0 || !strcmp(argv[0], "#"))
   {
      return;
   }

   S32 strlenCmd = (S32) strlen(argv[0]);

   // This is a legacy from the old Zap! days... we do bots differently in Bitfighter, so we'll just ignore this line if we find it.
   if(!stricmp(argv[0], "BotsPerTeam"))
      return;

   // LevelFormat was introduced in 019 to handle significant file format changes, like
   // with GridSize removal and the saving of real spacial coordinates.
   //
   // This should be the first line of the file
   else if(!stricmp(argv[0], "LevelFormat"))
   {
      if(argc < 2)
         logprintf(LogConsumer::LogLevelError, "Invalid LevelFormat provided (line %d)", lineNum);
      else
         mLevelFormat = (U32)atoi(argv[1]);

      mHasLevelFormat = true;

      return;
   }

   // Legacy Gridsize handling - levels used to have a 'GridSize' line that could be used to
   // multiply all points found in the level file.  Since version 019 this is no longer used
   // and all points are saved as the real spacial coordinates.
   //
   // If a level file contains this setting, we will use it to multiply all points found in
   // the level file.  However, once it is loaded and resaved in the editor, this setting will
   // disappear and all points will reflect their true, absolute nature.
   else if(!stricmp(argv[0], "GridSize"))
   {
      // We should have properly detected the level format by the time GridSize is found
      if(mLevelFormat == 1)
      {
         if(argc < 2)
            logprintf(LogConsumer::LogLevelError, "Improperly formed GridSize parameter (line %d)", lineNum);
         else
            mLegacyGridSize = (F32)atof(argv[1]);
      }
      else
         logprintf(LogConsumer::LogLevelError, "GridSize can no longer be used in level files (line %d)", lineNum);

      return;
   }

   else if(!stricmp(argv[0], "LevelDatabaseId"))
   {
      U32 id = atoi(argv[1]);
      if(id == 0)
      {
         logprintf(LogConsumer::LogLevelError, "Invalid LevelDatabaseId specified (line %d)", lineNum);
      }
      else
      {
         setLevelDatabaseId(id);
      }
      return;
   }

   // Parse GameType line... All game types are of form XXXXGameType
   else if(strlenCmd >= 8 && !strcmp(argv[0] + strlenCmd - 8, "GameType"))
   {
      // First check to see if we have a LevelFormat line, which should have been detected
      // by now since it's the first line of the file.  If it didn't find it, we are at
      // version 1 and we have to set the old GridSize to 255 as default
      //
      // This check is performed here because every file should have a game type..  right??
      if(!mHasLevelFormat)
      {
         mLevelFormat = 1;
         mLegacyGridSize = 255.f;
      }

      if(mGameType.isValid())
      {
         logprintf(LogConsumer::LogLevelError, "Duplicate GameType is not allowed (line %d)", lineNum);
         return;
      }

      // validateGameType() will return a valid GameType string -- either what's passed in, or the default if something bogus was specified
      TNL::Object *theObject = TNL::Object::create(GameType::validateGameType(argv[0]));

      GameType *gt = dynamic_cast<GameType *>(theObject);
      if(gt)
      {
         bool validArgs = gt->processArguments(argc - 1, argv + 1, NULL);
         if(!validArgs)
            logprintf(LogConsumer::LogLevelError, "GameType has incorrect parameters (line %d)", lineNum);

         gt->addToGame(this, database);
      }
      else
         logprintf(LogConsumer::LogLevelError, "Could not create a GameType");
      
      return;
   }

   if(getGameType() && processLevelParam(argc, argv, lineNum)) 
   {
      // Do nothing here -- all the action is in the if statement
   }
   else if(getGameType() && processPseudoItem(argc, argv, levelFileName, database, id, lineNum))
   {
      // Do nothing here -- all the action is in the if statement
   }
   
   else
   {
      string objName;

      // Convert any NexusFlagItem into FlagItem, only NexusFlagItem will show up on ship
      if(!stricmp(argv[0], "HuntersFlagItem") || !stricmp(argv[0], "NexusFlagItem"))
         objName = "FlagItem";

      // Convert legacy Hunters* objects
      else if(stricmp(argv[0], "HuntersNexusObject") == 0 || stricmp(argv[0], "NexusObject") == 0)
         objName = "NexusZone";

      else
         objName = argv[0];


      if(!getGameType())   // Must have a GameType at this point, if not, we will add one to prevent problems loading a level with missing GameType
      {
         logprintf(LogConsumer::LogLevelError, "First line of level is missing GameType in level \"%s\"", levelFileName.c_str());
         GameType *gt = new GameType();
         gt->addToGame(this, database);
      }

      TNL::Object *theObject = TNL::Object::create(objName.c_str());    // Create an object of the type specified on the line

      SafePtr<BfObject> object  = dynamic_cast<BfObject *>(theObject);  // Force our new object to be a BfObject
      BfObject *eObject = dynamic_cast<BfObject *>(theObject);


      if(!object && !eObject)    // Well... that was a bad idea!
      {
         if(!mLevelLoadTriggeredWarnings.contains(objName))
         {
            logprintf(LogConsumer::LogLevelError, "Unknown object type \"%s\" in level \"%s\" (line %d)", objName.c_str(), levelFileName.c_str(), lineNum);
            mLevelLoadTriggeredWarnings.push_back(objName);
         }

         delete theObject;
         return;
      }

      // Object was valid
      bool validArgs = object->processArguments(argc - 1, argv + 1, this);

      // ProcessArguments() might delete this object (this happens with multi-dest teleporters), so isValid() could be false
      // even when the object is entirely legit
      if(validArgs && object.isValid())  
      {
         object->setUserAssignedId(id, false);
         object->addToGame(this, database);

         computeWorldObjectExtents();    // Make sure this is current if we process a robot that needs this for intro code

         // Mark the item as being a ghost (client copy of a server object) so that the object will not trigger server-side tests
         // The only time this code is run on the client is when loading into the editor.
         if(!isServer())
            object->markAsGhost();
      }
      else
      {
         if(!validArgs)
            logprintf(LogConsumer::LogLevelError, "Invalid arguments in object \"%s\" in level \"%s\" (line %d)", objName.c_str(), levelFileName.c_str(), lineNum);

         delete object.getPointer();
      }
   }
}


// Returns true if we've handled the line (even if it handling it means that the line was bogus); returns false if
// caller needs to create an object based on the line
bool Game::processLevelParam(S32 argc, const char **argv, S32 lineNum)
{
   if(!stricmp(argv[0], "Team"))
      onReadTeamParam(argc, argv, lineNum);

   // TODO: Create better way to change team details from level scripts: https://code.google.com/p/bitfighter/issues/detail?id=106
   else if(!stricmp(argv[0], "TeamChange"))   // For level script. Could be removed when there is a better way to change team names and colors.
      onReadTeamChangeParam(argc, argv);

   else if(!stricmp(argv[0], "Specials"))
      onReadSpecialsParam(argc, argv, lineNum);

   else if(!strcmp(argv[0], "Script"))
      onReadScriptParam(argc, argv);

   else if(!stricmp(argv[0], "LevelName"))
      onReadLevelNameParam(argc, argv);
   
   else if(!stricmp(argv[0], "LevelDescription"))
      onReadLevelDescriptionParam(argc, argv);

   else if(!stricmp(argv[0], "LevelCredits"))
      onReadLevelCreditsParam(argc, argv);

   else if(!stricmp(argv[0], "MinPlayers"))     // Recommend a min number of players for this map
   {
      if(argc > 1)
         getGameType()->setMinRecPlayers(atoi(argv[1]));
   }
   else if(!stricmp(argv[0], "MaxPlayers"))     // Recommend a max number of players for this map
   {
      if(argc > 1)
         getGameType()->setMaxRecPlayers(atoi(argv[1]));
   }
   else
      return false;     // Line not processed; perhaps the caller can handle it?

   return true;         // Line processed; caller can ignore it
}


// Write out the game processed above; returns multiline string
string Game::toLevelCode() const
{
   string str;

   GameType *gameType = getGameType();

   str = "LevelFormat " + itos(CurrentLevelFormat) + "\n";

   str += string(gameType->toLevelCode() + "\n");

   str += string("LevelName ")        + writeLevelString(gameType->getLevelName().c_str()) + "\n";
   str += string("LevelDescription ") + writeLevelString(gameType->getLevelDescription()) + "\n";
   str += string("LevelCredits ")     + writeLevelString(gameType->getLevelCredits()->getString()) + "\n";

   if(getLevelDatabaseId())
      str += string("LevelDatabaseId ") + itos(getLevelDatabaseId()) + "\n";

   for(S32 i = 0; i < mActiveTeamManager->getTeamCount(); i++)
      str += mActiveTeamManager->getTeam(i)->toLevelCode() + "\n";

   str += gameType->getSpecialsLine() + "\n";

   if(gameType->getScriptName() != "")
      str += "Script " + gameType->getScriptLine() + "\n";

   str += string("MinPlayers") + (gameType->getMinRecPlayers() > 0 ? " " + itos(gameType->getMinRecPlayers()) : "") + "\n";
   str += string("MaxPlayers") + (gameType->getMaxRecPlayers() > 0 ? " " + itos(gameType->getMaxRecPlayers()) : "") + "\n";

   return str;
}


// Only occurs in scripts; could be in editor or on server
void Game::onReadTeamChangeParam(S32 argc, const char **argv)
{
   if(argc >= 2)   // Enough arguments?
   {
      S32 teamNumber = atoi(argv[1]);   // Team number to change

      if(teamNumber >= 0 && teamNumber < getTeamCount())
      {
         AbstractTeam *team = getNewTeam();
         team->processArguments(argc - 1, argv + 1);          // skip one arg
         replaceTeam(team, teamNumber);
      }
   }
}


void Game::onReadSpecialsParam(S32 argc, const char **argv, S32 lineNum)
{         
   for(S32 i = 1; i < argc; i++)
      if(!getGameType()->processSpecialsParam(argv[i]))
         logprintf(LogConsumer::LogLevelError, "Invalid specials parameter: %s (line %d)", argv[i], lineNum);
}


void Game::onReadScriptParam(S32 argc, const char **argv)
{
   Vector<string> args;

   // argv[0] is always "Script"
   for(S32 i = 1; i < argc; i++)
      args.push_back(argv[i]);

   getGameType()->setScript(args);
}


static string getString(S32 argc, const char **argv)
{
   string s;
   for(S32 i = 1; i < argc; i++)
   {
      s += argv[i];
      if(i < argc - 1)
         s += " ";
   }

   return s;
}


void Game::onReadLevelNameParam(S32 argc, const char **argv)
{
   string s = trim(getString(argc, argv));

   getGameType()->setLevelName(s.substr(0, MAX_GAME_NAME_LEN).c_str());
}


void Game::onReadLevelDescriptionParam(S32 argc, const char **argv)
{
   string s = getString(argc, argv);
   getGameType()->setLevelDescription(s.substr(0, MAX_GAME_DESCR_LEN));
}


void Game::onReadLevelCreditsParam(S32 argc, const char **argv)
{
   string s = getString(argc, argv);
   getGameType()->setLevelCredits(s.substr(0, MAX_GAME_DESCR_LEN).c_str());
}


// Only used during level load process...  actually, used at all?  If so, should be combined with similar code in gameType
// Not used during normal game load... used by tests and lua_setGameTime()
void Game::setGameTime(F32 timeInMinutes)
{
   GameType *gt = getGameType();

   TNLAssert(gt, "Null gametype!");

   if(gt)
      gt->setGameTime(timeInMinutes * 60 * 1000);  // Time in ms
}


// If there is no valid connection to master server, perodically try to create one.
// If user is playing a game they're hosting, they should get one master connection
// for the client and one for the server.
// Called from both clientGame and serverGame idle fuctions, so think of this as a kind of idle
void Game::checkConnectionToMaster(U32 timeDelta)
{
   if(mConnectionToMaster.isValid() && mConnectionToMaster->isEstablished())
      mTimeUnconnectedToMaster = 0;
   else if(mReadyToConnectToMaster)
      mTimeUnconnectedToMaster += timeDelta;

   if(!mConnectionToMaster.isValid())      // It's valid if it isn't null, so could be disconnected and would still be valid
   {
      Vector<string> *masterServerList = mSettings->getMasterServerList();

      if(masterServerList->size() == 0)
         return;

      if(mNextMasterTryTime < timeDelta && mReadyToConnectToMaster)
      {
         if(!mNameToAddressThread)
         {
            if(mHaveTriedToConnectToMaster && masterServerList->size() >= 2)
            {  
               // Rotate the list so as to try each one until we find one that works...
               masterServerList->push_back(string(masterServerList->get(0)));  // don't remove string(...), or else this line is a mystery why push_back an empty string.
               masterServerList->erase(0);
            }

            const char *addr = masterServerList->get(0).c_str();

            mHaveTriedToConnectToMaster = true;
            logprintf(LogConsumer::LogConnection, "%s connecting to master [%s]", isServer() ? "Server" : "Client", addr);

            mNameToAddressThread = new NameToAddressThread(addr);
            mNameToAddressThread->start();
         }
         else
         {
            if(mNameToAddressThread->mDone)
            {
               if(mNameToAddressThread->mAddress.isValid())
               {
                  TNLAssert(!mConnectionToMaster.isValid(), "Already have connection to master!");
                  mConnectionToMaster = new MasterServerConnection(this);

                  mConnectionToMaster->connect(mNetInterface, mNameToAddressThread->mAddress);
               }
   
               mNextMasterTryTime = GameConnection::MASTER_SERVER_FAILURE_RETRY_TIME;     // 10 secs, just in case this attempt fails
               delete mNameToAddressThread;
               mNameToAddressThread = NULL;
            }
         }
      }
      else if(!mReadyToConnectToMaster)
         mNextMasterTryTime = 0;
      else
         mNextMasterTryTime -= timeDelta;
   }

   processAnonymousMasterConnection();
}


void Game::processAnonymousMasterConnection()
{
   // Connection doesn't exist yet
   if(!mAnonymousMasterServerConnection.isValid())
      return;

   // Connection has already been initiated
   if(mAnonymousMasterServerConnection->isInitiator())
      return;

   // Try to open a socket to master server
   if(!mNameToAddressThread)
   {
      Vector<string> *masterServerList = mSettings->getMasterServerList();

      // No master server addresses?
      if(masterServerList->size() == 0)
         return;

      const char *addr = masterServerList->get(0).c_str();

      mNameToAddressThread = new NameToAddressThread(addr);
      mNameToAddressThread->start();
   }
   else
   {
      if(mNameToAddressThread->mDone)
      {
         if(mNameToAddressThread->mAddress.isValid())
            mAnonymousMasterServerConnection->connect(mNetInterface, mNameToAddressThread->mAddress);

         delete mNameToAddressThread;
         mNameToAddressThread = NULL;
      }
   }
}


// Called by both ClientGame::idle and ServerGame::idle
void Game::idle(U32 timeDelta)
{
   mSecondaryThread->idle();
}


Game::DeleteRef::DeleteRef(BfObject *o, U32 d)
{
   theObject = o;
   delay = d;
}


void Game::addToDeleteList(BfObject *theObject, U32 delay)
{
   TNLAssert(!theObject->isGhost(), "Can't delete ghosting Object");
   mPendingDeleteObjects.push_back(DeleteRef(theObject, delay));
}


// Cycle through our pending delete list, and either delete an object or update its timer
void Game::processDeleteList(U32 timeDelta)
{
   for(S32 i = 0; i < mPendingDeleteObjects.size(); i++) 
      if(timeDelta > mPendingDeleteObjects[i].delay)
      {
         BfObject *g = mPendingDeleteObjects[i].theObject;
         delete g;
         mPendingDeleteObjects.erase_fast(i);
         i--;
      }
      else
         mPendingDeleteObjects[i].delay -= timeDelta;
}


// Delete all objects of specified type  --> currently only used to remove all walls from the game and in tests
void Game::deleteObjects(U8 typeNumber)
{
   fillVector.clear();
   mGameObjDatabase->findObjects(typeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(fillVector[i]);
      obj->deleteObject(0);
   }
}


// Not currently used
void Game::deleteObjects(TestFunc testFunc)
{
   fillVector.clear();
   mGameObjDatabase->findObjects(testFunc, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(fillVector[i]);
      obj->deleteObject(0);
   }
}


void Game::computeWorldObjectExtents()
{
   mWorldExtents = mGameObjDatabase->getExtents();
}


Rect Game::computeBarrierExtents()
{
   Rect extents;

   fillVector.clear();
   mGameObjDatabase->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
      extents.unionRect(fillVector[i]->getExtent());

   return extents;
}


Point Game::computePlayerVisArea(Ship *ship) const
{
   F32 fraction = ship->getSensorZoomFraction();

   static const Point regVis(PLAYER_VISUAL_DISTANCE_HORIZONTAL, PLAYER_VISUAL_DISTANCE_VERTICAL);
   static const Point sensVis(PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_HORIZONTAL, PLAYER_SENSOR_PASSIVE_VISUAL_DISTANCE_VERTICAL);

   if(ship->hasModule(ModuleSensor))
      return regVis + (sensVis - regVis) * fraction;
   else
      return sensVis + (regVis - sensVis) * fraction;
}


extern void exitToOs(S32 errcode);

// Make sure name is unique.  If it's not, make it so.  The problem is that then the client doesn't know their official name.
// This makes the assumption that we'll find a unique name before testing U32_MAX combinations.
string Game::makeUnique(const char *name)
{
   if(name[0] == 0)  // No zero-length name allowed
      name = "ChumpChange";

   U32 index = 0;
   string proposedName = name;

   bool unique = false;

   while(!unique)
   {
      unique = true;

      for(S32 i = 0; i < getClientCount(); i++)
      {
         if(proposedName == getClientInfo(i)->getName().getString())     // Collision detected!
         {
            unique = false;

            char numstr[U32_MAX_DIGITS + 2];    // + 1 for the ., +1 for the \0

            dSprintf(numstr, ARRAYSIZE(numstr), ".%d", index);

            // Max length name can be such that when number is appended, it's still less than MAX_PLAYER_NAME_LENGTH
            S32 maxNamePos = MAX_PLAYER_NAME_LENGTH - (S32)strlen(numstr); 
            proposedName = string(name).substr(0, maxNamePos) + numstr;     // Make sure name won't grow too long

            index++;

            if(index == U32_MAX)
            {
               logprintf(LogConsumer::LogError, "Too many players using the same name!  Aaaargh!");
               exitToOs(1);
            }
            break;
         }
      }
   }

   return proposedName;
}


// Called when ClientGame and ServerGame are destructed, and new levels are loaded on the server
void Game::cleanUp()
{
   mGameObjDatabase->removeEverythingFromDatabase();
   mActiveTeamManager->clearTeams();      // Will in effect delete any teams herein

   // Delete any objects on the delete list
   processDeleteList(U32_MAX);

   if(mGameType.isValid() && !mGameType->isGhost())
      delete mGameType.getPointer();
}


const Rect *Game::getWorldExtents() const
{
   return &mWorldExtents;
}


const Color *Game::getTeamColor(S32 teamId) const
{
   return mActiveTeamManager->getTeamColor(teamId);
}


const Color *Game::getTeamHealthBarColor(S32 teamId) const
{
   return mActiveTeamManager->getTeamHealthBarColor(teamId);
}


void Game::setPreviousLevelName(const string &name)
{
   // Do nothing (but will be overidded in ClientGame)
}


void Game::onReadTeamParam(S32 argc, const char **argv, S32 lineNum)
{
   if(getTeamCount() < MAX_TEAMS)     // Too many teams?
   {
      AbstractTeam *team = getNewTeam();
      if(team->processArguments(argc, argv))
         addTeam(team);
   }
   else
      logprintf(LogConsumer::LogLevelError, "Cannot have more than %d teams: ignoring team (line %d)", MAX_TEAMS, lineNum);
}


void Game::setActiveTeamManager(TeamManager *teamManager)
{
   mActiveTeamManager = teamManager;
}


// Overridden on client
void Game::setLevelDatabaseId(U32 id)
{
   mLevelDatabaseId = id;
}


U32 Game::getLevelDatabaseId() const
{
   return mLevelDatabaseId;
}


// Server only
void Game::onFlagMounted(S32 teamIndex)
{
   if(mGameType)
      mGameType->onFlagMounted(teamIndex);
}


// Server only
void Game::itemDropped(Ship *ship, MoveItem *item, DismountMode dismountMode)
{
   if(mGameType)
      mGameType->itemDropped(ship, item, dismountMode);
}


const Color *Game::getObjTeamColor(const BfObject *obj) const
{ 
   return mGameType->getTeamColor(obj);
}


bool Game::objectCanDamageObject(BfObject *damager, BfObject *victim) const
{
   return mGameType ? mGameType->objectCanDamageObject(damager, victim) : false;
}


void Game::releaseFlag(const Point &pos, const Point &vel, const S32 count) const
{
   if(mGameType)
      mGameType->releaseFlag(pos, vel, count);
}


S32 Game::getRenderTime() const
{
   return mGameType->getRemainingGameTimeInMs() + mGameType->getRenderingOffset();
}


Vector<AbstractSpawn *> Game::getSpawnPoints(TypeNumber typeNumber, S32 teamIndex)
{
   return mGameType->getSpawnPoints(typeNumber, teamIndex);
}


void Game::addFlag(FlagItem *flag)
{
   if(mGameType)
      mGameType->addFlag(flag);
}


void Game::shipTouchFlag(Ship *ship, FlagItem *flag)
{
   if(mGameType)
      mGameType->shipTouchFlag(ship, flag);
}


void Game::shipTouchZone(Ship *ship, GoalZone *zone)
{
   if(mGameType)
      mGameType->shipTouchZone(ship, zone);
}


bool Game::isTeamGame() const
{
   return mGameType->isTeamGame();
}


Timer &Game::getGlowZoneTimer()
{
   return mGameType->mZoneGlowTimer;
}


S32 Game::getGlowingZoneTeam()
{
   return mGameType->mGlowingZoneTeam;
}


string Game::getScriptName() const
{
   return mGameType->getScriptName();
}


bool Game::levelHasLoadoutZone()
{
   return mGameType && mGameType->levelHasLoadoutZone();
}


void Game::updateShipLoadout(BfObject *shipObject)
{
   return mGameType->updateShipLoadout(shipObject);
}


void Game::sendChat(const StringTableEntry &senderName, ClientInfo *senderClientInfo, const StringPtr &message, bool global, S32 teamIndex)
{
   if(mGameType)
      mGameType->sendChat(senderName, senderClientInfo, message, global, teamIndex);
}


void Game::sendPrivateChat(const StringTableEntry &senderName, const StringTableEntry &receiverName, const StringPtr &message)
{
   if(mGameType)
      mGameType->sendPrivateChat(senderName, receiverName, message);
}


void Game::sendAnnouncementFromController(const string &message)
{
   if(mGameType)
      mGameType->displayAnnouncement(message);
}


void Game::updateClientChangedName(ClientInfo *clientInfo, StringTableEntry newName)
{
   if(mGameType)
      mGameType->updateClientChangedName(clientInfo, newName);
}


// Static method - only used for "illegal" activities
const GridDatabase *Game::getServerGameObjectDatabase()
{
   return GameManager::getServerGame()->getGameObjDatabase();
}


// This is not a very good way of seeding the prng, but it should generate unique, if not cryptographicly secure, streams.
// We'll get 4 bytes from the time, up to 12 bytes from the name, and any left over slots will be filled with unitialized junk.
// Static method
void Game::seedRandomNumberGenerator(const string &name)
{
   U32 time = Platform::getRealMilliseconds();
   const S32 timeByteCount = 4;
   const S32 totalByteCount = 16;

   S32 nameBytes = min((S32)name.length(), totalByteCount - timeByteCount);     // # of bytes we get from the provided name

   unsigned char buf[totalByteCount] = {0};

   // Bytes from the time
   buf[0] = U8(time);
   buf[1] = U8(time >> 8);
   buf[2] = U8(time >> 16);
   buf[3] = U8(time >> 24);

   // Bytes from the name
   for(S32 i = 0; i < nameBytes; i++)
      buf[i + timeByteCount] = name.at(i);

   Random::addEntropy(buf, totalByteCount);     // May be some uninitialized bytes at the end of the buffer, but that's ok
}


bool Game::objectCanDamageObject(BfObject *damager, BfObject *victim)
{
   if(!getGameType())
      return true;
   else
      return getGameType()->objectCanDamageObject(damager, victim);
}


U32 Game::getMaxPlayers() const 
{
   TNLAssert(false, "Not implemented for this class!");
   return 0;
}

   
void Game::gotPingResponse(const Address &address, const Nonce &nonce, U32 clientIdentityToken, S32 clientId)
{
   TNLAssert(false, "Not implemented for this class!");
}


void Game::gotQueryResponse(const Address &address, S32 serverId, 
                            const Nonce &nonce, const char *serverName, const char *serverDescr, 
                            U32 playerCount, U32 maxPlayers, U32 botCount, bool dedicated, bool test, bool passwordRequired)
{
   TNLAssert(false, "Not implemented for this class!");
}


void Game::displayMessage(const Color &msgColor, const char *format, ...) const
{
   TNLAssert(false, "Not implemented for this class!");
}


ClientInfo *Game::getLocalRemoteClientInfo() const
{
   TNLAssert(false, "Not implemented for this class!");
   return NULL;
}


void Game::quitEngineerHelper()
{
   TNLAssert(false, "Not implemented for this class!");
}


bool Game::isDedicated() const  
{
   return false;
}


Point Game::worldToScreenPoint(const Point *p, S32 canvasWidth, S32 canvasHeight) const
{
   TNLAssert(false, "Not implemented for this class!");
   return Point(0,0);
}


F32 Game::getCommanderZoomFraction() const
{
   TNLAssert(false, "Not implemented for this class!");
   return 0;
}


void Game::renderBasicInterfaceOverlay() const
{
   TNLAssert(false, "Not implemented for this class!");
}


void Game::emitTextEffect(const string &text, const Color &color, const Point &pos) const
{
   TNLAssert(false, "Not implemented for this class!");
}


string Game::getPlayerName() const
{
   TNLAssert(false, "Not implemented for this class!");
   return "";
}


void Game::addInlineHelpItem(HelpItem item) const
{
   TNLAssert(false, "Not implemented for this class!");
}


void Game::removeInlineHelpItem(HelpItem item, bool markAsSeen) const
{
   TNLAssert(false, "Not implemented for this class!");
}


F32 Game::getObjectiveArrowHighlightAlpha() const
{
   TNLAssert(false, "Not implemented for this class!");
   return 0;
}


// In seconds
S32 Game::getRemainingGameTime() const
{
   if(mGameType)     // Can be NULL at the end of a game
      return mGameType->getRemainingGameTime();
   else
      return 0;
}


Master::DatabaseAccessThread *Game::getSecondaryThread()
{
   return  mSecondaryThread;
}


};


