//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "ChatHelper.h"

#include "FontManager.h"
#include "UIManager.h"
#include "ChatCommands.h"
#include "ClientGame.h"
#include "Console.h"
#include "LevelSource.h"      // For LevelInfo used in level name tab-completion
#include "UIChat.h"           // For font sizes and such
#include "UIInstructions.h"   // For code to activate help screen

#include "ScissorsManager.h"
#include "DisplayManager.h"
#include "Renderer.h"

#include "Colors.h"

#include "RenderUtils.h"
#include "stringUtils.h"

#ifdef BF_PLATFORM_3DS
#include "Interface3ds.h"
#endif

#include <algorithm>

namespace Zap
{
   CommandInfo chatCmds[] = {   
   //  cmdName          cmdCallback                 cmdArgInfo cmdArgCount   helpCategory helpGroup lines,  helpArgString            helpTextString

   { "dlmap",    &ChatCommands::downloadMapHandler, { STR },       1,      ADV_COMMANDS,     0,     1,    {"<level>"},            "Download the level from the online level database" },
   { "rate",     &ChatCommands::rateMapHandler,     { STR },       1,      ADV_COMMANDS,     0,     1,    {"<up | neutral | down>"}, "Rate this level on the level database (up or down)" },
   { "comment",  &ChatCommands::commentMapHandler,  { STR },       1,      ADV_COMMANDS,     0,     1,    {"<comment>"},          "Post a comment on this level to the level database" },
   { "password", &ChatCommands::submitPassHandler,  { STR },       1,      ADV_COMMANDS,     0,     1,    {"<password>"},         "Request admin or level change permissions"  },
   { "servvol",  &ChatCommands::servVolHandler,     { xINT },      1,      ADV_COMMANDS,     0,     1,    {"<0-10>"},             "Set volume of server"  },
   { "getmap",   &ChatCommands::getMapHandler,      { STR },       1,      ADV_COMMANDS,     1,     1,    {"[file]"},             "Save currently playing level in [file], if allowed" },
   { "idle",     &ChatCommands::idleHandler,        {  },          0,      ADV_COMMANDS,     1,     1,    {  },                   "Place client in idle mode (AFK)" },
   { "pm",       &ChatCommands::pmHandler,          { NAME, STR }, 2,      ADV_COMMANDS,     1,     1,    {"<name>","<message>"}, "Send private message to player" },
   { "mute",     &ChatCommands::muteHandler,        { NAME },      1,      ADV_COMMANDS,     1,     1,    {"<name>"},             "Toggle hiding chat messages from <name>" },
   { "vmute",    &ChatCommands::voiceMuteHandler,   { NAME },      1,      ADV_COMMANDS,     1,     1,    {"<name>"},             "Toggle muting voice chat from <name>" },
                 
   { "mvol",     &ChatCommands::mVolHandler,      { xINT },      1,      SOUND_COMMANDS,   2,     1,    {"<0-10>"},             "Set music volume"      },
   { "svol",     &ChatCommands::sVolHandler,      { xINT },      1,      SOUND_COMMANDS,   2,     1,    {"<0-10>"},             "Set SFX volume"        },
   { "vvol",     &ChatCommands::vVolHandler,      { xINT },      1,      SOUND_COMMANDS,   2,     1,    {"<0-10>"},             "Set voice chat volume" },
   { "mnext",    &ChatCommands::mNextHandler,     {  },          0,      SOUND_COMMANDS,   2,     1,    {  },                   "Play next track in the music list" },
   { "mprev",    &ChatCommands::mPrevHandler,     {  },          0,      SOUND_COMMANDS,   2,     1,    {  },                   "Play previous track in the music list" },

   { "add",         &ChatCommands::addTimeHandler,         { xINT },  1, LEVEL_COMMANDS,  0,  1,  {"<time in minutes>"},                      "Add time to the current game" },
   { "next",        &ChatCommands::nextLevelHandler,       {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Start next level" },
   { "prev",        &ChatCommands::prevLevelHandler,       {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Replay previous level" },
   { "restart",     &ChatCommands::restartLevelHandler,    {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Restart current level" },
   { "random",      &ChatCommands::randomLevelHandler,     {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Start random level" },
   { "map",         &ChatCommands::mapLevelHandler,        { LEVEL }, 1, LEVEL_COMMANDS,  0,  1,  {"<level name>"},                           "Jump to a specific level" },
   { "shownextlevel",&ChatCommands::showNextLevelHandler,  {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Show name of the next level" },
   { "showprevlevel",&ChatCommands::showPrevLevelHandler,  {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Show name of the previous level" },

   { "settime",     &ChatCommands::setTimeHandler,         { xINT },  1, LEVEL_COMMANDS,  0,  1,  {"<time in minutes>"},                      "Set play time for the level" },
   { "setscore",    &ChatCommands::setWinningScoreHandler, { xINT },  1, LEVEL_COMMANDS,  0,  1,  {"<score>"},                                "Set score to win the level" },
   { "resetscore",  &ChatCommands::resetScoreHandler,      {  },      0, LEVEL_COMMANDS,  0,  1,  {  },                                       "Reset all scores to zero" },

   { "addbot",      &ChatCommands::addBotHandler,          { STR, TEAM, STR },       3, BOT_COMMANDS,    1,  2,  {"[file]", "[team name or num]","[args]"},          "Add bot from [file] to [team num], pass [args] to bot" },
   { "addbots",     &ChatCommands::addBotsHandler,         { xINT, STR, TEAM, STR }, 4, BOT_COMMANDS,    1,  2,  {"[count]","[file]","[team name or num]","[args]"}, "Add [count] bots from [file] to [team num], pass [args] to bot" },
   { "kickbot",     &ChatCommands::kickBotHandler,         {  },                     1, BOT_COMMANDS,    1,  1,  {  },                                               "Kick a bot" },
   { "kickbots",    &ChatCommands::kickBotsHandler,        {  },                     1, BOT_COMMANDS,    1,  1,  {  },                                               "Remove all bots from game" },

   { "announce",           &ChatCommands::announceHandler,           { STR },        1, ADMIN_COMMANDS,  0,  1,  {"<announcement>"},      "Announce an important message" },
   { "kick",               &ChatCommands::kickPlayerHandler,         { NAME },       1, ADMIN_COMMANDS,  0,  1,  {"<name>"},              "Kick a player from the game" },
   { "ban",                &ChatCommands::banPlayerHandler,          { NAME, xINT }, 2, ADMIN_COMMANDS,  0,  1,  {"<name>","[duration]"}, "Ban a player from the server (IP-based, def. = 60 mins)" },
   { "banip",              &ChatCommands::banIpHandler,              { STR, xINT },  2, ADMIN_COMMANDS,  0,  1,  {"<ip>","[duration]"},   "Ban an IP address from the server (def. = 60 mins)" },
   { "setlevpass",         &ChatCommands::setLevPassHandler,         { STR },        1, ADMIN_COMMANDS,  0,  1,  {"[passwd]"},            "Set level change password (use blank to clear)" },
   { "setserverpass",      &ChatCommands::setServerPassHandler,      { STR },        1, ADMIN_COMMANDS,  0,  1,  {"<passwd>"},            "Set server password (use blank to clear)" },
   { "leveldir",           &ChatCommands::setLevelDirHandler,        { STR },        1, ADMIN_COMMANDS,  0,  1,  {"<new level folder>"},  "Set leveldir param on the server (changes levels available)" },
   {"setgloballevelscript",&ChatCommands::setGlobalLevelScriptHandler,{ STR },       1, ADMIN_COMMANDS,  0,  1,  {"<script>"},             "Change currently running global levelgen script" },
   { "setservername",      &ChatCommands::setServerNameHandler,      { STR },        1, ADMIN_COMMANDS,  0,  1,  {"<name>"},              "Set server name" },
   { "setserverdescr",     &ChatCommands::setServerDescrHandler,     { STR },        1, ADMIN_COMMANDS,  0,  1,  {"<descr>"},             "Set server description" },
   { "setserverwelcome",   &ChatCommands::setServerWelcomeMsgHandler, { STR },        1, ADMIN_COMMANDS,  0,  1,  {"<descr>"},             "Set server welcome message (use blank to disable)" },
   { "deletecurrentlevel", &ChatCommands::deleteCurrentLevelHandler, { },            0, ADMIN_COMMANDS,  0,  1,  {""},                    "Mark current level as deleted" },
   { "undeletelevel",      &ChatCommands::undeleteLevelHandler,      { },            0, ADMIN_COMMANDS,  0,  1,  {""},                    "Undelete most recently deleted level" },
   { "gmute",              &ChatCommands::globalMuteHandler,         { NAME },       1, ADMIN_COMMANDS,  0,  1,  {"<name>"},              "Globally mute/unmute a player" },
   { "rename",             &ChatCommands::renamePlayerHandler,       { NAME, STR },  2, ADMIN_COMMANDS,  0,  1,  {"<from>","<to>"},       "Give a player a new name" },
   { "maxbots",            &ChatCommands::setMaxBotsHandler,         { xINT },       1, ADMIN_COMMANDS,  0,  1,  {"<count>"},             "Set the maximum bots allowed for this server" },
   { "shuffle",            &ChatCommands::shuffleTeams,              { },            0, ADMIN_COMMANDS,  0,  1,  { "" },                  "Randomly reshuffle teams" },
#ifdef TNL_DEBUG
   { "pause",              &ChatCommands::pauseHandler,              { },            0, ADMIN_COMMANDS,  0,  1,  { "" },                  "TODO: add 'PAUSED' display while paused" },
#endif

   { "setownerpass",       &ChatCommands::setOwnerPassHandler,       { STR },        1, OWNER_COMMANDS,  0,  1,  {"[passwd]"},            "Set owner password" },
   { "setadminpass",       &ChatCommands::setAdminPassHandler,       { STR },        1, OWNER_COMMANDS,  0,  1,  {"[passwd]"},            "Set admin password" },
   { "shutdown",           &ChatCommands::shutdownServerHandler,     { xINT, STR },  2, OWNER_COMMANDS,  0,  1,  {"[time]","[message]"},  "Start orderly shutdown of server (def. = 10 secs)" },

   { "showcoords", &ChatCommands::showCoordsHandler,    {  },        0, DEBUG_COMMANDS, 0,  1, {  },          "Show ship coordinates" },
   { "showzones",  &ChatCommands::showZonesHandler,     {  },        0, DEBUG_COMMANDS, 0,  1, {  },          "Show bot nav mesh zones" },
   { "showids",    &ChatCommands::showIdsHandler,       {  },        0, DEBUG_COMMANDS, 0,  1, {  },          "Show object ids" },
   { "showpaths",  &ChatCommands::showPathsHandler,     {  },        0, DEBUG_COMMANDS, 0,  1, {  },          "Show robot navigation paths" },
   { "showbots",   &ChatCommands::showBotsHandler,      {  },        0, DEBUG_COMMANDS, 0,  1, {  },          "Show all robots" },
   { "pausebots",  &ChatCommands::pauseBotsHandler,     {  },        0, DEBUG_COMMANDS, 0,  1, {  },          "Pause all bots; reissue to start again" },
   { "stepbots",   &ChatCommands::stepBotsHandler,      { xINT },    1, DEBUG_COMMANDS, 1,  1, {"[steps]"},   "Advance bots by number of steps (default = 1)"},
   { "linewidth",  &ChatCommands::lineWidthHandler,     { xINT },    1, DEBUG_COMMANDS, 1,  1, {"[number]"},  "Change width of all lines (default = 2)" },
   { "maxfps",     &ChatCommands::maxFpsHandler,        { xINT },    1, DEBUG_COMMANDS, 1,  1, {"<number>"},  "Set maximum speed of game in frames per second" },
   { "lag",        &ChatCommands::lagHandler, {xINT,xINT,xINT,xINT}, 4, DEBUG_COMMANDS, 1,  2, {"<send lag>", "[% of send drop packets]", "[receive lag]", "[% of receive drop packets]" }, "Set additional lag and dropped packets for testing bad networks" },
   { "clearcache", &ChatCommands::clearCacheHandler,    {  },        0, DEBUG_COMMANDS, 1,  1, { },           "Clear any cached scripts, forcing them to be reloaded" },

   // The following are only available in debug builds!
#ifdef TNL_DEBUG
   { "showobjectoutlines", &ChatCommands::showObjectOutlinesHandler, {  },     0, DEVELOPER_COMMANDS, 1, 1, { },                 "Show HelpItem object outlines on all objects" },    
   { "showhelpitem",       &ChatCommands::showHelpItemHandler,       { xINT }, 0, DEVELOPER_COMMANDS, 1, 1, {"<help item id>" }, "Show specified help item" },
#endif
};


const S32 ChatHelper::chatCmdSize = ARRAYSIZE(chatCmds); // So instructions will now how big chatCmds is
static const S32 CHAT_COMPOSE_FONT_SIZE = 12;

static void makeCommandCandidateList();      // Forward delcaration

ChatHelper::ChatHelper()
{
   mLineEditor = LineEditor(200, "", 50);

   mCurrentChatType = NoChat;
   makeCommandCandidateList();

   setAnimationTime(65);    // Menu appearance time

   mHistory = Vector<string>();
   mHistoryIndex = 0;
}

// Destructor
ChatHelper::~ChatHelper()
{
   // Do nothing
}


HelperMenu::HelperMenuType ChatHelper::getType() { return ChatHelperType; }


void ChatHelper::activate(ChatType chatType)
{
   mCurrentChatType = chatType;
   gInterface3ds.showKeyboard();
   getGame()->setBusyChatting(true);

   // Make sure we have a history slot to represent the command we'll be entering in this session
   if(mHistory.size() == 0 || mHistory.last() != "")
      mHistory.push_back("");
   mHistoryIndex = mHistory.size() - 1;
}


bool ChatHelper::isCmdChat()
{
   return mLineEditor.at(0) == '/' || mCurrentChatType == CmdChat;
}


void ChatHelper::render()
{
   Renderer& renderer = Renderer::get();

   FontManager::pushFontContext(InputContext);
   const char *promptStr;

   Color baseColor;

   if(isCmdChat())      // Whatever the underlying chat mode, seems we're entering a command here
   {
      baseColor = Colors::cmdChatColor;
      promptStr = mCurrentChatType ? "(Command): /" : "(Command): ";
   }
   else if(mCurrentChatType == TeamChat)    // Team chat (goes to all players on team)
   {
      baseColor = Colors::teamChatColor;
      promptStr = "(Team): ";
   }
   else                                     // Global in-game chat (goes to all players in game)
   {
      baseColor = Colors::globalChatColor;
      promptStr = "(Global): ";
   }

   // Protect against crashes while game is initializing... is this really needed??
   if(!getGame()->getConnectionToServer())
      return;

   // Size of chat composition elements
//   static const S32 CHAT_COMPOSE_FONT_GAP = CHAT_COMPOSE_FONT_SIZE / 4;

   static const S32 BOX_HEIGHT = CHAT_COMPOSE_FONT_SIZE + 10;

   S32 xPos = UserInterface::horizMargin;

   // Define some vars for readability:
   S32 promptWidth = getStringWidth(CHAT_COMPOSE_FONT_SIZE, promptStr);
   S32 nameSize   = getStringWidthf(CHAT_COMPOSE_FONT_SIZE, "%s: ", getGame()->getClientInfo()->getName().getString());
   S32 nameWidth  = max(nameSize, promptWidth);
   // Above block repeated below...

   S32 ypos = IN_GAME_CHAT_DISPLAY_POS + CHAT_COMPOSE_FONT_SIZE + 11;      // Top of the box when fully displayed
   S32 realYPos = ypos;

   bool isAnimating = isOpening() || isClosing();

   // Adjust for animated effect
   if(isAnimating)
      ypos += S32((getFraction()) * BOX_HEIGHT);

   S32 boxWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth() - 2 * UserInterface::horizMargin - (nameWidth - promptWidth) - 230;

   // Reuse this to avoid startup and breakdown costs
   static ScissorsManager scissorsManager;

   // Only need to set scissors if we're scrolling.  When not scrolling, we control the display by only showing
   // the specified number of lines; there are normally no partial lines that need vertical clipping as 
   // there are when we're scrolling.  Note also that we only clip vertically, and can ignore the horizontal.
   scissorsManager.enable(isAnimating, getGame()->getSettings()->getIniSettings()->mSettings.getVal<DisplayMode>("WindowMode"), 
                          0.0f, F32(realYPos - 3), F32(DisplayManager::getScreenInfo()->getGameCanvasWidth()), F32(BOX_HEIGHT));

   // Render text entry box like thingy
   F32 top = (F32)ypos - 3;

   F32 vertices[] = {
         (F32)xPos,            top,
         (F32)xPos + boxWidth, top,
         (F32)xPos + boxWidth, top + BOX_HEIGHT,
         (F32)xPos,            top + BOX_HEIGHT
   };

   for(S32 i = 1; i >= 0; i--)
   {
      renderer.setColor(baseColor, i ? .25f : .4f);
      renderer.renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, i ? RenderType::TriangleFan : RenderType::LineLoop);
   }

   renderer.setColor(baseColor);

   // Display prompt
   S32 xStartPos   = xPos + 3 + promptWidth;

   drawString(xPos + 3, ypos, CHAT_COMPOSE_FONT_SIZE, promptStr);  // draw prompt

   // Display typed text
   S32 displayWidth = drawStringAndGetWidth(xStartPos, ypos, CHAT_COMPOSE_FONT_SIZE, mLineEditor.getDisplayString().c_str());

   // If we've just finished entering a chat cmd, show next parameter
   if(isCmdChat())
   {
      string line = mLineEditor.getString();
      Vector<string> words = parseStringAndStripLeadingSlash(line.c_str());

      if(words.size() > 0)
      {
         for(S32 i = 0; i < chatCmdSize; i++)
         {
            const char *cmd = words[0].c_str();

            if(!stricmp(cmd, chatCmds[i].cmdName.c_str()))
            {
               // My thinking here is that if the number of quotes is odd, the last argument is not complete, even if
               // it ends in a space.  There may be an edge case that voids this argument, but our use is simple enough 
               // that this should work well.  If a number is even, num % 2 will be 0.
               S32 numberOfQuotes = (S32) count(line.begin(), line.end(), '"');
               if(chatCmds[i].cmdArgCount >= words.size() && line[line.size() - 1] == ' ' && numberOfQuotes % 2 == 0)
               {
                  renderer.setColor(baseColor * .5);
                  drawString(xStartPos + displayWidth, ypos, CHAT_COMPOSE_FONT_SIZE, chatCmds[i].helpArgString[words.size() - 1].c_str());
               }

               break;
            }
         }
      }
   }

   renderer.setColor(baseColor);
   mLineEditor.drawCursor(xStartPos, ypos, CHAT_COMPOSE_FONT_SIZE);

   // Restore scissors settings -- only used during scrolling
   scissorsManager.disable();
   FontManager::popFontContext();
}


void ChatHelper::onActivated()
{
   Parent::onActivated();
}


// When chatting, show command help if user presses F1
void ChatHelper::activateHelp(UIManager *uiManager)
{
   uiManager->getUI<InstructionsUserInterface>()->activatePage(InstructionsUserInterface::InstructionAdvancedCommands);
}


// Make a list of all players in the game
static void makePlayerNameList(Game *game, Vector<string> &nameCandidateList)
{
   nameCandidateList.clear();

   for(S32 i = 0; i < game->getClientCount(); i++)
      nameCandidateList.push_back(((Game *)game)->getClientInfo(i)->getName().getString());
}


static void makeTeamNameList(const Game *game, Vector<string> &nameCandidateList)
{
   nameCandidateList.clear();

   for(S32 i = 0; i < game->getTeamCount(); i++)
      nameCandidateList.push_back(game->getTeamName(i).getString());
}


static void makeLevelNameList(Game *game, Vector<string> &nameCandidateList)
{
   nameCandidateList.clear();

   TNLAssert(dynamic_cast<ClientGame*>(game), "Not a client game?");
   ClientGame *clientGame = static_cast<ClientGame*>(game);

   GameConnection *gameConnection = clientGame->getConnectionToServer();
   if(!gameConnection)
      return;

   for(S32 i = 0; i < gameConnection->mLevelInfos.size(); i++)
      nameCandidateList.push_back(gameConnection->mLevelInfos[i].mLevelName.getString());
}


static Vector<string> commandCandidateList;

static Vector<string> *getCandidateList(Game *game, CommandInfo *commandInfo, S32 arg)
{
   if(arg == 0)         // ==> Command completion
      return &commandCandidateList;

   else if(arg > 0)     // ==> Arg completion
   {
      if(commandInfo != NULL && arg <= commandInfo->cmdArgCount)     // Found a command
      {
         ArgTypes argType = commandInfo->cmdArgInfo[arg - 1];  // What type of arg are we expecting?

         static Vector<string> nameCandidateList;     // Reusable container

         if(argType == NAME)           // ==> Player name completion
         {  
            makePlayerNameList(game, nameCandidateList);    // Creates a list of all player names
            return &nameCandidateList;
         }

         else if(argType == TEAM)      // ==> Team name completion
         {
            makeTeamNameList(game, nameCandidateList);
            return &nameCandidateList;
         }

         else if(argType == LEVEL)      // ==> Level name completion
         {
            makeLevelNameList(game, nameCandidateList);
            return &nameCandidateList;
         }
         // else no arg completion for you!
      }
   }
   
   return NULL;                        // ==> No completion options
}


// Returns true if key was used, false if not
bool ChatHelper::processInputCode(InputCode inputCode)
{
   // Check for backspace before processing parent because parent will use backspace to close helper, but we want to use
   // it as a, well, a backspace key!
   if(Parent::processInputCode(inputCode))
      return true;
   else if(inputCode == KEY_ENTER)
      issueChat();

   else if(inputCode == KEY_UP)
         upArrowPressed();

   else if(inputCode == KEY_DOWN)
         downArrowPressed();

   else if(inputCode == KEY_TAB)      // Auto complete any commands
   {
      if(isCmdChat())     // It's a command!  Complete!  Complete!
      {
         // First, parse line into words
         Vector<string> words = parseString(mLineEditor.getString());

         bool needLeadingSlash = false;

         // Handle leading slash when command is entered from ordinary chat prompt
         if(words.size() > 0 && words[0][0] == '/')
         {
            // Special case: User has entered process by starting with global chat, and has typed "/" then <tab>
            if(mLineEditor.getString() == "/")
               words.clear();          // Clear -- it's as if we're at a fresh "/" prompt where the user has typed nothing
            else
               words[0].erase(0, 1);   // Strip char -- remove leading "/" so it's as if were at a regular "/" prompt

            needLeadingSlash = true;   // We'll need to add the stripped "/" back in later
         }
               
         S32 arg;                 // Which word we're looking at
         string partial;          // The partially typed word we're trying to match against
         const char *first;       // First arg we entered (will match partial if we're still entering the first one)
         
         // Check for trailing space --> http://www.suodenjoki.dk/us/archive/2010/basic-string-back.htm
         if(words.size() > 0 && *mLineEditor.getString().rbegin() != ' ')   
         {
            arg = words.size() - 1;          // No trailing space --> current arg is the last word we've been typing
            partial = words[arg];            // We'll be matching against what we've typed so far
            first = words[0].c_str();      
         }
         else if(words.size() > 0)           // We've entered a word, pressed space indicating word is complete, 
         {                                   // but have not started typing the next word.  We'll let user cycle through every
            arg = words.size();              // possible value for next argument.
            partial = "";
            first = words[0].c_str(); 
         }
         else     // If the editor is empty, or if final character is a space, then we need to set these params differently
         {
            arg = words.size();              // Trailing space --> current arg is the next word we've not yet started typing
            partial = "";
            first = "";                      // We'll be matching against an empty list since we've typed nothing so far
         }

         // Figure out which command we've got.  Can return NULL if command isn't found or
         // we have a partial command
         CommandInfo *commandInfo = getCommandInfo(first);

         // Special case for multiple words as the last arg of a command
         bool multiWordLastArg = false;
         if(commandInfo != NULL && arg > commandInfo->cmdArgCount)
         {
            bool lastArgIsEmpty = (partial == "");

            // If our last arg is empty, end at the previous one
            S32 end = lastArgIsEmpty ? arg - 1 : arg;

            string newPartial = words[commandInfo->cmdArgCount];
            for(S32 i = commandInfo->cmdArgCount + 1; i <= end; i++)
               newPartial = newPartial + " " + words[i];

            // Set the arg to what it should be with the multiple words
            arg = lastArgIsEmpty ? commandInfo->cmdArgCount + 1 : commandInfo->cmdArgCount;

            // Set our new search string
            partial = newPartial;

            multiWordLastArg = true;
         }

         // Grab our candidates for tab-completion
         Vector<string> *candidates = getCandidateList(getGame(), commandInfo, arg);     // Could return NULL

         // If the command string has quotes in it, use the last space up to the first quote
         const string *entry = mLineEditor.getStringPtr();

         std::size_t lastChar = string::npos;
         if(entry->find_first_of("\"") != string::npos)
            lastChar = entry->find_first_of("\"");

         string appender = " ";

         std::size_t pos = entry->find_last_of(' ', lastChar);

         // Completion position is different if we've used multiple words in our last argument
         if(multiWordLastArg)
            pos = (entry->size() - 1) - partial.size();

         if(pos == string::npos)                         // String does not contain a space, requires special handling
         {
            pos = 0;
            if(words.size() <= 1 && needLeadingSlash)    // ugh!  More special cases!
               appender = "/";
            else
               appender = "";
         }

         mLineEditor.completePartial(candidates, partial, pos, appender);
      }
      else // Username chat completion
      {
         // First, parse line into words
         Vector<string> words = parseString(mLineEditor.getString());

         string partial;          // The partially typed word we're trying to match against

         // Check for trailing space --> http://www.suodenjoki.dk/us/archive/2010/basic-string-back.htm
         if(words.size() > 0 && *mLineEditor.getString().rbegin() != ' ')
            partial = words[words.size()-1];            // We'll be matching against what we've typed so far

         else
         {
            partial = "";
            return false; // Remove this line if you want to enable username completion for strings that end with a space
                           // or for empty chat lines
                           // for example: "hello " (cycling through all usernames)
         }

         const string *entry = mLineEditor.getStringPtr();
         static Vector<string> names;

         makePlayerNameList(getGame(), names);

         // If the command string has quotes in it, use the last space up to the first quote
         std::size_t lastChar = string::npos;
         if(entry->find_first_of("\"") != string::npos)
            lastChar = entry->find_first_of("\"");

         string appender = "";

         std::size_t pos = entry->find_last_of(' ', lastChar) + 1;
         if (pos == string::npos)
            pos = 0;

         mLineEditor.completePartial(&names, partial, pos, appender);
      }
   }
   else
      return mLineEditor.handleKey(inputCode);

   return true;
}


// Recall earlier message/command
void ChatHelper::upArrowPressed()
{
   if(mHistoryIndex > 0)
   {
      mHistory[mHistoryIndex] = mLineEditor.getString();       // Save any edits we've made to this line
      mHistoryIndex--;
      mLineEditor.setString(mHistory[mHistoryIndex]);
   }
}


// Recall more recent message/command
void ChatHelper::downArrowPressed()
{
   if(mHistoryIndex < mHistory.size() - 1)
   {
      mHistory[mHistoryIndex] = mLineEditor.getString();      // Save any edits we've made to this line
      mHistoryIndex++;
      mLineEditor.setString(mHistory[mHistoryIndex]);
   }
}


const char *ChatHelper::getChatMessage() const
{
   return mLineEditor.c_str();
}


static void makeCommandCandidateList()
{
   for(S32 i = 0; i < ChatHelper::chatCmdSize; i++)
      commandCandidateList.push_back(chatCmds[i].cmdName);

   commandCandidateList.sort(alphaSort);
}


void ChatHelper::onTextInput(char ascii)
{
   // Pass the key on to the console for processing
   if(gConsole.onKeyDown(ascii))
      return;

   // Make sure we have a chat box open
   if(mCurrentChatType != NoChat)
      // Append any keys to the chat message
      if(ascii)
         // Protect against crashes while game is initializing (because we look at the ship for the player's name)
         if(getGame()->getConnectionToServer())     // getGame() cannot return NULL here
            mLineEditor.addChar(ascii);
}


// User has finished entering a chat message and pressed <enter>
void ChatHelper::issueChat()
{
   TNLAssert(mCurrentChatType != NoChat, "Not in chat mode!");

   if(!mLineEditor.isEmpty())
   {
      // Check if chat buffer holds a message or a command
      if(isCmdChat())    // It's a command
         runCommand(getGame(), mLineEditor.c_str());
      else               // It's a chat message
      {
         getGame()->sendChat(mCurrentChatType == GlobalChat, mLineEditor.c_str());   // Broadcast message
         
         // Player has demonstrated ability to send messages
         getGame()->getUIManager()->removeInlineHelpItem(HowToChatItem, true);
      }

      // Manage command history  --> should we only store /commands in here?  Currently saves every issued chat
      string trimmed = trim(mLineEditor.getString());
      if(mHistory.size() > 1 && trimmed == mHistory[mHistory.size() - 2])     // Don't double up on strings in the history
         mHistory[mHistory.size() - 1] = "";    
      else if(trimmed != "")                                                  // Don't store empty or whitespace strings
         mHistory[mHistory.size() - 1] = trimmed;
      mHistoryIndex = mHistory.size();
   }

   exitHelper();     // Hide chat display
}


CommandInfo *ChatHelper::getCommandInfo(const char *command)
{
   for(S32 i = 0; i < ChatHelper::chatCmdSize; i++)
      if(!stricmp(chatCmds[i].cmdName.c_str(), command))
         return &chatCmds[i];

   return NULL;
}


// Process a command entered at the chat prompt
// Returns true if command was handled (even if it was bogus); returning false will cause command to be passed on to the server
// Runs on client; returns true unless we don't want to undelay a delayed spawn when command is entered
// Static method
void ChatHelper::runCommand(ClientGame *game, const char *input)
{
   Vector<string> words = parseStringAndStripLeadingSlash(input); 

   if(words.size() == 0)            // Just in case, must have 1 or more words to check the first word as command
      return;

   GameConnection *gc = game->getConnectionToServer();

   if(!gc)
   {
      game->displayErrorMessage("!!! Not connected to server");
      return;
   }

   for(U32 i = 0; i < ARRAYSIZE(chatCmds); i++)
      if(lcase(words[0]) == chatCmds[i].cmdName)
      {
         (*(chatCmds[i].cmdCallback))(game, words);
         return; 
      }

   serverCommandHandler(game, words);     // Command unknown to client, will pass it on to server

   return;
}


// Use this method when you need to keep client/server compatibility between bitfighter
// versions (e.g. 015 -> 015a)
// If you are working on a new version (e.g. 016), then create an appropriate c2s handler function
// Static method
void ChatHelper::serverCommandHandler(ClientGame *game, const Vector<string> &words)
{
   Vector<StringPtr> args;

   for(S32 i = 1; i < words.size(); i++)
      args.push_back(StringPtr(words[i]));

   game->sendCommand(StringTableEntry(words[0], false), args);
}


// Need to handle the case where you do /idle while spawn delayed... you should NOT exit from spawn delay in that case
void ChatHelper::exitHelper()
{
   Parent::exitHelper();

   mLineEditor.clear();
   getGame()->setBusyChatting(false);
}


bool ChatHelper::isMovementDisabled() const { return true;  }
bool ChatHelper::isChatDisabled()     const { return false; }


};

