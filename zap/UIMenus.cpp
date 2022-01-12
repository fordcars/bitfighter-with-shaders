//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIMenus.h"

#include "UIGame.h"           // Only barely used
#include "UIEditor.h"         // Can get rid of this with a passthrough in MenuManager
#include "UIErrorMessage.h"   // Can get rid of this with a passthrough in MenuManager
#include "UICredits.h"
#include "UIQueryServers.h"
#include "UIGameParameters.h"
#include "UIHighScores.h"
#include "UIInstructions.h"
#include "UIKeyDefMenu.h"
#include "UINameEntry.h"
#include "UIManager.h"

#include "LevelSource.h"
#include "LevelDatabase.h"

#include "GameManager.h"
#include "ClientGame.h"
#include "ServerGame.h"
#include "gameType.h"            // Can get rid of this with some simple passthroughs
#include "IniFile.h"
#include "DisplayManager.h"
#include "Renderer.h"
#include "Joystick.h"
#include "JoystickRender.h"
#include "Colors.h"
#include "Cursor.h"
#include "VideoSystem.h"
#include "FontManager.h"
#include "SystemFunctions.h"
#include "masterConnection.h"

#include "gameObjectRender.h"    // For renderBitfighterLogo, glColor
#include "stringUtils.h"
#include "RenderUtils.h"

#include <algorithm>
#include <string>
#include <math.h>

#include "GameRecorderPlayback.h"

namespace Zap
{


extern void shutdownBitfighter();

////////////////////////////////////
////////////////////////////////////

// Constructor
MenuUserInterface::MenuUserInterface(ClientGame *game) : UserInterface(game)
{
   initialize();
}


MenuUserInterface::MenuUserInterface(ClientGame *game, const string &title) : UserInterface(game)
{
   initialize();

   mMenuTitle = title;
}


// Destructor
MenuUserInterface::~MenuUserInterface()
{
   // Do nothing
}


void MenuUserInterface::initialize()
{
   mMenuTitle = "MENU";
   mMenuSubTitle = "";

   selectedIndex = 0;
   itemSelectedWithMouse = false;
   mFirstVisibleItem = 0;
   mRenderInstructions = true;
   mRenderSpecialInstructions = true;
   mIgnoreNextMouseEvent = false;

   mAssociatedObject = NULL;

   // Max number of menu items we show on screen before we go into scrolling mode -- won't work with mixed size menus
   mMaxMenuSize = S32((DisplayManager::getScreenInfo()->getGameCanvasHeight() - 150) / (getTextSize(MENU_ITEM_SIZE_NORMAL) + getGap(MENU_ITEM_SIZE_NORMAL)));
}


// Gets run when menu is activated.  This is also called by almost all other menus/subclasses.
void MenuUserInterface::onActivate()
{
   mDisableShipKeyboardInput = true;       // Keep keystrokes from getting to game
   selectedIndex = 0;
   mFirstVisibleItem = 0;

   clearFadingNotice();
}


void MenuUserInterface::onReactivate()
{
   mDisableShipKeyboardInput = true;       // Keep keystrokes from getting to game
   clearFadingNotice();
}


void MenuUserInterface::clearMenuItems()
{
   mMenuItems.clear();
}


void MenuUserInterface::sortMenuItems()
{
   // Sorts alphanumerically by menuItem's prompt  ==> used for getting levels in the right order and such
   mMenuItems.sort([](const shared_ptr<MenuItem> &a, const shared_ptr<MenuItem> &b)
         {
            return alphaSort(a.get()->getPrompt(), b.get()->getPrompt());
         }
   );
}


S32 MenuUserInterface::addMenuItem(MenuItem *menuItem)
{
   menuItem->setMenu(this);
   mMenuItems.push_back(shared_ptr<MenuItem>(menuItem));

   return mMenuItems.size() - 1;
}


// For those times when you really need to add a pre-packaged menu item... normally, you won't need to do this
void MenuUserInterface::addWrappedMenuItem(shared_ptr<MenuItem> menuItem)
{
   menuItem->setMenu(this);
   mMenuItems.push_back(menuItem);
}


S32 MenuUserInterface::getMenuItemCount()
{
   return mMenuItems.size();
}


MenuItem *MenuUserInterface::getLastMenuItem()
{
   return mMenuItems.last().get();
}


MenuItem *MenuUserInterface::getMenuItem(S32 index)
{
   return mMenuItems[index].get();
}


void MenuUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   // Controls rate of scrolling long menus with mouse
   mScrollTimer.update(timeDelta);

   mFadingNoticeTimer.update(timeDelta);

   // Call mouse handler so users can scroll scrolling menus just by holding mouse in position
   // (i.e. we don't want to limit scrolling action only to times when user moves mouse)
   if(itemSelectedWithMouse)
      processMouse();
}


// Return index offset to account for scrolling menus; basically caluclates index of topmost visible item
S32 MenuUserInterface::getOffset()
{
   S32 offset = 0;

   if(isScrollingMenu())     // Do some sort of scrolling
   {
      // itemSelectedWithMouse basically lets users highlight the top and bottom items in a scrolling list,
      // which can't be done when using the keyboard
      if(selectedIndex - mFirstVisibleItem < (itemSelectedWithMouse ? 0 : 1))
         offset = selectedIndex - (itemSelectedWithMouse ? 0 : 1);
      else if( selectedIndex - mFirstVisibleItem > (mMaxMenuSize - (itemSelectedWithMouse ? 1 : 2)) )
         offset = selectedIndex - (mMaxMenuSize - (itemSelectedWithMouse ? 1 : 2));
      else offset = mFirstVisibleItem;
   }

   mFirstVisibleItem = checkMenuIndexBounds(offset);

   return mFirstVisibleItem;
}


bool MenuUserInterface::isScrollingMenu()
{
   return mMenuItems.size() > mMaxMenuSize;
}


S32 MenuUserInterface::checkMenuIndexBounds(S32 index)
{
   if(index < 0)
      return 0;
   
   if(index > getMaxFirstItemIndex())
      return getMaxFirstItemIndex();

   return index;
}


S32 MenuUserInterface::getBaseYStart() const
{
   return (DisplayManager::getScreenInfo()->getGameCanvasHeight() - min(mMenuItems.size(), mMaxMenuSize) *
           (getTextSize(MENU_ITEM_SIZE_NORMAL) + getGap(MENU_ITEM_SIZE_NORMAL))) / 2;
}


// Get vert pos of first menu item
S32 MenuUserInterface::getYStart() const
{
   return getBaseYStart();
}


static void renderMenuInstructions(GameSettings *settings)
{
   S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   F32 y = F32(canvasHeight - UserInterface::vertMargin - 20);
   const S32 size = 18;

   Renderer::get().setColor(Colors::white);

   if(settings->getInputMode() == InputModeKeyboard)
   {
      static const SymbolString KeyboardInstructions(
            "[[Up Arrow]], [[Down Arrow]] to choose | [[Enter]] to select | [[Esc]] exits menu", 
            settings->getInputCodeManager(), MenuHeaderContext, size, false, AlignmentCenter);

      KeyboardInstructions.render(Point(canvasWidth / 2, y + size));
   }
   else
   {
      static const SymbolString JoystickInstructions(
            "[[DPad Up]],  [[Dpad Down]] to choose | [[Start]] to select | [[Back]] exits menu", 
            settings->getInputCodeManager(), MenuHeaderContext, size, false, AlignmentCenter);

      JoystickInstructions.render(Point(canvasWidth / 2, y + size));
   }
}


static void renderArrow(S32 pos, bool pointingUp)
{
   static const S32 ARROW_WIDTH = 100;
   static const S32 ARROW_HEIGHT = 20;
   static const S32 ARROW_MARGIN = 5;
   Renderer& r = Renderer::get();

   S32 canvasWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth();

   S32 y = 0;
   if(pointingUp)    // Up arrow
      y = pos - (ARROW_HEIGHT + ARROW_MARGIN) - 7;
   else              // Down arrow
      y = pos + (ARROW_HEIGHT + ARROW_MARGIN) - 7;

   F32 vertices[] = {
         F32(canvasWidth - ARROW_WIDTH) / 2, F32(pos - ARROW_MARGIN - 7),
         F32(canvasWidth + ARROW_WIDTH) / 2, F32(pos - ARROW_MARGIN - 7),
         F32(canvasWidth) / 2,               F32(y)
   };

   for(S32 i = 1; i >= 0; i--)
   {
      // First create a black poly to blot out what's behind, then the arrow itself
      r.setColor(i ? Colors::black : Colors::blue);
      r.renderVertexArray(vertices, ARRAYSIZE(vertices) / 2, i ? RenderType::TriangleFan : RenderType::LineLoop);
   }
}


static void renderArrowAbove(S32 pos)
{
   renderArrow(pos, true);
}


static void renderArrowBelow(S32 pos)
{
   renderArrow(pos, false);
}


