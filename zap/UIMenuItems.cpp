//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIMenuItems.h"
#include "UIMenus.h"
#include "UIEditorMenus.h"

#include "DisplayManager.h"    // For DisplayManager::getScreenInfo() stuff
#include "Renderer.h"
#include "FontManager.h"

#include "LuaWrapper.h"
#include "LuaScriptRunner.h"

#include "Colors.h"

#include "stringUtils.h"
#include "RenderUtils.h"

#include <cmath>

namespace Zap
{

// Combined default C++ / Lua constructor
MenuItem::MenuItem(lua_State *L)
{
   initialize();
}


// Constructor
MenuItem::MenuItem(const string &displayVal)
{
   initialize();

   mDisplayVal = displayVal;
}


// Constructor
MenuItem::MenuItem(const string &displayVal, void (*callback)(ClientGame *, U32), const string &help, InputCode k1, InputCode k2)
{
   initialize();

   mDisplayVal = displayVal;
   mCallback = callback;
   mHelp = help;
   key1 = k1;
   key2 = k2;
}


// Constructor
MenuItem::MenuItem(S32 index, const string &displayVal, void (*callback)(ClientGame *, U32), 
                   const string &help, InputCode k1, InputCode k2)
{
   initialize();

   mDisplayVal = displayVal;
   mCallback = callback;
   mHelp = help;

   key1 = k1;
   key2 = k2;
   mIndex = (U32)index;
}


void MenuItem::initialize()
{
   mDisplayVal = "";
   key1 = KEY_UNKNOWN;
   key2 = KEY_UNKNOWN;
   mCallback = NULL;
   mHelp = "";
   mIndex = -1;
   mMenuItemSize = MENU_ITEM_SIZE_NORMAL;

   mEnterAdvancesItem = false;
   mSelectedColor = Colors::yellow;
   mUnselectedColor = Colors::white;
   mDisplayValAppendage = " >";

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
MenuItem::~MenuItem() 
{ 
   LUAW_DESTRUCTOR_CLEANUP;
} 


MenuItemTypes MenuItem::getItemType()
{
   return MenuItemType;
}


void MenuItem::setSize(MenuItemSize size)
{
   mMenuItemSize = size;
}


MenuItemSize MenuItem::getSize()
{
   return mMenuItemSize;
}


MenuUserInterface *MenuItem::getMenu()  
{ 
   return mMenu; 
}


S32 MenuItem::getIndex()
{
   return mIndex;
}


string MenuItem::getPrompt() const
{
   return mDisplayVal;
}


string MenuItem::getUnits() const
{
   return "";
}


void MenuItem::setSecret(bool secret)
{
   /* Do nothing */
}


const string MenuItem::getHelp()
{
   return mHelp;
}


void MenuItem::setHelp(string help)
{
   mHelp = help;
}


void MenuItem::setMenu(MenuUserInterface *menu) 
{ 
   mMenu = menu; 
}


// Shouldn't need to be overridden -- all redering routines should include xpos
void MenuItem::render(S32 ypos, S32 textsize, bool isSelected)
{
   render(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, ypos, textsize, isSelected);
}


const Color *MenuItem::getColor(bool isSelected)
{
   return isSelected ? &mSelectedColor : &mUnselectedColor;
}


void MenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   Renderer::get().setColor(*getColor(isSelected));

   FontManager::pushFontContext(MenuContext);
      drawCenteredStringf(xpos, ypos, textsize, "%s%s", getPrompt().c_str(), mDisplayValAppendage);
   FontManager::popFontContext();
}


S32 MenuItem::getWidth(S32 textsize)
{
   return getStringWidthf(textsize, "%s%s", getPrompt().c_str(), mDisplayValAppendage);
}


bool MenuItem::handleKey(InputCode inputCode)
{
   if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER ||
         inputCode == KEY_SPACE || inputCode == KEY_RIGHT || inputCode == MOUSE_LEFT)
   {
      UserInterface::playBoop();
      if(mCallback)
         mCallback(getMenu()->getGame(), mIndex);

      return true;
   }
   else
   {
      // Check individual entries for any shortcut keys
      return false;
   }
}


void MenuItem::handleTextInput(char ascii)
{
   // Do nothing
}


void MenuItem::setEnterAdvancesItem(bool enterAdvancesItem)
{
   mEnterAdvancesItem = enterAdvancesItem;
}


// Default implementations will be overridden by child classes
const char *MenuItem::getSpecialEditingInstructions() { return ""; } 
S32         MenuItem::getIntValue() const             { return 0;  }


void MenuItem::setValue(const string &val)        { /* Do nothing */ }
void MenuItem::setIntValue(S32 val)               { /* Do nothing */ }
void MenuItem::setFilter(LineEditorFilter filter) { /* Do nothing */ }


string MenuItem::getValueForWritingToLevelFile() const
{
   return itos(getIntValue());
}


string MenuItem::getValue() const
{
   return mDisplayVal;
}


void MenuItem::activatedWithShortcutKey()
{
   handleKey(MOUSE_LEFT);
}


bool MenuItem::enterAdvancesItem()
{
   return mEnterAdvancesItem;
}


bool MenuItem::hasTextInput()
{
   return false;
}


void MenuItem::setSelectedColor(const Color &color)
{
   mSelectedColor = color;
}


void MenuItem::setUnselectedColor(const Color &color)
{
   mUnselectedColor = color;
}


void MenuItem::setSelectedValueColor(const Color &color)   { /* Override in children */ }
void MenuItem::setUnselectedValueColor(const Color &color) { /* Override in children */ }


