//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIChat.h"

#include "FontManager.h"
#include "UIQueryServers.h"      // For menuID
#include "UIEditor.h"            // For excepting the editor from rendering in the background
#include "UIGame.h"              // For putting private messages into game console
#include "UIManager.h"

#include "masterConnection.h"
#include "DisplayManager.h"
#include "Renderer.h"
#include "ClientGame.h"
#include "Colors.h"
#include "SoundSystem.h"

#include "RenderUtils.h"

#include <algorithm>

namespace Zap
{

// Quickie constructor
ChatMessage::ChatMessage()
{
   /* Do nothing */
}

// "Real" constructor
ChatMessage::ChatMessage(string frm, string msg, Color col, bool isPriv, bool isSys)
{
   color = col;
   message = msg;
   from = frm;
   time = getShortTimeStamp(); // Record time message arrived
   isPrivate = isPriv;
   isSystem = isSys;
}

// Destructor
ChatMessage::~ChatMessage()
{
   // Do nothing
}


Vector<StringTableEntry> AbstractChat::mPlayersInGlobalChat;

const char *ARROW = ">";
const S32 AFTER_ARROW_SPACE = 5;

// Initialize some static vars
U32 AbstractChat::mColorPtr = 0;
U32 AbstractChat::mMessageCount = 0;


// By declaring these here, we avoid link errors
ChatMessage AbstractChat::mMessages[MESSAGES_TO_RETAIN];
std::map<string, Color> AbstractChat::mFromColors;       // Map nicknames to colors


AbstractChat::AbstractChat(ClientGame *game)
{
   mGame = game;
   mLineEditor = LineEditor(200, "", 50);
   mChatCursorPos = 0;
}

AbstractChat::~AbstractChat()
{
   // Do nothing
}

Color AbstractChat::getColor(string name)
{
   if(mFromColors.count(name) == 0)    
      mFromColors[name] = getNextColor();          

   return mFromColors[name]; 
}


// We received a new incoming chat message...  Add it to the list
void AbstractChat::newMessage(const string &from, const string &message, bool isPrivate, bool isSystem, bool fromSelf)
{
   // Don't display it if it is from a muted player
   if(mGame->isOnMuteList(from))
      return;

   // Choose a color
   Color color;

   if(fromSelf)
      color = Colors::white;                  
   else                                       
   {
      if(mFromColors.count(from) == 0)        // See if we have a color for this nick
         mFromColors[from] = getNextColor();  // If not, get one

      color = mFromColors[from];
   }

   mMessages[mMessageCount % MESSAGES_TO_RETAIN] = ChatMessage(from, message, color, isPrivate, isSystem);
   mMessageCount++;

   if(fromSelf && isPrivate)     // I don't think this can ever happen!  ==> Should be !fromSelf ?
      deliverPrivateMessage(from.c_str(), message.c_str());
}


void AbstractChat::setPlayersInGlobalChat(const Vector<StringTableEntry> &playerNicks)
{
   mPlayersInGlobalChat.clear();

   for(S32 i = 0; i < playerNicks.size(); i++)
      mPlayersInGlobalChat.push_back(playerNicks[i]);
}


void AbstractChat::playerJoinedGlobalChat(const StringTableEntry &playerNick)
{
   mPlayersInGlobalChat.push_back(playerNick);

   // Make the following be from us, so it will be colored white
   string msg = "----- Player " + string(playerNick.getString()) + " joined the conversation -----";
   newMessage(mGame->getClientInfo()->getName().getString(), msg, false, true, true);
   SoundSystem::playSoundEffect(SFXPlayerEnteredGlobalChat, mGame->getSettings()->getIniSettings()->sfxVolLevel);   // Make sound?
}


void AbstractChat::playerLeftGlobalChat(const StringTableEntry &playerNick)
{
   ChatUserInterface *ui = mGame->getUIManager()->getUI<ChatUserInterface>();

   for(S32 i = 0; i < ui->mPlayersInGlobalChat.size(); i++)
      if(ui->mPlayersInGlobalChat[i] == playerNick)
      {
         ui->mPlayersInGlobalChat.erase_fast(i);

         string msg = "----- Player " + string(playerNick.getString()) + " left the conversation -----";
         newMessage(mGame->getClientInfo()->getName().getString(), msg, false, true, true);
         
         SoundSystem::playSoundEffect(SFXPlayerLeftGlobalChat, mGame->getSettings()->getIniSettings()->sfxVolLevel);   // Me make sound!
         break;
      }
}


bool AbstractChat::isPlayerInGlobalChat(const StringTableEntry &playerNick)
{
   ChatUserInterface *ui = mGame->getUIManager()->getUI<ChatUserInterface>();

   for(S32 i = 0; i < ui->mPlayersInGlobalChat.size(); i++)
      if(ui->mPlayersInGlobalChat[i] == playerNick)
         return true;

   return false;
}


// We're using a rolling "wrap-around" array, and this figures out which array index we need to retrieve a message.
// First message has index == 0, second has index == 1, etc.
ChatMessage AbstractChat::getMessage(U32 index)
{
   return mMessages[index % MESSAGES_TO_RETAIN];
}


U32 AbstractChat::getMessageCount()
{
   return mMessageCount;
}


bool AbstractChat::composingMessage()
{
   return mLineEditor.length() > 0;
}


// Retrieve the next available chat text color
Color AbstractChat::getNextColor()
{
   static const Color colorList[] = {
      Color(0.55,0.55,0),     Color(1,0.55,0.55),
      Color(0,0.6,0),         Color(0.68,1,0.25),
      Color(0,0.63,0.63),     Color(0.275,0.51,0.71),
      Color(1,1,0),           Color(0.5,0.81,0.37),
      Color(0,0.75,1),        Color(0.93,0.91,0.67),
      Color(1,0.5,1),         Color(1,0.73,0.53),
      Color(0.86,0.078,1),    Color(0.78,0.08,0.52),
      Color(0.93,0.5,0),      Color(0.63,0.32,0.18),
      Color(0.5,1,1),         Color(1,0.73,1),
      Color(0.48,0.41,0.93)
   };

   mColorPtr++;
   if(mColorPtr >= ARRAYSIZE(colorList))     // Wrap-around
      mColorPtr = 0;

   return colorList[mColorPtr];
}


// Announce we're ducking out for a spell...
void AbstractChat::leaveGlobalChat()
{
   MasterServerConnection *conn = mGame->getConnectionToMaster();

   if(conn)
      conn->c2mLeaveGlobalChat();
}


void AbstractChat::renderMessages(U32 ypos, U32 lineCountToDisplay)  // ypos is starting location of first message
{
   // If no messages, don't waste resources on rendering
   if (mMessageCount == 0)
      return;

   FontManager::pushFontContext(ChatMessageContext);

   U32 firstMsg = (mMessageCount <= lineCountToDisplay) ? 0 : (mMessageCount - lineCountToDisplay);  // Don't use min/max because of U32/S32 issues!
   U32 ypos_top = ypos;
   ypos += (CHAT_FONT_SIZE + CHAT_FONT_MARGIN) * lineCountToDisplay;

   // Double pass.  First loop is just to calculate number of lines used, then second pass will render.
   bool renderLoop = false;
   do {
      for(U32 i = lineCountToDisplay - 1; i != U32_MAX; i--)
      {
         // No more rendering - we've rendered to the line count limit
         if(ypos <= ypos_top)
            break;

         // No more messages to display
         if(i >= min(firstMsg + lineCountToDisplay, mMessageCount))
            continue;  // Don't return / break, For loop is running in backwards.

         else
         {
            ChatMessage msg = getMessage(i + firstMsg);
            Renderer::get().setColor(msg.color);

            // Figure out the x position based on the message prefixes
            S32 xpos = UserInterface::horizMargin / 2;

            xpos += getStringWidthf(CHAT_TIME_FONT_SIZE, "[%s] ", msg.time.c_str());
            if(!msg.isSystem)
               xpos += getStringWidth(CHAT_FONT_SIZE, msg.from.c_str());     // No sender for system message
            if(msg.isPrivate)
               xpos += getStringWidth(CHAT_FONT_SIZE, "*");
            if(!msg.isSystem)
               xpos += getStringWidth(CHAT_FONT_SIZE, ARROW) + AFTER_ARROW_SPACE;

            S32 allowedWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth() - (2 * UserInterface::horizMargin) - xpos;

            // Calculate (and draw if in renderLoop) the message lines
            U32 lineCount = drawWrapText(msg.message, xpos, ypos, allowedWidth, ypos_top,
               AbstractChat::CHAT_FONT_SIZE + AbstractChat::CHAT_FONT_MARGIN,  // line height
               AbstractChat::CHAT_FONT_SIZE, // font size
               renderLoop);

            ypos -= (CHAT_FONT_SIZE + CHAT_FONT_MARGIN) * lineCount;

            // Draw the message prefixes
            if(renderLoop)
            {
               xpos = UserInterface::horizMargin / 2;
               xpos += drawStringAndGetWidthf((F32)xpos, F32(ypos + (CHAT_FONT_SIZE - CHAT_TIME_FONT_SIZE) / 2.f + 2),  // + 2 just looks better!
                     CHAT_TIME_FONT_SIZE, "[%s] ", msg.time.c_str());

               if(!msg.isSystem)
                  xpos += drawStringAndGetWidth(xpos, ypos, CHAT_FONT_SIZE, msg.from.c_str());     // No sender for system message

               if(msg.isPrivate)
                  xpos += drawStringAndGetWidth(xpos, ypos, CHAT_FONT_SIZE, "*");

               if(!msg.isSystem)
                  xpos += drawStringAndGetWidth(xpos, ypos, CHAT_FONT_SIZE, ARROW) + AFTER_ARROW_SPACE;
            }
         }
      }

      // Calculate position for renderLoop
      ypos = ypos_top + ypos_top - ypos + (CHAT_FONT_SIZE + CHAT_FONT_MARGIN) * lineCountToDisplay;

      renderLoop = !renderLoop;
   } while(renderLoop);

   FontManager::popFontContext();
}


// Render outgoing chat message composition line
void AbstractChat::renderMessageComposition(S32 ypos)
{
   Renderer& r = Renderer::get();
   const char *PROMPT_STR = "> ";     // For composition only
   const S32 promptWidth = getStringWidth(CHAT_FONT_SIZE, PROMPT_STR);
   const S32 xStartPos = UserInterface::horizMargin + promptWidth;

   FontManager::pushFontContext(InputContext);
   string displayString = mLineEditor.getDisplayString();

   r.setColor(Colors::cyan);
   drawString(UserInterface::horizMargin, ypos, CHAT_FONT_SIZE, PROMPT_STR);

   r.setColor(Colors::white);
   drawString(xStartPos, ypos, CHAT_FONT_SIZE, displayString.c_str());

   mLineEditor.drawCursor(xStartPos, ypos, CHAT_FONT_SIZE);
   FontManager::popFontContext();
}


// I think this function is broken... if you are in UIQueryServers, you don't get your message!
// TODO:  Verify or fix!!!  RAPTOR!
void AbstractChat::deliverPrivateMessage(const char *sender, const char *message)
{
   // If player not in UIChat or UIQueryServers, then display message in-game if possible.  2 line message.
   if(!mGame->getUIManager()->isCurrentUI<QueryServersUserInterface>())
   {
      GameUserInterface *gameUI = mGame->getUIManager()->getUI<GameUserInterface>();

      gameUI->onChatMessageReceived(Colors::privateF5MessageDisplayedInGameColor,
         "Private message from %s: Press [%s] to enter chat mode", 
         sender, gameUI->getInputCodeString(mGame->getSettings(), BINDING_OUTGAMECHAT));

      gameUI->onChatMessageReceived(Colors::privateF5MessageDisplayedInGameColor, "%s %s", ARROW, message);
   }
}


// Send chat message
void AbstractChat::issueChat()
{
   if(mLineEditor.length() > 0)
   {
      // Send message
      MasterServerConnection *conn = mGame->getConnectionToMaster();
      if(conn)
         conn->c2mSendChat(mLineEditor.c_str());

      // And display it locally
      newMessage(mGame->getClientInfo()->getName().getString(), mLineEditor.getString(), false, false, true);
   }
   clearChat();     // Clear message

   UserInterface::playBoop();
}


// Clear current message
void AbstractChat::clearChat()
{
   mLineEditor.clear();
}


void AbstractChat::renderChatters(S32 xpos, S32 ypos)
{
   Renderer& r = Renderer::get();

   if(mPlayersInGlobalChat.size() == 0)
   {
      r.setColor(Colors::white);
      drawString(xpos, ypos, CHAT_NAMELIST_SIZE, "No other players currently in lobby/chat room");
   }
   else
      for(S32 i = 0; i < mPlayersInGlobalChat.size(); i++)
      {
         const char *name = mPlayersInGlobalChat[i].getString();

         r.setColor(getColor(name));      // use it

         xpos += drawStringAndGetWidthf((F32)xpos, (F32)ypos, CHAT_NAMELIST_SIZE, "%s%s", name, (i < mPlayersInGlobalChat.size() - 1) ? "; " : "");
      }
}

////////////////////////////////////////
////////////////////////////////////////

// Constructor
ChatUserInterface::ChatUserInterface(ClientGame *game) : Parent(game), ChatParent(game)
{
   mRenderUnderlyingUI = false;
}

// Destructor
ChatUserInterface::~ChatUserInterface()
{
   // Do nothing
}


void ChatUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);
}


