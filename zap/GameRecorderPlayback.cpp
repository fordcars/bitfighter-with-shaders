//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "GameRecorderPlayback.h"

#include "GameRecorder.h"
#include "tnlBitStream.h"
#include "tnlNetObject.h"
#include "gameType.h"
#include "ServerGame.h"
#include "stringUtils.h"
#include "RenderUtils.h"

#include "ClientGame.h"
#include "UIManager.h"
#include "UIGame.h"
#include "DisplayManager.h"
#include "Renderer.h"
#include "Cursor.h"
#include "Timer.h"
#include "Colors.h"

#include "version.h"

namespace Zap
{


static void idleObjects(ClientGame *game, U32 timeDelta)
{
   const Vector<DatabaseObject *> *gameObjects = game->getGameObjDatabase()->findObjects_fast();

   // Visit each game object, handling moves and running its idle method
   for(S32 i = gameObjects->size() - 1; i >= 0; i--)
   {
      BfObject *obj = static_cast<BfObject *>((*gameObjects)[i]);

      if(obj->isDeleted())
         continue;

      Move m = obj->getCurrentMove();
      m.time = timeDelta;
      obj->setCurrentMove(m);
      obj->idle(BfObject::ClientIdlingNotLocalShip);  // on client, object is not our control object
   }

   // Idled during processMoreData for better seek accuracy
   //if(game->getGameType())
      //game->getGameType()->idle(BfObject::ClientIdlingNotLocalShip, timeDelta);
}


static void resetRenderState(ClientGame *game)
{
   const Vector<DatabaseObject *> *gameObjects = game->getGameObjDatabase()->findObjects_fast();

   for(S32 i = gameObjects->size() - 1; i >= 0; i--)
   {
      BfObject *obj = static_cast<BfObject *>((*gameObjects)[i]);

      if(obj->isDeleted())
         continue;

      MoveObject *obj2 = dynamic_cast<MoveObject *>(obj);
      if(obj2)
         obj2->copyMoveState(ActualState, RenderState);
   }
}


GameRecorderPlayback::GameRecorderPlayback(ClientGame *game, const char *filename) : GameConnection(game, false)
{
   mFile = NULL;
   mGame = game;
   mMilliSeconds = 0;
   mSizeToRead = 0;
   mCurrentTime = 0;
   mTotalTime = 0;
   mIsButtonHeldDown = false;

   if(!mFile)
      mFile = fopen(filename, "rb");

   if(mFile)
   {
      U8 data[4];
      data[0] = 0;
      fread(data, 1, 4, mFile);
      mGhostClassCount = data[1];
      mEventClassCount = U32(data[2]) | (U32(data[3]) << 8);
      if(mEventClassCount & 0x1000)
      {
         mPackUnpackShipEnergyMeter = true;
         mEventClassCount &= ~0x1000;
      }
      if(data[0] != CS_PROTOCOL_VERSION || 
         mEventClassCount > NetClassRep::getNetClassCount(getNetClassGroup(), NetClassTypeEvent) || 
         mGhostClassCount > NetClassRep::getNetClassCount(getNetClassGroup(), NetClassTypeObject))
      {
         fclose(mFile); // Wrong version, warn about this problem?
         mFile = NULL;
      }

      setGhostFrom(false);
      setGhostTo(true);
      mEventClassBitSize = getNextBinLog2(mEventClassCount);
      mGhostClassBitSize = getNextBinLog2(mGhostClassCount);
   }
   mConnectionState = Connected;
   mConnectionParameters.mIsInitiator = true;
   mConnectionParameters.mDebugObjectSizes = false;


   if(mFile)
   {
      S32 filepos = ftell(mFile);
      while(true)
      {
         U8 data[3];
         if(fread(data, 1, 3, mFile) != 3)
            break;
         U32 size = (U32(data[1] & 63) << 8) + data[0];
         U32 milli = S32((U32(data[1] >> 6) << 8) + data[2]);
         if(size == 0)
            break;
         mTotalTime += milli;
         fseek(mFile, size, SEEK_CUR);
      }
      fseek(mFile, filepos, SEEK_SET);
   }
}


GameRecorderPlayback::~GameRecorderPlayback()
{
   if(mFile)
      fclose(mFile);
}


bool GameRecorderPlayback::isValid()     { return mFile != NULL; }
bool GameRecorderPlayback::lostContact() { return false; }


void GameRecorderPlayback::addPendingMove(Move *theMove)
{
   bool nextButton = theMove->fire;
   bool prevButton = theMove->modulePrimary[0] || theMove->modulePrimary[1];

   if(!mIsButtonHeldDown && (nextButton || prevButton))
      changeSpectate(nextButton ? 1 : -1);

   mIsButtonHeldDown = nextButton || prevButton;
}
void GameRecorderPlayback::changeSpectate(S32 n)
{
   const Vector<RefPtr<ClientInfo> > &infos = *(mGame->getClientInfos());
   for(S32 i = 0; i < infos.size(); i++)
      if(infos[i].getPointer() == mClientInfoSpectating.getPointer())
      {
         n += i;
         break;
      }

   if(n < 0)
      n = infos.size() - 1;
   else if(n >= infos.size())
      n = 0;

   if(infos.size() != 0)
      mClientInfoSpectating = infos[n];

   updateSpectate();
}

void GameRecorderPlayback::updateSpectate()
{
   const Vector<RefPtr<ClientInfo> > &infos = *(mGame->getClientInfos());

   if(mClientInfoSpectating.isNull() && infos.size() != 0)
   {
      mClientInfoSpectating = mGame->findClientInfo(mClientInfoSpectatingName);
      if(mClientInfoSpectating.isNull())
         mClientInfoSpectating = infos[0];
   }
   
   if(mClientInfoSpectating.isValid())
   {
      mClientInfoSpectatingName = mClientInfoSpectating->getName();
      Ship *ship = mClientInfoSpectating->getShip();
      setControlObject(ship);

      if(ship)
      {
         mGame->newLoadoutHasArrived(*(ship->getLoadout()));
         mGame->getUIManager()->setListenerParams(ship->getPos(), ship->getVel());
      }
   }
}


void GameRecorderPlayback::processMoreData(U32 MilliSeconds)
{
   if(!mFile)
   {
      //disconnect(ReasonShutdown, "");
      return;
   }

   if(mSizeToRead != 0)
      idleObjects(mGame, MilliSeconds);

   U8 data[16384 - 1];  // 16 KB on stack memory (no memory allocation/deallocation speed cost)

   if(mGame->getGameType())
      mGame->getGameType()->idle(BfObject::ClientIdlingNotLocalShip, MilliSeconds < U32(mMilliSeconds) ? MilliSeconds : U32(mMilliSeconds));

   mMilliSeconds -= S32(MilliSeconds);

   while(mMilliSeconds < 0)
   {

      if(mSizeToRead != 0)
      {
         mPacketRecvBytesLast = mSizeToRead;
         mPacketRecvBytesTotal += mSizeToRead;
         mPacketRecvCount++;

         if(fread(data, 1, mSizeToRead, mFile) == mSizeToRead)
         {
            BitStream bstream(data, mSizeToRead);
            GhostConnection::readPacket(&bstream);
         }
         mSizeToRead = 0;
      }

      if(fread(data, 1, 3, mFile) != 3)
         break; // Could not read 3 bytes

      U32 size = (U32(data[1] & 63) << 8) + data[0];
      U32 milli = S32((U32(data[1] >> 6) << 8) + data[2]);
      mCurrentTime += milli;
      mMilliSeconds += milli;

      if(size == 0 || size >= sizeof(data)) // End of file?
      {
         mMilliSeconds = S32_MAX;
         break;
      }

      mSizeToRead = size;

      if(mGame->getGameType())
         mGame->getGameType()->idle(BfObject::ClientIdlingNotLocalShip, mMilliSeconds < 0 ? milli : milli - U32(mMilliSeconds));
   }

   updateSpectate();
}


void GameRecorderPlayback::restart()
{
   deleteLocalGhosts();
   mMilliSeconds = 0;
   mSizeToRead = 0;
   mCurrentTime = 0;
   clearRecvEvents();
   mGame->clearClientList();

   if(mFile)
      fseek(mFile, 4, SEEK_SET);
}

// --------

static void processPlaybackSelectionCallback(ClientGame *game, U32 index)             
{
   game->getUIManager()->getUI<PlaybackSelectUserInterface>()->processSelection(index);
}


PlaybackSelectUserInterface::PlaybackSelectUserInterface(ClientGame *game) : LevelMenuSelectUserInterface(game)
{
   // Do nothing
}


void PlaybackSelectUserInterface::onActivate()
{
//mLevels
   mMenuTitle = "Choose Recorded Game";

   const string &dir = getGame()->getSettings()->getFolderManager()->recordDir;

   S32 oldIndex = selectedIndex;

   clearMenuItems();
   mLevels.clear();

   const string extList[] = {GameRecorderServer::buildGameRecorderExtension()};
   getFilesFromFolder(dir, mLevels, extList, ARRAYSIZE(extList));

   if(mLevels.size() == 0)
      mMenuTitle = "No recorded games exists";  // TODO: Need better way to display this problem
   else
      mLevels.sort(alphaNumberSort);

   for(S32 i = 0; i < mLevels.size(); i++)
   {
      addMenuItem(new MenuItem(i, mLevels[i].c_str(), processPlaybackSelectionCallback, ""));
   }


   MenuUserInterface::onActivate();

   selectedIndex = oldIndex;
   if(selectedIndex >= mLevels.size())
      selectedIndex = mLevels.size() - 1;

   mFirstVisibleItem = selectedIndex - 5;
   if(mFirstVisibleItem < 0)
      mFirstVisibleItem = 0;
}


void PlaybackSelectUserInterface::processSelection(U32 index)
{
   string file = joindir(getGame()->getSettings()->getFolderManager()->recordDir, mLevels[index]);

   GameRecorderPlayback *gc = new GameRecorderPlayback(getGame(), file.c_str());
   if(!gc->isValid())
   {
      delete gc;
      getUIManager()->displayMessageBox("Error", "Press [[Esc]] to continue", "Recorded Gameplay not valid or not compatible");
      return;
   }

   if(gc->mTotalTime == 0)
   {
      delete gc;
      getUIManager()->displayMessageBox("Error", "Press [[Esc]] to continue", "Recorded Gameplay is empty");
      return;
   }

   // Close previous connection if it exists
   if(getGame()->getConnectionToServer() != NULL)
      getGame()->closeConnectionToGameServer();

   // Set new connection
   getGame()->setConnectionToServer(gc);

   UIManager *uiManager = getGame()->getUIManager();

   // If we've come from a previous playback UI
   if(uiManager->cameFrom<PlaybackGameUserInterface>())
   {
      // Reactivate
      uiManager->reactivate(uiManager->getUI<PlaybackGameUserInterface>());
   }
   // Otherwise start the playback UI directly
   else
      uiManager->activate(uiManager->getUI<PlaybackGameUserInterface>());

}

// --------

static void processPlaybackDownloadCallback(ClientGame *game, U32 index)             
{
   game->getUIManager()->getUI<PlaybackServerDownloadUserInterface>()->processSelection(index);
}


PlaybackServerDownloadUserInterface::PlaybackServerDownloadUserInterface(ClientGame *game) : LevelMenuSelectUserInterface(game)
{
   // Do nothing
}

void PlaybackServerDownloadUserInterface::onActivate()
{
   mMenuTitle = "Download Recorded Game";

   MenuUserInterface::onActivate();

   getGame()->getConnectionToServer()->c2sRequestRecordedGameplay(StringPtr(""));
}


void PlaybackServerDownloadUserInterface::processSelection(U32 index)
{
   if(U32(index) >= U32(mLevels.size()))
      return;

   getGame()->getConnectionToServer()->c2sRequestRecordedGameplay(StringPtr(mLevels[index].c_str()));
   MenuItem *item = getMenuItem(index);
   if(item)
   {
      string downloadedstring = mLevels[index] + " (downloaded)";

      // Call destructor and contructor without changing memory pointers... Got a better way to change multiple arguments?
      item->~MenuItem();
      new(item) MenuItem(index | 0x40000000, downloadedstring.c_str(), NULL, "");
   }
}


void PlaybackServerDownloadUserInterface::receivedLevelList(const Vector<string> &levels)
{
   mLevels = levels;
   clearMenuItems();
   for(S32 i = 0; i < mLevels.size(); i++)
   {
      addMenuItem(new MenuItem(i, mLevels[i].c_str(), processPlaybackDownloadCallback, ""));
   }
}


////////////////////////////////////////
////////////////////////////////////////
#define DISABLE_MOUSE_TIME 1000

PlaybackGameUserInterface::PlaybackGameUserInterface(ClientGame *game) : UserInterface(game)
{
   mGameInterface = game->getUIManager()->getUI<GameUserInterface>();
   mSpeed = 0;
   mSpeedRemainder = 0;
   mVisible = true;
   mDisableMouseTimer.setPeriod(DISABLE_MOUSE_TIME);
}


void PlaybackGameUserInterface::onActivate()
{
   mPlaybackConnection = dynamic_cast<GameRecorderPlayback *>(getGame()->getConnectionToServer());
   Cursor::enableCursor();
   mSpeed = 2;
   mSpeedRemainder = 0;
   mVisible = true;

   // Clear out any lingering server or chat messages
   mGameInterface->clearDisplayers();
}


void PlaybackGameUserInterface::onReactivate()
{
   Cursor::enableCursor();
}


const F32 playbackBar_x = 200;
const F32 playbackBar_y = 570;
const F32 playbackBar_w = 400;
const F32 playbackBar_h = 10;

const F32 playbackBarVertex[] = {
   playbackBar_x,                 playbackBar_y,
   playbackBar_x + playbackBar_w, playbackBar_y,
   playbackBar_x + playbackBar_w, playbackBar_y + playbackBar_h,
   playbackBar_x,                 playbackBar_y + playbackBar_h,
};


const F32 btn0_x = 200; // pause
const F32 btn1_x = 250; // slow play
const F32 btn2_x = 300; // play
const F32 btn3_x = 350; // fast forward
const F32 btn_y = 530;
const F32 btn_w = 20;
const F32 btn_h = 20;

const F32 btn_spectate_name_x = 400;

const F32 buttons_lines[] = {
   btn0_x + btn_w/3  , btn_y            , btn0_x            , btn_y,
   btn0_x + btn_w/3  , btn_y + btn_h    , btn0_x            , btn_y + btn_h,
   btn0_x            , btn_y + btn_h    , btn0_x            , btn_y,
   btn0_x + btn_w/3  , btn_y + btn_h    , btn0_x + btn_w/3  , btn_y,
   btn0_x + btn_w*2/3, btn_y            , btn0_x + btn_w    , btn_y,
   btn0_x + btn_w*2/3, btn_y + btn_h    , btn0_x + btn_w    , btn_y + btn_h,
   btn0_x + btn_w    , btn_y + btn_h    , btn0_x + btn_w    , btn_y,
   btn0_x + btn_w*2/3, btn_y + btn_h    , btn0_x + btn_w*2/3, btn_y,

   btn1_x + btn_w/4  , btn_y            , btn1_x            , btn_y,
   btn1_x + btn_w/4  , btn_y + btn_h    , btn1_x            , btn_y + btn_h,
   btn1_x            , btn_y + btn_h    , btn1_x            , btn_y,
   btn1_x + btn_w/4  , btn_y + btn_h    , btn1_x + btn_w/4  , btn_y,
   btn1_x + btn_w/2  , btn_y            , btn1_x + btn_w/2  , btn_y + btn_h,
   btn1_x + btn_w/2  , btn_y            , btn1_x + btn_w    , btn_y + btn_h/2,
   btn1_x + btn_w/2  , btn_y + btn_h    , btn1_x + btn_w    , btn_y + btn_h/2,

   btn2_x            , btn_y            , btn2_x            , btn_y + btn_h,
   btn2_x            , btn_y            , btn2_x + btn_w    , btn_y + btn_h/2,
   btn2_x            , btn_y + btn_h    , btn2_x + btn_w    , btn_y + btn_h/2,

   btn3_x            , btn_y            , btn3_x            , btn_y + btn_h,
   btn3_x            , btn_y            , btn3_x + btn_w/2  , btn_y + btn_h/2,
   btn3_x            , btn_y + btn_h    , btn3_x + btn_w/2  , btn_y + btn_h/2,
   btn3_x + btn_w/2  , btn_y            , btn3_x + btn_w/2  , btn_y + btn_h,
   btn3_x + btn_w/2  , btn_y            , btn3_x + btn_w    , btn_y + btn_h/2,
   btn3_x + btn_w/2  , btn_y + btn_h    , btn3_x + btn_w    , btn_y + btn_h/2,

};


bool PlaybackGameUserInterface::onKeyDown(InputCode inputCode)
{
   if(inputCode == MOUSE_LEFT)
   {
      F32 x = DisplayManager::getScreenInfo()->getMousePos()->x;
      F32 y = DisplayManager::getScreenInfo()->getMousePos()->y;
      
      if(y >= btn_y && y <= btn_y + btn_h)
      {
         if(x >= btn0_x && x <= btn0_x + btn_w)
            mSpeed = 0;
         else if(x >= btn1_x && x <= btn1_x + btn_w)
            mSpeed = 1;
         else if(x >= btn2_x && x <= btn2_x + btn_w)
            mSpeed = 2;
         else if(x >= btn3_x && x <= btn3_x + btn_w)
            mSpeed = 3;
         return true;
      }
      else if(y >= playbackBar_y && y <= playbackBar_y + playbackBar_h)
      {
         F32 x2 = (x - playbackBar_x) / playbackBar_w;
         if(x2 < 0)
            x2 = 0;
         if(x2 > 1)
            x2 = 1;

         U32 time = U32(x2 * mPlaybackConnection->mTotalTime);

         // TODO: maybe we need to seek more efficiency by avoiding having to start all over from the beginning
         if(time < mPlaybackConnection->mCurrentTime)
            mPlaybackConnection->restart();

         mPlaybackConnection->processMoreData(time - mPlaybackConnection->mCurrentTime);
         resetRenderState(getGame());

         return true;
      }
   }

   // Next player
   if(checkInputCode(BINDING_ADVWEAP, inputCode)
      || checkInputCode(BINDING_ADVWEAP2, inputCode)
      || checkInputCode(BINDING_FIRE, inputCode) )
   {
      mPlaybackConnection->changeSpectate(1);

      // Show controls and player name
      mDisableMouseTimer.reset();
      mVisible = true;
   }

   // Previous player
   else if(checkInputCode(BINDING_PREVWEAP, inputCode)
      || checkInputCode(BINDING_MOD1, inputCode)
      || checkInputCode(BINDING_MOD2, inputCode) )
   {
      mPlaybackConnection->changeSpectate(-1);

      // Show controls and player name
      mDisableMouseTimer.reset();
      mVisible = true;
   }

   // Handle a few UIGame specific keys that may be useful in playback
   else if(inputCode == KEY_ESCAPE || inputCode == BUTTON_BACK ||
         checkInputCode(BINDING_CMDRMAP, inputCode) ||
         checkInputCode(BINDING_SCRBRD, inputCode) ||
         checkInputCode(BINDING_HELP, inputCode) ||
         checkInputCode(BINDING_MISSION, inputCode) ||
         inputCode == KEY_M
         )
      mGameInterface->onKeyDown(inputCode);

   // Otherwise pass to parent
   else
      return Parent::onKeyDown(inputCode);

   return true;
}


void PlaybackGameUserInterface::onKeyUp(InputCode inputCode) { mGameInterface->onKeyUp(inputCode); }
void PlaybackGameUserInterface::onTextInput(char ascii)      { mGameInterface->onTextInput(ascii); }


void PlaybackGameUserInterface::onMouseMoved()
{
   // Reset mouse timer
   mDisableMouseTimer.reset();
   Cursor::enableCursor();

   // Show playback controls if mouse moves
   mVisible = true;
}


void PlaybackGameUserInterface::idle(U32 timeDelta)
{
   mGameInterface->idle(timeDelta);

   // Check to see if its time to disable cursor
   if(mDisableMouseTimer.update(timeDelta))
   {
      // If mouse is not hovering near the controls, disable it and the controls
      F32 y = DisplayManager::getScreenInfo()->getMousePos()->y;
      if(y < 500)
      {
         Cursor::disableCursor();
         mVisible = false;
      }
   }

   U32 idleTime = timeDelta;
   switch(mSpeed)
   {
      case 0: idleTime = 0; break;
      case 1: idleTime = (timeDelta + mSpeedRemainder) >> 2; mSpeedRemainder = (mSpeedRemainder + timeDelta) & 3; break;
      case 2: break;
      case 3: idleTime = timeDelta * 4; break;
   }

   if(idleTime != 0)
   {
      mGameInterface->idleFxManager(idleTime);
      mPlaybackConnection->processMoreData(idleTime);
   }

   // Cheap way to avoid letting the client move objects, because of pause/slow motion/fast forward
   getGame()->setGameSuspended_FromServerMessage(true); 
}


void PlaybackGameUserInterface::render()
{
   Renderer& r = Renderer::get();
   mGameInterface->render();

   if(mVisible)
   {
      // Draw fancy box around controls
      static const S32 cornerSize = 15,
            top = 510,
            bottom = 600,
            left = 180,
            right = 620;

      static const F32 controlBoxPoints[] = {
            left, bottom,  left, top,
            right - cornerSize, top,  right, top + cornerSize,
            right, bottom
      };

      // Fill
      r.setColor(Colors::black, 0.70f);
      r.renderVertexArray(controlBoxPoints, ARRAYSIZE(controlBoxPoints)/2, RenderType::TriangleFan);

      // Border
      r.setColor(Colors::blue);
      r.renderVertexArray(controlBoxPoints, ARRAYSIZE(controlBoxPoints)/2, RenderType::LineStrip);

      // Playback bar
      r.setColor(Colors::white);
      r.renderVertexArray(playbackBarVertex, 4, RenderType::LineLoop);

      F32 vertex[4];
      vertex[0] = mPlaybackConnection->mCurrentTime * playbackBar_w / mPlaybackConnection->mTotalTime + playbackBar_x;
      vertex[1] = playbackBar_y;
      vertex[2] = vertex[0];
      vertex[3] = playbackBar_y + playbackBar_h;
      r.renderVertexArray(vertex, 2, RenderType::Lines);

      r.renderVertexArray(buttons_lines, sizeof(buttons_lines) / (sizeof(buttons_lines[0]) * 2), RenderType::Lines);

      r.setColor(Colors::yellow);
      drawString(btn_spectate_name_x, btn_y, 15, mPlaybackConnection->mClientInfoSpectatingName.getString());
   }
}


}