/**
 * @luaclass MenuItem
 * 
 * @brief Simple menu item that calls a method or opens a submenu when selected.
 * 
 * @descr MenuItem is the parent class for all other MenuItems. 
 * 
 * Currently, you cannot instantiate a MenuItem from Lua, though you can
 * instatiate MenuItem subclasses.
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(MenuItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(MenuItem, LUA_METHODS);

#undef LUA_METHODS


const char *MenuItem::luaClassName = "MenuItem";
REGISTER_LUA_CLASS(MenuItem);


////////////////////////////////////
////////////////////////////////////


MessageMenuItem::MessageMenuItem(string displayVal, const Color &color) : MenuItem(displayVal)
{
   mDisplayValAppendage = "";
   mUnselectedColor = color;
}

// Destructor
MessageMenuItem::~MessageMenuItem()
{
   // Do nothing
}


////////////////////////////////////
////////////////////////////////////


// Constructor
ValueMenuItem::ValueMenuItem(const string &displayValue, void (*callback)(ClientGame *, U32),
                             const string &help, InputCode k1, InputCode k2) :
      Parent(displayValue, callback, help, k1, k2)
{
   initialize();
}

// Destructor
ValueMenuItem::~ValueMenuItem()
{
   // Do nothing
}


void ValueMenuItem::initialize()
{
   mSelectedValueColor = Colors::cyan;
   mUnselectedValueColor = Colors::cyan;
}


S32 clamp(S32 val, S32 min, S32 max)
{
   if(val < min) return min;
   if(val > max) return max;
   return val;
}


F32 clamp(F32 val, F32 min, F32 max)
{
   if(val < min) return min;
   if(val > max) return max;
   return val;
}


const Color *ValueMenuItem::getValueColor(bool isSelected)
{
   return isSelected ? &mSelectedValueColor : &mUnselectedValueColor;
}


void ValueMenuItem::setSelectedValueColor(const Color &color)
{
   mSelectedValueColor = color;
}


void ValueMenuItem::setUnselectedValueColor(const Color &color)
{
   mUnselectedValueColor = color;
}


////////////////////////////////////
////////////////////////////////////

// Constructor
ToggleMenuItem::ToggleMenuItem(string title, Vector<string> options, U32 currOption, bool wrap, 
                               void (*callback)(ClientGame *, U32), const string &help, InputCode k1, InputCode k2) :
      ValueMenuItem(title, callback, help, k1, k2)
{
   mOptions = options;
   mIndex = clamp(currOption, 0, mOptions.size() - 1);
   mWrap = wrap;
   mEnterAdvancesItem = true;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


// Destructor
ToggleMenuItem::~ToggleMenuItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


string ToggleMenuItem::getOptionText() const
{
   return mIndex < U32(mOptions.size()) ? mOptions[mIndex] : "INDEX OUT OF RANGE";
}


void ToggleMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   drawCenteredStringPair(xpos, ypos, textsize, MenuContext, InputContext, *getColor(isSelected), *getValueColor(isSelected),
                          getPrompt().c_str(), getOptionText().c_str());
}


S32 ToggleMenuItem::getWidth(S32 textsize)
{
   return getStringPairWidth(textsize, MenuContext, InputContext, getPrompt().c_str(), getOptionText().c_str());
}


bool ToggleMenuItem::handleKey(InputCode inputCode)
{
   U32 nextValAfterWrap = mWrap ? 0 : mIndex;

   if(inputCode == KEY_RIGHT || inputCode == MOUSE_LEFT || inputCode == MOUSE_WHEEL_DOWN)
   {
      mIndex = (mIndex == (U32)mOptions.size() - 1) ? nextValAfterWrap : mIndex + 1;

      if(mCallback)
         mCallback(getMenu()->getGame(), mIndex);

      UserInterface::playBoop();
      return true;
   }
   else if(inputCode == KEY_LEFT || inputCode == MOUSE_RIGHT || inputCode == MOUSE_WHEEL_UP)
   {      
      U32 nextValAfterWrap = mWrap ? mOptions.size() - 1 : mIndex;
      mIndex = (mIndex == 0) ? nextValAfterWrap : mIndex - 1;
      
      if(mCallback)
         mCallback(getMenu()->getGame(), mIndex);

      UserInterface::playBoop();
      return true;
   }

   else if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER || inputCode == KEY_SPACE)
   {
      mIndex = (mIndex == (U32)mOptions.size() - 1) ? nextValAfterWrap : mIndex + 1;

      if(mCallback)
         mCallback(getMenu()->getGame(), mIndex);

      UserInterface::playBoop();
      return true;
   }

   return false;
}


void ToggleMenuItem::handleTextInput(char ascii)
{
   if(ascii)     // Check for the first key of a menu entry.
      for(S32 i = 0; i < mOptions.size(); i++)
      {
         S32 index = (i + mIndex + 1) % mOptions.size();
         if(tolower(ascii) == tolower(mOptions[index].data()[0]))
         {
            mIndex = index;

            if(mCallback)
               mCallback(getMenu()->getGame(), mIndex);

            UserInterface::playBoop();
            return;
         }
      }
}


void ToggleMenuItem::activatedWithShortcutKey()
{
   /* Do nothing */
}


// Pulls values out of the table at specified index as strings, and puts them all into strings vector
static void getStringVectorFromTable(lua_State *L, S32 index, const char *methodName, Vector<string> &strings)
{
   strings.clear();

   if(!lua_istable(L, index))
   {
      char msg[256];
      dSprintf(msg, sizeof(msg), "%s expected table arg (which I wanted to convert to a string vector) at position %d", methodName, index);
      logprintf(LogConsumer::LogError, msg);

      THROW_LUA_EXCEPTION(L, msg);
   }

   // The following block loosely based on http://www.gamedev.net/topic/392970-lua-table-iteration-in-c---basic-walkthrough/

   lua_pushvalue(L, index);   // Push our table onto the top of the stack
   lua_pushnil(L);            // lua_next (below) will start the iteration, it needs nil to be the first key it pops

   // The table was pushed onto the stack at -1 (recall that -1 is equivalent to lua_gettop)
   // The lua_pushnil then pushed the table to -2, where it is currently located
   while(lua_next(L, -2))     // -2 is our table
   {
      // Grab the value at the top of the stack
      if(!lua_isstring(L, -1))
      {
         char msg[256];
         dSprintf(msg, sizeof(msg), "%s expected a table of strings -- invalid value at stack position %d, table element %d", 
                                    methodName, index, strings.size() + 1);
         logprintf(LogConsumer::LogError, msg);

         THROW_LUA_EXCEPTION(L, msg);
      }

      strings.push_back(lua_tostring(L, -1));

      lua_pop(L, 1);    // We extracted that value, pop it off so we can push the next element
   }

   // We've got all the elements in the table, so clear it off the stack
   lua_pop(L, 1);
}


