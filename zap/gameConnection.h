//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GAME_CONNECTION_H_
#define _GAME_CONNECTION_H_


#include "ChatCheck.h"                 // Parent class
#include "controlObjectConnection.h"   // Parent class
#include "dataConnection.h"            // Parent class DataSendable

#include "SharedConstants.h"           // For BADGE_COUNT constant
#include "GameTypesEnum.h"
#include "SoundSystemEnums.h"          // For NumSFXBuffers

#include "ship.h"                      // For Ship::EnergyMax
#include "ClientInfo.h"
#include "Engineerable.h"
#include "Timer.h"

#include "tnlNetConnection.h"

#include <time.h>


using namespace TNL;
using namespace std;

namespace Zap
{

class ClientGame;
class ServerGame;
struct LevelInfo;
class LuaPlayerInfo;
class GameSettings;
class LevelSource;

class GameConnection: public ControlObjectConnection, public ChatCheck
{
private:
   typedef ControlObjectConnection Parent;

   void initialize();

   time_t joinTime;
   bool mAcheivedConnection;

   // For saving passwords
   string mLastEnteredPassword;

   RefPtr<ClientInfo> mClientInfo;               // This could be either a FullClientInfo or a RemoteClientInfo
   LevelSource *mLevelSource;
   S32 mLevelUploadIndex;

protected:
#ifndef ZAP_DEDICATED
   ClientGame *mClientGame;         // NULL on server side, not available for dedicated build
#endif
   ServerGame *mServerGame;         // NULL on client side

private:
   bool mInCommanderMap;
   bool mWaitingForPermissionsReply;
   bool mGotPermissionsReply;

   bool mWantsScoreboardUpdates;    // Indicates if client has requested scoreboard streaming (e.g. pressing Tab key)
   bool mReadyForRegularGhosts;

   StringTableEntry mClientNameNonUnique; // For authentication, not unique name

   Timer mAuthenticationTimer;
   S32 mAuthenticationCounter;

   void displayMessage(U32 colorIndex, U32 sfxEnum, const char *message);    // Helper function
   void displayWelcomeMessage();


   StringTableEntry mServerName;
   GameSettings *mSettings;

   bool userAlreadyHasPermissions(const string &ownerPW, const string &adminPW, const string &levChangePW, const char *pass);

   void markCurrentLevelAsDeleted();
   string undeleteMostRecentlyDeletedLevel();               // Undoes above function

   void updateTimers_client(U32 timeDelta);
   void updateTimers_server(U32 timeDelta);

   S32 mUploadIndex;

public:
   bool mPackUnpackShipEnergyMeter; // Only true for game recorder
   U16 switchedTeamCount;

   U8 mVote;                     // 0 = not voted,  1 = vote yes,  2 = vote no    TODO: Make 
   U32 mVoteTime;

   U32 mWrongPasswordCount;
   static const U32 MAX_WRONG_PASSWORD = 20;  // too many wrong password, and client get disconnect

   Vector<LevelInfo> mLevelInfos;

   static const S32 MASTER_SERVER_FAILURE_RETRY_TIME = 10000;   // 10 secs
   static const U32 SPAWN_DELAY_TIME = 20000;                   // 20 seconds until eligible for being spawn delayed

   static const char *getConnectionStateString(S32 i);

   enum MessageColors
   {
      ColorWhite,
      ColorRed,
      ColorGreen,
      ColorBlue,
      ColorAqua,
      ColorYellow,
      ColorNuclearGreen,
      ColorCount,                         // Must be last, except aliases
      ColorSuccess = ColorNuclearGreen,   // Aliases for readability
      ColorInfo = ColorAqua               // Info message
   };

   enum ParamType             // Be careful changing the order of this list... c2sSetParam() expects this for message creation
   {
      LevelChangePassword = 0,
      AdminPassword,
      OwnerPassword,
      ServerPassword,
      ServerName,
      ServerDescription,
      ServerWelcomeMessage,
      LevelDir, 
      // PlaylistFile,     // TODO for 020 uncomment this and handle it!
      DeleteLevel,  
      UndeleteLevel,
      GlobalLevelScript,

      ParamTypeCount       // Must be last ==> Cannot change this value without breaking compatibility!
   };


#ifndef ZAP_DEDICATED
   explicit GameConnection(ClientGame *game, bool isLocalConnection);   // Constructor for ClientGame
#endif
   GameConnection();                                                    // Constructor for ServerGame
   virtual ~GameConnection();             // Destructor


#ifndef ZAP_DEDICATED
   ClientGame *getClientGame();
   void setClientGame(ClientGame *game);
#endif
   ServerGame *getServerGame();

   Timer mSwitchTimer;           // Timer controlling when player can switch teams after an initial switch

   void setClientNameNonUnique(StringTableEntry name);
   void setServerName(StringTableEntry name);

   ClientInfo *getClientInfo();
   void setClientInfo(ClientInfo *clientInfo);

   void onLocalConnection();

   virtual bool lostContact();

   string getServerName();