// Basic menu rendering
void MenuUserInterface::render()
{
   Renderer& r = Renderer::get();
   FontManager::pushFontContext(MenuContext);

   S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   // Draw the game screen, then dim it out so you can still see it under our overlay
   if(getGame()->getConnectionToServer())
      getUIManager()->renderAndDimGameUserInterface();


   FontManager::pushFontContext(MenuHeaderContext);

   // Title 
   if(mMenuTitle.length() != 0) // This check is to fix green dot from zero length underline on some systems including linux software renderer (linux command: LIBGL_ALWAYS_SOFTWARE=1 ./bitfighter)
   {
      r.setColor(Colors::green);
      drawCenteredUnderlinedString(vertMargin, 30, mMenuTitle.c_str());
   }
   
   // Subtitle
   r.setColor(mMenuSubTitleColor);
   drawCenteredString(vertMargin + 35, 18, mMenuSubTitle.c_str());

   // Instructions
   if(mRenderInstructions)
      renderMenuInstructions(getGame()->getSettings());

   FontManager::popFontContext();

   S32 count = mMenuItems.size();

   if(isScrollingMenu())     // Need some sort of scrolling?
      count = mMaxMenuSize;

   S32 yStart = getYStart();
   S32 offset = getOffset();

   S32 shrinkfact = 1;

   S32 y = yStart;

   for(S32 i = 0; i < count; i++)
   {
      MenuItemSize size = getMenuItem(i)->getSize();
      S32 textsize = getTextSize(size);
      S32 gap = getGap(size);
      S32 highlightVertOffset = 3;
      
      // Highlight selected item
      if(selectedIndex == i + offset)
         drawMenuItemHighlight(0,           y - gap / 2 + shrinkfact            + highlightVertOffset, 
                               canvasWidth, y + textsize + gap / 2 - shrinkfact + highlightVertOffset);

      S32 indx = i + offset;
      mMenuItems[indx]->render(y, textsize, selectedIndex == indx);

      y += textsize + gap;
   }

   // Render an indicator that there are scrollable items above and/or below
   if(isScrollingMenu())
   {
      if(offset > 0)                     // There are items above
         renderArrowAbove(yStart);

      if(offset < getMaxFirstItemIndex())     // There are items below
         renderArrowBelow(yStart + (getTextSize(MENU_ITEM_SIZE_NORMAL) + getGap(MENU_ITEM_SIZE_NORMAL)) * mMaxMenuSize + 6);
   }

   // Render a help string at the bottom of the menu
   if(U32(selectedIndex) < U32(mMenuItems.size()))
   {
      const S32 helpFontSize = 15;
      S32 ypos = canvasHeight - vertMargin - 50;

      // Render a special instruction line
      if(mRenderSpecialInstructions)
      {
         r.setColor(Colors::menuHelpColor, 0.6f);
         drawCenteredString(ypos, helpFontSize, mMenuItems[selectedIndex]->getSpecialEditingInstructions());
      }

      ypos -= helpFontSize + 5;
      r.setColor(Colors::yellow);
      drawCenteredString(ypos, helpFontSize, mMenuItems[selectedIndex]->getHelp().c_str());
   }

   // If we have a fading notice to show
   if(mFadingNoticeTimer.getCurrent() != 0)
   {
      // Calculate the fade
      F32 alpha = 1.0;
      if(mFadingNoticeTimer.getCurrent() < 1000)
         alpha = (F32) mFadingNoticeTimer.getCurrent() * 0.001f;

      const S32 textsize = 25;
      const S32 padding = 10;
      const S32 width = getStringWidth(textsize, mFadingNoticeMessage.c_str()) + (4 * padding);  // Extra padding to not collide with bevels
      const S32 left = (DisplayManager::getScreenInfo()->getGameCanvasWidth() - width) / 2;
      const S32 top = mFadingNoticeVerticalPosition;
      const S32 bottom = top + textsize + (2 * padding);
      const S32 cornerInset = 10;

      // Fill
      r.setColor(Colors::red40, alpha);
      drawFancyBox(left, top, DisplayManager::getScreenInfo()->getGameCanvasWidth() - left, bottom, cornerInset, RenderType::TriangleFan);

      // Border
      r.setColor(Colors::red, alpha);
      drawFancyBox(left, top, DisplayManager::getScreenInfo()->getGameCanvasWidth() - left, bottom, cornerInset, RenderType::LineLoop);

      r.setColor(Colors::white, alpha);
      drawCenteredString(top + padding, textsize, mFadingNoticeMessage.c_str());
   }

   renderExtras();  // Draw something unique on a menu

   FontManager::popFontContext();
}


// Calculates maximum index that the first item can have -- on non scrolling menus, this will be 0
S32 MenuUserInterface::getMaxFirstItemIndex()
{
   return max(mMenuItems.size() - mMaxMenuSize, 0);
}


// Fill responses with values from each menu item in turn
void MenuUserInterface::getMenuResponses(Vector<string> &responses)
{
   for(S32 i = 0; i < mMenuItems.size(); i++)
      responses.push_back(mMenuItems[i]->getValue());
}


// Handle mouse input, figure out which menu item we're over, and highlight it
void MenuUserInterface::onMouseMoved()
{
   if(mIgnoreNextMouseEvent)     // Suppresses spurious mouse events from the likes of SDL_WarpMouse
   {
      mIgnoreNextMouseEvent = false;
      return;
   }

   Parent::onMouseMoved();

   // Really only matters when starting to host game... don't want to be able to change menu items while the levels are loading.
   // This is purely an aesthetic issue, a minor irritant.
   if(GameManager::getHostingModePhase() == GameManager::LoadingLevels)
      return;

   itemSelectedWithMouse = true;
   Cursor::enableCursor();  // Show cursor when user moves mouse

   selectedIndex = getSelectedMenuItem();

   processMouse();
}


S32 MenuUserInterface::getSelectedMenuItem()
{
   S32 mouseY = (S32)DisplayManager::getScreenInfo()->getMousePos()->y;   

   S32 cumHeight = getYStart();

   // Mouse is above the top of the menu
   if(mouseY <= cumHeight)    // That's cumulative height, you pervert!
      return mFirstVisibleItem;

   // Mouse is on the menu
   for(S32 i = 0; i < getMenuItemCount() - 1; i++)
   {
      MenuItemSize size = getMenuItem(i)->getSize();
      S32 height = getGap(size) / 2 + getTextSize(size);

      cumHeight += height;

      if(mouseY < cumHeight)
         return i + mFirstVisibleItem;     

      cumHeight += getGap(size) / 2;
   }

   // Mouse is below bottom of menu
   return getMenuItemCount() - 1 + mFirstVisibleItem;
}


void MenuUserInterface::processMouse()
{
   if(isScrollingMenu())   // We have a scrolling situation here...
   {
      if(selectedIndex <= mFirstVisibleItem)                          // Scroll up
      {
         if(!mScrollTimer.getCurrent() && mFirstVisibleItem > 0)
         {
            mFirstVisibleItem--;
            mScrollTimer.reset(MOUSE_SCROLL_INTERVAL);
         }
         selectedIndex = mFirstVisibleItem;
      }
      else if(selectedIndex > mFirstVisibleItem + mMaxMenuSize - 1)  // Scroll down
      {
         if(!mScrollTimer.getCurrent() && selectedIndex > mFirstVisibleItem + mMaxMenuSize - 2)
         {
            mFirstVisibleItem++;
            mScrollTimer.reset(MOUSE_SCROLL_INTERVAL);
         }
         selectedIndex = mFirstVisibleItem + mMaxMenuSize - 1;
      }
      else
         mScrollTimer.clear();
   }

   if(selectedIndex < 0)                          // Scrolled off top of list
   {
      selectedIndex = 0;
      mFirstVisibleItem = 0;
   }
   else if(selectedIndex >= mMenuItems.size())    // Scrolled off bottom of list
   {
      selectedIndex = mMenuItems.size() - 1;
      mFirstVisibleItem = getMaxFirstItemIndex();
   }
}


bool MenuUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;

   // Capture mouse wheel on scrolling menus and use it to scroll.  Otherwise, let it be processed by individual menu items.
   // This will usually work because scrolling menus do not (at this time) contain menu items that themselves use the wheel.
   if(isScrollingMenu())
   {
      if(inputCode == MOUSE_WHEEL_DOWN)
      {
         mFirstVisibleItem = checkMenuIndexBounds(mFirstVisibleItem + 1);

         onMouseMoved();
         return true;
      }
      else if(inputCode == MOUSE_WHEEL_UP)
      {
         mFirstVisibleItem = checkMenuIndexBounds(mFirstVisibleItem - 1);

         onMouseMoved();
         return true;
      }
   }

   if(inputCode == KEY_UNKNOWN)
      return true;

   // Check for in autorepeat mode
   mRepeatMode = mKeyDown;
   mKeyDown = true;

   // Handle special case of keystrokes during hosting preparation phases
   if(GameManager::getHostingModePhase() == GameManager::LoadingLevels ||
      GameManager::getHostingModePhase() == GameManager::DoneLoadingLevels)
   {
      if(inputCode == KEY_ESCAPE)     // Can only get here when hosting
      {
         GameManager::setHostingModePhase(GameManager::NotHosting);

         getGame()->closeConnectionToGameServer();

         GameManager::deleteServerGame();
      }

      // All other keystrokes will be ignored
      return true;
   }

   // Process each key handler in turn until one works
   bool keyHandled = processMenuSpecificKeys(inputCode);

   if(!keyHandled)
      keyHandled = processKeys(inputCode);


   // Finally, since the user has indicated they want to use keyboard/controller input, hide the pointer
   if(!InputCodeManager::isMouseAction(inputCode) && inputCode != KEY_ESCAPE)
      Cursor::disableCursor();

   return keyHandled;
}


void MenuUserInterface::onTextInput(char ascii)
{

   if(U32(selectedIndex) < U32(mMenuItems.size()))
      mMenuItems[selectedIndex]->handleTextInput(ascii);
}


void MenuUserInterface::onKeyUp(InputCode inputCode)
{
   mKeyDown = false;
   mRepeatMode = false;
}


// Generic handler looks for keystrokes and translates them into menu actions
bool MenuUserInterface::processMenuSpecificKeys(InputCode inputCode)
{
   // Don't process shortcut keys if the current menuitem has text input
   if(U32(selectedIndex) < U32(mMenuItems.size()) && mMenuItems[selectedIndex]->hasTextInput())
      return false;

   // Check for some shortcut keys
   for(S32 i = 0; i < mMenuItems.size(); i++)
   {
      if(inputCode == mMenuItems[i]->key1 || inputCode == mMenuItems[i]->key2)
      {
         selectedIndex = i;

         mMenuItems[i]->activatedWithShortcutKey();
         itemSelectedWithMouse = false;
         return true;
      }
   }

   return false;
}


S32 MenuUserInterface::getTotalMenuItemHeight()
{
   S32 height = 0;
   for(S32 i = 0; i < mMenuItems.size(); i++)
   {
      MenuItemSize size = mMenuItems[i]->getSize();
      height += getTextSize(size) + getGap(size);
   }

   return height;
}