MenuItemTypes ToggleMenuItem::getItemType()
{
   return ToggleMenuItemType;
}


const char *ToggleMenuItem::getSpecialEditingInstructions()
{
   return "Use [<-] and [->] keys or mouse wheel to change value.";
}


S32 ToggleMenuItem::getValueIndex() const
{
   return mIndex;
}


void ToggleMenuItem::setValueIndex(U32 index)
{
   mIndex = index;
}


S32 ToggleMenuItem::getIntValue() const
{
   return getValueIndex();
}


void ToggleMenuItem::setIntValue(S32 value)
{
   setValueIndex(value);
}


string ToggleMenuItem::getValue() const
{
   return mOptions[mIndex];
}


//////////
// Lua interface

/**
 * @luaclass ToggleMenuItem
 * 
 * @brief Menu item that lets users choose one of several options.
 * 
 * @luafunc ToggleMenuItem::ToggleMenuItem(string name, table options, int currentIndex, bool wrap, string help)
 * 
 * @param name The text shown on the menu item.
 * @param options The options to be displayed.
 * @param currentIndex The index of the item to be selected initially (1 = first
 * item).
 * @param wrap `true`if the items should wrap around when you reach the last
 * index.
 * @param help A bit of help text.
 * 
 * The MenuItem will return the text of the item the user selected.
 * 
 * For example:
 * 
 * @code
 *   m = ToggleMenuItem.new("Type", { "BarrierMaker", "LoadoutZone", "GoalZone" }, 1, `true`, "Type of item to insert")
 * @endcode
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(ToggleMenuItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(ToggleMenuItem, LUA_METHODS);

#undef LUA_METHODS


const char *ToggleMenuItem::luaClassName = "ToggleMenuItem";
REGISTER_LUA_SUBCLASS(ToggleMenuItem, MenuItem);


// Lua Constructor, called from plugins
ToggleMenuItem::ToggleMenuItem(lua_State *L) : Parent("", NULL, "", KEY_NONE, KEY_NONE)
{
   const char *methodName = "ToggleMenuItem constructor";

   // Required items -- will throw if they are missing or misspecified
   mDisplayVal = getCheckedString(L, 1, methodName);
   getStringVectorFromTable(L, 2, methodName, mOptions);    // Fills mOptions with elements in a table 

   // Optional (but recommended) items
   mIndex = clamp(getInt(L, 3, 1) - 1, 0,  mOptions.size() - 1);   // First - 1 for compatibility with Lua's 1-based array index
   mWrap = getCheckedBool(L, 4, methodName, false);
   mHelp = getString(L, 4, "");

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


////////////////////////////////////
////////////////////////////////////

// Constructors
YesNoMenuItem::YesNoMenuItem(string title, bool currOption, const string &help, InputCode k1, InputCode k2) :
      ToggleMenuItem(title, Vector<string>(), currOption, true, NULL, help, k1, k2)
{
   initialize();

   setIndex(currOption);
}


// Lua Constructor
YesNoMenuItem::YesNoMenuItem(lua_State *L) : Parent("", Vector<string>(), 0, true, NULL, "")
{
   initialize();

   const char *methodName = "YesNoMenuItem constructor";

   // Required items -- will throw if they are missing or misspecified
   mDisplayVal = getCheckedString(L, 1, methodName);

   // Optional (but recommended) items
   setIndex(getInt(L, 2, 1) - 1);                // - 1 for compatibility with Lua's 1-based array index
   mHelp = getString(L, 3, "");
}


// Destructor
YesNoMenuItem::~YesNoMenuItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


string YesNoMenuItem::getValueForWritingToLevelFile() const
{
   return mIndex ? "yes" : "no";
}


void YesNoMenuItem::setValue(const string &val)
{
   mIndex = (val == "yes") ? 1 : 0;
}


S32 YesNoMenuItem::getIntValue() const
{
   return mIndex;
}


void YesNoMenuItem::setIntValue(S32 value)
{
   mIndex = (value == 0) ? 0 : 1;
}


void YesNoMenuItem::initialize()
{
   mEnterAdvancesItem = true;

   mOptions.push_back("No");     // 0
   mOptions.push_back("Yes");    // 1

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


void YesNoMenuItem::setIndex(S32 index)
{
   mIndex = clamp(index, 0, 1);
}


//////////
// Lua interface

/**
 * @luaclass YesNoMenuItem
 * 
 * @brief A specialized ToggleMenuItem prepopulated with Yes and No.
 * 
 * @descr To create a YesNoMenuItem from a plugin, use the following syntax:
 * 
 * @luafunc YesNoMenuItem::YesNoMenuItem(string name, int currentIndex, string help)
 * 
 * @param name The text shown on the menu item.
 * @param currentIndex The index of the item to be selected initially (1 = Yes,
 * 2 = No).
 * @param help A bit of help text.
 * 
 * The YesNoMenuItem will return 1 if the user selected Yes, 2 if No.
 * 
 * For example:
 * 
 * @code
 *   m = YesNoMenuItem.new("Hostile", 1, "Should this turret be hostile?")
 * @endcode
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(YesNoMenuItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(YesNoMenuItem, LUA_METHODS);

#undef LUA_METHODS


const char *YesNoMenuItem::luaClassName = "YesNoMenuItem";
REGISTER_LUA_SUBCLASS(YesNoMenuItem, ToggleMenuItem);


////////////////////////////////////
////////////////////////////////////

// Constructor
CounterMenuItem::CounterMenuItem(const string &title, S32 value, S32 step, S32 minVal, S32 maxVal, const string &units, 
                                 const string &minMsg, const string &help, InputCode k1, InputCode k2) :
   Parent(title, NULL, help, k1, k2)
{
   initialize();

   mStep = step;
   mMinValue = minVal;
   mMaxValue = maxVal;
   mUnits = units;
   mMinMsg = minMsg;   

   setIntValue(value);     // Needs to be done after mMinValue and mMaxValue are set
}


// Destructor
CounterMenuItem::~CounterMenuItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void CounterMenuItem::initialize()
{
   mEnterAdvancesItem = true;
   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


void CounterMenuItem::setValue(const string &val)
{
   setIntValue(atoi(val.c_str()));
}


void CounterMenuItem::setIntValue(S32 val)
{
   mValue = clamp(val, mMinValue, mMaxValue);
}


string CounterMenuItem::getOptionText() const
{
   return (mValue == mMinValue && mMinMsg != "") ? mMinMsg : itos(mValue) + getUnits();
}


void CounterMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   drawCenteredStringPair(xpos, ypos, textsize, MenuContext, InputContext, *getColor(isSelected), *getValueColor(isSelected),
                          getPrompt().c_str(), getOptionText().c_str());
}


S32 CounterMenuItem::getWidth(S32 textsize)
{
   return getStringPairWidth(textsize, MenuContext, InputContext, getPrompt().c_str(), getOptionText().c_str());
}


// Return true if key was handled, false otherwise
bool CounterMenuItem::handleKey(InputCode inputCode)
{
   if(inputCode == KEY_RIGHT || inputCode == MOUSE_LEFT || inputCode == MOUSE_WHEEL_UP)  
   {
      if(InputCodeManager::checkModifier(KEY_SHIFT))
      {
         increment(getBigIncrement());
         snap();
      }
      else
         increment(1);

      return true;
   }
   else if(inputCode == KEY_LEFT || inputCode == MOUSE_RIGHT || inputCode == MOUSE_WHEEL_DOWN)
   {
      if(InputCodeManager::checkModifier(KEY_SHIFT))
      {
         decrement(getBigIncrement());
         snap();
      }
      else
         decrement(1);

      return true;
   }

   else if(inputCode == KEY_BACKSPACE || inputCode == KEY_KEYPAD_PERIOD)
      backspace();

   else if(inputCode >= KEY_0 && inputCode <= KEY_9)
      enterDigit(inputCode - KEY_0);

   else if(inputCode >= KEY_KEYPAD0 && inputCode <= KEY_KEYPAD9)
      enterDigit(inputCode - KEY_KEYPAD0);


   return false;
}


void CounterMenuItem::increment(S32 fact) 
{ 
   setIntValue(mValue + mStep * fact);
}


void CounterMenuItem::decrement(S32 fact) 
{ 
   setIntValue(mValue - mStep * fact);
}


S32 CounterMenuItem::getBigIncrement()
{
   return 10;
}

void CounterMenuItem::backspace()
{
   mValue /= 10;
}


void CounterMenuItem::enterDigit(S32 digit)
{
   if(mValue > S32((U32(S32_MAX) + 9) / 10))
      mValue = S32_MAX;
   else
      mValue *= 10;

   if(mValue + digit < mValue) // Check for overflow
      mValue = S32_MAX;
   else
      mValue += digit;

   if(mValue > mMaxValue)
      mValue = mMaxValue;
}


MenuItemTypes CounterMenuItem::getItemType()
{
   return CounterMenuItemType;
}


S32 CounterMenuItem::getIntValue() const
{
   return mValue;
}


string CounterMenuItem::getValue() const
{
   return itos(mValue);
}


const char *CounterMenuItem::getSpecialEditingInstructions()
{
   return "Use [<-] and [->] keys or mouse wheel to change value. Hold [Shift] for bigger change.";
}


string CounterMenuItem::getUnits() const
{
   if(mUnits == "")
      return "";

   return " " + mUnits;
}


void CounterMenuItem::snap()
{
   // Do nothing 
}


void CounterMenuItem::activatedWithShortcutKey()
{
   // Do nothing 
}


//////////
// Lua interface

/**
 * @luaclass CounterMenuItem
 * 
 * @brief Menu item for entering a numeric value, with increment and decrement
 * controls.
 *
 * @luafunc CounterMenuItem::CounterMenuItem(string name, num startingVal, num step, num minVal, num maxVal, string units, string minText, string help)
 * 
 * @param name The text shown on the menu item.
 * @param startingVal The starting value of the menu item. 
 * @param step The amount by which to increase or decrease the value when the
 * arrow keys are used.
 * @param minVal The minimum allowable value that can be entered. 
 * @param maxVal The maximum allowable value that can be entered. 
 * @param units The units to be shown alongside the numeric item. Pass "" if you
 * don't want to display units.
 * @param minText The text shown on the menu item when the minimum value has
 * been reached. Pass "" to simply display the minimum value. 
 * @param help A bit of help text.
 * 
 * The MenuItem will return the value entered.
 * 
 * For example:
 * 
 * @code
 *   m = CounterMenuItem.new("Wall Thickness", 50, 1, 1, 50, "grid units", "", "Thickness of wall to be created")
 * @endcode
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(CounterMenuItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(CounterMenuItem, LUA_METHODS);

#undef LUA_METHODS


const char *CounterMenuItem::luaClassName = "CounterMenuItem";
REGISTER_LUA_SUBCLASS(CounterMenuItem, MenuItem);


// Lua Constructor, called from scripts
CounterMenuItem::CounterMenuItem(lua_State *L) : Parent("", NULL, "", KEY_NONE, KEY_NONE)
{
   const char *methodName = "CounterMenuItem constructor";

   initialize();

   try
   {
      // Required items -- will throw if they are missing or misspecified
      mDisplayVal = getCheckedString(L, 1, methodName);
      // mValue =  getInt(L, 2, methodName);  ==> set this later, after we've determined mMinValue and mMaxValue

      // Optional (but recommended) items
      mStep =     getInt(L, 3, 1);   
      mMinValue = getInt(L, 4, 0);   
      mMaxValue = getInt(L, 5, 100);   
      mUnits =    getString(L, 6, "");
      mMinMsg =   getString(L, 7, "");
      mHelp =     getString(L, 8, "");

      // Second required item
      setIntValue(getCheckedInt(L, 2, methodName));    // Set this one last so we'll know mMinValue and mMaxValue
   }
   catch(LuaException &e)
   {
      logprintf(LogConsumer::LogError, "Error constructing CounterMenuItem -- please see documentation");
      logprintf(LogConsumer::ConsoleMsg, "Usage: CounterMenuItem(<display val (str)> [step (i)] [min val (i)] [max val (i)] [units (str)] [min msg (str)] [help (str)] <value (int))");
      throw e;
   }
}


////////////////////////////////////
////////////////////////////////////

// Constructor
FloatCounterMenuItem::FloatCounterMenuItem(const string &title,
      F32 value, F32 step, F32 minVal, F32 maxVal, S32 decimalPlaces,
      const string &units, const string &minMsg, const string &help, InputCode k1, InputCode k2) :
   Parent(title, NULL, help, k1, k2)
{
   initialize();

   mStep = step;
   mMinValue = minVal;
   mMaxValue = maxVal;
   mUnits = units;
   mMinMsg = minMsg;
   mDecimalPlaces = decimalPlaces;
   mPrecision = (S32) std::pow((F32)10, mDecimalPlaces);

   setFloatValue(value);     // Needs to be done after mMinValue and mMaxValue are set
}


// Destructor
FloatCounterMenuItem::~FloatCounterMenuItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void FloatCounterMenuItem::initialize()
{
   mEnterAdvancesItem = true;
   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


void FloatCounterMenuItem::setFloatValue(F32 val)
{
   F32 rounded = floor(val * mPrecision + 0.5) / mPrecision;

   mValue = clamp(rounded, mMinValue, mMaxValue);
}


void FloatCounterMenuItem::setValue(const string &val)
{
   setFloatValue(Zap::stof(val));
}


void FloatCounterMenuItem::setIntValue(S32 val)
{
   // This may change your integer to be the min/max
   setFloatValue(F32(val));
}


string FloatCounterMenuItem::getOptionText()
{
   return (mValue == mMinValue && mMinMsg != "") ? mMinMsg : getValue() + " " + getUnits();
}


void FloatCounterMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   drawCenteredStringPair(xpos, ypos, textsize, MenuContext, InputContext, *getColor(isSelected), *getValueColor(isSelected),
                          getPrompt().c_str(), getOptionText().c_str());
}


S32 FloatCounterMenuItem::getWidth(S32 textsize)
{
   return getStringPairWidth(textsize, MenuContext, InputContext, getPrompt().c_str(), getOptionText().c_str());
}


bool FloatCounterMenuItem::handleKey(InputCode inputCode)
{
   if(inputCode == KEY_RIGHT || inputCode == MOUSE_LEFT || inputCode == MOUSE_WHEEL_UP)
   {
      if(InputCodeManager::checkModifier(KEY_SHIFT))
      {
         increment(getBigIncrement());
         snap();
      }
      else
         increment(1);

      return true;
   }
   else if(inputCode == KEY_LEFT || inputCode == MOUSE_RIGHT || inputCode == MOUSE_WHEEL_DOWN)
   {
      if(InputCodeManager::checkModifier(KEY_SHIFT))
      {
         decrement(getBigIncrement());
         snap();
      }
      else
         decrement(1);

      return true;
   }

   else if(inputCode == KEY_BACKSPACE || inputCode == KEY_KEYPAD_PERIOD)
      backspace();

   else if(inputCode >= KEY_0 && inputCode <= KEY_9)
      enterDigit(inputCode - KEY_0);

   else if(inputCode >= KEY_KEYPAD0 && inputCode <= KEY_KEYPAD9)
      enterDigit(inputCode - KEY_KEYPAD0);


   return false;
}


void FloatCounterMenuItem::increment(S32 fact)
{
   setFloatValue(mValue + mStep * fact);
}


void FloatCounterMenuItem::decrement(S32 fact)
{
   setFloatValue(mValue - mStep * fact);
}


F32 FloatCounterMenuItem::getBigIncrement()
{
   return 10.f;
}

void FloatCounterMenuItem::backspace()
{
   setFloatValue(mValue /= 10);
}


void FloatCounterMenuItem::enterDigit(S32 digit)
{
   logprintf("value 1: %f", mValue);
   if(mValue > F32((F64(F32_MAX) + 9) *.1))
      mValue = F32_MAX;
   else
      mValue *= 10;

   logprintf("value 2: %f", mValue);
   if(mValue + digit < mValue) // Check for overflow
      mValue = F32_MAX;
   else
      mValue += (digit / F32(mPrecision));

   logprintf("value 3: %f", mValue);
   if(mValue > mMaxValue)
      mValue = mMaxValue;
}


MenuItemTypes FloatCounterMenuItem::getItemType()
{
   return FloatCounterMenuItemType;
}


S32 FloatCounterMenuItem::getIntValue() const
{
   // This is rounded!  So it may not be what you want!
   return S32(mValue);
}


string FloatCounterMenuItem::getValue() const
{
   return ftos(mValue, mDecimalPlaces);
}


const char *FloatCounterMenuItem::getSpecialEditingInstructions()
{
   return "Use [<-] and [->] keys or mouse wheel to change value. Hold [Shift] for bigger change.";
}


string FloatCounterMenuItem::getUnits() const
{
   return mUnits;
}


void FloatCounterMenuItem::snap()
{
   // Do nothing
}


void FloatCounterMenuItem::activatedWithShortcutKey()
{
   // Do nothing
}


//////////
// Lua interface

/**
 * @luaclass FloatCounterMenuItem
 *
 * @brief Menu item for entering a floating value, with increment and decrement
 * controls.
 *
 * @luafunc FloatCounterMenuItem::FloatCounterMenuItem(string name, num startingVal, num step, num minVal, num maxVal, num decimalPlaces, string units, string minText, string help)
 *
 * @param name The text shown on the menu item.
 * @param startingVal The starting value of the menu item.
 * @param step The amount by which to increase or decrease the value when the
 * arrow keys are used.
 * @param minVal The minimum allowable value that can be entered.
 * @param maxVal The maximum allowable value that can be entered.
 * @param decimalPlaces The number of decimal places of accuracy to use.
 * @param units The units to be shown alongside the numeric item. Pass "" if you
 * don't want to display units.
 * @param minText The text shown on the menu item when the minimum value has
 * been reached. Pass "" to simply display the minimum value.
 * @param help A bit of help text.
 *
 * The MenuItem will return the value entered.
 *
 * For example:
 *
 * @code
 *   m = FloatCounterMenuItem.new("Angle", 1.5, 0.1, 0.1, 100, 3, "radians", "", "Angle of object to rotate")
 * @endcode
 */
