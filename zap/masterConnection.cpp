//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "masterConnection.h"

#include "version.h"
#include "ServerGame.h"
#include "LevelDatabase.h"
#include "gameConnection.h"
#include "gameType.h"

#ifndef ZAP_DEDICATED
#  include "ClientGame.h"
#endif

#include "tnlNetInterface.h"

namespace Zap
{

TNL_IMPLEMENT_NETCONNECTION(MasterServerConnection, NetClassGroupMaster, false);


// Constructor
MasterServerConnection::MasterServerConnection(Game *game)   
{
   mGame = game;
   mHostOnServerAvailable = false;

   // Assign a default id now, will be overwritten with a value from the master when we make our connection
   // This id is sent out with ping and query responses in order to identify servers that may have a different
   // internal and external ip address.
   mClientId = 0;

   mCurrentQueryId = 0;

   // Determine connection type based on Game that is running
   // An anonymous connection can be set with setConnectionType()
   if(mGame->isServer())
      mConnectionType = MasterConnectionTypeServer;
   else
      mConnectionType = MasterConnectionTypeClient;

   setIsConnectionToServer();
   setIsAdaptive();
}


MasterServerConnection::MasterServerConnection()
{
   TNLAssert(false, "We need a default constructor, but we never actually use it!");
}


MasterServerConnection::~MasterServerConnection()
{
   // Do nothing
}

bool MasterServerConnection::isHostOnServerAvailable()
{
   return mHostOnServerAvailable;
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cHostOnServerAvailable, (bool yes))
{
   mHostOnServerAvailable = yes;
}

void MasterServerConnection::startServerQuery(bool hostOnServer)
{
   // Invalidate old queries
   mCurrentQueryId++;

   // And automatically do a server query as well - you may not want to do things
   // in this order in your own clients.
   if(hostOnServer)
      c2mQueryHostServers(mCurrentQueryId);
   else
      c2mQueryServers(mCurrentQueryId);
}


#ifndef ZAP_DEDICATED


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cQueryServersResponse, (U32 queryId, Vector<IPAddress> ipList))
{
   if(mGame->isServer())
      return;

   // Only process results from current query, ignoring anything older...
   if(queryId != mCurrentQueryId || mGame->isServer())
      return;

   // The master server sends out an empty list to signify and "end of transmission".  We'll build up a
   // list of servers here until we get that final EOT list, and then send the entire list to the UI.
   // We need to do this because the UI will look for servers that it knows about that are not on the master 
   // list, and will remove them from the display.  If we send the list in parts, the UI will remove any known servers that
   // don't happen to be on that part.
   if(ipList.size() > 0)
   {
      // Store these intermediate results
      for(S32 i = 0; i < ipList.size(); i++)
         mServerList.push_back(ServerAddr(ipList[i], 0));
   }
   else  // Empty list received, transmission complete, send whole list on to the UI
   {
      static_cast<ClientGame *>(mGame)->gotServerListFromMaster(mServerList);

      mServerList.clear();
   }
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cQueryServersResponse_019a, 
                           (U32 queryId, Vector<IPAddress> ipList, Vector<S32> serverIdList))
{
   if(mGame->isServer())
      return;

   // Only process results from current query, ignoring anything older...
   if(queryId != mCurrentQueryId || mGame->isServer())
      return;

   TNLAssert(ipList.size() == serverIdList.size(), "Expect the same number of elements!");
   if(ipList.size() != serverIdList.size())
      return;

   // The master server sends out an empty list to signify and "end of transmission".  We'll build up a
   // list of servers here until we get that final EOT list, and then send the entire list to the UI.
   // We need to do this because the UI will look for servers that it knows about that are not on the master 
   // list, and will remove them from the display.  If we send the list in parts, the UI will remove any known servers that
   // don't happen to be on that part.
   // Note that for every entry in ipList, there will be a corresponding entry in serverIdList.
   if(ipList.size() > 0)
   {
      // Store these intermediate results
      for(S32 i = 0; i < ipList.size(); i++)
         mServerList.push_back(ServerAddr(ipList[i], serverIdList[i]));
   }
   else  // Empty list received, transmission complete, send whole list on to the UI
   {
      static_cast<ClientGame *>(mGame)->gotServerListFromMaster(mServerList);

      mServerList.clear();
   }
}
#endif


void MasterServerConnection::cancelArrangedConnectionAttempt()
{
   mCurrentQueryId++;
}