// Process the keys that work on all menus -- return true if key was handled, false otherwise
bool MenuUserInterface::processKeys(InputCode inputCode)
{
   inputCode = InputCodeManager::convertJoystickToKeyboard(inputCode);
   
   if(Parent::onKeyDown(inputCode))
   { 
      // Do nothing 
   }
   else if(U32(selectedIndex) >= U32(mMenuItems.size()))  // Probably empty menu... Can only go back.
   {
      onEscape();
   }
   else if(mMenuItems[selectedIndex]->handleKey(inputCode))
   {
      // Do nothing
   }
   else if(inputCode == KEY_ENTER || (inputCode == KEY_SPACE && !mMenuItems[selectedIndex]->hasTextInput()))
   {
      playBoop();
      if(inputCode != MOUSE_LEFT)
         itemSelectedWithMouse = false;

      else // it was MOUSE_LEFT after all
      {
         // Make sure we're actually pointing at a menu item before we process it
         S32 yStart = getYStart();
         const Point *mousePos = DisplayManager::getScreenInfo()->getMousePos();

         getSelectedMenuItem();

         if(mousePos->y < getYStart() || yStart + getTotalMenuItemHeight())
            return true;
      }

      mMenuItems[selectedIndex]->handleKey(inputCode);

      if(mMenuItems[selectedIndex]->enterAdvancesItem())
         advanceItem();
   }

   else if(inputCode == KEY_ESCAPE)
   {
      playBoop();
      onEscape();
   }
   else if(inputCode == KEY_UP || (inputCode == KEY_TAB && InputCodeManager::checkModifier(KEY_SHIFT)))   // Prev item
   {
      selectedIndex--;
      itemSelectedWithMouse = false;

      if(selectedIndex < 0)                        // Scrolling off the top
      {
         if(isScrollingMenu() && mRepeatMode)      // Allow wrapping on long menus only when not in repeat mode
         {
            selectedIndex = 0;               // No wrap --> (first item)
            return true;                     // (leave before playBoop)
         }
         else                                         // Always wrap on shorter menus
            selectedIndex = mMenuItems.size() - 1;    // Wrap --> (select last item)
      }
      playBoop();
   }

   else if(inputCode == KEY_DOWN || inputCode == KEY_TAB)    // Next item
      advanceItem();

   // If nothing was handled, return false
   else
      return false;

   // If we made it here, then something was handled
   return true;
}


S32 MenuUserInterface::getTextSize(MenuItemSize size) const
{
   return size == MENU_ITEM_SIZE_NORMAL ? 23 : 15;
}


S32 MenuUserInterface::getGap(MenuItemSize size) const
{
   return 18;
}


void MenuUserInterface::renderExtras() const
{
   /* Do nothing */
}


void MenuUserInterface::advanceItem()
{
   selectedIndex++;
   itemSelectedWithMouse = false;

   if(selectedIndex >= mMenuItems.size())     // Scrolling off the bottom
   {
      if(isScrollingMenu() && mRepeatMode)    // Allow wrapping on long menus only when not in repeat mode
      {
         selectedIndex = getMenuItemCount() - 1;                 // No wrap --> (last item)
         return;                                                 // (leave before playBoop)
      }
      else                     // Always wrap on shorter menus
         selectedIndex = 0;    // Wrap --> (first item)
   }
   playBoop();
}


void MenuUserInterface::onEscape()
{
   // Do nothing
}


BfObject *MenuUserInterface::getAssociatedObject()
{
   return mAssociatedObject;
}


void MenuUserInterface::setAssociatedObject(BfObject *obj)
{
   mAssociatedObject = obj;
}


// Set a fading notice on a menu
void MenuUserInterface::setFadingNotice(U32 time, S32 top, const string &message)
{
   mFadingNoticeTimer.reset(time);
   mFadingNoticeVerticalPosition = top;
   mFadingNoticeMessage = message;
}


// Clear the notice
void MenuUserInterface::clearFadingNotice()
{
   mFadingNoticeTimer.clear();
}


////////////////////////////////////////
////////////////////////////////////////

bool MenuUserInterfaceWithIntroductoryAnimation::mFirstTime = true;


// Constructor
MenuUserInterfaceWithIntroductoryAnimation::MenuUserInterfaceWithIntroductoryAnimation(ClientGame *game) : Parent(game)
{
   mShowingAnimation = false;
}


// Destructor
MenuUserInterfaceWithIntroductoryAnimation::~MenuUserInterfaceWithIntroductoryAnimation()
{
   // Do nothing
}


void MenuUserInterfaceWithIntroductoryAnimation::onActivate()
{
   if(mFirstTime)
   {
      mFadeInTimer.reset(FadeInTime);
      getUIManager()->activate<SplashUserInterface>();   // Show splash screen the first time through
      mShowingAnimation = true;
      mFirstTime = false;
   }
}


void MenuUserInterfaceWithIntroductoryAnimation::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);
   mFadeInTimer.update(timeDelta);

   mShowingAnimation = false;
}


void MenuUserInterfaceWithIntroductoryAnimation::render()
{
   Parent::render();

   // Fade in the menu here if we are showing it the first time...  this will tie in
   // nicely with the splash screen, and make the transition less jarring and sudden
   if(mFadeInTimer.getCurrent())
      dimUnderlyingUI(mFadeInTimer.getFraction());

   // Render logo at top, never faded
   renderStaticBitfighterLogo();
}


bool MenuUserInterfaceWithIntroductoryAnimation::onKeyDown(InputCode inputCode)
{
   if(mShowingAnimation)
   {
      mShowingAnimation = false;    // Stop animations if a key is pressed
      return true;                  // Swallow the keystroke
   }

   return Parent::onKeyDown(inputCode);
}


// Take action based on menu selection
void MenuUserInterfaceWithIntroductoryAnimation::processSelection(U32 index)
{
   mShowingAnimation = false;
}


////////////////////////////////////////
////////////////////////////////////////

//////////
// MainMenuUserInterface callbacks
//////////

static void joinSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getUI<QueryServersUserInterface>()->mHostOnServer = false;
   game->getUIManager()->activate<QueryServersUserInterface>();
}

static void hostSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<HostMenuUserInterface>();
}

static void helpSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<InstructionsUserInterface>();
}

static void optionsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<OptionsMenuUserInterface>();
}

static void highScoresSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<HighScoresUserInterface>();
}

static void editorSelectedCallback(ClientGame *game, U32 unused)
{
   GameSettings *settings = game->getSettings();
   FolderManager *folderManager = settings->getFolderManager();
   UIManager *uiManager = game->getUIManager();

   // Never did resolve a leveldir... no editing for you!
   if(folderManager->levelDir == "")
   {
      ErrorMessageUserInterface *ui = uiManager->getUI<ErrorMessageUserInterface>();
      ui->reset();
      ui->setTitle("HOUSTON, WE HAVE A PROBLEM");
      ui->setMessage("No valid level folder was found, so I cannot start the level editor.\n\n"
                     "Check the LevelDir parameter in your INI file or your command-line parameters to "
                     "make sure you have correctly specified a valid folder.");
      ui->setInstr("Press [[Esc]] to continue");

      uiManager->activate(ui);

      return;
   }

   game->setLevelDatabaseId(LevelDatabase::NOT_IN_DATABASE);      // <=== Should not be here... perhaps in editor onActivate?
   game->getUIManager()->activate<LevelNameEntryUserInterface>();
}


static void creditsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<CreditsUserInterface>();
}


static void quitSelectedCallback(ClientGame *game, U32 unused)
{
   shutdownBitfighter();
}


//////////

// Constructor
MainMenuUserInterface::MainMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "";
   mMOTD[0] = 0;
   mMenuSubTitle = "";
   mRenderInstructions = false;

   mNeedToUpgrade = false;           // Assume we're up-to-date until we hear from the master
   mShowedUpgradeAlert = false;      // So we don't show the upgrade message more than once

   InputCode keyHelp = getInputCode(game->getSettings(), BINDING_HELP);

   addMenuItem(new MenuItem("JOIN LAN/INTERNET GAME", joinSelectedCallback,       "", KEY_J));
   addMenuItem(new MenuItem("HOST GAME",              hostSelectedCallback,       "", KEY_H));
   addMenuItem(new MenuItem("HOW TO PLAY",            helpSelectedCallback,       "", KEY_I, keyHelp));
   addMenuItem(new MenuItem("OPTIONS",                optionsSelectedCallback,    "", KEY_O));
   addMenuItem(new MenuItem("HIGH SCORES",            highScoresSelectedCallback, "", KEY_S));
   addMenuItem(new MenuItem("LEVEL EDITOR",           editorSelectedCallback,     "", KEY_L, KEY_E));
   addMenuItem(new MenuItem("CREDITS",                creditsSelectedCallback,    "", KEY_C));
   addMenuItem(new MenuItem("QUIT",                   quitSelectedCallback,       "", KEY_Q));
}


// Destructor
MainMenuUserInterface::~MainMenuUserInterface()
{
   // Do nothing
}


void MainMenuUserInterface::onActivate()
{
   Parent::onActivate();

   mColorTimer.reset(ColorTime);
   mColorTimer2.reset(ColorTime2);
   mTransDir = true;
}


// Set the MOTD we received from the master
void MainMenuUserInterface::setMOTD(const char *motd)
{
   strncpy(mMOTD, motd, MOTD_LEN);     

   motdArriveTime = getGame()->getCurrentTime();    // Used for scrolling the message
}


// Set needToUpgrade flag that tells us the client is out-of-date
void MainMenuUserInterface::setNeedToUpgrade(bool needToUpgrade)
{
   mNeedToUpgrade = needToUpgrade;

   if(mNeedToUpgrade && !mShowedUpgradeAlert)
      showUpgradeAlert();
}


void MainMenuUserInterface::render()
{
   S32 canvasWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth();

   static const S32 MOTD_VERT_POS = 540;

   // Draw our Message-Of-The-Day, if we have one
   if(strcmp(mMOTD, "") != 0)
   {
      // Draw message, scrolling
      U32 width = getStringWidth(20, mMOTD);
      U32 totalWidth = width + canvasWidth;
      U32 pixelsPerSec = 100;
      U32 delta = getGame()->getCurrentTime() - motdArriveTime;
      delta = U32(delta * pixelsPerSec * 0.001) % totalWidth;

      FontManager::pushFontContext(MotdContext);
      Renderer::get().setColor(Colors::white);
      drawString(canvasWidth - delta, MOTD_VERT_POS, 20, mMOTD);
      FontManager::popFontContext();
   }

   // Parent renderer might dim what we've drawn so far, so run it last so it can have access to everything
   Parent::render();
}


void MainMenuUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   if(mColorTimer.update(timeDelta))
   {
      mColorTimer.reset(ColorTime);
      mTransDir = !mTransDir;
   }

   if(mColorTimer2.update(timeDelta))
   {
      mColorTimer2.reset(ColorTime2);
      mTransDir2 = !mTransDir2;
   }
}


S32 MainMenuUserInterface::getYStart() const
{
   return getBaseYStart() + 40;
}


bool MainMenuUserInterface::getNeedToUpgrade()
{
   return mNeedToUpgrade;
}


void MainMenuUserInterface::renderExtras() const
{
   Renderer::get().setColor(Colors::white);
   const S32 size = 16;
   drawCenteredString(DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - size, size, "join us @ www.bitfighter.org");
}


void MainMenuUserInterface::showUpgradeAlert()
{
   ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

   ui->reset();
   ui->setTitle("OUTDATED VERSION");
   ui->setMessage("You are running an older version of Bitfighter.  You will only be able to "
                  "play with players who still have the same outdated version.\n\n"
                  "To get the latest, visit bitfighter.org");

   getUIManager()->activate(ui);

   mShowedUpgradeAlert = true;            // Only show this alert once per session -- we don't need to beat them over the head with it!
}


void MainMenuUserInterface::onEscape()
{
   shutdownBitfighter();    // Quit!
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
OptionsMenuUserInterface::OptionsMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "OPTIONS MENU";
}


// Destructor
OptionsMenuUserInterface::~OptionsMenuUserInterface()
{
   // Do nothing
}


void OptionsMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


//////////
// Callbacks for Options menu

static void inputCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<InputOptionsMenuUserInterface>();
}


static void soundOptionsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<SoundOptionsMenuUserInterface>();
}


static void inGameHelpSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<InGameHelpOptionsUserInterface>();
}


// User has clicked on Display Mode menu item -- switch screen mode
static void setDisplayModeCallback(ClientGame *game, U32 mode)
{
   GameSettings *settings = game->getSettings();

   // Change display state based on selected normal display mode
   VideoSystem::StateReason reason = VideoSystem::StateReasonModeDirectWindowed;
   if((DisplayMode)mode == DISPLAY_MODE_FULL_SCREEN_STRETCHED)
      reason = VideoSystem::StateReasonModeDirectFullscreenStretched;
   else if((DisplayMode)mode == DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED)
      reason = VideoSystem::StateReasonModeDirectFullscreenUnstretched;

   VideoSystem::updateDisplayState(settings, reason);
}


//////////

// Used below and by UIEditor
MenuItem *getWindowModeMenuItem(U32 displayMode)
{
   Vector<string> opts;   
   // These options are aligned with the DisplayMode enum
   opts.push_back("WINDOWED");
   opts.push_back("FULLSCREEN STRETCHED");
   opts.push_back("FULLSCREEN");

   return new ToggleMenuItem("DISPLAY MODE:", opts, displayMode, true, 
                             setDisplayModeCallback, "Set the game mode to windowed or fullscreen", KEY_G);
}


void OptionsMenuUserInterface::setupMenus()
{
   clearMenuItems();
   Vector<string> opts;

   GameSettings *settings = getGame()->getSettings();

   addMenuItem(new MenuItem(getMenuItemCount(), "INPUT", inputCallback, 
                        "Joystick settings, Remap keys", KEY_I));

   addMenuItem(new MenuItem(getMenuItemCount(), "SOUNDS & MUSIC", soundOptionsSelectedCallback, 
                        "Change sound and music related options", KEY_S));

   addMenuItem(new MenuItem(getMenuItemCount(), "IN-GAME HELP", inGameHelpSelectedCallback, 
                        "Change settings related to in-game tutorial/help", KEY_H));

   addMenuItem(new YesNoMenuItem("AUTOLOGIN:", !settings->shouldShowNameEntryScreenOnStartup(), 
                                 "If selected, you will automatically log in "
                                 "on start, bypassing the first screen", KEY_A));

#ifndef TNL_OS_MOBILE
   addMenuItem(getWindowModeMenuItem((U32)settings->getIniSettings()->mSettings.getVal<DisplayMode>("WindowMode")));
#endif

#ifdef INCLUDE_CONN_SPEED_ITEM
   opts.clear();
   opts.push_back("VERY LOW");
   opts.push_back("LOW");
   opts.push_back("MEDIUM");  // there is 5 options, -2 (very low) to 2 (very high)
   opts.push_back("HIGH");
   opts.push_back("VERY HIGH");

   addMenuItem(new ToggleMenuItem("CONNECTION SPEED:", opts, settings->getIniSettings()->connectionSpeed + 2, true, 
                                  setConnectionSpeedCallback, "Speed of your connection, if your ping goes too high, try slower speed.",  KEY_E));
#endif
}


// Save options to INI file, and return to our regularly scheduled program
void OptionsMenuUserInterface::onEscape()
{
   bool autologin = getMenuItem(3)->getIntValue();

   getGame()->getSettings()->setAutologin(autologin);

   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
   getUIManager()->reactivatePrevUI();      //mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
InputOptionsMenuUserInterface::InputOptionsMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "INPUT OPTIONS";
}


// Destructor
InputOptionsMenuUserInterface::~InputOptionsMenuUserInterface()
{
   // Do nothing
}


void InputOptionsMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


void InputOptionsMenuUserInterface::render()
{
   Parent::render();
}

//////////
// Callbacks for InputOptions menu
static void setControlsCallback(ClientGame *game, U32 val)
{
   game->getSettings()->getIniSettings()->mSettings.setVal("ControlMode", RelAbs(val));
}


static void defineKeysCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<KeyDefMenuUserInterface>();
}


static void addControllerOptions(Vector<string> *opts)
{
   opts->clear();
   opts->push_back("Keyboard");
   
   map<S32,string>::iterator it;
   for(it = GameSettings::DetectedControllerList.begin();
         it != GameSettings::DetectedControllerList.end(); it++)
   {
      // Not too long of a string or we'll overflow the menu option
      string name = it->second;
      if(name.size() >= 20)
         name = name.substr(0, 17) + "...";

      opts->push_back("Controller " + itos(it->first + 1) + ": " + name);
   }
}


static S32 INPUT_MODE_MENU_ITEM_INDEX = 0;

// Must be static; keeps track of the number of sticks the user had last time
// the setInputModeCallback was run. That lets the function know if it needs
// to rebuild the menu because of new stick values available.
static S32 controllers = -1;

static void setInputModeCallback(ClientGame *game, U32 inputModeIndex)
{
   GameSettings *settings = game->getSettings();

   // Refills GameSettings::DetectedJoystickNameList to allow people to plug in joystick while in this menu...
   Joystick::initJoystick(settings);

   // If there is a different number of sticks than previously detected
   if(controllers != S32(GameSettings::DetectedControllerList.size()))
   {
      ToggleMenuItem *menuItem = dynamic_cast<ToggleMenuItem *>(game->getUIManager()->getUI<InputOptionsMenuUserInterface>()->
                                                                getMenuItem(INPUT_MODE_MENU_ITEM_INDEX));

      // Rebuild this menu with the new number of sticks
      if(menuItem)
         addControllerOptions(&menuItem->mOptions);

      // Loop back to the first index if we hit the end of the list
      if(inputModeIndex > (U32)GameSettings::DetectedControllerList.size())
      {
         inputModeIndex = 0;
         menuItem->setValueIndex(0);
      }

      // Special case handler for common situation
      if(controllers == 0 && GameSettings::DetectedControllerList.size() == 1)      // User just plugged a stick in
         menuItem->setValueIndex(1);

      // Save the current number of sticks
      controllers = S32(GameSettings::DetectedControllerList.size());
   }

   if(inputModeIndex == 0)
      settings->getInputCodeManager()->setInputMode(InputModeKeyboard);
   else
      settings->getInputCodeManager()->setInputMode(InputModeJoystick);


   if(inputModeIndex >= 1)
      GameSettings::UseControllerIndex = inputModeIndex - 1;

   Joystick::enableJoystick(settings, true);
}


//////////

void InputOptionsMenuUserInterface::setupMenus()
{
   clearMenuItems();
   
   Vector<string> opts;

   GameSettings *settings = getGame()->getSettings();

   Joystick::initJoystick(settings);            // Refresh joystick list
   Joystick::enableJoystick(settings, true);    // Refresh joystick list

   addControllerOptions(&opts);

   // Weird.  must re-engineer
   U32 inputMode = (U32)settings->getInputMode();   // 0 = keyboard, 1 = joystick
   if(inputMode == InputModeJoystick)
      inputMode += GameSettings::UseControllerIndex;

   addMenuItem(new ToggleMenuItem("PRIMARY INPUT:", 
                                  opts, 
                                  inputMode,
                                  true, 
                                  setInputModeCallback, 
                                  "Specify whether you want to play with your keyboard or joystick", 
                                  KEY_P, KEY_I));

   INPUT_MODE_MENU_ITEM_INDEX = getMenuItemCount() - 1;

   addMenuItem(new MenuItem(getMenuItemCount(), "DEFINE KEYS / BUTTONS", defineKeysCallback,
                            "Remap keyboard or joystick controls", KEY_D, KEY_K));

   opts.clear();
   opts.push_back(ucase(toString(Relative)));
   opts.push_back(ucase(toString(Absolute)));
   TNLAssert(Relative < Absolute, "Items added in wrong order!");

   RelAbs mode = settings->getIniSettings()->mSettings.getVal<RelAbs>("ControlMode");

   addMenuItem(new ToggleMenuItem("CONTROLS:", opts, (U32)mode, true, 
                                  setControlsCallback, "Set controls to absolute (normal) or relative (like a tank) mode", KEY_C));
}


// Save options to INI file, and return to our regularly scheduled program
void InputOptionsMenuUserInterface::onEscape()
{
   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
   getUIManager()->reactivatePrevUI();      
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SoundOptionsMenuUserInterface::SoundOptionsMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "SOUND OPTIONS";
}


// Destructor
SoundOptionsMenuUserInterface::~SoundOptionsMenuUserInterface()
{
   // Do nothing
}


void SoundOptionsMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


static string getVolMsg(F32 volume)
{
   S32 vol = U32((volume + 0.05) * 10.0);

   string msg = itos(vol);

   if(vol == 0)
      msg += " [MUTE]";

   return msg;
}


