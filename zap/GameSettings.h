//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GAME_SETTINGS_H_
#define _GAME_SETTINGS_H_

#include "config.h"
#include "InputCode.h"        // For InputCodeManager def
#include "LevelSource.h"
#include "LoadoutTracker.h"

#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>
#include <map>

using namespace std;
using namespace TNL;

namespace CmdLineParams
{

enum ParamId {
   LOGIN_NAME,
   LOGIN_PASSWORD,
   WINDOW_MODE,
   FULLSCREEN_MODE,
   FULLSCREEN_STRETCH,
   WINDOW_POS,
   WINDOW_WIDTH,
   USE_STICK,
   NO_MUSIC,
   MASTER_ADDRESS,

   DEDICATED,
   HOST_ON_DEDICATED,
   SERVER_PASSWORD,
   OWNER_PASSWORD,
   ADMIN_PASSWORD,
   NO_ADMIN_PASSWORD,
   LEVEL_CHANGE_PASSWORD,
   NO_LEVEL_CHANGE_PASSWORD,
   HOST_NAME,
   HOST_DESCRIPTION,
   MAX_PLAYERS_PARAM,
   HOST_ADDRESS,

   LEVEL_LIST,
   USE_FILE,

   ROOT_DATA_DIR,
   PLUGIN_DIR,
   LEVEL_DIR,
   PLAYLIST_FILE_DIR,
   INI_DIR,
   LOG_DIR,
   SCRIPTS_DIR,
   ROBOT_DIR,
   SHADER_DIR,
   SCREENSHOT_DIR,
   SFX_DIR,
   MUSIC_DIR,
   FONTS_DIR,
   RECORD_DIR,

   SIMULATED_LOSS,
   SIMULATED_LAG,
   SIMULATED_STUTTER,
   FORCE_UPDATE,

   SEND_RESOURCE,
   GET_RESOURCE,
   SHOW_RULES,
   SHOW_LUA_CLASSES,
   HELP,
   VERSION,

   PARAM_COUNT
};

};


using namespace CmdLineParams;

namespace Zap
{

class BanList;
struct PluginBinding;


enum SettingSource {
   INI,
   CMD_LINE,
   DEFAULT
};


class GameSettings
{
   typedef map<string,UserSettings> UserSettingsMap;

private:
   // Some items will be passthroughs to the underlying INI object; however, if a value can differ from the INI setting 
   // (such as when it can be overridden from the cmd line, or is set remotely), then we'll need to store the working value locally.

   string mHostName;             // Server name used when hosting a game (default set in config.h, set in INI or on cmd line)
   string mHostDescr;            // Brief description of host
   string mWelcomeMessage;       // Message displayed to players when they join server (blank to disable)

   string mPlayerName, mPlayerPassword;   // Resolved name/password, either from INI for cmdLine or login screen
   bool mPlayerNameSpecifiedOnCmdLine;

   // Various passwords
   string mServerPassword;
   string mOwnerPassword;
   string mAdminPassword;
   string mLevelChangePassword;

   Vector<string> mLevelSkipList;      // Levels we'll never load, to create a pseudo delete function for remote server mgt  <=== does this ever get loaded???
   static FolderManager *mFolderManager;
   InputCodeManager mInputCodeManager;

   BanList *mBanList;                  // Our ban list

   //CmdLineSettings mCmdLineSettings;
   IniSettings mIniSettings;

   // Store params read from the cmd line
   Vector<string> mCmdLineParams[CmdLineParams::PARAM_COUNT];

   // User settings storage
   UserSettingsMap mUserSettings;

   Vector<string> mMasterServerList;
   bool mMasterServerSpecifiedOnCmdLine;

   // Helper functions:
   // This first lot return the first value following the cmd line parameter cast to various types
   string getString(ParamId paramId) const;
   U32 getU32(ParamId paramId) const;
   F32 getF32(ParamId paramId) const;

   DisplayMode resolveCmdLineSpecifiedDisplayMode();  // Tries to figure out what display mode was specified on the cmd line, if any
      
   Vector<LoadoutTracker> mLoadoutPresets;

   Vector<string> mConfigurationErrors;
   Vector<string> getLevelList(const string &levelDir, bool ignoreCmdLine);    
   Vector<string> getPlaylist();       

public:
   GameSettings();            // Constructor
   virtual ~GameSettings();   // Destructor

   static CIniFile iniFile;
   static CIniFile userPrefs;

   static const S32 LoadoutPresetCount = 6;     // How many presets do we save?

   static const U16 DEFAULT_GAME_PORT = 28000;


   void readCmdLineParams(const Vector<string> &argv);
   void resolveDirs();