void MasterServerConnection::requestArrangedConnection(const Address &remoteAddress)
{
   mCurrentQueryId++;

   c2mRequestArrangedConnection(mCurrentQueryId, remoteAddress.toIPAddress(),
      getInterface()->getFirstBoundInterfaceAddress().toIPAddress(),
      new ByteBuffer((U8 *) "ZAP!", 5));
}


void MasterServerConnection::updateServerStatus(StringTableEntry levelName, StringTableEntry levelType,
      U32 botCount, U32 playerCount, U32 maxPlayers, U32 infoFlags)
{
   s2mUpdateServerStatus(levelName, levelType, botCount, playerCount, maxPlayers, infoFlags);
}


S32 MasterServerConnection::getClientId() const
{
   return mClientId;
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2sClientRequestedArrangedConnection, 
                              (U32 requestId, Vector<IPAddress> possibleAddresses, ByteBufferPtr connectionParameters))
{
   if(!mGame->isServer())   // We're not a server!  Reject connection!
   {
      logprintf(LogConsumer::LogConnection, "Rejecting arranged connection from %s, We're not a server!", 
                Address(possibleAddresses[0]).toString());

      const char *NotAServer = "Not a server";

      s2mRejectArrangedConnection(requestId, ByteBufferPtr(new ByteBuffer((U8 *)NotAServer, sizeof(NotAServer))));
      return;
   }

   // From here on, we're running on a game server that the master is trying to arrange a connection with

   Vector<Address> fullPossibleAddresses;
   for(S32 i = 0; i < possibleAddresses.size(); i++)
      fullPossibleAddresses.push_back(Address(possibleAddresses[i]));

   // Ok, let's do the arranged connection!
   U8 data[Nonce::NonceSize * 2 + SymmetricCipher::KeySize * 2];

   TNL::Random::read(data, sizeof(data));
   IPAddress localAddress = getInterface()->getFirstBoundInterfaceAddress().toIPAddress();
   ByteBufferPtr b = new ByteBuffer(data, sizeof(data));
   b->takeOwnership();

   // Let the server know we're accepting the connection, and pass back our buffer of random data (b)
   s2mAcceptArrangedConnection(requestId, localAddress, b);    
   GameConnection *conn = new GameConnection();

   conn->setNetAddress(fullPossibleAddresses[0]);

   logprintf(LogConsumer::LogConnection, "Accepting arranged connection from %s", 
             Address(fullPossibleAddresses[0]).toString());

   U32 bufferSize = sizeof(data) - 2 * Nonce::NonceSize;
   ByteBufferPtr theSharedData = new ByteBuffer(data + 2 * Nonce::NonceSize, bufferSize);
   Nonce nonce(data);
   Nonce serverNonce(data + Nonce::NonceSize);
   theSharedData->takeOwnership();

   conn->connectArranged(getInterface(), fullPossibleAddresses, nonce, serverNonce, theSharedData, false);
}


#ifndef ZAP_DEDICATED

TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cArrangedConnectionAccepted, 
                           (U32 requestId, Vector<IPAddress> possibleAddresses, ByteBufferPtr connectionData))
{
   if(!mGame->isServer() && requestId == mCurrentQueryId && 
                  connectionData->getBufferSize() >= Nonce::NonceSize * 2 + SymmetricCipher::KeySize * 2)
   {
      logprintf(LogConsumer::LogConnection, "Remote host accepted arranged connection.");

      Vector<Address> fullPossibleAddresses;
      for(S32 i = 0; i < possibleAddresses.size(); i++)
         fullPossibleAddresses.push_back(Address(possibleAddresses[i]));

      ByteBufferPtr theSharedData =
                     new ByteBuffer(
                           (U8 *) connectionData->getBuffer() + Nonce::NonceSize * 2,
                           connectionData->getBufferSize() - Nonce::NonceSize * 2 );

      Nonce nonce(connectionData->getBuffer());
      Nonce serverNonce(connectionData->getBuffer() + Nonce::NonceSize);

      // Client is creating new connection to game server
      ClientGame *clientGame = static_cast<ClientGame *>(mGame);
      GameConnection *gameConnection = new GameConnection(clientGame, false);
      clientGame->setConnectionToServer(gameConnection);

      gameConnection->connectArranged(getInterface(), fullPossibleAddresses, nonce, serverNonce, theSharedData, true);
   }
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cArrangedConnectionRejected, (U32 requestId, ByteBufferPtr rejectData))
{
   if(mGame->isServer())
      return;

   if(requestId == mCurrentQueryId)
   {
      rejectData->takeOwnership();
      rejectData->resize(rejectData->getBufferSize() + 1);
      char *rejectString = (char *)rejectData->getBuffer();  // Convert a rejectData into a string, adding a null terminate character
      rejectString[rejectData->getBufferSize() - 1] = 0;

      logprintf(LogConsumer::LogConnection, "Arranged connection rejected: %s", rejectString);
      static_cast<ClientGame *>(mGame)->connectionToServerRejected(rejectString);
   }
}