//////////
// Callbacks for SoundOptions menu
static void setSFXVolumeCallback(ClientGame *game, U32 vol)
{
   game->getSettings()->getIniSettings()->sfxVolLevel = F32(vol) / 10;
}

static void setMusicVolumeCallback(ClientGame *game, U32 vol)
{
   game->getSettings()->getIniSettings()->setMusicVolLevel(F32(vol) / 10);
}

static void setVoiceVolumeCallback(ClientGame *game, U32 vol)
{
   F32 oldVol = game->getSettings()->getIniSettings()->voiceChatVolLevel;
   game->getSettings()->getIniSettings()->voiceChatVolLevel = F32(vol) / 10;
   if((oldVol == 0) != (vol == 0) && game->getConnectionToServer())
      game->getConnectionToServer()->s2rVoiceChatEnable(vol != 0);
}


static void setVoiceEchoCallback(ClientGame *game, U32 val)
{
   game->getSettings()->getIniSettings()->mSettings.setVal("VoiceEcho", YesNo(val));
}


void SoundOptionsMenuUserInterface::setupMenus()
{
   clearMenuItems();
   Vector<string> opts;

   GameSettings *settings = getGame()->getSettings();

   for(S32 i = 0; i <= 10; i++)
      opts.push_back(getVolMsg( F32(i) / 10 ));

   addMenuItem(new ToggleMenuItem("SFX VOLUME:",        opts, U32((settings->getIniSettings()->sfxVolLevel + 0.05) * 10.0), false, 
                                  setSFXVolumeCallback,   "Set sound effects volume", KEY_S));

   if(settings->getSpecified(NO_MUSIC))
         addMenuItem(new MessageMenuItem("MUSIC MUTED FROM COMMAND LINE", Colors::red));
   else
      addMenuItem(new ToggleMenuItem("MUSIC VOLUME:",      opts, U32((settings->getIniSettings()->getMusicVolLevel() + 0.05) * 10.0), false,
                                     setMusicVolumeCallback, "Set music volume", KEY_M));

   addMenuItem(new ToggleMenuItem("VOICE CHAT VOLUME:", opts, U32((settings->getIniSettings()->voiceChatVolLevel + 0.05) * 10.0), false, 
                                  setVoiceVolumeCallback, "Set voice chat volume", KEY_V));
   opts.clear();
   opts.push_back("DISABLED");      // No == 0
   opts.push_back("ENABLED");       // Yes == 1
   addMenuItem(new ToggleMenuItem("VOICE ECHO:", opts, (U32)settings->getIniSettings()->mSettings.getVal<YesNo>("VoiceEcho"), true, 
                                  setVoiceEchoCallback, "Toggle whether you hear your voice on voice chat",  KEY_E));
}


// Save options to INI file, and return to our regularly scheduled program
void SoundOptionsMenuUserInterface::onEscape()
{
   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
   getUIManager()->reactivatePrevUI();      //mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
InGameHelpOptionsUserInterface::InGameHelpOptionsUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "IN-GAME HELP OPTIONS";
}


// Destructor
InGameHelpOptionsUserInterface::~InGameHelpOptionsUserInterface()
{
   // Do nothing
}


void InGameHelpOptionsUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


static void resetMessagesCallback(ClientGame *game, U32 val)
{
   game->resetInGameHelpMessages();

   game->getUIManager()->getUI<InGameHelpOptionsUserInterface>()->setFadingNotice(FOUR_SECONDS, 400, "Messages Reset");
}


void InGameHelpOptionsUserInterface::setupMenus()
{
   clearMenuItems();

   GameSettings *settings = getGame()->getSettings();

   bool showingInGameHelp = settings->getShowingInGameHelp();
   addMenuItem(new YesNoMenuItem("SHOW IN-GAME HELP:", showingInGameHelp, "Show help/tutorial messages in game", KEY_H));

   addMenuItem(new MenuItem(getMenuItemCount(), "RESET HELP MESSAGES", resetMessagesCallback, 
                           "Reset all help/tutorial messages to their unseen state", KEY_R));
}


// Save options to INI file, and return to our regularly scheduled program
void InGameHelpOptionsUserInterface::onEscape()
{
   bool show = getMenuItem(0)->getIntValue() == 1;    // 1 ==> Yes

   getGame()->setShowingInGameHelp(show);
   
   getGame()->getSettings()->setShowingInGameHelp(show);
   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());

   getUIManager()->reactivatePrevUI();      //mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
RobotOptionsMenuUserInterface::RobotOptionsMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "ROBOT OPTIONS";
}


// Destructor
RobotOptionsMenuUserInterface::~RobotOptionsMenuUserInterface()
{
   // Do nothing
}


void RobotOptionsMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


void RobotOptionsMenuUserInterface::setupMenus()
{
   clearMenuItems();

   IniSettings *iniSettings = getGame()->getSettings()->getIniSettings();

   addMenuItem(new YesNoMenuItem("PLAY WITH BOTS:", iniSettings->playWithBots,
         "Add robots to balance the teams?",  KEY_B, KEY_P));

    // This doesn't have a callback so we'll handle it in onEscape - make sure to set the correct index!
   addMenuItem(new CounterMenuItem("MINIMUM PLAYERS:", iniSettings->minBalancedPlayers,
         1, 2, 32, "bots", "", "Bots will be added until total player count meets this value", KEY_M));
}


// Save options to INI file
void RobotOptionsMenuUserInterface::onEscape()
{
   saveSettings();
   getUIManager()->reactivatePrevUI();
}


void RobotOptionsMenuUserInterface::saveSettings()
{
   // Save our minimum players, get the correct index of the appropriate menu item
   getGame()->getSettings()->getIniSettings()->playWithBots = getMenuItem(0)->getIntValue() == 1;
   getGame()->getSettings()->getIniSettings()->minBalancedPlayers = getMenuItem(1)->getIntValue();

   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ServerAdvancedMenuUserInterface::ServerAdvancedMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "ADVANCED OPTIONS";
}


// Destructor
ServerAdvancedMenuUserInterface::~ServerAdvancedMenuUserInterface()
{
   // Do nothing
}


void ServerAdvancedMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


static void hostOnServerCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getUI<QueryServersUserInterface>()->mHostOnServer = true;
   game->getUIManager()->activate<QueryServersUserInterface>();
}


void ServerAdvancedMenuUserInterface::setupMenus()
{
   clearMenuItems();

   GameSettings *settings = getGame()->getSettings();

   addMenuItem(new TextEntryMenuItem("GLOBAL SCRIPT:", settings->getGlobalLevelgenScript(),
      "<None>", "Levelgen script to run with every level", MaxWelcomeMessageLen, KEY_S));

   addMenuItem(new YesNoMenuItem("ALLOW MAP DOWNLOADS:", settings->getIniSettings()->allowGetMap,
         "Can users download maps from this server", KEY_M));

   addMenuItem(new YesNoMenuItem("RECORD GAMES:", settings->getIniSettings()->enableGameRecording,
         "Will the server record games (requires lots of disk space)", KEY_R));

   // Note, Don't move "HOST ON SERVER" above "RECORD GAMES" unless
   // first checking HostMenuUserInterface::saveSettings if it saves correctly
   if(getGame()->getConnectionToMaster() && getGame()->getConnectionToMaster()->isHostOnServerAvailable())
      addMenuItem(new MenuItem("HOST ON SERVER", hostOnServerCallback, "Upload and run levels on a proxy server", KEY_H));
}


// Save options to INI file
void ServerAdvancedMenuUserInterface::onEscape()
{
   saveSettings();
   getUIManager()->reactivatePrevUI();
}


void ServerAdvancedMenuUserInterface::saveSettings()
{
//   // Save our minimum players, get the correct index of the appropriate menu item
//   getGame()->getSettings()->getIniSettings()->playWithBots = getMenuItem(0)->getIntValue() == 1;
//   getGame()->getSettings()->getIniSettings()->minBalancedPlayers = getMenuItem(1)->getIntValue();
//
//   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
   GameSettings *settings = getGame()->getSettings();

   settings->setGlobalLevelgenScript(getMenuItem(OPT_GLOBALSCR)->getValue());
   settings->getIniSettings()->allowGetMap = (getMenuItem(OPT_GETMAP)->getIntValue() != 0);
   settings->getIniSettings()->enableGameRecording = (getMenuItem(OPT_RECORD)->getIntValue() != 0);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
ServerPasswordsMenuUserInterface::ServerPasswordsMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "SERVER PASSWORDS";
}


// Destructor
ServerPasswordsMenuUserInterface::~ServerPasswordsMenuUserInterface()
{
   // Do nothing
}


void ServerPasswordsMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


static S32 LevelChangePwItemIndex = -1;
static S32 AdminPwItemIndex = -1;
static S32 ConnectionPwItemIndex = -1;

void ServerPasswordsMenuUserInterface::setupMenus()
{
   clearMenuItems();

   GameSettings *settings = getGame()->getSettings();

   LevelChangePwItemIndex =
   addMenuItem(new TextEntryMenuItem("LEVEL CHANGE PASSWORD:", settings->getLevelChangePassword(), 
                                     "<Anyone can change levels>", 
                                     "Grants access to change the levels, and set duration and winning score", 
                                     MAX_PASSWORD_LENGTH, KEY_L));

   AdminPwItemIndex =
   addMenuItem(new TextEntryMenuItem("ADMIN PASSWORD:", settings->getAdminPassword(),       
                                     "<No remote admin access>", 
                                     "Allows you to kick/ban players, change their teams, and set most server parameters", 
                                     MAX_PASSWORD_LENGTH, KEY_A));

   ConnectionPwItemIndex =
   addMenuItem(new TextEntryMenuItem("CONNECTION PASSWORD:", settings->getServerPassword(), 
                                     "<Anyone can connect>", 
                                     "If the Connection password is set, players need to know it to join the server", 
                                     MAX_PASSWORD_LENGTH, KEY_C));
}


// Save options to INI file
void ServerPasswordsMenuUserInterface::onEscape()
{
   saveSettings();
   getUIManager()->reactivatePrevUI();
}


