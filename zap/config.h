//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _CONFIG_H_
#define _CONFIG_H_

// This file contains definitions of two structs that are used to store our
// INI settings and command line param settings, which are read separately,
// but processed jointly.  Some default values are provided here as well,
// especially for the INI settings...  if the INI is deleted, these defaults
// will be used to rebuild it.

#include "Color.h"      // For Color def
#include "ConfigEnum.h" // For DisplayMode
#include "Settings.h"

#include "tnlTypes.h"
#include "tnlNetStringTable.h"
#include "tnlVector.h"

#include <string>
#include <map>

using namespace std;
using namespace TNL;


namespace Zap
{

extern const char *MASTER_SERVER_LIST_ADDRESS;

////////////////////////////////////
////////////////////////////////////

class GameSettings;
struct CmdLineSettings;


struct FolderManager 
{
   // Constructors
   FolderManager();
   virtual ~FolderManager();

   FolderManager(const string &levelDir, const string &robotDir,    const string &shaderDir, const string &sfxDir,
                 const string &musicDir, const string &iniDir,      const string &logDir,    const string &screenshotDir,
                 const string &luaDir,   const string &rootDataDir, const string &pluginDir, const string &fontsDir,
                 const string &recordDir);

   string levelDir;
   string robotDir;
   string shaderDir;
   string sfxDir;
   string musicDir;
   string iniDir;
   string logDir;
   string screenshotDir;
   string luaDir;
   string rootDataDir;
   string pluginDir;
   string fontsDir;
   string recordDir;

   void resolveDirs(GameSettings *settings);                                  
   void resolveDirs(const string &root);
   void resolveLevelDir(GameSettings *settings);                                 
   string resolveLevelDir(const string &levelDir);

   string findLevelFile(const string &filename) const;
   static string findLevelFile(const string &levelDir, const string &filename);

   Vector<string> getScriptFolderList() const;
   Vector<string> getPluginFolderList() const;
   Vector<string> getHelperScriptFolderList() const;

   string findLevelGenScript(const string &fileName) const;
   string findScriptFile(const string &filename) const;
   string findPlugin(const string &filename) const;
   string findBotFile(const string &filename) const;

   string findShaderFile(const string &filename) const;
};


////////////////////////////////////////
////////////////////////////////////////

class GameSettings;

struct CmdLineSettings
{
   CmdLineSettings();      // Constructor
   virtual ~CmdLineSettings();

   void init();
   
   bool dedicatedMode;     // Will server be dedicated?

   string server;
   string masterAddress;   // Use this master server

   F32 loss;               // Simulate packet loss (0-1)
   U32 lag;                // Simulate server lag (in ms)
   U32 stutter;            // Simulate VPS CPU stutter (0-1000)

   bool forceUpdate;       // For testing updater
   string dedicated;       // Holds bind address specified on cmd line following -dedicated parameter
   string name;
   string password;

   string hostname;
   string hostaddr;        // Address to listen on when we're host (e.g. IP:localhost:1234 or IP:Any:6666 or whatever)
   string hostdescr;       // One-line description of server
   string serverPassword;  // Password required to connect to server
   string adminPassword;   // Password required to perform certain admin functions
   string levelChangePassword;   // Password required to change levels and such

   FolderManager dirs;

   S32 maxPlayers;

   DisplayMode displayMode;    // Fullscreen param supplied

   S32 winWidth;
   S32 xpos;
   S32 ypos;

   Vector<string> specifiedLevels;
};


////////////////////////////////////////
////////////////////////////////////////

// Keep track of which keys editor plugins have been bound to
struct PluginBinding
{
   string key;          // Key user presses
   string script;       // Plugin script that gets run
   string help;         // To be shown in help
};


////////////////////////////////////////
////////////////////////////////////////

// For holding user-specific settings
struct UserSettings
{
   // Not really an enum at the moment...
   enum ExperienceLevels {
      // 0-20, 20-50, 50-100, 100-200, 200-500, 500-1000, 1000-2000, 2000-5000, 5000+
      LevelCount = 9
   };

   UserSettings();      // Constructor
   ~UserSettings();     // Destructor

   string name;
   bool levelupItemsAlreadySeen[LevelCount];
};


////////////////////////////////////////
////////////////////////////////////////

class CIniFile;
class InputCodeManager;

struct IniSettings      // With defaults specified
{
private:
   F32 musicVolLevel;   // Use getter/setter!

public:
   IniSettings();       // Constructor
   virtual ~IniSettings();

   Settings mSettings;

   DisplayMode oldDisplayMode;
   bool alwaysStartInKeyboardMode;