// Display the MOTD that is set by the master server
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSetMOTD, (StringPtr masterName, StringPtr motdString))
{
   if(mGame->isServer())
      return;

   setMasterName(masterName.getString());
   static_cast<ClientGame *>(mGame)->setMOTD(motdString.getString());

   terminateIfAnonymous();
}


// The master server has looked at our name and password, and determined if we're in the database properly.  Here's its reply.
// The ClientInfo that gets filled here is the FullClientInfo that lives on he client, and describes the player to themselves.
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSetAuthenticated_019, 
                                    (RangedU32<0, AuthenticationStatusCount> authStatus, Int<BADGE_COUNT> badges, 
                                     U16 gamesPlayed, StringPtr correctedName))
{
   if(mGame->isServer())
      return;

   ClientGame *clientGame = static_cast<ClientGame *>(mGame);

   if((AuthenticationStatus)authStatus.value == AuthenticationStatusAuthenticatedName)
   {
      clientGame->correctPlayerName(correctedName.getString());
      clientGame->getClientInfo()->setAuthenticated(true, badges, gamesPlayed);

      GameConnection *gc = clientGame->getConnectionToServer();
      if(gc)
         gc->c2sSetAuthenticated();    // Tell server that the client is (or claims to be) authenticated
   }
   else 
      clientGame->getClientInfo()->setAuthenticated(false, NO_BADGES, 0);
}
#endif


// Now we know that player with specified id has an approved name
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2sSetAuthenticated_019, (Vector<U8> id, StringTableEntry name,
         RangedU32<0,AuthenticationStatusCount> status, Int<BADGE_COUNT> badges, U16 gamesPlayed))
{
   if(!mGame->isServer())
      return;

   Nonce clientId(id);     // Reconstitute our id into a nonce

   for(S32 i = 0; i < mGame->getClientCount(); i++)
   {
      ClientInfo *clientInfo = mGame->getClientInfo(i);

      // Robots don't have valid clientId, so this will never match a bot
      if(clientInfo->getId()->isValid() && *clientInfo->getId() == clientId)    
      {
         if(status == AuthenticationStatusAuthenticatedName)
         {
            clientInfo->setAuthenticated(true, badges, gamesPlayed);       // Broadcasts status to other clients

            // Auto-rename other non-authenticated clients to avoid stealing the authenticated name
            for(S32 j = 0; j < mGame->getClientCount(); j++)
            {
               ClientInfo *c = mGame->getClientInfo(j);

               if(c->getName() == name && (*c->getId() != clientId))
               {
                  if(c->isAuthenticated()) {
                     c->getConnection()->disconnect(ReasonNone, "Another login was detected");
                  } else {
                     //makeUnique will think the name is in use by self, and rename it.
                     mGame->updateClientChangedName(c, mGame->makeUnique(c->getName().getString()).c_str());
                  }
               }
            }

            StringTableEntry oldName = clientInfo->getName();
            clientInfo->setName("");                               // Avoid unique self
            StringTableEntry uniqueName = mGame->makeUnique(name.getString()).c_str();  // The new name
            clientInfo->setName(oldName);                          // Restore name to properly get it updated to clients

            if(clientInfo->getName() != uniqueName)
               mGame->updateClientChangedName(clientInfo, uniqueName);
         }
         else if(status == AuthenticationStatusUnauthenticatedName)
         {  
            // Client gets two bites at the apple, to cover rare race condition
            if(clientInfo->getConnection()->getAuthenticationCounter() > 1)  
               clientInfo->setAuthenticated(false, NO_BADGES, 0);
            else
               clientInfo->getConnection()->resetAuthenticationTimer();
         }
         else if(status == AuthenticationStatusTryAgainLater)
            clientInfo->getConnection()->resetAuthenticationTimer();

         break;
      }
   }
}


#ifndef ZAP_DEDICATED
// Alert user to the fact that their client is (or is not) out of date
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSendUpdgradeStatus, (bool needToUpgrade))
{
   if(mGame->isServer())
      return;

   static_cast<ClientGame *>(mGame)->setNeedToUpgrade(needToUpgrade);
}