void ServerPasswordsMenuUserInterface::saveSettings()
{
   TNLAssert(LevelChangePwItemIndex != -1, "Need to call setupMenus first!");
   GameSettings *settings = getGame()->getSettings();

   settings->setAdminPassword      (getMenuItem(AdminPwItemIndex)->getValue(),       true);
   settings->setLevelChangePassword(getMenuItem(LevelChangePwItemIndex)->getValue(), true);
   settings->setServerPassword     (getMenuItem(ConnectionPwItemIndex)->getValue(),  true);

   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
NameEntryUserInterface::NameEntryUserInterface(ClientGame *game) : Parent(game)
{
   mMenuTitle = "";
   mReason = NetConnection::ReasonNone;
   mRenderInstructions = false;
}


// Destructor
NameEntryUserInterface::~NameEntryUserInterface()
{
   // Do nothing
}


void NameEntryUserInterface::setReactivationReason(NetConnection::TerminationReason reason) 
{ 
   mReason = reason; 
   mMenuTitle = ""; 
}


void NameEntryUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenu();
   getGame()->setReadyToConnectToMaster(false);
}


// User has entered name and password, and has clicked OK
static void nameAndPasswordAcceptCallback(ClientGame *clientGame, U32 unused)
{
   UIManager *uiManager = clientGame->getUIManager();
   NameEntryUserInterface *ui = uiManager->getUI<NameEntryUserInterface>();

   if(uiManager->hasPrevUI())
      uiManager->reactivatePrevUI();
   else
      uiManager->activate<MainMenuUserInterface>();

   string enteredName = ui->getMenuItem(1)->getValueForWritingToLevelFile();

   string enteredPassword;
   bool savePassword = false;

   if(ui->getMenuItemCount() > 2)
   {
      enteredPassword = ui->getMenuItem(2)->getValueForWritingToLevelFile();
      savePassword    = ui->getMenuItem(3)->getIntValue() != 0;
   }

   clientGame->userEnteredLoginCredentials(enteredName, enteredPassword, savePassword);
}


//static bool first = true;

void NameEntryUserInterface::setupMenu()
{
   clearMenuItems();
   mRenderSpecialInstructions = false;

   addMenuItem(new MenuItem("PLAY", nameAndPasswordAcceptCallback, ""));
   addMenuItem(new TextEntryMenuItem("NICKNAME:", getGame()->getSettings()->getIniSettings()->mSettings.getVal<string>("LastName"), 
                                    getGame()->getSettings()->getDefaultName(), "", MAX_PLAYER_NAME_LENGTH));

   getMenuItem(1)->setFilter(nickNameFilter);  // Quotes are incompatible with PHPBB3 logins, %s are used for var substitution

   //if(!first)
   //{
      MenuItem *menuItem;

      menuItem = new TextEntryMenuItem("PASSWORD:", getGame()->getSettings()->getPlayerPassword(), "", "", MAX_PLAYER_PASSWORD_LENGTH);
      menuItem->setSecret(true);
      addMenuItem(menuItem);

      // If we have already saved a PW, this defaults to yes; to no otherwise
      menuItem = new YesNoMenuItem("SAVE PASSWORD:", getGame()->getSettings()->getPlayerPassword() != "", "");
      menuItem->setSize(MENU_ITEM_SIZE_SMALL);
      addMenuItem(menuItem);
   //}

   //first = false;
}


void NameEntryUserInterface::renderExtras() const
{
   const S32 size = 15;
   const S32 gap = 5;
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   const S32 rows = 3;
   S32 row = 0;

   S32 instrGap = mRenderInstructions ? 30 : 0;

   Renderer::get().setColor(Colors::menuHelpColor);

   row++;

   drawCenteredString(canvasHeight - vertMargin - instrGap - (rows - row) * size - (rows - row) * gap, size, 
            "A password is only needed if you are using a reserved name.  You can reserve your");
   row++;

   drawCenteredString(canvasHeight - vertMargin - instrGap - (rows - row) * size - (rows - row) * gap, size, 
            "nickname by registering for the bitfighter.org forums.  Registration is free.");


   if(mReason == NetConnection::ReasonBadLogin || mReason == NetConnection::ReasonInvalidUsername)
   {
      string message = "If you have reserved this name by registering for "
                       "the forums, enter your forum password below. Otherwise, "
                       "this user name may be reserved. Please choose another.";

      renderMessageBox("Invalid Name or Password", "Press [[Esc]] to continue", message, 3, -190);
   }
}


// Save options to INI file, and return to our regularly scheduled program
void NameEntryUserInterface::onEscape()
{
   shutdownBitfighter();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
HostMenuUserInterface::HostMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   mMenuTitle ="HOST A GAME";

   mEditingIndex = -1;     // Not editing at the start
}


// Destructor
HostMenuUserInterface::~HostMenuUserInterface()
{
   // Do nothing
}


void HostMenuUserInterface::onActivate()
{
   Parent::onActivate();
   setupMenus();
}


static void startHostingCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->getUI<HostMenuUserInterface>()->saveSettings();

   GameSettingsPtr settings = game->getSettingsPtr();

   LevelSourcePtr levelSource = LevelSourcePtr(settings->chooseLevelSource(game));

   initHosting(settings, levelSource, false, false);
}


static void robotOptionsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<RobotOptionsMenuUserInterface>();
}


static void passwordOptionsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<ServerPasswordsMenuUserInterface>();

}


static void advancedOptionsSelectedCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<ServerAdvancedMenuUserInterface>();

}


static void playbackGamesCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<PlaybackSelectUserInterface>();
}


void HostMenuUserInterface::setupMenus()
{
   clearMenuItems();

   GameSettings *settings = getGame()->getSettings();

   // These menu items MUST align with the MenuItems enum
   addMenuItem(new MenuItem("START HOSTING", startHostingCallback, "", KEY_H));

   addMenuItem(new MenuItem(getMenuItemCount(), "ROBOTS", robotOptionsSelectedCallback,
         "Add robots and adjust their settings", KEY_R));

   addMenuItem(new TextEntryMenuItem("SERVER NAME:", settings->getHostName(), 
                                     "<Bitfighter Host>", "Server name shown in the game lobby",
                                     MaxServerNameLen,  KEY_N));

   addMenuItem(new TextEntryMenuItem("DESCRIPTION:", settings->getHostDescr(),                    
                                     "<Empty>", "Server description shown in the game lobby",
                                     MaxServerDescrLen, KEY_D));

   addMenuItem(new TextEntryMenuItem("WELCOME MSG:", settings->getWelcomeMessage(),
                                       "<Empty>", "Message shown to players when they join the server",
                                       MaxWelcomeMessageLen, KEY_W));

   addMenuItem(new MenuItem(getMenuItemCount(), "PASSWORDS", passwordOptionsSelectedCallback,
         "Set server passwords/permissions", KEY_P));

   addMenuItem(new MenuItem(getMenuItemCount(), "ADVANCED", advancedOptionsSelectedCallback,
         "Other advanced server options", KEY_A));

   addMenuItem(new MenuItem("PLAYBACK GAMES",    playbackGamesCallback,  "Playback previously recorded games"));
}


// Save options to INI file, and return to our regularly scheduled program
// This only gets called when escape not already handled by preprocessKeys(), i.e. when we're not editing
void HostMenuUserInterface::onEscape()
{
   saveSettings();
   getUIManager()->reactivatePrevUI();     
}


// Save parameters and get them into the INI file
void HostMenuUserInterface::saveSettings()
{
   GameSettings *settings = getGame()->getSettings();

   settings->setHostName (getMenuItem(OPT_NAME)->getValue(),  true);
   settings->setHostDescr(getMenuItem(OPT_DESCR)->getValue(), true);
   settings->setWelcomeMessage(getMenuItem(OPT_WELCOME)->getValue(), true);

   saveSettingsToINI(&GameSettings::iniFile, getGame()->getSettings());
}


void HostMenuUserInterface::render()
{
   Parent::render();
   getUIManager()->renderLevelListDisplayer();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
GameMenuUserInterface::GameMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   mMenuTitle = "GAME MENU";
}


// Destructor
GameMenuUserInterface::~GameMenuUserInterface()
{
   // Do nothing
}


void GameMenuUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   GameConnection *gc = getGame()->getConnectionToServer();

   if(gc && gc->waitingForPermissionsReply() && gc->gotPermissionsReply())      // We're waiting for a reply, and it has arrived
   {
      gc->setWaitingForPermissionsReply(false);
      buildMenu();                                                   // Update menu to reflect newly available options
   }
}


void GameMenuUserInterface::onActivate()
{
   Parent::onActivate();
   buildMenu();
   mMenuSubTitle = "";
   mMenuSubTitleColor = Colors::cyan;
}


void GameMenuUserInterface::onReactivate()
{
   mMenuSubTitle = "";
}


static void endGameCallback(ClientGame *game, U32 unused)
{
   game->closeConnectionToGameServer();

   GameManager::deleteServerGame();
}


static void addTwoMinsCallback(ClientGame *game, U32 unused)
{
   if(game->getGameType())
      game->getGameType()->addTime(2 * 60 * 1000);

   game->getUIManager()->reactivatePrevUI();     // And back to our regularly scheduled programming!
}


static void chooseNewLevelCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<LevelMenuUserInterface>();
}


static void restartGameCallback(ClientGame *game, U32 unused)
{
   game->getConnectionToServer()->c2sRequestLevelChange(REPLAY_LEVEL, false);
   game->getUIManager()->reactivatePrevUI();     // And back to our regularly scheduled programming! 
}


static void robotsGameCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<RobotsMenuUserInterface>();
}


static void levelChangeOrAdminPWCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<LevelChangeOrAdminPasswordEntryUserInterface>();
}


static void kickPlayerCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->showPlayerActionMenu(PlayerActionKick);
}


static void downloadRecordedGameCallback(ClientGame *game, U32 unused)
{
   game->getUIManager()->activate<PlaybackServerDownloadUserInterface>();
}


