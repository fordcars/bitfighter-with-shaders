//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _MENU_ITEMS_H_
#define _MENU_ITEMS_H_

#include "teamInfo.h"      // For Team def
#include "lineEditor.h"
#include "Color.h"

#include <string>


namespace Zap
{
class Game;

using namespace std;

enum MenuItemTypes {
   MenuItemType,
   ToggleMenuItemType,
   CounterMenuItemType,
   TimeCounterMenuItemType,
   TextEntryMenuItemType,   
   PlayerMenuItemType,
   TeamMenuItemType,
   FloatCounterMenuItemType
};

enum PlayerType {
   PlayerTypePlayer,
   PlayerTypeAdmin,
   PlayerTypeRobot,
   PlayerTypeIrrelevant
};

enum MenuItemSize {
   MENU_ITEM_SIZE_SMALL,
   MENU_ITEM_SIZE_NORMAL
};

class ClientGame;
class MenuUserInterface;

////////////////////////////////////
////////////////////////////////////

class MenuItem
{
private:
   S32 mIndex;
   MenuItemSize mMenuItemSize;
   virtual void initialize();

protected:
   string mDisplayVal;     // Text displayed on menu
   string mHelp;           // An optional help string
      
   MenuUserInterface *mMenu;

   Color mSelectedColor;
   Color mUnselectedColor;

   bool mEnterAdvancesItem;
   void (*mCallback)(ClientGame *, U32);

   const char *mDisplayValAppendage;      // Typically the ">" that is appended to menu items

   virtual string getUnits() const;

public:
   // Constructors
   MenuItem(lua_State *L = NULL);         // Combined default C++ / Lua constructor  ==> used at all?
   MenuItem(const string &displayVal);
   MenuItem(const string &displayVal, void (*callback)(ClientGame *, U32), const string &help, 
            InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);
   MenuItem(S32 index, const string &prompt, void (*callback)(ClientGame *, U32), const string &help, 
            InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);

   virtual ~MenuItem();       // Destructor

   InputCode key1;            // Allow two shortcut keys per menu item...
   InputCode key2;

   virtual MenuItemTypes getItemType();

   virtual void render(S32 ypos, S32 textsize, bool isSelected);              // Renders item horizontally centered on screen
   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);    // Renders item horizontally centered on xpos
   virtual S32 getWidth(S32 textsize);

   const Color *getColor(bool isSelected);

   S32 getIndex();      // Only used once...  TODO: Get rid of this, and perhaps user-assigned indices altogether

   const string getHelp();
   void setHelp(string help);

   MenuUserInterface *getMenu();
   void setMenu(MenuUserInterface *menu);

   string getPrompt() const;

   virtual void setSecret(bool secret);

   // When enter is pressed, should selection advance to the next item?
   virtual void setEnterAdvancesItem(bool enterAdvancesItem);

   MenuItemSize getSize();
   void setSize(MenuItemSize size);
   
   virtual const char *getSpecialEditingInstructions();
   virtual S32 getIntValue() const;
   virtual string getValueForWritingToLevelFile() const;
   virtual string getValue() const;      // Basic menu item returns its text when selected... overridden by other types
   virtual void setValue(const string &val);
   virtual void setIntValue(S32 val);

   virtual bool handleKey(InputCode inputCode);
   virtual void handleTextInput(char ascii);
   virtual void setFilter(LineEditorFilter filter);
   virtual void activatedWithShortcutKey();

   virtual bool enterAdvancesItem();

   // Some menu items have text input that require that key presses like KEY_SPACE and
   // shortcut keys be be turned off (i.e. not processed as a key press)
   virtual bool hasTextInput();

   void setSelectedColor(const Color &color);
   void setUnselectedColor(const Color &color);
   virtual void setSelectedValueColor(const Color &color);
   virtual void setUnselectedValueColor(const Color &color);

   ///// Lua interface
   // Top level Lua methods
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(MenuItem);

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

// Used to jam a message into a menu structure... currently used to show the "waiting for server to allow you to switch teams" msg
class MessageMenuItem : public MenuItem
{
public:
   MessageMenuItem(string displayVal, const Color &color);
   virtual ~MessageMenuItem();
};


////////////////////////////////////////
////////////////////////////////////////

// Parent class for all things that have both a name and a value, i.e. anything that's not a regular menuItem
// User provides typed input, value is returned as a string
// Provides some additional functionality; not used directly
class ValueMenuItem : public MenuItem
{
   typedef MenuItem Parent;

private:
   virtual void initialize();

protected:
   Color mSelectedValueColor;       // Color of value when selected
   Color mUnselectedValueColor;     // Color of value when unselected

   const Color *getValueColor(bool isSelected);
   void setSelectedValueColor(const Color &color);
   void setUnselectedValueColor(const Color &color);

public:
   ValueMenuItem(const string &displayValue, void (*callback)(ClientGame *, U32), const string &help, InputCode k1, InputCode k2);
   virtual ~ValueMenuItem();
};


////////////////////////////////////////
////////////////////////////////////////

class ToggleMenuItem : public ValueMenuItem
{
   typedef ValueMenuItem Parent;

private:
   string getOptionText() const;    // Helper function
   
protected:
   U32 mIndex;
   bool mWrap;

public:
   ToggleMenuItem(string title, Vector<string> options, U32 currOption, bool wrap, 
                  void (*callback)(ClientGame *, U32), const string &help, InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);
   virtual ~ToggleMenuItem();  // Destructor