// Handle incoming chat message
// Runs on client only (but initiated by master)
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSendChat, (StringTableEntry playerNick, bool isPrivate, StringPtr message))
{
   if(mGame->isServer())
      return;

   static_cast<ClientGame *>(mGame)->gotGlobalChatMessage(playerNick.getString(), message.getString(), isPrivate);
}


// Set the list of players in chat, typically called when player joins chatroom and needs to know who's there
// Runs on client only (but initiated by master)
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cPlayersInGlobalChat, (Vector<StringTableEntry> playerNicks))
{
   if(mGame->isServer())
      return;

   static_cast<ClientGame *>(mGame)->setPlayersInGlobalChat(playerNicks);
}


// Handle players joining or leaving chat session
// Runs on client only (but initiated by master)
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cPlayerJoinedGlobalChat, (StringTableEntry playerNick))
{
   if(mGame->isServer())
      return;

   static_cast<ClientGame *>(mGame)->playerJoinedGlobalChat(playerNick);
}


// Handle players joining or leaving chat session
// Runs on client only (but initiated by master)
TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cPlayerLeftGlobalChat, (StringTableEntry playerNick))
{
   if(mGame->isServer())
      return;

   static_cast<ClientGame *>(mGame)->playerLeftGlobalChat(playerNick);
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSendHighScores, (Vector<StringTableEntry> groupNames, 
                           Vector<string> names, Vector<string> scores))
{
   if(mGame->isServer())
      return;

   static_cast<ClientGame *>(mGame)->setHighScores(groupNames, names, scores);
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSendPlayerLevelRating, (U32 databaseId, RangedU32<0,2> normalizedRating))
{
   TNLAssert(LevelDatabase::isLevelInDatabase(databaseId), "Should not have received a rating for this level!");

   // Verify that these ratings are for the current level (and that it hasn't somehow changed from underneath us)
   S32 rating = normalizedRating - 1;     // We want -1 -> 1, but send 0 -> 2

   if(databaseId == mGame->getLevelDatabaseId())
   {
      ClientGame *clientGame = static_cast<ClientGame *>(mGame);
      clientGame->gotPlayerLevelRating(rating);
   }
}


TNL_IMPLEMENT_RPC_OVERRIDE(MasterServerConnection, m2cSendTotalLevelRating, (U32 databaseId, S16 rating))
{
   TNLAssert(LevelDatabase::isLevelInDatabase(databaseId), "Should not have received a rating for this level!");
   if(!LevelDatabase::isLevelInDatabase(databaseId))
      return;

   if(databaseId == mGame->getLevelDatabaseId())
   {
      // Verify that these ratings are for the current level (and that it hasn't somehow changed from underneath us)
      ClientGame *clientGame = static_cast<ClientGame *>(mGame);

      if(rating == NotReallyInTheDatabase)
         clientGame->levelIsNotReallyInTheDatabase();
      else
         clientGame->gotTotalLevelRating(rating);
   }
      
}
#endif


// Set if this is to be an anonymous connection only
void MasterServerConnection::setConnectionType(MasterConnectionType type)
{
   mConnectionType = type;
}


// Set if this is to be a anonymous connection only
MasterConnectionType MasterServerConnection::getConnectionType()
{
   return mConnectionType;
}


// Set master server name
void MasterServerConnection::setMasterName(string name)
{
   mMasterName = name;
}


// Retrieve master server name
string MasterServerConnection::getMasterName()
{
   return mMasterName;
}


