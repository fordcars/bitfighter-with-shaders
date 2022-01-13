//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "TeamShuffleHelper.h"

#include "ClientGame.h"
#include "DisplayManager.h"
#include "Renderer.h"
#include "FontManager.h"
#include "Colors.h"
#include "SymbolShape.h"

#include "RenderUtils.h"


#include <cmath>


namespace Zap
{

// Constructor
TeamShuffleHelper::TeamShuffleHelper() : Parent()
{
   playersPerTeam = 0;
   topMargin      = 0;
   leftMargin     = 0;
   columnWidth    = 0;
   rowHeight      = 0;
   maxColumnWidth = 0;
   rows           = 0;
   cols           = 0;
   teamCount      = 0;

   setAnimationTime(0);    // Transition time, in ms
}


// Destructor
TeamShuffleHelper::~TeamShuffleHelper()
{
   // Do nothing
}


HelperMenu::HelperMenuType TeamShuffleHelper::getType() { return ShuffleTeamsHelperType; }


// Randomly fill teams with clientInfos
void TeamShuffleHelper::shuffle()
{
   teamCount = getGame()->getTeamCount();

   mTeams.resize(teamCount);
   for(S32 i = 0; i < teamCount; i++)
      mTeams[i].clear();

   const Vector<RefPtr<ClientInfo> > *clientInfos = getGame()->getClientInfos();

   playersPerTeam = S32(ceil(F32(clientInfos->size()) / F32(mTeams.size())));

   for(S32 i = 0; i < clientInfos->size(); i++)
   {
      while(true)  
      {
         S32 index = TNL::Random::readI(0, mTeams.size() - 1);
         if(mTeams[index].size() < playersPerTeam)
         {
            mTeams[index].push_back(clientInfos->get(i));
            break;
         }
      }
   }

   calculateRenderSizes();
}


void TeamShuffleHelper::onActivated()
{
   Parent::onActivated();

   shuffle();
}


static F32 TEXT_SIZE_FACTOR = 1.2f;     // Give 20% breathing room for text 

void TeamShuffleHelper::calculateRenderSizes()
{
   switch(teamCount)
   {
      case 1:
         cols = 1;
         break;
      case 2:
      case 4:
         cols = 2;
         break;
      case 3:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
         cols = 3;
         break;
      default:
         TNLAssert(false, "Unexpected number of teams!");
         cols = 1;
         break;
   }

   rows = (S32)ceil((F32)teamCount / (F32)cols);

   columnWidth = -1;
   maxColumnWidth = (DisplayManager::getScreenInfo()->getGameCanvasWidth() - 100) / cols;
   rowHeight = (2 * vpad) + S32((playersPerTeam + 1) * TEXT_SIZE * TEXT_SIZE_FACTOR);  

   for(S32 i = 0; i < mTeams.size(); i++)
      for(S32 j = 0; j < mTeams[i].size(); j++)
      {
         S32 width = getStringWidth(TEXT_SIZE, mTeams[i][j]->getName().getString());

         if(width > columnWidth)
         {
            if(width > maxColumnWidth)
            {
               columnWidth = maxColumnWidth;
               break;
            }
            else
               columnWidth = width;
         }
      }

   topMargin  = (DisplayManager::getScreenInfo()->getGameCanvasHeight() - rows * rowHeight   - (rows - 1) * margin) / 2;
   leftMargin = (DisplayManager::getScreenInfo()->getGameCanvasWidth()  - cols * columnWidth - (cols - 1) * margin) / 2;

   columnWidth += 3 * hpad;
}


extern void drawHorizLine(S32 x1, S32 x2, S32 y);
extern void drawFilledRoundedRect(const Point &pos, S32 width, S32 height, const Color &fillColor, 
                                  const Color &outlineColor, S32 radius, F32 alpha = 1.0);

void TeamShuffleHelper::render()
{
   Renderer& r = Renderer::get();

   FontManager::pushFontContext(TeamShuffleContext);

   for(S32 i = 0; i < rows; i++)
      for(S32 j = 0; j < cols; j++)
      {
         if(i * cols + j >= teamCount)
            break;

         S32 x = leftMargin + j * (columnWidth + margin);
         S32 y = topMargin  + i * (rowHeight   + margin);

         S32 teamIndex = i * cols + j;

         Color c = *getGame()->getTeamColor(teamIndex);
         c *= .2f;

         drawFilledRoundedRect(Point(x + columnWidth / 2, y + rowHeight / 2), 
                               columnWidth, rowHeight, 
                               c, *getGame()->getTeamColor(teamIndex), 8);

         r.setColor(*getGame()->getTeamColor(teamIndex));
         drawString(x + hpad, y + vpad, TEXT_SIZE, getGame()->getTeamName(teamIndex).getString());

         drawHorizLine(x + hpad, x + columnWidth - hpad, y + vpad + TEXT_SIZE + 3);

         r.setColor(Colors::white);
         for(S32 k = 0; k < mTeams[teamIndex].size(); k++)
            drawString(x + hpad, y + S32(vpad + (k + 1) * TEXT_SIZE_FACTOR * TEXT_SIZE + 3),
                  TEXT_SIZE, mTeams[teamIndex][k]->getName().getString());
      }

   r.setColor(Colors::green);

   static const UI::SymbolString Instructions(
         "[[Enter]] to accept | [[Space]] to reshuffle | [[Esc]] to cancel", 
         getGame()->getSettings()->getInputCodeManager(), TeamShuffleContext, 20, false, AlignmentCenter);

   static Point RenderPos(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2,
                          DisplayManager::getScreenInfo()->getGameCanvasHeight() - 60);

   Instructions.render(RenderPos);

   FontManager::popFontContext();
}


// Return true if key did something, false if key had no effect
// Runs on client
bool TeamShuffleHelper::processInputCode(InputCode inputCode)
{
   if(Parent::processInputCode(inputCode))    // Check for cancel keys
      return true;

   if(inputCode == KEY_SPACE)
      shuffle();

   else if(inputCode == KEY_ENTER)
   {
      exitHelper();

      // Now determine if a player is going to change teams
      for(S32 i = 0; i < mTeams.size(); i++)
         for(S32 j = 0; j < mTeams[i].size(); j++)
         {
            ClientInfo *thisClientInfo = mTeams[i][j];

            // Where did the client go?
            if(!thisClientInfo)
               continue;

            // If the client's team is the same as the shuffled one, no need to switch
            if(thisClientInfo->getTeamIndex() == i)
               continue;

            // Trigger a team change for the player
            getGame()->changePlayerTeam(thisClientInfo->getName(), i);
         }
   }

   return true;
}


const char *TeamShuffleHelper::getCancelMessage() const
{
   return "Shuffle canceled -- teams unchanged";
}


void TeamShuffleHelper::onPlayerJoined()
{
   shuffle();
}


void TeamShuffleHelper::onPlayerQuit()
{
   shuffle();
}


// Only activated via chat cmd
InputCode TeamShuffleHelper::getActivationKey()
{
   return KEY_NONE;
}


bool TeamShuffleHelper::isMovementDisabled() const
{
   return true;
}


};

