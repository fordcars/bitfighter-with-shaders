//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "LevelFilesForTesting.h"
#include "stringUtils.h"
#include "tnlVector.h"

namespace Zap
{

using namespace std;

string getLevelCode1()
{
   return
      "GameType 10 8\n"
      "LevelName \"Test Level\"\n"                             // Has quotes
      "LevelDescription \"This is a basic test level\"\n"      // Has quotes
      "LevelCredits level creator\n"                           // No quotes
      "GridSize 255\n"
      "Team Bluey 0 0 1\n"
      "Specials\n"
      "MinPlayers\n"
      "MaxPlayers\n"
      "BarrierMaker 40 -1 -1 -1 1\n"
      "RepairItem 0 1 10\n"
      "Spawn 0 -0.6 0\n"
      "Teleporter 5 5 10 10\n"
      "TestItem 1 1\n"
   ;
}


// This level has a spawn in a LoadoutZone, with a ResourceItem directly south of the spawn
string getLevelCodeForTestingEngineer1()
{
   return
      "GameType 10 92\n"
      "LevelName Engineer Test Bed One\n"
      "LevelDescription Level for testing Engineer\n"
      "LevelCredits Bitfighter Test Engineer #42445\n"
      "GridSize 255\n"
      "Team Blue 0 0 1\n"
      "Specials Engineer\n"
      "MinPlayers\n"
      "MaxPlayers\n"
      "LoadoutZone 0   1 0   1 1   0 1   0 0\n"
      "Spawn 0   .5 .5\n"
      "ResourceItem   0.5 1\n"
   ;
}


// Botspec looks like this:
// BBB BB  for 2 teams with 3 bots on first team, 2 on second
// Use 0 for a team with no bots
string getLevelCodeForEmptyLevelWithBots(const string &botSpec)
{
   Vector<string> words = parseString(botSpec);
   S32 teams = words.size();

   string level = 
      "LevelFormat 2\n"
      "GameType 10 8\n"
      "LevelName TwoBots\n"
      "LevelDescription\n"
      "LevelCredits Tyler Derden\n";

   for(S32 i = 0; i < teams; i++)
   {
      level += "Team team" + itos(i) + " 0 0 0\n";
   }

   level += 
      "Specials\n"
      "MinPlayers\n"
      "MaxPlayers\n";

   for(S32 i = 0; i < teams; i++)
      if(words[i] != "0")
         for(S32 j = 0; j < words[i].size(); j++)
            level += "Robot " + itos(i) + " s_bot\n";

   return level;
}

};