//                Fn name                  Param profiles            Profile count
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(FloatCounterMenuItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(FloatCounterMenuItem, LUA_METHODS);

#undef LUA_METHODS


const char *FloatCounterMenuItem::luaClassName = "FloatCounterMenuItem";
REGISTER_LUA_SUBCLASS(FloatCounterMenuItem, MenuItem);


// Lua Constructor, called from scripts
FloatCounterMenuItem::FloatCounterMenuItem(lua_State *L) : Parent("", NULL, "", KEY_NONE, KEY_NONE)
{
   const char *methodName = "FloatCounterMenuItem constructor";

   initialize();

   try
   {
      // Required items -- will throw if they are missing or misspecified
      mDisplayVal = getCheckedString(L, 1, methodName);
      // mValue =  getInt(L, 2, methodName);  ==> set this later, after we've determined mMinValue and mMaxValue

      // Optional (but recommended) items
      mStep =     getFloat(L, 3, 0.1);
      mMinValue = getFloat(L, 4, 0.1);
      mMaxValue = getFloat(L, 5, 1000);
      mDecimalPlaces = getInt(L, 6, 3);
      mUnits =    getString(L, 7, "");
      mMinMsg =   getString(L, 8, "");
      mHelp =     getString(L, 9, "");

      // Second required item
      setFloatValue(getCheckedFloat(L, 2, methodName));    // Set this one last so we'll know mMinValue and mMaxValue
   }
   catch(LuaException &e)
   {
      logprintf(LogConsumer::LogError, "Error constructing FloatCounterMenuItem -- please see documentation");
      logprintf(LogConsumer::ConsoleMsg, "Usage: FloatCounterMenuItem(<display val (str)> [step (f)] [min val (f)] [max val (f)] [decimal places (i)] [units (str)] [min msg (str)] [help (str)] <value (int))");
      throw e;
   }
}



////////////////////////////////////
////////////////////////////////////

TimeCounterMenuItem::TimeCounterMenuItem(const string &title, S32 value, S32 maxVal, const string &zeroMsg, const string &help,
                                         S32 step, InputCode k1, InputCode k2) :
   CounterMenuItem(title, value, step, 0, maxVal, "", zeroMsg, help, k1, k2)
{
   mEditingSeconds = false;
}


// Destructor
TimeCounterMenuItem::~TimeCounterMenuItem()
{
   // Do nothing
}


S32 TimeCounterMenuItem::getBigIncrement()
{
   return 12;  // 12 * 5sec = 1 minute
}

void TimeCounterMenuItem::backspace()
{
   S32 minutes = mValue / 60;
   S32 seconds = mValue % 60;
   if(mEditingSeconds)
      seconds /= 10;
   else
      minutes /= 10;
   mValue = minutes * 60 + seconds;
}
void TimeCounterMenuItem::enterDigit(S32 digit)
{
   U32 minutes = U32(mValue) / 60;
   U32 seconds = U32(mValue) % 60;
   if(mEditingSeconds)
   {
      seconds = seconds * 10 + digit;
      if(seconds >= 60)
         seconds = digit;
   }
   else
   {
      minutes = minutes * 10 + digit;
      if(minutes > S32(U32(U32(S32_MAX) + 59) / U32(60)))
         minutes = S32(U32(U32(S32_MAX) + 59) / U32(60));
   }

   mValue = S32(minutes * 60 + seconds);
   if(mValue < 0)
      mValue = S32_MAX;

   if(mValue > mMaxValue)
      mValue = mMaxValue;
}


// Return true if key was handled, false otherwise
bool TimeCounterMenuItem::handleKey(InputCode inputCode)
{
   if(inputCode == KEY_SEMICOLON || inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER)
      mEditingSeconds = !mEditingSeconds;
   else
      return Parent::handleKey(inputCode);
   return true;
}


string TimeCounterMenuItem::getUnits() const
{
   return mValue >= 60 ? " mins" : " secs";
}


MenuItemTypes TimeCounterMenuItem::getItemType()
{
   return TimeCounterMenuItemType;
}


void TimeCounterMenuItem::setValue(const string &val)
{
   mValue = S32((atof(val.c_str()) * 60 + 2.5) / 5) * 5;  // Snap to nearest 5 second interval
}


string TimeCounterMenuItem::getOptionText() const
{
   return (mValue == mMinValue && mMinMsg != "") ?
         mMinMsg :
         // If not minimum, make sure we return min/seconds
         ((mValue < 60) ? itos(mValue) : itos(mValue / 60) + ":" + ((mValue % 60) < 10 ? "0" : "") + itos(mValue % 60)) + getUnits();
}


string TimeCounterMenuItem::getValueForWritingToLevelFile() const
{
   return ftos((F32) mValue / 60.0f, 3);  // Time in minutes, with fraction
}


////////////////////////////////////
////////////////////////////////////

TimeCounterMenuItemSeconds::TimeCounterMenuItemSeconds(const string &title, S32 value, S32 maxVal, const string &zeroMsg, 
                                                       const string &help, InputCode k1, InputCode k2) :
   TimeCounterMenuItem(title, value, maxVal, zeroMsg, help, 1, k1, k2)
{
   // Do nothing
}


// Destructor
TimeCounterMenuItemSeconds::~TimeCounterMenuItemSeconds()
{
   // Do nothing
}


S32 TimeCounterMenuItemSeconds::getBigIncrement()
{
   return 5;
}


void TimeCounterMenuItemSeconds::setValue(const string &val)
{
   mValue = atoi(val.c_str());
}


string TimeCounterMenuItemSeconds::getValueForWritingToLevelFile() const
{
   return itos(mValue);
}


void TimeCounterMenuItemSeconds::snap()
{
   mValue = S32((mValue / getBigIncrement()) * getBigIncrement());
}


////////////////////////////////////
////////////////////////////////////

PlayerMenuItem::PlayerMenuItem(S32 index, const char *text, void (*callback)(ClientGame *, U32), InputCode k1, PlayerType type) :
      MenuItem(index, text, callback, "", k1, KEY_UNKNOWN)
{
   mType = type;
}

// Destructor
PlayerMenuItem::~PlayerMenuItem()
{
   // Do nothing
}


string PlayerMenuItem::getOptionText() const
{
   string text = getPrompt();

   // Add a player type prefix if requested
   if(mType == PlayerTypePlayer)
      text = "[Player] " + text;
   else if(mType == PlayerTypeAdmin)
      text = "[Admin] " + text;
   else if(mType == PlayerTypeRobot)
      text = "[Robot] " + text;

   return text;
}


void PlayerMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   Renderer::get().setColor(*getColor(isSelected));
   drawCenteredString(xpos, ypos, textsize, getOptionText().c_str());
}