void ChatUserInterface::setRenderUnderlyingUI(bool render)
{
   mRenderUnderlyingUI = render;
}


static const S32 VERT_FOOTER_SIZE   = 20;
static const S32 MENU_TITLE_SIZE    = 24;
static const S32 TITLE_SUBTITLE_GAP = 5;
static const S32 MENU_SUBTITLE_SIZE = 18;

void ChatUserInterface::render()
{
   Renderer& r = Renderer::get();

   // If there is an underlying menu or other UI screen, render and dim it.
   //
   // We will skip rendering if the editor is a parent UI because of a couple
   // of difficult-to-solve issues:
   //  1. Fullscreen mode in editor usually has a different aspect ratio when
   //     compared to the rest of the game (incl. the chat UI)
   //  2. The editor may have other sub-UIs opened (like QuickMenuUIs) that
   //     may not handle the UIManager stack appropriately (likely a bug) and
   //     will cause stack overflows
   if((mRenderUnderlyingUI && getUIManager()->hasPrevUI()) &&
         !getUIManager()->cameFrom<EditorUserInterface>())
   {
      getUIManager()->renderPrevUI(this);  // ...render it...
      dimUnderlyingUI();
   }

   FontManager::pushFontContext(MenuContext);

   // Render header
   renderHeader();

   // And footer
   r.setColor(Colors::green);
   S32 vertFooterPos = DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - VERT_FOOTER_SIZE;
   drawCenteredString(vertFooterPos, VERT_FOOTER_SIZE - 2, "Type your message | ENTER to send | ESC exits");

   renderChatters(horizMargin, vertFooterPos - CHAT_NAMELIST_SIZE - CHAT_FONT_MARGIN * 2);

   // Render incoming chat msgs
   r.setColor(Colors::white);

   U32 y = UserInterface::vertMargin + 60;

   static const S32 chatAreaHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight() - 2 * vertMargin -   // Screen area less margins
                     VERT_FOOTER_SIZE -                                                     // Instructions at the bottom
                     CHAT_NAMELIST_SIZE - CHAT_FONT_MARGIN * 2  -                           // Names of those in chatroom
                     MENU_TITLE_SIZE - TITLE_SUBTITLE_GAP - MENU_SUBTITLE_SIZE -            // Title/subtitle display
                     CHAT_FONT_SIZE - CHAT_FONT_MARGIN -                                    // Chat composition
                     CHAT_FONT_SIZE;                                                        // Not sure... just need a little more space??

   static const S32 MessageDisplayCount = chatAreaHeight / (CHAT_FONT_SIZE + CHAT_FONT_MARGIN);

   renderMessages(y, MessageDisplayCount);
   renderMessageComposition(vertFooterPos - 45);

   // Give user notice that there is no connection to master, and thus chatting is ineffectual
   MasterServerConnection *masterConn = getGame()->getConnectionToMaster();
   if(!(masterConn && masterConn->getConnectionState() == NetConnection::Connected))
   {
      static const S32 fontsize = 20;
      static const S32 fontgap = 5;
      static const S32 margin = 20;

      static const char* line1 = "Not connected to Master Server";
      static const char* line2 = "Your chat messages cannot be relayed";

      static const S32 CORNER_INSET = 15;
      static const S32 yPos1 = 200;
      static const S32 yPos2 = yPos1 + (2 * (fontsize + fontgap + margin));

      const S32 width = getStringWidth(fontsize, line2);

      S32 canvasWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth();
      S32 xPos1 = (canvasWidth - width) / 2 - margin;
      S32 xPos2 = xPos1 + width + (2 * margin);

      drawFilledFancyBox(xPos1, yPos1, xPos2, yPos2, CORNER_INSET, Colors::red40, 1.0, Colors::red);

      r.setColor(Colors::white);
      drawCenteredString(yPos1 + margin, fontsize, line1);
      drawCenteredString(yPos1 + margin + fontsize + fontgap, fontsize, line2);
   }

   FontManager::popFontContext();
}


