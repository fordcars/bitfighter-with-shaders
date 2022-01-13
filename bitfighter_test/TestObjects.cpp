//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

// Bitfighter Tests

#define BF_TEST

#include "gtest/gtest.h"

#include "tnlNetObject.h"
#include "tnlGhostConnection.h"
#include "tnlPlatform.h"

#include "BfObject.h"
#include "gameType.h"
#include "ServerGame.h"
#include "ClientGame.h"
#include "SystemFunctions.h"

#include "GeomUtils.h"
#include "stringUtils.h"
#include "RenderUtils.h"

#include "luaLevelGenerator.h"


#include "TestUtils.h"

#include <string>
#include <cmath>

#ifdef TNL_OS_WIN32
#  include <windows.h>   // For ARRAYSIZE
#endif

namespace Zap {
using namespace std;

class ObjectTest : public testing::Test
{
   public:
      static void process(ServerGame *game, S32 argc, const char **argv)
      {
         for(S32 j = 1; j <= argc; j++)
         {
            game->cleanUp();
            game->processLevelLoadLine(j, 0, argv, game->getGameObjDatabase(), "some_non_existing_filename.level", 1);
         }
      }
};


/**
 * Ensures that processArguments with long, non-sensical argv does not segfault
 * for all registered NetClasses and a few special level directives.
 */
TEST_F(ObjectTest, ProcessArgumentsSanity) 
{
   ServerGame *game = newServerGame();
   const S32 argc = 40;
   const char *argv[argc] = {
      "This first string will be replaced by 'argv[0] =' below",
      "3", "4", "3", "6", "6", "4", "2", "6", "6", "3", 
      "4", "3", "4", "3", "6", "6", "4", "2", "6", "6", 
      "4", "3", "4", "3", "6", "6", "4", "2", "6", "6", 
      "4", "3", "4", "3", "6", "blah", "4", "2", "6" };

   // Substitute the class name for argv[0]
   U32 count = TNL::NetClassRep::getNetClassCount(NetClassGroupGame, NetClassTypeObject);
   for(U32 i = 0; i < count; i++)
   {
      NetClassRep *netClassRep = TNL::NetClassRep::getClass(NetClassGroupGame, NetClassTypeObject, i);
      argv[0] = netClassRep->getClassName();

      process(game, argc, argv);
   }

#define t(n) {argv[0] = n; process(game, argc, argv);}
   t("BarrierMaker");
   t("LevelName");
   t("LevelCredits");
   t("Script");
   t("MinPlayers");
   t("MaxPlayers");
   t("Team");
#undef t

   delete game;
}



struct GhostingRecord
{
  bool server;
  bool client;
};

/**
 * Instantiate and transmit one object of every registered type from a server to
 * a client. Ensures that the associated code paths do not crash.
 */
TEST_F(ObjectTest, GhostingSanity)
{
   // Track the transmission state of each object
   U32 classCount = TNL::NetClassRep::getNetClassCount(NetClassGroupGame, NetClassTypeObject);
   Vector<GhostingRecord> ghostingRecords;
   ghostingRecords.resize(classCount);
   for(U32 i = 0; i < classCount; i++)
   {
      ghostingRecords[i].server = false;
      ghostingRecords[i].client = false;
   }

   // Create our pair of connected games
   GamePair gamePair;
   ClientGame *clientGame = gamePair.getClient(0);
   ServerGame *serverGame = gamePair.server;

   // Basic geometry to plug into polygon objects
   Vector<Point> geom;
   geom.push_back(Point(0,0));
   geom.push_back(Point(1,0));
   geom.push_back(Point(0,1));

   Vector<Point> geom_speedZone;
   geom_speedZone.push_back(Point(400,0));
   geom_speedZone.push_back(Point(400,1));

   // Create one of each type of registered NetClass
   for(U32 i = 0; i < classCount; i++)
   {
      NetClassRep *netClassRep = TNL::NetClassRep::getClass(NetClassGroupGame, NetClassTypeObject, i);
      Object *obj = netClassRep->create();
      BfObject *bfobj = dynamic_cast<BfObject *>(obj);

      // Skip registered classes that aren't BfObjects (e.g. GameType) or don't have
      // a geometry at this point (ForceField)
      if(bfobj && bfobj->hasGeometry())
      {
         // First, add some geometry
         bfobj->setExtent(Rect(0,0,1,1));
         if(bfobj->getObjectTypeNumber() == SpeedZoneTypeNumber)
         {
            bfobj->GeomObject::setGeom(geom_speedZone);
            bfobj->onGeomChanged();
         }
         else
            bfobj->GeomObject::setGeom(geom);
         bfobj->addToGame(serverGame, serverGame->getGameObjDatabase());

         ghostingRecords[i].server = true;
      }
      else
         delete bfobj;
   }

   // Idle to allow object replication
   gamePair.idle(10, 10);

   // Check whether the objects created on the server made it onto the client
   const Vector<DatabaseObject *> *objects = clientGame->getGameObjDatabase()->findObjects_fast();
   for(S32 i = 0; i < objects->size(); i++)
   {
      BfObject *bfobj = dynamic_cast<BfObject *>((*objects)[i]);
      if(bfobj->getClassRep() != NULL)  // Barriers and some other objects might not be ghostable...
      {
         U32 id = bfobj->getClassId(NetClassGroupGame);
         ghostingRecords[id].client = true;
      }
   }

   for(U32 i = 0; i < classCount; i++)
   {
      NetClassRep *netClassRep = TNL::NetClassRep::getClass(NetClassGroupGame, NetClassTypeObject, i);

      // Expect that all objects on the server are on the client, with the
      // exception of PolyWalls and ForceFields, because these do not follow the
      // normal ghosting model.
      string className = netClassRep->getClassName();
      if(className != "PolyWall" && className != "ForceField")
         EXPECT_EQ(ghostingRecords[i].server, ghostingRecords[i].client) << " className=" << className;
      else
         EXPECT_NE(ghostingRecords[i].server, ghostingRecords[i].client) << " className=" << className;
   }
}


/**
 * Test some Lua commands to all objects
 */
TEST_F(ObjectTest, LuaSanity)
{
   // Track the transmission state of each object
   U32 classCount = TNL::NetClassRep::getNetClassCount(NetClassGroupGame, NetClassTypeObject);

   // Create our pair of connected games
   GamePair gamePair;
   ClientGame *clientGame = gamePair.getClient(0);
   ServerGame *serverGame = gamePair.server;

   // Basic geometry to plug into polygon objects
   Vector<Point> geom;
   geom.push_back(Point(0,0));
   geom.push_back(Point(1,0));
   geom.push_back(Point(0,1));

   lua_State *L = lua_open();

   // Create one of each type of registered NetClass
   for(U32 i = 0; i < classCount; i++)
   {
      NetClassRep *netClassRep = TNL::NetClassRep::getClass(NetClassGroupGame, NetClassTypeObject, i);
      Object *obj = netClassRep->create();
      BfObject *bfobj = dynamic_cast<BfObject *>(obj);

      // Skip registered classes that aren't BfObjects (e.g. GameType) or don't have
      // a geometry at this point (ForceField)
      if(bfobj && bfobj->hasGeometry())
      {
         // First, add some geometry
         bfobj->setExtent(Rect(0,0,1,1));
         bfobj->GeomObject::setGeom(geom);

         // Lua testing
         lua_pushinteger(L, 1);
         bfobj->lua_setTeam(L);
         lua_pop(L, 1);
         lua_pushinteger(L, -2);
         bfobj->lua_setTeam(L);
         lua_pop(L, 1);
         luaPushPoint(L, 2.3f, 4.3f);
         bfobj->lua_setPos(L);
         lua_pop(L, 1);

         bfobj->addToGame(serverGame, serverGame->getGameObjDatabase());
      }
      else
         delete bfobj;
   }
   lua_close(L);
}


// Given a series of points, return "point.new(x1, y1), point.new(x2, y2), ..." or "{ point.new(x1, y1), point.new(x2, y2), ... }"
string pointsToLuaList(const Vector<Point> &points, bool asTable)
{
   string pointList = "";

   if(asTable)
      pointList += "{ ";

   for(S32 i = 0; i < points.size(); i++)
   {
      pointList += "point.new(" + points[i].toString() + ")";
      if(i < points.size() - 1)
         pointList += ", ";
   }

   if(asTable)
      pointList += " }";

   return pointList;
}


void createVerifyDeleteItem(ServerGame *serverGame, LuaLevelGenerator &levelgen, const string &luaTypeName, TypeNumber typeNumber, S32 objId, S32 teamIndex, const Vector<Point> &geom, bool asTable) {
   EXPECT_TRUE(levelgen.runString("obj = " + luaTypeName + ".new(" + pointsToLuaList(geom, asTable) + ", " + to_string(teamIndex + 1) + ")"));
   EXPECT_TRUE(levelgen.runString("obj:setId(" + to_string(objId) + ")"));
   EXPECT_TRUE(levelgen.runString("levelgen:addItem(obj)"));

   // Verify the object is as we expect
   BfObject *obj = serverGame->getGameObjDatabase()->findObjectById(objId);
   EXPECT_EQ(geom.size(), obj->getVertCount());
   EXPECT_EQ(teamIndex, obj->getTeam());

   // Verify actual coordiates of points (getting pretty pedantic here!)
   for(S32 i = 0; i < obj->getVertCount(); i++)
   {
      EXPECT_EQ(geom[i].x, obj->getVert(i).x);
      EXPECT_EQ(geom[i].y, obj->getVert(i).y);
   }

   serverGame->deleteObjects(typeNumber);     // Marks items as ready to delete
   serverGame->processDeleteList(1);          // Actually delete the objects
}

TEST_F(ObjectTest, CreateObjectsFromLua)
{
   ServerGame *serverGame = newServerGame();
   GameSettingsPtr settings = serverGame->getSettingsPtr();

   // Set-up our environment
   LuaScriptRunner::startLua(settings->getFolderManager()->luaDir);

   // Set up a levelgen object, with no script
   LuaLevelGenerator levelgen(serverGame);

   // Ensure environment set-up
   levelgen.prepareEnvironment();

   vector<Point> geom {Point(10, 10), Point(100, 100), Point(-20, -50)};

   // Compare adding objects using a list of points with a table of points.  Documentation states both are permitted.
   createVerifyDeleteItem(serverGame, levelgen, "LoadoutZone", LoadoutZoneTypeNumber, 1, 0, geom, true);
   createVerifyDeleteItem(serverGame, levelgen, "LoadoutZone", LoadoutZoneTypeNumber, 1, 0, geom, false);

   createVerifyDeleteItem(serverGame, levelgen, "GoalZone", GoalZoneTypeNumber, 1, 0, geom, true);
   createVerifyDeleteItem(serverGame, levelgen, "GoalZone", GoalZoneTypeNumber, 1, 0, geom, false);

   createVerifyDeleteItem(serverGame, levelgen, "LineItem", LineTypeNumber, 1, 0, geom, true);
   createVerifyDeleteItem(serverGame, levelgen, "LineItem", LineTypeNumber, 1, 0, geom, false);

   delete serverGame;
}


   
}; // namespace Zap