S32 PlayerMenuItem::getWidth(S32 textsize)
{
   return getStringWidth(textsize, getOptionText().c_str());
}


MenuItemTypes PlayerMenuItem::getItemType()
{
   return PlayerMenuItemType;
}


void PlayerMenuItem::activatedWithShortcutKey()
{
   /* Do nothing */
}


////////////////////////////////////
////////////////////////////////////

TeamMenuItem::TeamMenuItem(S32 index, AbstractTeam *team, void (*callback)(ClientGame *, U32), InputCode inputCode, bool isCurrent) :
               MenuItem(index, team->getName().getString(), callback, "", inputCode, KEY_UNKNOWN)
{
   mTeam = team;
   mIsCurrent = isCurrent;
   mUnselectedColor = *team->getColor();
   mSelectedColor = *team->getColor();
}

// Destructor
TeamMenuItem::~TeamMenuItem()
{
   // Do nothing
}


string TeamMenuItem::getOptionText() const
{
   Team *team = (Team *)mTeam;

   // Static may help reduce allocation/deallocation churn at the cost of 2K memory; not sure either are really a problem
   static char buffer[2048];  
   dSprintf(buffer, sizeof(buffer), "%s%s  [ %d | %d | %d ]", mIsCurrent ? "* " : "", getPrompt().c_str(), 
                                                           team->getPlayerCount(), team->getBotCount(), team->getScore());

   return buffer;
}


void TeamMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   Renderer::get().setColor(*getColor(isSelected));
   drawCenteredStringf(xpos, ypos, textsize, getOptionText().c_str());
}


S32 TeamMenuItem::getWidth(S32 textsize)
{
   return getStringWidth(textsize, getOptionText().c_str());
}


MenuItemTypes TeamMenuItem::getItemType()
{
   return TeamMenuItemType;
}


void TeamMenuItem::activatedWithShortcutKey()
{
   // Do nothing
}


////////////////////////////////////
////////////////////////////////////

TextEntryMenuItem::TextEntryMenuItem(const string &title, const string &val, const string &emptyVal, const string &help, U32 maxLen, InputCode k1, InputCode k2) :
         ValueMenuItem(title, NULL, help, k1, k2),
         mLineEditor(LineEditor(maxLen, val))
{
   initialize();
   mEmptyVal = emptyVal;
}


// Destructor
TextEntryMenuItem::~TextEntryMenuItem()
{
   LUAW_DESTRUCTOR_CLEANUP;
}


void TextEntryMenuItem::initialize()
{
   mEnterAdvancesItem = true;
   mTextEditedCallback = NULL;

   LUAW_CONSTRUCTOR_INITIALIZATIONS;
}


string TextEntryMenuItem::getOptionText() const
{
   return mLineEditor.getString() != "" ? mLineEditor.getDisplayString() : mEmptyVal;
}


void TextEntryMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   Color textColor;     
   if(mLineEditor.getString() == "" && mEmptyVal != "")
      textColor.set(.4, .4, .4);
   else if(isSelected)
      textColor.set(Colors::red);
   else
      textColor.set(Colors::cyan);

   S32 xpos2 = drawCenteredStringPair(xpos, ypos, textsize, MenuContext, InputContext, *getColor(isSelected), textColor,
                                      getPrompt().c_str(), getOptionText().c_str());

   Renderer::get().setColor(Colors::red);      // Cursor is always red
   if(isSelected)
   {
      FontManager::pushFontContext(InputContext);
      mLineEditor.drawCursor(xpos2, ypos, textsize);
      FontManager::popFontContext();
   }
}


S32 TextEntryMenuItem::getWidth(S32 textsize)
{
   return getStringPairWidth(textsize, MenuContext, InputContext, getPrompt().c_str(), getOptionText().c_str());
}


bool TextEntryMenuItem::handleKey(InputCode inputCode)
{ 
   bool handled = mLineEditor.handleKey(inputCode);
   if(mTextEditedCallback)
      mTextEditedCallback(mLineEditor.getString(), mMenu->getAssociatedObject());

   return handled;
}


bool TextEntryMenuItem::hasTextInput()
{
   return true;
}