void GameMenuUserInterface::buildMenu()
{
   clearMenuItems();
   GameSettings *settings = getGame()->getSettings();
   
   // Save input mode so we can see if we need to display alert if it changes
   lastInputMode = settings->getInputMode();  

   addMenuItem(new MenuItem("OPTIONS",      optionsSelectedCallback, "", KEY_O));
   addMenuItem(new MenuItem("INSTRUCTIONS", helpSelectedCallback,    "", KEY_I, getInputCode(settings, BINDING_HELP)));


   GameConnection *gc = (getGame())->getConnectionToServer();

   if(gc)
   {
      // Add normal menu options for when we're not playing recorded games
      if(dynamic_cast<GameRecorderPlayback *>(gc) == NULL)
      {
         GameType *gameType = getGame()->getGameType();

         // Add any game-specific menu items
         if(gameType)
         {
            mGameType = gameType;
            gameType->addClientGameMenuOptions(getGame(), this);
         }

         if(gc->getClientInfo()->isLevelChanger())
         {
            addMenuItem(new MenuItem("ROBOTS",               robotsGameCallback,     "", KEY_B, KEY_R));
            addMenuItem(new MenuItem("PLAY DIFFERENT LEVEL", chooseNewLevelCallback, "", KEY_L, KEY_P));
            addMenuItem(new MenuItem("ADD TIME (2 MINS)",    addTwoMinsCallback,     "", KEY_T, KEY_2));
            addMenuItem(new MenuItem("RESTART LEVEL",        restartGameCallback,    ""));
         }

         if(gc->getClientInfo()->isAdmin())
         {
            // Add any game-specific menu items
            if(gameType)
            {
               mGameType = gameType;
               gameType->addAdminGameMenuOptions(this);
            }

            addMenuItem(new MenuItem("KICK A PLAYER", kickPlayerCallback, "", KEY_K));
         }

         // Owner already has max permissions, so don't show option to enter a password
         if(!gc->getClientInfo()->isOwner())
            addMenuItem(new MenuItem("ENTER PASSWORD", levelChangeOrAdminPWCallback, "", KEY_A, KEY_E));

         if((gc->mSendableFlags & GameConnection::ServerFlagHasRecordedGameplayDownloads) && !gc->isLocalConnection())
            addMenuItem(new MenuItem("DOWNLOAD RECORDED GAME", downloadRecordedGameCallback, ""));
         }

      // Else add these options if we're playing recorded games
      else
      {
         addMenuItem(new MenuItem("PLAYBACK GAMES", playbackGamesCallback, "Playback previously recorded games"));
      }
   }

   if(getUIManager()->cameFrom<EditorUserInterface>())    // Came from editor
      addMenuItem(new MenuItem("RETURN TO EDITOR", endGameCallback, "", KEY_Q, KEY_R));
   else
      addMenuItem(new MenuItem("QUIT GAME",        endGameCallback, "", KEY_Q));
}


void GameMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();      //mGameUserInterface

   // Show alert about input mode changing, if needed
   bool inputModesChanged = (lastInputMode != getGame()->getInputMode());
   getUIManager()->getUI<GameUserInterface>()->resetInputModeChangeAlertDisplayTimer(inputModesChanged ? 2800 : 0);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelMenuUserInterface::LevelMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   // Do nothing
}


// Destructor
LevelMenuUserInterface::~LevelMenuUserInterface()
{
   // Do nothing
}


static const char *UPLOAD_LEVELS = "UPLOAD LEVELS";
static const char *ALL_LEVELS = "All Levels";
static const U32 ALL_LEVELS_MENUID = 0x80000001;
static const U32 UPLOAD_LEVELS_MENUID = 0x80000002;


static void selectLevelTypeCallback(ClientGame *game, U32 level)
{
   LevelMenuSelectUserInterface *ui = game->getUIManager()->getUI<LevelMenuSelectUserInterface>();

   // First entry will be "All Levels", subsequent entries will be level types populated from mLevelInfos
   if(level == ALL_LEVELS_MENUID)
      ui->category = ALL_LEVELS;
   else if(level == UPLOAD_LEVELS_MENUID)
      ui->category = UPLOAD_LEVELS;

   else
   {
      // Replace the following with a getLevelCount() function on game??
      GameConnection *gc = game->getConnectionToServer();
      if(!gc || U32(gc->mLevelInfos.size()) < level)
         return;

      ui->category = gc->mLevelInfos[level - 1].getLevelTypeName();
   }

  game->getUIManager()->activate(ui);
}


void LevelMenuUserInterface::onActivate()
{
   Parent::onActivate();
   mMenuTitle = "CHOOSE LEVEL TYPE";

   // replace with getLevelCount() method on game?
   GameConnection *gc = getGame()->getConnectionToServer();
   if(!gc || !gc->mLevelInfos.size())
      return;

   clearMenuItems();

   char c[] = "A";   // Shortcut key
   addMenuItem(new MenuItem(ALL_LEVELS_MENUID, ALL_LEVELS, selectLevelTypeCallback, "", InputCodeManager::stringToInputCode(c)));

   // Cycle through all levels, looking for unique type strings
   for(S32 i = 0; i < gc->mLevelInfos.size(); i++)
   {
      bool found = false;

      for(S32 j = 0; j < getMenuItemCount(); j++)
         if(strcmp(gc->mLevelInfos[i].getLevelTypeName(), "") == 0 || 
            strcmp(gc->mLevelInfos[i].getLevelTypeName(), getMenuItem(j)->getPrompt().c_str()) == 0)     
         {
            found = true;
            break;            // Skip over levels with blank names or duplicate entries
         }

      if(!found)              // Not found above, must be a new type
      {
         const char *gameTypeName = gc->mLevelInfos[i].getLevelTypeName();
         c[0] = gameTypeName[0];
         c[1] = '\0';
//         logprintf("LEVEL - %s", name.c_str());
         addMenuItem(new MenuItem(i + 1, gameTypeName, selectLevelTypeCallback, "", InputCodeManager::stringToInputCode(c)));
      }
   }

   sortMenuItems();

   if((gc->mSendableFlags & GameConnection::ServerFlagAllowUpload) && !gc->isLocalConnection())   // local connection is useless, already have all maps..
      addMenuItem(new MenuItem(UPLOAD_LEVELS_MENUID, UPLOAD_LEVELS, selectLevelTypeCallback, "", InputCodeManager::stringToInputCode(c)));
}


void LevelMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();    // to mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
RobotsMenuUserInterface::RobotsMenuUserInterface(ClientGame *game) : MenuUserInterface(game)
{
   // Do nothing
}


// Destructor
RobotsMenuUserInterface::~RobotsMenuUserInterface()
{
   // Do nothing
}


// Can only get here if the player has the appropriate permissions, so no need for a further check
static void moreRobotsAcceptCallback(ClientGame *game, U32 index)
{
   GameType *gameType = game->getGameType();

   if(gameType)
   {
      Vector<StringPtr> args;
      gameType->c2sSendCommand("MoreBots", args);
   }

   // Player has demonstrated ability to add bots... no need to show help item
   game->getUIManager()->getUI<GameUserInterface>()->removeInlineHelpItem(AddBotsItem, true);

   // Back to the game!   
   game->getUIManager()->reactivateGameUI();
}


// Can only get here if the player has the appropriate permissions, so no need for a further check
static void fewerRobotsAcceptCallback(ClientGame *game, U32 index)
{
   if(game->getBotCount() == 0)
      game->displayErrorMessage("!!! There are no robots to kick");

   GameType *gameType = game->getGameType();

   if(gameType)
   {
      Vector<StringPtr> args;
      gameType->c2sSendCommand("FewerBots", args);
   }

   // Back to the game!
   game->getUIManager()->reactivateGameUI();
}


static void removeRobotsAcceptCallback(ClientGame *game, U32 index)
{
   game->getGameType()->c2sKickBots();
   game->getUIManager()->reactivateGameUI();
}


void RobotsMenuUserInterface::onActivate()
{
   Parent::onActivate();

   clearMenuItems();

   addMenuItem(new MenuItem("MORE ROBOTS",       moreRobotsAcceptCallback,   "Add a robot to each team",        KEY_M));
   addMenuItem(new MenuItem("FEWER ROBOTS",      fewerRobotsAcceptCallback,  "Remove a robot from each team",   KEY_F));
   addMenuItem(new MenuItem("REMOVE ALL ROBOTS", removeRobotsAcceptCallback, "Remove all robots from the game", KEY_R));
}


void RobotsMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();    // to mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LevelMenuSelectUserInterface::LevelMenuSelectUserInterface(ClientGame *game) : Parent(game)
{
   // When you start typing a name, any character typed within the mStillTypingNameTimer period will be considered
   // to be the next character of the name, rather than a new entry
   mStillTypingNameTimer.setPeriod(1000);
}


// Destructor
LevelMenuSelectUserInterface::~LevelMenuSelectUserInterface()
{
   // Do nothing
}


static void processLevelSelectionCallback(ClientGame *game, U32 index)             
{
   game->getUIManager()->getUI<LevelMenuSelectUserInterface>()->processSelection(index);
}


const U32 UPLOAD_LEVELS_BIT = 0x80000000;

void LevelMenuSelectUserInterface::processSelection(U32 index)     
{
   Parent::onActivate();
   GameConnection *gc = getGame()->getConnectionToServer();

   if((index & UPLOAD_LEVELS_BIT) && (index & (~UPLOAD_LEVELS_BIT)) < U32(mLevels.size()))
   {
      FolderManager *folderManager = getGame()->getSettings()->getFolderManager();
      string filename = strictjoindir(folderManager->levelDir, mLevels[index & (~UPLOAD_LEVELS_BIT)]);

      if(!gc->TransferLevelFile(filename.c_str()))
         getGame()->displayErrorMessage("!!! Can't upload level: unable to read file");
   }
   else
      gc->c2sRequestLevelChange(index, false);     // The selection index is the level to load

   getUIManager()->reactivateGameUI();             // Back to the game
}