void ChatUserInterface::renderHeader()
{
   Renderer& r = Renderer::get();
   FontManager::pushFontContext(MenuHeaderContext);

   // Draw title, subtitle, and footer
   r.setColor(Colors::green);
   drawCenteredString(vertMargin, MENU_TITLE_SIZE, "GAME LOBBY / GLOBAL CHAT");

   r.setColor(Colors::red);
   string subtitle = "Not currently connected to any game server";

   if(getGame()->getConnectionToServer())
   {
      r.setColor(Colors::yellow);
      string name = getGame()->getConnectionToServer()->getServerName();
      if(name == "")
         subtitle = "Connected to game server with no name";
      else
         subtitle = "Connected to game server \"" + name + "\"";
   }

   drawCenteredString(vertMargin + MENU_TITLE_SIZE + TITLE_SUBTITLE_GAP, MENU_SUBTITLE_SIZE, subtitle.c_str());

   FontManager::popFontContext();
}


bool ChatUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      { /* Do nothing */ }
   else if(inputCode == KEY_ESCAPE || checkInputCode(BINDING_OUTGAMECHAT, inputCode))
      onEscape();
   else if (inputCode == KEY_ENTER)                // Submits message
      issueChat();
   else
      return mLineEditor.handleKey(inputCode);

   // A key was handled
   return true;
}