   void resetConnectionStatus();    // Clears/initializes some things between levels

   void submitPassword(const char *password);

   void unsuspendGame();

   void sendLevelList();

   bool isReadyForRegularGhosts();
   void setReadyForRegularGhosts(bool ready);

   bool wantsScoreboardUpdates();
   void setWantsScoreboardUpdates(bool wantsUpdates);

   virtual void onStartGhosting();  // Gets run when game starts
   virtual void onEndGhosting();    // Gets run when game is over


   // Tell UI we're waiting for password confirmation from server
   void setWaitingForPermissionsReply(bool waiting);
   bool waitingForPermissionsReply();

   // Tell UI whether we've received password confirmation from server
   void setGotPermissionsReply(bool gotReply);
   bool gotPermissionsReply();

   // Suspend/unsuspend game, s2c and c2s
   TNL_DECLARE_RPC(s2rSetSuspendGame, (bool isSuspend));

   void undelaySpawn();

   // Delay/undelay spawn
   TNL_DECLARE_RPC(s2cPlayerSpawnDelayed, (U8 waitTimeInOneTenthsSeconds));
   TNL_DECLARE_RPC(s2cPlayerSpawnUndelayed, ());
   TNL_DECLARE_RPC(c2sPlayerSpawnUndelayed, ());
   TNL_DECLARE_RPC(c2sPlayerRequestSpawnDelayed, (bool incursPenalty));


   // Player using engineer module
   TNL_DECLARE_RPC(c2sEngineerDeployObject,  (RangedU32<0,EngineeredItemCount> objectType));
   TNL_DECLARE_RPC(c2sEngineerInterrupted,   (RangedU32<0,EngineeredItemCount> objectType));
   TNL_DECLARE_RPC(s2cEngineerResponseEvent, (RangedU32<0,EngineerEventCount>  event));

   TNL_DECLARE_RPC(s2cDisableWeaponsAndModules, (bool disable));

   // Chage passwords on the server
   void changeParam(const char *param, ParamType type);

   TNL_DECLARE_RPC(c2sSubmitPassword, (StringPtr pass));

   // Tell server that the client is (or claims to be) authenticated
   TNL_DECLARE_RPC(c2sSetAuthenticated, ());       
   // Tell clients a player is authenticated, and pass on some badge info while we're on the phone
   TNL_DECLARE_RPC(s2cSetAuthenticated, (StringTableEntry name, bool isAuthenticated, Int<BADGE_COUNT> badges, U16 gamesPlayed));   

   TNL_DECLARE_RPC(c2sSetVoteMapParam, (U8 voteLength, U8 voteLengthToChangeTeam, U8 voteRetryLength, S32 voteYesStrength, S32 voteNoStrength, S32 voteNothingStrength,
                                        bool voteEnable, bool allowGetMap, bool allowMapUpload, bool randomLevels));
   TNL_DECLARE_RPC(c2sSetParam, (StringPtr param, RangedU32<0, ParamTypeCount> paramType));


   TNL_DECLARE_RPC(s2cSetRole, (RangedU32<0, ClientInfo::MaxRoles> role, bool notify));
   TNL_DECLARE_RPC(s2cWrongPassword, ());

   TNL_DECLARE_RPC(s2cSetServerName, (StringTableEntry name));
   TNL_DECLARE_RPC(s2cDisplayAnnouncement, (string message));


   bool isInCommanderMap();

   TNL_DECLARE_RPC(c2sRequestCommanderMap, ());
   TNL_DECLARE_RPC(c2sReleaseCommanderMap, ());

   TNL_DECLARE_RPC(s2cCreditEnergy, (SignedInt<18> energy));
   TNL_DECLARE_RPC(s2cSetFastRechargeTime, (U32 time));

   TNL_DECLARE_RPC(c2sRequestLoadout, (Vector<U8> loadout));   // Client has changed his loadout configuration

   TNL_DECLARE_RPC(s2cDisplayMessageESI, (RangedU32<0, ColorCount> color, RangedU32<0, NumSFXBuffers> sfx,
                   StringTableEntry formatString, Vector<StringTableEntry> e, Vector<StringPtr> s, Vector<S32> i));
   TNL_DECLARE_RPC(s2cDisplayMessageE, (RangedU32<0, ColorCount> color, RangedU32<0, NumSFXBuffers> sfx,
                   StringTableEntry formatString, Vector<StringTableEntry> e));
   TNL_DECLARE_RPC(s2cTouchdownScored, (RangedU32<0, NumSFXBuffers> sfx, S32 team, StringTableEntry formatString, Vector<StringTableEntry> e, Point scorePos));

   TNL_DECLARE_RPC(s2cDisplayMessage, (RangedU32<0, ColorCount> color, RangedU32<0, NumSFXBuffers> sfx, StringTableEntry formatString));

   // These could be consolidated
   TNL_DECLARE_RPC(s2cDisplaySuccessMessage, (StringTableEntry formatString));    
   TNL_DECLARE_RPC(s2cDisplayErrorMessage,   (StringTableEntry formatString));    
   TNL_DECLARE_RPC(s2cDisplayConsoleMessage, (StringTableEntry formatString));