   string getHostName();
   void setHostName(const string &hostName, bool updateINI);

   string getHostDescr();
   void setHostDescr(const string &hostDescr, bool updateINI);

   string getWelcomeMessage() const;
   void setWelcomeMessage(const string &welcomeMessage, bool updateINI);

   string getGlobalLevelgenScript() const;
   void setGlobalLevelgenScript(const string& GlobalLevelgenScript);

   string getServerPassword();
   void setServerPassword(const string &ServerPassword, bool updateINI);

   string getOwnerPassword();
   void setOwnerPassword(const string &OwnerPassword, bool updateINI);

   string getAdminPassword();
   void setAdminPassword(const string &AdminPassword, bool updateINI);

   string getLevelChangePassword();
   void setLevelChangePassword(const string &LevelChangePassword, bool updateINI);

   InputCodeManager *getInputCodeManager(); 

   Vector<string> *getLevelSkipList();
   Vector<string> *getSpecifiedLevels();

   void setLoginCredentials(const string &name, const string &password, bool savePassword);


   bool getSpecified(ParamId paramId);                      // Returns true if parameter was present, false if not

   // Variations on generating a list of levels
   Vector<string> getLevelList();                            // Generic, grab a list of levels based on current settings
   Vector<string> getLevelList(const string &levelFolder);   // Grab a list of levels from the specified level folder; ignore anything in the INI

public:
   static S32 UseControllerIndex;  // Which SDL2 controller index are we using

   Vector<string> *getMasterServerList();
   void saveMasterAddressListInIniUnlessItCameFromCmdLine();
   
   static FolderManager *getFolderManager();
   FolderManager getCmdLineFolderManager();    // Return a FolderManager struct populated with settings specified on cmd line

   BanList *getBanList();

   string getHostAddress();
   U32 getMaxPlayers();

   void save();

   IniSettings *getIniSettings();

   void runCmdLineDirectives();

   bool shouldShowNameEntryScreenOnStartup();

   const Color *getWallFillColor() const;
   const Color *getWallOutlineColor() const;

   void setQueryServerSortColumn(S32 column, bool ascending);
   S32  getQueryServerSortColumn();   
   bool getQueryServerSortAscending();


   // Accessor methods
   U32 getSimulatedStutter();
   F32 getSimulatedLoss();
   U32 getSimulatedLag();

   string getDefaultName();

   bool getForceUpdate();

   string getPlayerName();

   void updatePlayerName(const string &name);

   void setAutologin(bool autologin);

   string getPlayerPassword();

   bool isDedicatedServer();

   string getLevelDir(SettingSource source) const;
   string getPlaylistFile();
   bool isUsingPlaylist();
   string getlevelLoc();

   LevelSource *chooseLevelSource(Game *game); // determines what levelsource you want to use

   LoadoutTracker getLoadoutPreset(S32 index);
   void setLoadoutPreset(const LoadoutTracker *preset, S32 index);

   void addConfigurationError(const string &errorMessage);
   Vector<string> getConfigurationErrors();

   // Other methods
   void saveLevelChangePassword(const string &serverName, const string &password);
   void saveAdminPassword(const string &serverName, const string &password);
   void saveOwnerPassword(const string &serverName, const string &password);

   void forgetLevelChangePassword(const string &serverName);
   void forgetAdminPassword(const string &serverName);
   void forgetOwnerPassword(const string &serverName);

   void onFinishedLoading();     // Should be run after INI and cmd line params have been read

   static void getRes(GameSettings *settings, const Vector<string> &words);
   static void sendRes(GameSettings *settings, const Vector<string> &words);
   static void showRules(GameSettings *settings, const Vector<string> &words);
   static void showHelp(GameSettings *settings, const Vector<string> &words);
   static void showVersion(GameSettings *settings, const Vector<string> &words);

   static map<S32,string> DetectedControllerList;   // List of joysticks we found attached to this machine

   // Dealing with saved passwords for servers
   static void saveServerPassword(const string &serverName, const string &password);
   static string getServerPassword(const string &serverName);
   static void deleteServerPassword(const string &serverName);

   bool isLevelOnSkipList(const string &filename) const;
   void addLevelToSkipList(const string &filename);
   void removeLevelFromSkipList(const string &filename);
   void saveSkipList() const;

   // InputCode related
   InputMode getInputMode();

   // In-game help messages
   void setShowingInGameHelp(bool show);
   bool getShowingInGameHelp();

   // User settings
   const UserSettings *addUserSettings(const UserSettings &userSettings);     // Returns pointer to inserted item
   const UserSettings *getUserSettings(const string &name);
};


typedef shared_ptr<GameSettings> GameSettingsPtr;


};


#endif