void LevelMenuSelectUserInterface::onActivate()
{
   Parent::onActivate();
   mMenuTitle = "CHOOSE LEVEL [" + category + "]";

   mNameSoFar = "";
   mStillTypingNameTimer.clear();

   // Replace with a getLevelCount() method on Game?
   ClientGame *game = getGame();
   GameConnection *gc = game->getConnectionToServer();

   if(!gc || !gc->mLevelInfos.size())
      return;

   clearMenuItems();

   mLevels.clear();

   char c[2];
   c[1] = 0;   // null termination

   if(!strcmp(category.c_str(), UPLOAD_LEVELS))
   {
      // Get all the playable levels in levelDir
      mLevels = getGame()->getSettings()->getLevelList();     

      for(S32 i = 0; i < mLevels.size(); i++)
      {
         c[0] = mLevels[i].c_str()[0];
         addMenuItem(new MenuItem(i | UPLOAD_LEVELS_BIT, mLevels[i].c_str(), processLevelSelectionCallback, "", InputCodeManager::stringToInputCode(c)));
      }
   }
 
   for(S32 i = 0; i < gc->mLevelInfos.size(); i++)
   {
      if(gc->mLevelInfos[i].mLevelName == "")   // Skip levels with blank names --> but all should have names now!
         continue;

      if(strcmp(gc->mLevelInfos[i].getLevelTypeName(), category.c_str()) == 0 || category == ALL_LEVELS)
      {
         const char *levelName = gc->mLevelInfos[i].mLevelName.getString();
         c[0] = levelName[0];
         addMenuItem(new MenuItem(i, levelName, processLevelSelectionCallback, "", InputCodeManager::stringToInputCode(c)));
      }
   }

   sortMenuItems();
   mFirstVisibleItem = 0;

   if(itemSelectedWithMouse)
      onMouseMoved();
   else
      selectedIndex = 0;
}


void LevelMenuSelectUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);
   if(mStillTypingNameTimer.update(timeDelta))
      mNameSoFar = "";
}


// Override parent, and make keys simply go to first level with that letter, rather than selecting it automatically
bool LevelMenuSelectUserInterface::processMenuSpecificKeys(InputCode inputCode)
{
   string inputString = InputCodeManager::inputCodeToPrintableChar(inputCode);

   if(inputString == "")
      return false;
   
   mNameSoFar.append(inputString);

   string mNameSoFarLc = lcase(mNameSoFar);

   if(stringContainsAllTheSameCharacter(mNameSoFarLc))
   {
      selectedIndex = getIndexOfNext(mNameSoFarLc.substr(0, 1));

      if(mNameSoFar.size() > 1 && lcase(getMenuItem(selectedIndex)->getValue()).substr(0, mNameSoFar.length()) != mNameSoFarLc)
         mNameSoFar = mNameSoFar.substr(0, mNameSoFar.length() - 1);    // Remove final char, the one we just added above
   }
   else
      selectedIndex = getIndexOfNext(mNameSoFarLc);


   mStillTypingNameTimer.reset();
   itemSelectedWithMouse = false;

   // Move the mouse to the new selection to make things "feel better"
   MenuItemSize size = getMenuItem(getOffset())->getSize();
   S32 y = getYStart();

   for(S32 j = getOffset(); j < selectedIndex; j++)
   {
      size = getMenuItem(j)->getSize();
      y += getTextSize(size) + getGap(size);
   }

   y += getTextSize(size) / 2;

   // WarpMouse fires a mouse event, which will cause the cursor to become visible, which we don't want.  Therefore,
   // we must resort to the kind of gimicky/hacky method of setting a flag, telling us that we should ignore the
   // next mouse event that comes our way.  It might be better to handle this at the Event level, by creating a custom
   // method called WarpMouse that adds the suppression.  At this point, however, the only place we care about this
   // is here so...  well... this works.
#ifndef BF_PLATFORM_3DS
   SDL_WarpMouseInWindow(DisplayManager::getScreenInfo()->sdlWindow, (S32)DisplayManager::getScreenInfo()->getMousePos()->x, y);
#endif
   Cursor::disableCursor();
   mIgnoreNextMouseEvent = true;
   playBoop();

   return true;
}


// Return index of next level starting with specified string; if none exists, returns current index.
// If startingWith is only one character, the entry we're looking for could be behind us.  See tests
// for examples of this.
S32 LevelMenuSelectUserInterface::getIndexOfNext(const string &startingWithLc)
{
   TNLAssert(startingWithLc.length() > 0, "Did not expect an empty string here!");
   TNLAssert(startingWithLc == lcase(startingWithLc), "Expected a lowercased string here");

   bool first = true;
   bool multiChar = startingWithLc.length() > 1;
   S32 offset = multiChar ? 0 : 1;

   // Loop until we hit the end of the list, or we hit an item that sorts > our startingString (meaning we overshot).
   // But we only care about overshoots in multiChar mode because there could well be single-char hits behind us in the list.
   while(true)
   {
      if(selectedIndex + offset >= getMenuItemCount())    // Hit end of list -- loop to beginning
         offset = -selectedIndex;

      string prospectiveItem = lcase(getMenuItem(selectedIndex + offset)->getValue());

      if(prospectiveItem.substr(0, startingWithLc.size()) == startingWithLc)
         return selectedIndex + offset;

      if(offset == 0 && !first)
         break;

      offset++;
      first = false;
   }

   // Found no match; return current index
   return selectedIndex;
}


void LevelMenuSelectUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();    // to LevelMenuUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
PlayerMenuUserInterface::PlayerMenuUserInterface(ClientGame *game) : Parent(game)
{
   // Do nothing
}

// Destructor
PlayerMenuUserInterface::~PlayerMenuUserInterface()
{
   // Do nothing
}


static void playerSelectedCallback(ClientGame *game, U32 index) 
{
   game->getUIManager()->getUI<PlayerMenuUserInterface>()->playerSelected(index);
}


void PlayerMenuUserInterface::playerSelected(U32 index)
{
   // When we created the menu, names were not sorted, and item indices were assigned in "natural order".  Then
   // the menu items were sorted by name, and now the indices are now jumbled.  This bit here tries to get the
   // new, actual list index of an item given its original index.
   for(S32 i = 0; i < getMenuItemCount(); i++)
      if(getMenuItem(i)->getIndex() == (S32)index)
      {
         index = i;
         break;
      }

   GameType *gt = getGame()->getGameType();

   if(action == PlayerActionChangeTeam)
   {
      TeamMenuUserInterface *ui = getUIManager()->getUI<TeamMenuUserInterface>();
      ui->nameToChange = getMenuItem(index)->getPrompt();

      getUIManager()->activate<TeamMenuUserInterface>();     // Show menu to let player select a new team
   }

   else if(gt)    // action == Kick
      gt->c2sKickPlayer(getMenuItem(index)->getPrompt());


   if(action != PlayerActionChangeTeam)      // Unless we need to move on to the change team screen...
      getUIManager()->reactivateGameUI();    // ...it's back to the game!
}


// By putting the menu building code in render, menus can be dynamically updated
void PlayerMenuUserInterface::render()
{
   clearMenuItems();

   GameConnection *conn = getGame()->getConnectionToServer();
   if(!conn)
      return;

   char c[] = "A";      // Dummy shortcut key
   for(S32 i = 0; i < getGame()->getClientCount(); i++)
   {
      ClientInfo *clientInfo = ((Game *)getGame())->getClientInfo(i);      // Lame!

      strncpy(c, clientInfo->getName().getString(), 1);        // Grab first char of name for a shortcut key

      // Will be used to show admin/player/robot prefix on menu
      PlayerType pt = clientInfo->isRobot() ? PlayerTypeRobot : (clientInfo->isAdmin() ? PlayerTypeAdmin : PlayerTypePlayer);    

      PlayerMenuItem *newItem = new PlayerMenuItem(i, clientInfo->getName().getString(), playerSelectedCallback, 
                                                   InputCodeManager::stringToInputCode(c), pt);
      newItem->setUnselectedColor(*getGame()->getTeamColor(clientInfo->getTeamIndex()));

      addMenuItem(newItem);
   }

   sortMenuItems();

   if(action == PlayerActionKick)
      mMenuTitle = "CHOOSE PLAYER TO KICK";
   else if(action == PlayerActionChangeTeam)
      mMenuTitle = "CHOOSE WHOSE TEAM TO CHANGE";
   else
      TNLAssert(false, "Unknown action!");

   Parent::render();
}


void PlayerMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();   //mGameUserInterface
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
TeamMenuUserInterface::TeamMenuUserInterface(ClientGame *game) : Parent(game)
{
   mMenuSubTitle = "[Human Players | Bots | Score]";
}

// Destructor
TeamMenuUserInterface::~TeamMenuUserInterface()
{
   // Do nothing
}


static void processTeamSelectionCallback(ClientGame *game, U32 index)        
{
   game->getUIManager()->getUI<TeamMenuUserInterface>()->processSelection(index);
}


void TeamMenuUserInterface::processSelection(U32 index)        
{
   // Make sure user isn't just changing to the team they're already on...
   if(index != (U32)getGame()->getTeamIndex(nameToChange.c_str()))
   {
      // Check if was initiated by an admin (PlayerUI is the kick/change team player-pick admin menu)
      if(getUIManager()->getPrevUI() == getUIManager()->getUI<PlayerMenuUserInterface>())        
      {
         StringTableEntry e(nameToChange.c_str());
         getGame()->changePlayerTeam(e, index);    // Index will be the team index
      }
      else                                         // Came from player changing own team
         getGame()->changeOwnTeam(index); 
   }

   getUIManager()->reactivateGameUI();             // Back to the game!
}


// By reconstructing our menu at render time, changes to teams caused by others will be reflected immediately
void TeamMenuUserInterface::render()
{
   clearMenuItems();

   getGame()->countTeamPlayers();                     // Make sure numPlayers is correctly populated

   char c[] = "A";                                    // Dummy shortcut key, will change below
   for(S32 i = 0; i < getGame()->getTeamCount(); i++)
   {
      AbstractTeam *team = getGame()->getTeam(i);
      strncpy(c, team->getName().getString(), 1);     // Grab first char of name for a shortcut key

      bool isCurrent = (i == getGame()->getTeamIndex(nameToChange.c_str()));
      
      addMenuItem(new TeamMenuItem(i, team, processTeamSelectionCallback, InputCodeManager::stringToInputCode(c), isCurrent));
   }

   string name = "";
   Ship *ship = getGame()->getLocalPlayerShip();

   if(ship && ship->getClientInfo())
      name = ship->getClientInfo()->getName().getString();

   if(name != nameToChange)    // i.e. names differ, this isn't the local player
   {
      name = nameToChange;
      name += " ";
   }
   else
      name = "";

   // Finally, set menu title
   mMenuTitle = "TEAM TO SWITCH " + name + "TO";       // No space before the TO!

   Parent::render();
}


void TeamMenuUserInterface::onEscape()
{
   getUIManager()->reactivatePrevUI();
}



};