// Send a connection request to the master server.  Also, tell them a little about ourselves.
// Must match MasterServerConnection::readConnectRequest()!!
void MasterServerConnection::writeConnectRequest(BitStream *bstream)
{
   Parent::writeConnectRequest(bstream);

   bstream->write(MASTER_PROTOCOL_VERSION);  // Version of the protocol we'll be using to communicate with the master
   bstream->write(CS_PROTOCOL_VERSION);      // Version of the Client-Server protocol we use (can only play with others using same version)
   bstream->write(BUILD_VERSION);            // Current build of this game

   // Added in master protocol 6
   bstream->writeEnum(mConnectionType, MasterConnectionTypeCount);

   // We're a server
   if(mConnectionType == MasterConnectionTypeServer)
   {
      ServerGame *serverGame = static_cast<ServerGame *>(mGame);

      bstream->write((U32) serverGame->getRobotCount());      // number of bots
      bstream->write((U32) serverGame->getPlayerCount());     // num players       --> will always be 0 or 1?
      bstream->write((U32) serverGame->getMaxPlayers());      // max players
      bstream->write((U32) serverGame->mInfoFlags);           // info flags (1=>test host, i.e. from editor)

      string levelName = serverGame->getGameType()->getLevelName();
      if(serverGame->isTestServer())
         levelName = LevelSource::TestFileName;

      bstream->writeString(levelName.c_str());                     // Level name
      bstream->writeString(GameType::getGameTypeName(serverGame->getGameType()->getGameTypeId())); // Level type

      bstream->writeString(serverGame->getSettings()->getHostName().c_str());       // Server name
      bstream->writeString(serverGame->getSettings()->getHostDescr().c_str());      // Server description
   }

   // We're a client
   else if(mConnectionType == MasterConnectionTypeClient)
   {
#ifndef ZAP_DEDICATED
      ClientGame *clientGame = (ClientGame *)mGame;
      ClientInfo *clientInfo = clientGame->getClientInfo();

      // First controller's autodetect string (for research purposes!)
      const char *controllerName = "";
      if(GameSettings::DetectedControllerList.size() > 0)
         controllerName = GameSettings::DetectedControllerList.begin()->second.c_str();

      bstream->writeString(controllerName);

      bstream->writeString(clientGame->getPlayerName().c_str());          // User's nickname
      bstream->writeString(clientGame->getPlayerPassword().c_str());      // and whatever password they supplied

      // Starting with MASTER_PROTOCOL_VERSION 6 we will write an 8 bit set of flags
      bstream->writeInt(clientInfo->getPlayerFlagstoSendToMaster(), 8);

      clientInfo->getId()->write(bstream);
#endif
   }

   else if(mConnectionType == MasterConnectionTypeAnonymous)
   {
      // Do nothing.  We're anonymous!
   }
}


// Client side reads ConnectAccept
bool MasterServerConnection::readConnectAccept(BitStream *stream, NetConnection::TerminationReason &reason)
{
   if(!Parent::readConnectAccept(stream, reason))
      return false;

   stream->read(&mClientId);     // This ID is guaranteed unique across all clients/servers

   return true;
}


void MasterServerConnection::onConnectionEstablished()
{
   mGame->onConnectedToMaster();
}


// A still-being-established connection has been terminated
void MasterServerConnection::onConnectionTerminated(TerminationReason reason, const char *reasonStr)   
{
#ifndef ZAP_DEDICATED
   if(!mGame->isServer())
   {
      TNLAssert(dynamic_cast<ClientGame *>(mGame), "mGame is not ClientGame");
      static_cast<ClientGame *>(mGame)->onConnectionToMasterTerminated(reason, reasonStr, true);
   }
#endif
}


// A still-being-established connection has been terminated
void MasterServerConnection::onConnectTerminated(TerminationReason reason, const char *reasonStr)   
{
#ifndef ZAP_DEDICATED
   if(!mGame->isServer())
   {
      TNLAssert(dynamic_cast<ClientGame *>(mGame), "mGame is not ClientGame");
      static_cast<ClientGame *>(mGame)->onConnectionToMasterTerminated(reason, reasonStr, false);
   }
#endif
}


// When we fire this off, we'll be expecting a return message in m2sSetAuthenticated()
void MasterServerConnection::requestAuthentication(StringTableEntry clientName, Nonce clientId)
{
   s2mRequestAuthentication(clientId.toVector(), clientName);
}


void MasterServerConnection::terminateIfAnonymous()
{
   if(mConnectionType != MasterConnectionTypeAnonymous)
      return;

   // Actions performed on an anonymous connection should terminate the connection
   disconnect(ReasonAnonymous, "");
}


AnonymousMasterServerConnection::AnonymousMasterServerConnection(Game *game) : Parent(game)
{
   mConnectionType = MasterConnectionTypeAnonymous;
   mConnectionCallback = NULL;
}


AnonymousMasterServerConnection::~AnonymousMasterServerConnection()
{
   // Do nothing
}


// Set a function to call upon establishing a connection
void AnonymousMasterServerConnection::setConnectionCallback(MasterConnectionCallback callback)
{
   mConnectionCallback = callback;
}


MasterConnectionCallback AnonymousMasterServerConnection::getConnectionCallback()
{
   return mConnectionCallback;
}


void AnonymousMasterServerConnection::onConnectionEstablished()
{
   Parent::onConnectionEstablished();

   // Run the callback
   // This uses some funky syntax.  See sections 2.1 and 2.5 of:
   //   http://www.newty.de/fpt/fpt.html#chapter2
   if(mConnectionCallback)
   {
      (*this.*mConnectionCallback)();
      mConnectionCallback = NULL;
   }
}


};


