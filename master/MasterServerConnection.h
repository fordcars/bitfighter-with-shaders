//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _MASTER_SERVER_CONNECTION_H_
#define _MASTER_SERVER_CONNECTION_H_


#include "GameConnectRequest.h"
#include "masterInterface.h"

#include "../zap/ChatCheck.h"
#include "../zap/Intervals.h"

#include "tnlVector.h"
#include "tnlNetStringTable.h"

#include <map>
#include <string>


namespace Master 
{


class MasterServerConnection;
class MasterServer;

using namespace std;

struct ThreadingStruct
{
   bool isValid;
   bool isBusy;     // For multithreading
   U32 lastClock;   // Data can get old
   Vector<SafePtr<MasterServerConnection> > waitingClients;

public:
   ThreadingStruct();
   virtual ~ThreadingStruct();

   void resetClock();
   bool isExpired();
   void addClientToWaitingList(MasterServerConnection *connection);

   virtual U32 getCacheExpiryTime() = 0;
};


struct HighScores : public ThreadingStruct
{
   Vector<StringTableEntry> groupNames;
   Vector<string> names;
   Vector<string> scores;
   S32 scoresPerGroup;

   U32 getCacheExpiryTime();
};


struct LevelRating : public ThreadingStruct
{
private:
   S16 mRating;

public:
   U32 databaseId;
   bool receivedUpdateByClientWhileBusy;  // Flag signaling that something changed while this thread was working

   virtual U32 getCacheExpiryTime() = 0;

   // Constructor:
   LevelRating();

   S16 getRating();
   void setRating(S16 rating);
   void setRatingMagicValue(S16 rating);
};


struct TotalLevelRating : public LevelRating
{
   U32 getCacheExpiryTime();
};


struct PlayerLevelRating : public LevelRating
{
   StringTableEntry playerName;

   U32 getCacheExpiryTime();
};


class MasterServerConnection : public MasterServerInterface, public Zap::ChatCheck
{
private:
   typedef MasterServerInterface Parent;

   string mLoggingStatus;

public:
   static HighScores highScores;    // Cached high scores
   static map<U32, TotalLevelRating> totalLevelRatings;

private:
   Int<BADGE_COUNT> mBadges;
   Int<BADGE_COUNT> getBadges();

   U16 mGamesPlayed;
   U16 getGamesPlayed();

   void sendMotd();

   MasterConnectionType mConnectionType;
   static MasterServer *mMaster;

   void sendM2cQueryServersResponse(U32 queryId, const Vector<IPAddress> &addresses, const Vector<S32> &serverIdList);


public:

   ///
public:
   static Vector<GameConnectRequest *> gConnectList;


   /// @name Connection Info
   ///
   /// General information about this connection.
   ///
   /// @{

   ///
   U32              mStrikeCount;      ///< Number of "strikes" this connection has... 3 strikes and you're out!
   U32              mLastQueryId;      ///< The last query id for info from this master.
   U32              mLastActivityTime; ///< The last time we got a request or an update from this host.

   /// A list of connection requests we're working on fulfilling for this connection.
   Vector< GameConnectRequest* > mConnectList;


   U32              mCMProtocolVersion; // Version of the protocol we'll be using to converse with the client
   U32              mCSProtocolVersion; // Protocol version client will use to talk to server (client can only play with others 
                                        // using this same version)
   U32              mClientBuild;       // Build number of the client (different builds may use same protocols)

   U32              mInfoFlags;         // Info flags describing this server.
   U32              mPlayerCount;       // Current number of players on this server.
   U32              mMaxPlayers;        // Maximum number of players on this server.
   U32              mNumBots;           // Current number of bots on this server.

   StringTableEntry mLevelName;        ///<<=== TODO: Change to const char *
   StringTableEntry mLevelType;
   StringTableEntry mPlayerOrServerName;        // Player's nickname, hopefully unique, but not enforced, or server's name
   Nonce mPlayerId;                             // (Hopefully) unique ID of this player

   S32 mClientId;                               // Guranteed unique ID assigned my master

   bool mAuthenticated;                         // True if user was authenticated, false if not
   bool mIsDebugClient;                         // True if client is running from a debug build

   StringTableEntry mServerDescr;               // Server description
   bool isInGlobalChat;

   bool mIsMasterAdmin;

   bool mIsIgnoredFromList;

   StringTableEntry mAutoDetectStr;             // Player's joystick autodetect string, for research purposes

public:
   static Vector<SafePtr<MasterServerConnection> > gLeaveChatTimerList;
   U32 mLeaveGlobalChatTimer;
   bool mChatTooFast;

   /// Constructor initializes the linked list info with "safe" values
   /// so we don't explode if we destruct right away.
   MasterServerConnection();

   /// Destructor removes the connection from the doubly linked list of server connections
   ~MasterServerConnection();


   enum PHPBB3AuthenticationStatus
   {
      Authenticated,
      CantConnect,
      UnknownUser,
      WrongPassword,
      InvalidUsername,
      Unsupported,
      UnknownStatus
   };

   // Check username & password against database
   static PHPBB3AuthenticationStatus verifyCredentials(string &username, string password);

   PHPBB3AuthenticationStatus checkAuthentication(const char *password, bool doNotDelay = false);
   void processAutentication(StringTableEntry newName, PHPBB3AuthenticationStatus status, TNL::Int<32> badges,
                             U16 gamesPlayed);

   // Client has contacted us and requested a list of active servers
   // that match their criteria.
   //
   // The query server method builds a piecewise list of servers
   // that match the client's particular filter criteria and
   // sends it to the client, followed by a QueryServersDone RPC.
   TNL_DECLARE_RPC_OVERRIDE(c2mQueryServers, (U32 queryId));
   TNL_DECLARE_RPC_OVERRIDE(c2mQueryHostServers, (U32 queryId));
   void c2mQueryServersOption(U32 queryId, bool hostonly);

