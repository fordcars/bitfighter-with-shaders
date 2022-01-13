//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _CHATCMDS_H_
#define _CHATCMDS_H_

#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>

using namespace TNL;
using namespace std;

namespace Zap
{

static const S32 MAX_CMDS = 9;

enum ArgTypes {
   NAME,    // Player name (can be tab-completed)
   TEAM,    // Team name (can be tab-completed)
   LEVEL,   // Level name (can be tab-completed)
   xINT,    // Integer argument
   STR,     // String argument
   PT,      // Point argument (only used by Lua scripts)
   ARG_TYPES
};

enum HelpCategories {
   ADV_COMMANDS,
   SOUND_COMMANDS,
   LEVEL_COMMANDS,
   BOT_COMMANDS,
   ADMIN_COMMANDS,
   OWNER_COMMANDS,
   DEBUG_COMMANDS,
   DEVELOPER_COMMANDS,     // Only include in debug builds
   COMMAND_CATEGORIES
};


class ClientGame;


struct CommandInfo 
{
   string cmdName;
   void (*cmdCallback)(ClientGame *game, const Vector<string> &args);
   ArgTypes cmdArgInfo[MAX_CMDS];
   S32 cmdArgCount;
   HelpCategories helpCategory;
   S32 helpGroup;
   S32 lines;                    // # lines required to display help (usually 1, occasionally 2)
   string helpArgString[MAX_CMDS];
   string helpTextString;
};

}

using namespace Zap;


namespace ChatCommands 
{

void announceHandler           (ClientGame *game, const Vector<string> &args);
void mVolHandler               (ClientGame *game, const Vector<string> &args);    
void sVolHandler               (ClientGame *game, const Vector<string> &args);    
void vVolHandler               (ClientGame *game, const Vector<string> &args);
void servVolHandler            (ClientGame *game, const Vector<string> &args);
void mNextHandler              (ClientGame *game, const Vector<string> &args);
void mPrevHandler              (ClientGame *game, const Vector<string> &args);
void getMapHandler             (ClientGame *game, const Vector<string> &args);
void nextLevelHandler          (ClientGame *game, const Vector<string> &args);
void prevLevelHandler          (ClientGame *game, const Vector<string> &args);
void restartLevelHandler       (ClientGame *game, const Vector<string> &args);
void randomLevelHandler        (ClientGame *game, const Vector<string> &args);
void mapLevelHandler           (ClientGame *game, const Vector<string> &args);
void showNextLevelHandler      (ClientGame *game, const Vector<string> &args);
void shutdownServerHandler     (ClientGame *game, const Vector<string> &args);
void showPrevLevelHandler      (ClientGame *game, const Vector<string> &args);
void kickPlayerHandler         (ClientGame *game, const Vector<string> &args);
void submitPassHandler         (ClientGame *game, const Vector<string> &args);
void showCoordsHandler         (ClientGame *game, const Vector<string> &args);
void showIdsHandler            (ClientGame *game, const Vector<string> &args);
void showZonesHandler          (ClientGame *game, const Vector<string> &args);
void showPathsHandler          (ClientGame *game, const Vector<string> &args);
void pauseBotsHandler          (ClientGame *game, const Vector<string> &args);
void stepBotsHandler           (ClientGame *game, const Vector<string> &args);
void setAdminPassHandler       (ClientGame *game, const Vector<string> &args);
void setOwnerPassHandler       (ClientGame *game, const Vector<string> &args);
void setServerPassHandler      (ClientGame *game, const Vector<string> &args);
void setLevPassHandler         (ClientGame *game, const Vector<string> &args);
void setServerNameHandler      (ClientGame *game, const Vector<string> &args);
void setServerDescrHandler     (ClientGame *game, const Vector<string> &args);
void setServerWelcomeMsgHandler(ClientGame *game, const Vector<string> &args);
void setLevelDirHandler        (ClientGame *game, const Vector<string> &args);
void setGlobalLevelScriptHandler(ClientGame *game,const Vector<string> &args);
void pmHandler                 (ClientGame *game, const Vector<string> &args);
void muteHandler               (ClientGame *game, const Vector<string> &args);
void voiceMuteHandler          (ClientGame *game, const Vector<string> &args);
void maxFpsHandler             (ClientGame *game, const Vector<string> &args);
void lagHandler                (ClientGame *game, const Vector<string> &args);
void clearCacheHandler         (ClientGame *game, const Vector<string> &args);
void lineWidthHandler          (ClientGame *game, const Vector<string> &args);
void idleHandler               (ClientGame *game, const Vector<string> &args);
void showPresetsHandler        (ClientGame *game, const Vector<string> &args);
void deleteCurrentLevelHandler (ClientGame *game, const Vector<string> &args);
void undeleteLevelHandler      (ClientGame *game, const Vector<string> &args);
void addTimeHandler            (ClientGame *game, const Vector<string> &args);
void setTimeHandler            (ClientGame *game, const Vector<string> &args);
void setWinningScoreHandler    (ClientGame *game, const Vector<string> &args);
void resetScoreHandler         (ClientGame *game, const Vector<string> &args);
void addBotHandler             (ClientGame *game, const Vector<string> &args);
void addBotsHandler            (ClientGame *game, const Vector<string> &args);
void kickBotHandler            (ClientGame *game, const Vector<string> &args);
void kickBotsHandler           (ClientGame *game, const Vector<string> &args);
void showBotsHandler           (ClientGame *game, const Vector<string> &args);
void setMaxBotsHandler         (ClientGame *game, const Vector<string> &args);
void banPlayerHandler          (ClientGame *game, const Vector<string> &args);
void banIpHandler              (ClientGame *game, const Vector<string> &args);
void renamePlayerHandler       (ClientGame *game, const Vector<string> &args);
void globalMuteHandler         (ClientGame *game, const Vector<string> &args);
void shuffleTeams              (ClientGame *game, const Vector<string> &args);
void downloadMapHandler        (ClientGame *game, const Vector<string> &args);
void rateMapHandler            (ClientGame *game, const Vector<string> &args);
void commentMapHandler         (ClientGame *game, const Vector<string> &args);
void pauseHandler              (ClientGame *game, const Vector<string> &args);


// The following are only available in debug builds!
#ifdef TNL_DEBUG
   void showObjectOutlinesHandler(ClientGame *game, const Vector<string> &args);
   void showHelpItemHandler      (ClientGame *game, const Vector<string> &args);
#endif


};

#endif