   F32 sfxVolLevel;                 // SFX volume (0 = silent, 1 = full bore)
   F32 voiceChatVolLevel;           // Ditto
   F32 alertsVolLevel;              // And again

   F32 getMusicVolLevel();
   F32 getRawMusicVolLevel();

   void setMusicVolLevel(F32 vol);

   Vector<PluginBinding> getDefaultPluginBindings() const;

   bool diagnosticKeyDumpMode;      // True if want to dump keystrokes to the screen

   bool allowGetMap;                // allow '/GetMap' command
   bool allowDataConnections;       // Specify whether data connections are allowed on this computer

   U32 maxDedicatedFPS;
   U32 maxFPS;


   string masterAddress;            // Default address of our master server
   string name;                     // Player name (none by default)    
   string password;                 // Player password (none by default) 
   string defaultName;              // Name used if user hits <enter> on name entry screen
   string lastPassword;
   string lastEditorName;           // Name of file most recently edited by the user

   string hostname;                 // Server name when in host mode
   string hostaddr;                 // User-specified address/port of server
   string hostdescr;                // One-line description of server
   string welcomeMessage;
   string serverPassword;
   string ownerPassword;
   string adminPassword;
   string levelChangePassword;      // Password to allow access to level changing functionality on non-local server
   string levelDir;                 // Folder where levels are stored, by default
   S32 maxPlayers;                  // Max number of players that can play on local server
   S32 maxBots;
   bool playWithBots;               // Should the server add bots
   S32 minBalancedPlayers;          // If bot auto-balance, make sure there are at least this many players
   bool enableServerVoiceChat;      // No voice chat allowed in server if disabled
   bool allowTeamChanging;
   bool enableGameRecording;
   bool kickIdlePlayers;

   S32 connectionSpeed;

   bool randomLevels;
   bool skipUploads;

   bool allowMapUpload;
   bool allowAdminMapUpload;
   bool allowLevelgenUpload;

   bool voteEnable;
   U32 voteLength;
   U32 voteLengthToChangeTeam;
   U32 voteRetryLength;
   S32 voteYesStrength;
   S32 voteNoStrength;
   S32 voteNothingStrength;

   bool useUpdater;                 // Use updater system (windows only)

   // Server display settings in join menu
   S32 queryServerSortColumn;
   bool queryServerSortAscending;
      
   Vector<PluginBinding> pluginBindings;  // Keybindings for the editor plugins

   // Game window location when in windowed mode
   S32 winXPos;
   S32 winYPos;
   F32 winSizeFact;

   bool musicMutedOnCmdLine;

   // Testing values
   bool neverConnectDirect;
   Color wallFillColor;
   Color wallOutlineColor;
   U16 clientPortNumber;
   bool disableScreenSaver;

   // Logging options   --   true will enable logging these events, false will disable
   bool logConnectionProtocol;
   bool logNetConnection;
   bool logEventConnection;
   bool logGhostConnection;
   bool logNetInterface;
   bool logPlatform;
   bool logNetBase;
   bool logUDP;

   bool logFatalError;        
   bool logError;             
   bool logWarning;    
   bool logConfigurationError;
   bool logConnection;        
   bool logLevelLoaded;    
   bool logLevelError;
   bool logLuaObjectLifecycle;
   bool luaScriptMessage;
   bool serverFilter;  
   bool logStats;

   string mySqlStatsDatabaseServer;
   string mySqlStatsDatabaseName;
   string mySqlStatsDatabaseUser;
   string mySqlStatsDatabasePassword;

   string defaultRobotScript;
   string globalLevelScript;

   Vector<StringTableEntry> levelList;

   Vector<string> reservedNames;
   Vector<string> reservedPWs;

   U32 version;

   Vector<string> prevServerListFromMaster;
   Vector<string> alwaysPingList;

   // Some static methods for converting between bit arrays and INI friendly strings
   static void clearbits(bool *items, S32 itemCount);
   static string bitArrayToIniString(const bool *items, S32 itemCount);
   static void iniStringToBitArray(const string &vals, bool *items, S32 itemCount);

   static void loadUserSettingsFromINI(CIniFile *ini, GameSettings *settings);    // Load user-specific settings
   static void saveUserSettingsToINI(const string &name, CIniFile *ini, GameSettings *settings);
};


void saveSettingsToINI  (CIniFile *ini, GameSettings *settings);
void loadSettingsFromINI(CIniFile *ini, GameSettings *settings);    // Load standard game settings

void writeSkipList(CIniFile *ini, const Vector<string> *levelSkipList);

};

#endif


