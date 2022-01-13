//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIEditorInstructions.h"

#include "UIManager.h"
#include "UIEditor.h"         // For PluginInfo def

#include "ClientGame.h"       // For usage with getGame()
#include "barrier.h"     
#include "BotNavMeshZone.h"   // For Border class def
#include "gameObjectRender.h"
#include "DisplayManager.h"
#include "Renderer.h"
#include "VertexStylesEnum.h"
#include "FontManager.h"

#include "Colors.h"
#include "Intervals.h"

#include "GeomUtils.h"        // For polygon triangulation
#include "RenderUtils.h"
#include "stringUtils.h"

#include <cmath>


namespace Zap
{

using UI::SymbolString;
using UI::SymbolStringSet;

// Constructor
EditorInstructionsUserInterface::EditorInstructionsUserInterface(ClientGame *game) :
   Parent(game),
   mAnimTimer(ONE_SECOND),
   mConsoleInstructions(10),
   mScriptInstr(LineGap),
   mScriptBindings(LineGap)
{
   GameSettings *settings = getGame()->getSettings();
   mCurPage = 0;
   mAnimStage = 0;

   Vector<UI::SymbolShapePtr> symbols;

   // Two pages, two columns, two groups in each column
   SymbolStringSet keysInstrLeft1(LineGap),  keysBindingsLeft1(LineGap);
   SymbolStringSet keysInstrRight1(LineGap), keysBindingsRight1(LineGap);
   SymbolStringSet keysInstrLeft2(LineGap),  keysBindingsLeft2(LineGap);
   SymbolStringSet keysInstrRight2(LineGap), keysBindingsRight2(LineGap);


   // Add some headers to our 4 columns
   symbols.clear();
   symbols.push_back(SymbolString::getSymbolText("Action", HeaderFontSize, HelpContext, secColor));
   keysInstrLeft1 .add(SymbolString(symbols));
   keysInstrRight1.add(SymbolString(symbols));
   keysInstrLeft2 .add(SymbolString(symbols));
   keysInstrRight2.add(SymbolString(symbols));

   symbols.clear();
   symbols.push_back(SymbolString::getSymbolText("Control", HeaderFontSize, HelpContext, secColor));
   keysBindingsLeft1 .add(SymbolString(symbols));
   keysBindingsRight1.add(SymbolString(symbols));
   keysBindingsLeft2 .add(SymbolString(symbols));
   keysBindingsRight2.add(SymbolString(symbols));

   // Add horizontal line to first column (will draw across all)
   symbols.clear();
   symbols.push_back(SymbolString::getHorizLine(735, -14, 8, &Colors::gray70));
   keysInstrLeft1.add(SymbolString(symbols));
   keysInstrLeft2.add(SymbolString(symbols));

   symbols.clear();
   symbols.push_back(SymbolString::getBlankSymbol(0, 5));
   keysInstrRight1.add   (SymbolString(symbols));
   keysInstrRight2.add   (SymbolString(symbols));
   keysBindingsLeft1.add (SymbolString(symbols));
   keysBindingsLeft2.add (SymbolString(symbols));
   keysBindingsRight1.add(SymbolString(symbols));
   keysBindingsRight2.add(SymbolString(symbols));


   // For page 1 of general instructions
   ControlStringsEditor controls1Left[] = {
   { "HEADER", "Navigation" },
         { "Pan Map",                  "[[W]]/[[A]]/[[S]]/[[D]] or"},
         { " ",                         "Arrow Keys"},
         { "Zoom In",                  "[[ZoomIn]] or [[Ctrl+Up Arrow]]" },
         { "Zoom Out",                 "[[ZoomOut]] or [[Ctrl+Down Arrow]]" },
         { "Center Display",           "[[ResetView]]" },
         { "Toggle Script Results",    "[[RunLevelgenScript]]" },
         { "Copy Results Into Editor", "[[InsertGenItems]]" },
         { "Show/Hide Plugins Pane",   "[[DockmodeItems]]"},
      { "-", "" },         // Horiz. line
   { "HEADER", "Editing" },
         { "Cut/Copy/Paste",           "[[CutSelection]] / [[CopySelection]] / [[PasteSelection]]"},
         { "Delete Selection",         "[[Del]]" },
         { "Undo",                     "[[UndoAction]]" },
         { "Redo",                     "[[RedoAction]]" }
   };

   ControlStringsEditor controls1Right[] = {
   { "HEADER", "Object Shortcuts" },
         { "Insert Teleporter",     "[[PlaceNewTeleporter]]" },
         { "Insert Spawn Point",    "[[PlaceNewSpawn]]" },
         { "Insert Repair",         "[[PlaceNewRepair]]" },
         { "Insert Turret",         "[[PlaceNewTurret]]" },
         { "Insert Force Field",    "[[PlaceNewForcefield]]" },
         { "Insert Mine",           "[[PlaceNewMine]]" },
      { "-", "" },         // Horiz. line
   { "HEADER", "Assigning Teams" },
         { "Set object's team",     "[[1]]-[[9]]" },
         { "Set object to neutral", "[[0]]" },
         { "Set object to hostile", "[[Shift+0]]" },
      { "-", "" },         // Horiz. line
         { "Save",                  "[[SaveLevel]]" },
         { "Reload from file",      "[[ReloadLevel]]" }
   };


   // For page 2 of general instructions
   ControlStringsEditor controls2Left[] = {
   { "HEADER", "Size & Rotation" },
         { "Flip horizontal/vertical", "[[FlipItemHorizontal]] / [[FlipItemVertical]]" },
         { "Rotate object in place",   "[[RotateSpinCCW]] / [[RotateSpinCW]]" },
         { "Rotate about (0,0)",       "[[RotateCCWOrigin]] / [[RotateCWOrigin]]" },
         { "Free rotate in place",     "[[RotateCentroid]]" },
         { "Free rotate about (0,0)",  "[[RotateOrigin]]" },
         { "Scale selection",          "[[ResizeSelection]]" },

      { "-", "" },      // Horiz. line
         { "Press or Hold [[NoGridSnapping]] to suspend grid snapping", "" },
         { "[[NoSnapping]] to suspend vertex snapping",                 "" },
         { "Hold [[PreviewMode]] to view a reference ship",             "" },
         { "Press [[ToggleEditMode]] to toggle object/vertex selection modes", "" }
   };



   ControlStringsEditor controls2Right[] = {
      { "HEADER", "Object IDs" },
      { "Edit Object ID",            "[[#]] or [[!]]" },
      { "Toggle display of all IDs", "[[Ctrl+#]]" }
   };

   pack(keysInstrLeft1,  keysBindingsLeft1, controls1Left, ARRAYSIZE(controls1Left));

   pack(keysInstrRight1, keysBindingsRight1, controls1Right, ARRAYSIZE(controls1Right));

   pack(keysInstrLeft2,  keysBindingsLeft2,  controls2Left,  ARRAYSIZE(controls2Left));
   pack(keysInstrRight2, keysBindingsRight2, controls2Right, ARRAYSIZE(controls2Right));

   S32 centeringOffset = getStringWidth(HelpContext, HeaderFontSize, "Control") / 2;

   // Use default width here as the editor could be using a different canvas size
   S32 screenWidth = DisplayManager::getScreenInfo()->getDefaultCanvasWidth();

   mCol1 = horizMargin;
   mCol2 = horizMargin + S32(screenWidth * 0.25) + 45;
   mCol3 = horizMargin + S32(screenWidth * 0.5);
   mCol4 = horizMargin + S32(screenWidth * 0.75) + 45;

   mSymbolSets1Left.addSymbolStringSet(keysInstrLeft1,      UI::AlignmentLeft,   mCol1);
   mSymbolSets1Left.addSymbolStringSet(keysBindingsLeft1,   UI::AlignmentCenter, mCol2 + centeringOffset);
   mSymbolSets1Right.addSymbolStringSet(keysInstrRight1,    UI::AlignmentLeft,   mCol3);
   mSymbolSets1Right.addSymbolStringSet(keysBindingsRight1, UI::AlignmentCenter, mCol4 + centeringOffset);

   mSymbolSets2Left.addSymbolStringSet(keysInstrLeft2,      UI::AlignmentLeft,   mCol1);
   mSymbolSets2Left.addSymbolStringSet(keysBindingsLeft2,   UI::AlignmentCenter, mCol2 + centeringOffset);
   mSymbolSets2Right.addSymbolStringSet(keysInstrRight2,    UI::AlignmentLeft,   mCol3);
   mSymbolSets2Right.addSymbolStringSet(keysBindingsRight2, UI::AlignmentCenter, mCol4 + centeringOffset);


   // Prepare special instructions
   ControlStringsEditor helpBindLeft[] = 
   { 
      { "Help",               "[[Help]]"       },
      { "Team Editor",        "[[TeamEditor]]" }
   };

   pack(mSpecialKeysInstrLeft,  mSpecialKeysBindingsLeft, helpBindLeft, ARRAYSIZE(helpBindLeft));
   
   ControlStringsEditor helpBindRight[] = 
   {
      { "Game Params Editor", "[[GameParameterEditor]]" },
      { "Lobby Chat",         "[[OutOfGameChat]]"       }
   };

   pack(mSpecialKeysInstrRight, mSpecialKeysBindingsRight, helpBindRight, ARRAYSIZE(helpBindRight));


   string wallInstructions[] =
   {
      "[[BULLET]] Create walls with right mouse button; hold [[~]] to create line",
      "[[BULLET]] Finish wall by left-clicking mouse",
      "[[BULLET]] Drag and drop individual vertices or an entire wall",
      "[[BULLET]] Split wall at selected vertex with [[\\]]",
      "[[BULLET]] Join contiguous wall segments, polywalls, or zones with [[J]]",
      "[[BULLET]] Change wall thickness with [[+]] & [[-]] (use [[Shift]] for smaller changes)"
   };

   pack(mWallInstr, wallInstructions, ARRAYSIZE(wallInstructions));


   string scriptInstructions[] =
   {
      "Scripts can be used to generate level items at runtime, to monitor and respond",
      "to events during gameplay, or both.  These scripts are referred to as ",
      "\"levelgen scripts.\"  Scripts are written in Lua, and can monitor or manipulate",
      "a range of objects and events.  You can create levelgen scripts using the text",
      "editor of your choice.  Levelgen scripts should have the extension \".lua\"",
      "or \".levelgen\", and can be stored either in your levels folder, or in the scripts",
      "folder.  Generally, if your script is only used for a single level, it should be",
      "stored with the levels.  If you share a level that depends on a script, you'll have",
      "to remember to provide the script as well.",
      "",
      "A full scripting reference and some basic tutorials can be found on the Bitfighter",
      "wiki.",
         // Coming in 020
      //"",
      //"The current levels folder is: [[FOLDER_NAME:level]]",
      //"",
      //"The current scripts folder is: [[FOLDER_NAME:scripts]]",
   };

   pack(mScriptInstr, scriptInstructions, ARRAYSIZE(scriptInstructions));


   symbols.clear();
   SymbolString::symbolParse(mGameSettings->getInputCodeManager(), "Open the console by pressing [[/]]",
                             symbols, HelpContext, FontSize, true, &Colors::green, keyColor);

   mConsoleInstructions.add(SymbolString(symbols));

   ///// Plugin pages

   const Vector<PluginInfo> *pluginInfos = getUIManager()->getUI<EditorUserInterface>()->getPluginInfos();

   // Determine how many plugins we have and adjust our page count accordingly
   mPluginPageCount = 0;

   static const S32 PLUGINS_PER_PAGE = 15;
   if(pluginInfos->size() > 0)
      mPluginPageCount = ((pluginInfos->size() - 1) / PLUGINS_PER_PAGE) + 1;

   string tabstr = "[[TAB_STOP:200]]";

   for(S32 i = 0; i < mPluginPageCount; i++)
   {
      UI::SymbolStringSet pluginSymbolSet(10);

      symbols.clear();
      SymbolString::symbolParse(settings->getInputCodeManager(), "Plugins are scripts that can manipuate items in the editor.",
                                symbols, HelpContext, FontSize, true, &Colors::green, keyColor);
      pluginSymbolSet.add(SymbolString(symbols));

      symbols.clear();
      SymbolString::symbolParse(settings->getInputCodeManager(), "See the Bitfighter wiki for info on creating your own.",
                                symbols, HelpContext, FontSize, true, &Colors::green, keyColor);
      pluginSymbolSet.add(SymbolString(symbols));

      // Using TAB_STOP:0 below will cause the text and the horiz. line to be printed in the same space, creating a underline effect
      symbols.clear();
      symbols.push_back(SymbolString::getHorizLine(735, FontSize, FontSize + 4, &Colors::gray70));
      SymbolString::symbolParse(settings->getInputCodeManager(), "[[TAB_STOP:0]]Key" + tabstr + "Description",
                                symbols, HelpContext, FontSize, true, &Colors::yellow, keyColor);
      pluginSymbolSet.add(SymbolString(symbols));

      S32 start = i + (PLUGINS_PER_PAGE * i);
      S32 end = MIN(i + (PLUGINS_PER_PAGE * (i + 1)), pluginInfos->size());
      for(S32 j = start; j < end; j++)
      {
         string binding = pluginInfos->get(j).binding;

         string key = "";
         if(pluginInfos->get(j).bindingCollision)
            key = "- KEY CLASH -";
         else if(binding != "")
            key = "[[" + pluginInfos->get(j).binding + "]]";  // Add the [[ & ]] to make it parsable

         string instr = pluginInfos->get(j).description;

         symbols.clear();
         SymbolString::symbolParse(settings->getInputCodeManager(), key + tabstr + instr,
                                   symbols, HelpContext, FontSize, txtColor, keyColor);
         pluginSymbolSet.add(SymbolString(symbols));
      }

      mPluginInstructions.push_back(pluginSymbolSet);
   }


   // Generate page headers, aligned with pages
   mPageHeaders.push_back("BASIC COMMANDS");
   mPageHeaders.push_back("ADVANCED COMMANDS");
   mPageHeaders.push_back("WALLS AND LINES");
   mPageHeaders.push_back("ADDING SCRIPTS");
   mPageHeaders.push_back("SCRIPTING CONSOLE");

   TNLAssert(mPageHeaders.size() == NonPluginPageCount, "Wrong number of headers!");

   for(S32 i = 0; i < mPluginPageCount; i++)
      mPageHeaders.push_back("PLUGINS PAGE " + itos(i+1));
}


// Destructor
EditorInstructionsUserInterface::~EditorInstructionsUserInterface()
{
   // Do nothing
}


void EditorInstructionsUserInterface::onActivate()
{
   mCurPage = 0;     // Start at the beginning, silly!
   onPageChanged();
}


S32 EditorInstructionsUserInterface::getPageCount() const
{
   return NonPluginPageCount + mPluginPageCount;
}


void EditorInstructionsUserInterface::render()
{
   FontManager::pushFontContext(HelpContext);

   Parent::render(mPageHeaders[mCurPage].c_str(), mCurPage + 1, getPageCount());

   static ControlStringsEditor consoleCommands[] = {
      { "Coming soon...", "Coming soon..." },
      { "", "" },      // End of list
   };


   if(mCurPage == 0)
      renderPageCommands(1);
   else if(mCurPage == 1)
      renderPageCommands(2);
   else if(mCurPage == 2)
      renderPageWalls();
   else if(mCurPage == 3)
      renderScripting();
   else if(mCurPage == 4)
      renderConsoleCommands(mConsoleInstructions, consoleCommands);
   else if(mCurPage >= NonPluginPageCount)
      mPluginInstructions[mCurPage - NonPluginPageCount].render(horizMargin, 60, AlignmentLeft);
   else
      TNLAssert(false, "Should never get here -- did you just add a page?");

   FontManager::popFontContext();
}


// This has become rather ugly and inelegant.  But you shuold see UIInstructions.cpp!!!
void EditorInstructionsUserInterface::renderPageCommands(S32 page) const
{
   S32 y = 60;             // Is 65 in UIInstructions::render()...

   if(page == 1)
   {
      mSymbolSets1Left.render(y);
      mSymbolSets1Right.render(y);
   }
   else
   {      
      mSymbolSets2Left.render(y);
      mSymbolSets2Right.render(y);
   }

   y = 486;
   Renderer::get().setColor(*secColor);
   drawCenteredString(y, 20, "These special keys are also usually active:");

   y += 45;

   mSpecialKeysInstrLeft.render (mCol1, y, AlignmentLeft);
   mSpecialKeysInstrRight.render(mCol3, y, AlignmentLeft);

   S32 centeringOffset = getStringWidth(HelpContext, HeaderFontSize, "Control") / 2;

   mSpecialKeysBindingsLeft.render (mCol2 + centeringOffset, y, AlignmentCenter);
   mSpecialKeysBindingsRight.render(mCol4 + centeringOffset, y, AlignmentCenter);
}


// Draw animated creation of walls
void EditorInstructionsUserInterface::renderPageWalls() const
{
   Renderer& r = Renderer::get();
   //drawStringf(400, 100, 25, "%d", mAnimStage);     // Useful to have around when things go wrong!

   S32 vertOffset = 20;

   Vector<Point> points;

   points.push_back(Point(150, 100 + vertOffset));
   points.push_back(Point(220, 190 + vertOffset));

   if(mAnimStage > 0 && mAnimStage < 10)
      points.push_back(Point(350, 80 + vertOffset));

   else if(mAnimStage == 10)
      points.push_back(Point(350, 150 + vertOffset));

   else if(mAnimStage >= 11)
      points.push_back(Point(350, 200 + vertOffset));

   if(mAnimStage > 1)
      points.push_back(Point(470, 140 + vertOffset));

   if(mAnimStage > 2)
      points.push_back(Point(550, 120 + vertOffset));

   if(mAnimStage == 4)
      points.push_back(Point(650, 100 + vertOffset));

   else if(mAnimStage == 5)
      points.push_back(Point(690, 130 + vertOffset));

   else if(mAnimStage >= 6)
      points.push_back(Point(650, 170 + vertOffset));

   // Inefficient to do this every tick, but the page won't be rendered often
   if(mAnimStage > 6)
   {
      const F32 width = 25;

      Vector<DatabaseObject *> wallSegments;

      // Build out segment data for this line
      Vector<Vector<Point> > segmentData;
      barrierLineToSegmentData(points, segmentData);

      for(S32 i = 0; i < segmentData.size(); i++)
      {
         // Create a new segment, and add it to the list.  The WallSegment constructor will add it to the specified database.
         WallSegment *newSegment = new WallSegment(mWallSegmentManager.getWallSegmentDatabase(),
                                                   segmentData[i], width);
         wallSegments.push_back(newSegment);
      }

      Vector<Point> edges;
      mWallSegmentManager.clipAllWallEdges(&wallSegments, edges);      // Remove interior wall outline fragments

      for(S32 i = 0; i < wallSegments.size(); i++)
      {
         WallSegment *wallSegment = static_cast<WallSegment *>(wallSegments[i]);
         wallSegment->renderFill(Point(0,0), Colors::EDITOR_WALL_FILL_COLOR);
      }

      renderWallEdges(edges, *getGame()->getSettings()->getWallOutlineColor());

      for(S32 i = 0; i < wallSegments.size(); i++)
         delete wallSegments[i];
   }

   r.setColor(mAnimStage <= 11 ? Colors::yellow : Colors::NeutralTeamColor);
   r.setLineWidth(gLineWidth3);

   r.renderPointVector(&points, RenderType::LineStrip);
   r.setLineWidth(gDefaultLineWidth);

   FontManager::pushFontContext(OldSkoolContext);

   for(S32 i = 0; i < points.size(); i++)
   {
      S32 vertNum = S32(((F32)i  / 2) + 0.5);     // Ick!

      if(i < (points.size() - ((mAnimStage > 6) ? 0 : 1) ) && !(i == 4 && (mAnimStage == 9 || mAnimStage == 10 || mAnimStage == 11)))
         renderVertex(SelectedItemVertex, points[i], vertNum, 1);
      else if(mAnimStage == 9 || mAnimStage == 10 || mAnimStage == 11)
         renderVertex(SelectedVertex, points[i], vertNum);
      else  // mAnimStage > 11, moving vertices about
         renderVertex(HighlightedVertex, points[i], -1, 1);
   }

   FontManager::popFontContext();

   mWallInstr.render(50, 300, UI::AlignmentLeft);     // The written instructions block
}


void EditorInstructionsUserInterface::renderScripting() const
{
   mScriptInstr.render(30, 100, AlignmentLeft);     // The written instructions block
}


void EditorInstructionsUserInterface::onPageChanged()
{
   mAnimTimer.reset();
   mAnimStage = 0;
}


void EditorInstructionsUserInterface::nextPage()
{
   mCurPage++;
   if(mCurPage == getPageCount())
      mCurPage = 0;

   onPageChanged();
}


void EditorInstructionsUserInterface::prevPage()
{
   mCurPage--;
   if(mCurPage < 0)
      mCurPage = getPageCount() - 1;

   onPageChanged();
}


void EditorInstructionsUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   if(mAnimTimer.update(timeDelta))
   {
      mAnimTimer.reset();
      mAnimStage++;
      if(mAnimStage >= 18)
         mAnimStage = 0;
   }
}


void EditorInstructionsUserInterface::exitInstructions()
{
   playBoop();
   getUIManager()->reactivatePrevUI();      // To EditorUserInterface, probably
}


bool EditorInstructionsUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode)) { /* Do nothing */ }

   else if(inputCode == KEY_LEFT || inputCode == BUTTON_DPAD_LEFT || inputCode == BUTTON_DPAD_UP || inputCode == KEY_UP)
   {
      playBoop();
      prevPage();
   }
   else if(inputCode == KEY_RIGHT        || inputCode == KEY_SPACE || inputCode == BUTTON_DPAD_RIGHT ||
           inputCode == BUTTON_DPAD_DOWN || inputCode == KEY_ENTER || inputCode == KEY_DOWN)
   {
      playBoop();
      nextPage();
   }
   // F1 has dual use... advance page, then quit out of help when done
   else if(checkInputCode(BINDING_HELP, inputCode))
   {
      if(mCurPage < getPageCount())
         nextPage();
      else
         exitInstructions();
   }
   else if(inputCode == KEY_ESCAPE  || inputCode == BUTTON_BACK)
      exitInstructions();
   // Nothing was handled
   else
      return false;

   // A key was handled
   return true;
}

};