   virtual MenuItemTypes getItemType();
   virtual const char *getSpecialEditingInstructions();
   virtual S32 getValueIndex() const;
   virtual void setValueIndex(U32 index);
   virtual S32 getIntValue() const;
   virtual void setIntValue(S32 value);
   
   virtual string getValue() const;

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
   virtual S32 getWidth(S32 textsize);

   virtual bool handleKey(InputCode inputCode);
   virtual void handleTextInput(char ascii);

   virtual void activatedWithShortcutKey();

   Vector<string> mOptions;

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(ToggleMenuItem);
   explicit ToggleMenuItem(lua_State *L);      // Constructor called from Lua

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

class YesNoMenuItem : public ToggleMenuItem
{
   typedef ToggleMenuItem Parent;

private:
   virtual void initialize();
   void setIndex(S32 index);     // Sets mIndex, but with range checking

public:
   YesNoMenuItem(string title, bool currOption, const string &help, InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);
   virtual ~YesNoMenuItem();  // Destructor

   virtual string getValueForWritingToLevelFile() const;
   virtual void setValue(const string &val);
   virtual S32 getIntValue() const;
   virtual void setIntValue(S32 value);

   /////// Lua Interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(YesNoMenuItem);
   explicit YesNoMenuItem(lua_State *L);      // Constructor called from Lua

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];

};

////////////////////////////////////////
////////////////////////////////////////

class CounterMenuItem : public ValueMenuItem
{
   typedef ValueMenuItem Parent;

private:
   virtual string getOptionText() const;     // Helper function, overridden in TimeCounterMenuItem
   virtual void initialize();

protected:
   S32 mValue;
   S32 mStep;
   S32 mMinValue;
   S32 mMaxValue;
   string mUnits;
   string mMinMsg;

   virtual void increment(S32 fact = 1); 
   virtual void decrement(S32 fact = 1);
   virtual S32 getBigIncrement();    // How much our counter is incremented when shift is down (multiplier)
   virtual void backspace();
   virtual void enterDigit(S32 digit);
   virtual string getUnits() const;

public:
   CounterMenuItem(const string &title, S32 value, S32 step, S32 minVal, S32 maxVal, 
                   const string &units, const string &minMsg, 
                   const string &help, InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);

   virtual ~CounterMenuItem();  // Destructor

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
   virtual S32 getWidth(S32 textsize);

   virtual MenuItemTypes getItemType();
   virtual S32 getIntValue() const;
   virtual void setValue(const string &val);
   virtual void setIntValue(S32 val);
   virtual string getValue() const;
   virtual const char *getSpecialEditingInstructions();
   virtual bool handleKey(InputCode inputCode);

   virtual void snap();

   virtual void activatedWithShortcutKey();

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(CounterMenuItem);
   explicit CounterMenuItem(lua_State *L);      // Constructor called from Lua

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};

////////////////////////////////////////
////////////////////////////////////////

class FloatCounterMenuItem : public ValueMenuItem
{
   typedef ValueMenuItem Parent;

private:
   virtual string getOptionText();     // Helper function, overridden in TimeCounterMenuItem
   virtual void setFloatValue(F32 val);// Helper to always clamp value
   virtual void initialize();

protected:
   F32 mValue;
   F32 mStep;
   F32 mMinValue;
   F32 mMaxValue;
   S32 mDecimalPlaces;
   S32 mPrecision;
   string mUnits;
   string mMinMsg;

   virtual void increment(S32 fact = 1);
   virtual void decrement(S32 fact = 1);
   virtual F32 getBigIncrement();    // How much our counter is incremented when shift is down (multiplier)
   virtual void backspace();
   virtual void enterDigit(S32 digit);

public:
   FloatCounterMenuItem(const string &title, F32 value, F32 step, F32 minVal, F32 maxVal, S32 decimalPlaces,
                   const string &units, const string &minMsg,
                   const string &help, InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);

   virtual ~FloatCounterMenuItem();  // Destructor

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
   virtual S32 getWidth(S32 textsize);

   virtual MenuItemTypes getItemType();
   virtual S32 getIntValue() const;
   virtual void setIntValue(S32 val);
   virtual string getValue() const;
   virtual void setValue(const string &val);
   virtual const char *getSpecialEditingInstructions();
   virtual bool handleKey(InputCode inputCode);

   virtual string getUnits() const;

   virtual void snap();

   virtual void activatedWithShortcutKey();