   /// checkActivityTime validates that this particular connection is
   /// not issuing too many requests at once in an attempt to DOS
   /// by flooding either the master server or any other server
   /// connected to it.  A client whose last activity time falls
   /// within the specified delta gets a strike... 3 strikes and
   /// you're out!  Strikes go away after being good for a while.
   bool checkActivityTime(U32 timeDeltaMinimum);

   void removeConnectRequest(GameConnectRequest *gcr);

   GameConnectRequest *findAndRemoveRequest(U32 requestId);

   static void setMasterServer(MasterServer *master);

   S32 getClientId() const;

   MasterServerConnection *findClient(Nonce &clientId);   // Should be const, but that won't compile for reasons not yet determined!!


   // Write a current count of clients/servers for display on a website, using JSON format
   // This gets updated whenver we gain or lose a server, at most every 5 seconds (currently)
   static void writeClientServerList_JSON();

   bool isAuthenticated();

   // This is called when a client wishes to arrange a connection with a server
   TNL_DECLARE_RPC_OVERRIDE(c2mRequestArrangedConnection, (U32 requestId, IPAddress remoteAddress, IPAddress internalAddress,
      ByteBufferPtr connectionParameters));

   /// s2mAcceptArrangedConnection is sent by a server to notify the master that it will accept the connection
   /// request from a client.  The requestId parameter sent by the MasterServer in m2sClientRequestedArrangedConnection
   /// should be sent back as the requestId field.  The internalAddress is the server's self-determined IP address.

   // Called to indicate a connect request is being accepted.
   TNL_DECLARE_RPC_OVERRIDE(s2mAcceptArrangedConnection, (U32 requestId, IPAddress internalAddress, ByteBufferPtr connectionData));


   // s2mRejectArrangedConnection notifies the Master Server that the server is rejecting the arranged connection
   // request specified by the requestId.  The rejectData will be passed along to the requesting client.
   // Called to indicate a connect request is being rejected.
   TNL_DECLARE_RPC_OVERRIDE(s2mRejectArrangedConnection, (U32 requestId, ByteBufferPtr rejectData));

   // s2mUpdateServerStatus updates the status of a server to the Master Server, specifying the current game
   // and mission types, any player counts and the current info flags.
   // Updates the master with the current status of a game server.
   TNL_DECLARE_RPC_OVERRIDE(s2mUpdateServerStatus, (StringTableEntry levelName, StringTableEntry levelType,
      U32 botCount, U32 playerCount, U32 maxPlayers, U32 infoFlags));

   void processIsAuthenticated(Zap::GameStats *gameStats);
   void writeStatisticsToDb(Zap::VersionedGameStats &stats);
   void writeAchievementToDb(U8 achievementId, const StringTableEntry &playerNick);
   void writeLevelInfoToDb(const string &hash, const string &levelName, const string &creator,
                           const StringTableEntry &gameType, bool hasLevelGen, U8 teamCount, S32 winningScore, S32 gameDurationInSeconds);


   HighScores   *getHighScores(S32 scoresPerGroup);
   TotalLevelRating *getLevelRating(U32 databaseId);
   PlayerLevelRating *getLevelRating(U32 databaseId, const StringTableEntry &mPlayerOrServerName);

   static void removeOldEntriesFromRatingsCache();          // Keep our caches from growing too large


   void sendPlayerLevelRating(U32 databaseId, S32 rating);  // Helper that wraps m2cSendPlayerLevelRating

   TNL_DECLARE_RPC_OVERRIDE(s2mSendStatistics, (Zap::VersionedGameStats stats));
   TNL_DECLARE_RPC_OVERRIDE(s2mAcheivementAchieved, (U8 achievementId, StringTableEntry playerNick));
   TNL_DECLARE_RPC_OVERRIDE(s2mSendLevelInfo, (string hash, string levelName, string creator,
      StringTableEntry gametype, bool hasLevelGen, U8 teamCount, S32 winningScore, S32 gameDurationInSeconds));

   // Send message-of-the-day
   TNL_DECLARE_RPC_OVERRIDE(c2mRequestMOTD, ());

   // Send high scores stats to client
   TNL_DECLARE_RPC_OVERRIDE(c2mRequestHighScores, ());

   // Send level rating to client
   TNL_DECLARE_RPC_OVERRIDE(c2mRequestLevelRating, (U32 databaseId));
   TNL_DECLARE_RPC_OVERRIDE(c2mSetLevelRating, (U32 databaseId, RangedU32<0, 2> rating));


   // Game server wants to know if user name has been verified
   TNL_DECLARE_RPC_OVERRIDE(s2mRequestAuthentication, (Vector<U8> id, StringTableEntry name));

   static string cleanName(string name);
   bool readConnectRequest(BitStream *stream, NetConnection::TerminationReason &reason);
   void writeConnectAccept(BitStream *stream);
   void onConnectionEstablished();


   TNL_DECLARE_RPC_OVERRIDE(c2mJoinGlobalChat, ());
   TNL_DECLARE_RPC_OVERRIDE(c2mLeaveGlobalChat, ());

   TNL_DECLARE_RPC_OVERRIDE(c2mSendChat, (StringPtr message));

   // Got out-of-game chat message from client, need to relay it to others
   TNL_DECLARE_RPC_OVERRIDE(s2mChangeName, (StringTableEntry name));
   TNL_DECLARE_RPC_OVERRIDE(s2mServerDescription, (StringTableEntry descr));

   TNL_DECLARE_NETCONNECTION(MasterServerConnection);
};


}

#endif