   TNL_DECLARE_RPC(s2cDisplayMessageBox, (StringTableEntry title, StringTableEntry instr, Vector<StringTableEntry> message));

   TNL_DECLARE_RPC(s2cAddLevel, (StringTableEntry name, RangedU32<0, GameTypesCount> type));
   TNL_DECLARE_RPC(s2cRemoveLevel, (S32 index));
   TNL_DECLARE_RPC(c2sAddLevel, (StringTableEntry name, RangedU32<0, GameTypesCount> type, S32 minPlayers, S32 maxPlayers, S32 index));
   TNL_DECLARE_RPC(c2sRemoveLevel, (S32 index));
   TNL_DECLARE_RPC(s2cRequestLevel, (S32 index));

   TNL_DECLARE_RPC(c2sRequestLevelChange, (S32 newLevelIndex, bool isRelative));
   TNL_DECLARE_RPC(c2sShowNextLevel, ());
   TNL_DECLARE_RPC(c2sRequestShutdown, (U16 time, StringPtr reason));
   TNL_DECLARE_RPC(c2sRequestCancelShutdown, ());
   TNL_DECLARE_RPC(s2cInitiateShutdown, (U16 time, StringTableEntry name, StringPtr reason, bool originator));
   TNL_DECLARE_RPC(s2cCancelShutdown, ());

   TNL_DECLARE_RPC(s2cSetIsBusy, (StringTableEntry name, bool isBusy));

   TNL_DECLARE_RPC(c2sSetIsBusy, (bool isBusy));

   TNL_DECLARE_RPC(c2sSetServerAlertVolume, (S8 vol));
   TNL_DECLARE_RPC(c2sRenameClient, (StringTableEntry newName));

   TNL_DECLARE_RPC(c2sRequestCurrentLevel, ());

   enum ServerFlags {
      ServerFlagAllowUpload = BIT(0),
      ServerFlagHasRecordedGameplayDownloads = BIT(1),
      ServerFlagHostingLevels = BIT(2),
      // U8 max!
   };

   enum LevelFileTransmissionStage { // for s2rSendDataParts only
      TransmissionLevelFile = 1,
      TransmissionLevelGenFile = 2,
      TransmissionDone = 4,
      TransmissionRecordedGame = 8
   };

   U8 mSendableFlags;
private:
   ByteBuffer *mDataBuffer;
   ByteBuffer *mDataBufferLevelGen;
   string mFileName; // used for game recorder filename
public:

   TNL_DECLARE_RPC(s2rSendableFlags, (U8 flags));
   TNL_DECLARE_RPC(s2rSendDataParts, (U8 type, ByteBufferPtr data));
   TNL_DECLARE_RPC(s2rTransferFileSize, (U32 size));
   TNL_DECLARE_RPC(c2sRequestRecordedGameplay, (StringPtr file));
   TNL_DECLARE_RPC(s2cListRecordedGameplays, (Vector<string> files));
   TNL_DECLARE_RPC(s2cSetFilename, (string filename));
   bool TransferLevelFile(const char *filename);
   bool TransferRecordedGameplay(const char *filename);
   void ReceivedLevelFile(const U8 *leveldata, U32 levelsize, const U8 *levelgendata, U32 levelgensize);
   void ReceivedRecordedGameplay(const U8 *filedata, U32 filedatasize);
   F32 getFileProgressMeter();


   Vector<SafePtr<ByteBuffer> > mPendingTransferData; // Only used for progress meter
   U32 mReceiveTotalSize;

   bool mVoiceChatEnabled;  // server side: false when this client have set the voice volume to zero, which means don't send voice to this client
                            // client side: this can allow or disallow sending voice to server
   TNL_DECLARE_RPC(s2rVoiceChatEnable, (bool enabled));

   void resetAuthenticationTimer();
   S32 getAuthenticationCounter();

   void requestAuthenticationVerificationFromMaster();
   virtual void updateTimers(U32 timeDelta);

   void displayMessageE(U32 color, U32 sfx, StringTableEntry formatString, Vector<StringTableEntry> e);

   static const U8 CONNECT_VERSION;  // may be useful in future version with same CS protocol number
   U8 mConnectionVersion;  // the CONNECT_VERSION of the other side of this connection

   void writeConnectRequest(BitStream *stream);
   bool readConnectRequest(BitStream *stream, NetConnection::TerminationReason &reason);
   void writeConnectAccept(BitStream *stream);
   bool readConnectAccept(BitStream *stream, NetConnection::TerminationReason &reason);

   void setConnectionSpeed(S32 speed);

   void onConnectionEstablished();
   void onConnectionEstablished_client();
   void onConnectionEstablished_server();

   void onConnectTerminated(TerminationReason r, const char *notUsed);
   void onConnectionTerminated(TerminationReason r, const char *string);
   void disconnect(TerminationReason r, const char *reason);


   TNL_DECLARE_NETCONNECTION(GameConnection);
};

};

#endif