   ///// Lua interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(FloatCounterMenuItem);
   explicit FloatCounterMenuItem(lua_State *L);      // Constructor called from Lua

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};


////////////////////////////////////////
////////////////////////////////////////

class TimeCounterMenuItem : public CounterMenuItem
{
   typedef CounterMenuItem Parent;

private:
   string getOptionText() const;
   bool mEditingSeconds;

protected:
   virtual S32 getBigIncrement();
   virtual void backspace();
   virtual void enterDigit(S32 digit);
   virtual string getUnits() const;

public:
   TimeCounterMenuItem(const string &title, S32 value, S32 maxVal, const string &zeroMsg, const string &help, 
                       S32 step = 5, InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);
   virtual ~TimeCounterMenuItem();

   virtual bool handleKey(InputCode inputCode);

   virtual MenuItemTypes getItemType();
   virtual void setValue (const string &val);
   virtual string getValueForWritingToLevelFile() const;
};


////////////////////////////////////////
////////////////////////////////////////

// As TimeCounterMenuItem, but reporting time in seconds, and with increments of 1 second, rather than 5
class TimeCounterMenuItemSeconds : public TimeCounterMenuItem
{
protected:
   virtual S32 getBigIncrement();

public:
   TimeCounterMenuItemSeconds(const string &title, S32 value, S32 maxVal, const string &zeroMsg, const string &help, 
                              InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);
   virtual ~TimeCounterMenuItemSeconds();

   virtual void setValue (const string &val);
   virtual string getValueForWritingToLevelFile() const;

   virtual void snap();
};

////////////////////////////////////
////////////////////////////////////

class TextEntryMenuItem : public ValueMenuItem
{
   typedef ValueMenuItem Parent;

private:
   string mEmptyVal;
   string getOptionText() const;    // Helper function

   virtual void initialize();

protected:
      LineEditor mLineEditor;
      void (*mTextEditedCallback)(string, BfObject *);

public:
   // Contstuctor
   TextEntryMenuItem(const string &title, const string &val, const string &emptyVal, const string &help, U32 maxLen, 
                     InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);

   virtual ~TextEntryMenuItem();  // Destructor

   virtual MenuItemTypes getItemType();

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
   virtual S32 getWidth(S32 textsize);

   virtual bool handleKey(InputCode inputCode);
   virtual void handleTextInput(char ascii);

   LineEditor *getLineEditor();
   void setLineEditor(LineEditor editor);

   virtual string getValueForWritingToLevelFile() const;

   virtual string getValue() const;
   void setValue(const string &val);

   virtual void setFilter(LineEditorFilter filter);

   virtual void activatedWithShortcutKey();

   virtual void setTextEditedCallback(void (*callback)(string, BfObject *));

   virtual void setSecret(bool secret);

   virtual bool hasTextInput();

   /////// Lua Interface
   LUAW_DECLARE_CLASS_CUSTOM_CONSTRUCTOR(TextEntryMenuItem);
   explicit TextEntryMenuItem(lua_State *L);      // Constructor called from Lua

   static const char *luaClassName;
   static const luaL_Reg luaMethods[];
   static const LuaFunctionProfile functionArgs[];
};

////////////////////////////////////
////////////////////////////////////

class MaskedTextEntryMenuItem : public TextEntryMenuItem
{
   typedef TextEntryMenuItem Parent;

public:
   MaskedTextEntryMenuItem(string title, string val, string emptyVal, const string &help, U32 maxLen, 
                          InputCode k1 = KEY_UNKNOWN, InputCode k2 = KEY_UNKNOWN);
   virtual ~MaskedTextEntryMenuItem();
};

////////////////////////////////////
////////////////////////////////////

class SimpleTextEntryMenuItem : public TextEntryMenuItem
{
   typedef TextEntryMenuItem Parent;

private:
   bool mHasError;

public:
   SimpleTextEntryMenuItem(string title, U32 length, void (*callback)(ClientGame *, U32));
   virtual ~SimpleTextEntryMenuItem();

   void setHasError(bool hasError);

   virtual bool handleKey(InputCode inputCode);

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
};



////////////////////////////////////
////////////////////////////////////

class PlayerMenuItem : public MenuItem
{
private:
   PlayerType mType;                // Type of player, for name menu
   string getOptionText() const;    // Helper function

public:
   // Constructor
   PlayerMenuItem(S32 index, const char *text, void (*callback)(ClientGame *, U32), InputCode k1, PlayerType type);
   virtual ~PlayerMenuItem();

   virtual MenuItemTypes getItemType();

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
   virtual S32 getWidth(S32 textsize);

   virtual void activatedWithShortcutKey();
};


////////////////////////////////////
////////////////////////////////////

class TeamMenuItem : public MenuItem
{
private:
   AbstractTeam *mTeam;
   bool mIsCurrent;                 // Is this a player's current team? 
   string getOptionText() const;    // Helper function

public:
   TeamMenuItem(S32 index, AbstractTeam *team, void (*callback)(ClientGame *, U32), InputCode inputCode, bool isCurrent);
   virtual ~TeamMenuItem();

   virtual MenuItemTypes getItemType();

   virtual void render(S32 xpos, S32 ypos, S32 textsize, bool isSelected);
   virtual S32 getWidth(S32 textsize);

   virtual void activatedWithShortcutKey();

};

};
#endif


