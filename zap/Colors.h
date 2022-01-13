//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef COLORS_H_
#define COLORS_H_

#include "Color.h"

namespace Zap 
{

namespace Colors 
{
   // Basic colors
   const Color red(1,0,0);
   const Color green(0,1,0);
   const Color blue(0,0,1);
   const Color yellow(1,1,0);
   const Color cyan(0,1,1);
   const Color magenta(1,0,1);
   const Color black(0,0,0);
   const Color white(1,1,1);

   // Grays
   const Color gray20(0.20);
   const Color gray40(0.40);
   const Color gray50(0.50);
   const Color gray60(0.60);
   const Color gray67(0.67);
   const Color gray70(0.70);
   const Color gray75(0.75);
   const Color gray80(0.80);

   // Yellow-oranges
   const Color yellow40(.40f, .40f, 0);
   const Color yellow70(.70f, .70f, 0);
   const Color orange50(1, .50f, 0);      // Rabbit orange
   const Color orange67(1, .67f ,0);      // A more reddish orange

   // "Pale" colors
   const Color paleRed(1, .50f, .50f);    // A washed out red
   const Color paleGreen(.50f, 1, .50f);  // A washed out green
   const Color paleBlue(.50f, .50f, 1);   // A washed out blue
   const Color palePurple(1, .50f, 1);    // A washed out purple (pinkish?)

   // Reds
   const Color red30(.30f, 0, 0);
   const Color red35(.35f, 0, 0);
   const Color red40(.40f, 0, 0);
   const Color red50(.50f, 0, 0);
   const Color red60(.60f, 0, 0);
   const Color red80(.80f, 0, 0);

   // Greens
   const Color richGreen(0, .35f, 0);
   const Color green50(0, .50f, 0);
   const Color green65(0, .65f, 0);
   const Color green80(0, .80f, 0);

   // Blues
   const Color blue40(0, 0, .40f);
   const Color blue80(0, 0, .80f);

   // Some special colors
   const Color menuHelpColor(green);
   const Color idlePlayerNameColor(gray50);
   const Color standardPlayerNameColor(white);
   const Color streakPlayerNameColor(red80);
   const Color infoColor(cyan);

   const Color overlayMenuSelectedItemColor   = Color(1.0f, 0.1f, 0.1f);
   const Color overlayMenuUnselectedItemColor = Color(0.1f, 1.0f, 0.1f);
   const Color overlayMenuHelpColor           = Color(.2, .8, .8);

   // Chat colors
   const Color globalChatColor(0.9, 0.9, 0.9);
   const Color teamChatColor(Colors::green);
   const Color cmdChatColor(Colors::red);
   const Color privateF5MessageDisplayedInGameColor(Colors::blue);


   // Specialties
   const Color gold(.85f, .85f, .10f);
   const Color silver(.90f, .91f, .98f);
   const Color bronze(.55f, .47f, .33f);

   const Color wallFillColor(Colors::paleBlue);

   // Editor colors
   const Color EDITOR_HIGHLIGHT_COLOR(Colors::white);
   const Color EDITOR_SELECT_COLOR(Colors::yellow);
   const Color EDITOR_PLAIN_COLOR(Colors::gray75);

   const Color EDITOR_WALL_FILL_COLOR(.5f, .5f, 1.0f);

   // Special named colors
   const Color NexusOpenColor(0, 0.7, 0);
   const Color NexusClosedColor(0.85, 0.3, 0);
   const Color DefaultWallFillColor(0, 0, 0.15);
   const Color DefaultWallOutlineColor(Colors::blue);
   const Color ErrorMessageTextColor(Colors::paleRed);
   const Color NeutralTeamColor(Colors::gray80);         // Objects that are neutral (on team -1)
   const Color HostileTeamColor(Colors::gray50);         // Objects that are "hostile-to-all" (on team -2)
   const Color MasterServerBlue(0.8, 0.8, 1);            // Messages about successful master server statii
   const Color HelpItemRenderColor(Colors::green);       // Render color for inline-help messages
   const Color DisabledGray(Colors::gray40);             // Color for disabled commands and menu options
};

}

#endif /* COLORS_H_ */