void TextEntryMenuItem::handleTextInput(char ascii)
{
   if(ascii)
   {
      mLineEditor.addChar(ascii);

      if(mTextEditedCallback)
         mTextEditedCallback(mLineEditor.getString(), mMenu->getAssociatedObject());
   }
}


MenuItemTypes TextEntryMenuItem::getItemType()
{
   return TextEntryMenuItemType;
}


LineEditor *TextEntryMenuItem::getLineEditor()
{
   return &mLineEditor;
}


void TextEntryMenuItem::setLineEditor(LineEditor editor)
{
   mLineEditor = editor;
}


string TextEntryMenuItem::getValueForWritingToLevelFile() const
{
   return mLineEditor.getString() != "" ? mLineEditor.getString() : mEmptyVal;
}


string TextEntryMenuItem::getValue() const
{
   return mLineEditor.getString();
}


void TextEntryMenuItem::setValue(const string &val)
{
   mLineEditor.setString(val);
}


void TextEntryMenuItem::setFilter(LineEditorFilter filter)
{
   mLineEditor.setFilter(filter);
}


void TextEntryMenuItem::activatedWithShortcutKey()
{
   /* Do nothing */
}


void TextEntryMenuItem::setTextEditedCallback(void(*callback)(string, BfObject *))
{
   mTextEditedCallback = callback;
}


void TextEntryMenuItem::setSecret(bool secret)
{
   mLineEditor.setSecret(secret);
}


//////////
// Lua interface

/**
 * @luaclass TextEntryMenuItem
 * 
 * @brief Menu item allowing users to enter a text value.
 * 
 * @luafunc TextEntryMenuItem::TextEntryMenuItem(string name, string initial, string empty, int maxLength, string help)
 * 
 * @param name The text shown on the menu item.
 * @param initial The initial text in the menu item.
 * @param empty The text to display when the menu item is empty.
 * @param maxLength The maximum number of characters to allow (default: 32)
 * @param help A bit of help text.
 * 
 * The MenuItem will return the text which the user entered.
 * 
 * For example:
 * 
 * @code
 *   m = TextEntryMenuItem.new("Player Name", "ChumpChange", "<no name entered>", 64, "The new player's name")
 * @endcode
 */
//                Fn name                  Param profiles            Profile count                           
#define LUA_METHODS(CLASS, METHOD) \

GENERATE_LUA_FUNARGS_TABLE(TextEntryMenuItem, LUA_METHODS);
GENERATE_LUA_METHODS_TABLE(TextEntryMenuItem, LUA_METHODS);

#undef LUA_METHODS


const char *TextEntryMenuItem::luaClassName = "TextEntryMenuItem";
REGISTER_LUA_SUBCLASS(TextEntryMenuItem, MenuItem);


// Lua Constructor
TextEntryMenuItem::TextEntryMenuItem(lua_State *L) : Parent("", NULL, "", KEY_NONE, KEY_NONE)
{
   initialize();

   const char *methodName = "TextEntryMenuItem constructor";

   // Required items -- will throw if they are missing or misspecified
   mDisplayVal = getString(L, 1, methodName);

   // Optional (but recommended) items
   mLineEditor.setString(getString(L, 2, ""));
   mEmptyVal = getString(L, 3, "");
   mLineEditor.mMaxLen = getInt(L, 4, 32);
   mHelp = getString(L, 5, "");
}


////////////////////////////////////
////////////////////////////////////

MaskedTextEntryMenuItem::MaskedTextEntryMenuItem(string title, string val, string emptyVal, const string &help, 
                                                 U32 maxLen, InputCode k1, InputCode k2) :
   Parent(title, val, emptyVal, help, maxLen, k1, k2)
{
   mLineEditor.setSecret(true);
}

// Destructor
MaskedTextEntryMenuItem::~MaskedTextEntryMenuItem()
{
   // Do nothing
}


////////////////////////////////////
////////////////////////////////////

SimpleTextEntryMenuItem::SimpleTextEntryMenuItem(string title, U32 length, void (*callback)(ClientGame *, U32)) :
   Parent(title, "", "", "", length)
{
   mCallback = callback;
   mHasError = false;
}

// Destructor
SimpleTextEntryMenuItem::~SimpleTextEntryMenuItem()
{
   // Do nothing
}


void SimpleTextEntryMenuItem::setHasError(bool hasError)
{
   mHasError = hasError;
}


bool SimpleTextEntryMenuItem::handleKey(InputCode inputCode)
{
   if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER)
   {
      // Call the menu item main callback unless we have an error
      if(mCallback && !mHasError)
         mCallback(getMenu()->getGame(), 0);  // 0 is unused

      return true;
   }

   bool handled = mLineEditor.handleKey(inputCode);

   // Call this menu item's callback if the lineEditor handled the key (it is also run in hasTextInput() )
   if(mTextEditedCallback && handled)
      mTextEditedCallback(mLineEditor.getString(), mMenu->getAssociatedObject());

   return handled;
}


void SimpleTextEntryMenuItem::render(S32 xpos, S32 ypos, S32 textsize, bool isSelected)
{
   Color textColor(Colors::cyan);

   S32 xpos2 = drawCenteredStringPair(xpos, ypos, textsize, MenuContext, InputContext, *getColor(false), textColor,
         getPrompt().c_str(), mLineEditor.getDisplayString().c_str());

   Renderer::get().setColor(Colors::red);      // Cursor is always red

   FontManager::pushFontContext(InputContext);
   mLineEditor.drawCursor(xpos2, ypos, textsize);
   FontManager::popFontContext();
}


};