void ChatUserInterface::onTextInput(char ascii)
{
   if(ascii)                                  // Other keys - add key to message
      mLineEditor.addChar(ascii);
}


// Run when UIChat is called in normal UI mode
void ChatUserInterface::onActivate()
{
   MasterServerConnection *masterConn = getGame()->getConnectionToMaster();

   if(masterConn && masterConn->isEstablished())
      masterConn->c2mJoinGlobalChat();

   // Only clear the chat list if the previous UI was NOT UIQueryServers
   if(getUIManager()->getPrevUI() != getUIManager()->getUI<QueryServersUserInterface>())
      mPlayersInGlobalChat.clear();

   mRenderUnderlyingUI = true;
   mDisableShipKeyboardInput = true;       // Prevent keystrokes from getting to game
}


void ChatUserInterface::onOutGameChat()
{
   // Escape chat only if the previous UI isn't UIQueryServers
   // This is to prevent spamming the chat window with joined/left messages
   if(getUIManager()->getPrevUI() == getUIManager()->getUI<QueryServersUserInterface>())
      getUIManager()->reactivatePrevUI();
   else
      onEscape();
}


void ChatUserInterface::onEscape()
{
   // Don't leave if UIQueryServers is a parent unless we're in-game...
   // Is UIQueryServers supposed to be a parent of UIGame??
   if(!getUIManager()->cameFrom<QueryServersUserInterface>() || getUIManager()->cameFrom<GameUserInterface>())
      leaveGlobalChat();

   getUIManager()->reactivatePrevUI();
   playBoop();
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SuspendedUserInterface::SuspendedUserInterface(ClientGame *game) : Parent(game)
{
   // Do nothing
}

// Destructor
SuspendedUserInterface::~SuspendedUserInterface()
{
   // Do nothing
}


void SuspendedUserInterface::renderHeader()
{
   Renderer& r = Renderer::get();

   if(getGame()->isSuspended())
   {
      r.setColor(Colors::white);
      drawCenteredString(vertMargin, MENU_TITLE_SIZE, "-- GAME SUSPENDED -- ");
   }
   else
   {
      r.setColor(Colors::red);
      drawCenteredString(vertMargin, MENU_TITLE_SIZE, "!! GAME RESTARTED !! ");
   }

   string subtitle = "Not currently connected to any game server";

   if(getGame()->getConnectionToServer())
   {
      string name = getGame()->getConnectionToServer()->getServerName();
      if(name == "")
         subtitle = "Connected to game server with no name";
      else
         subtitle = "Connected to game server \"" + name + "\"";
   }

   r.setColor(Colors::green);
   drawCenteredString(vertMargin + MENU_TITLE_SIZE + TITLE_SUBTITLE_GAP, MENU_SUBTITLE_SIZE, subtitle.c_str());
}


void SuspendedUserInterface::onOutGameChat()
{
   // Do nothing
}

};


