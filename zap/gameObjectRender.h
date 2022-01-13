//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GAMEOBJECTRENDER_H_
#define _GAMEOBJECTRENDER_H_

#ifndef ZAP_DEDICATED

#include "tnl.h"

#include "Geometry.h"

#include "Rect.h"
#include "Color.h"
#include "SharedConstants.h"     // For MeritBadges enum
#include "ShipShape.h"

#include "BfObject.h"            // Need to use BfObject with SafePtr.  Whether this is really needed is a different question.

#include <string>

using namespace TNL;
using namespace std;

namespace Zap
{

static const S32 NO_NUMBER = -1;

class Ship;
class WallItem;


//////////
// Primitives
extern void drawFilledCircle(const Point &pos, F32 radius, const Color *color = NULL);
extern void drawFilledSector(const Point &pos, F32 radius, F32 start, F32 end);
extern void drawCentroidMark(const Point &pos, F32 radius);

extern void drawRoundedRect(const Point &pos, F32 width, F32 height, F32 radius);
extern void drawRoundedRect(const Point &pos, S32 width, S32 height, S32 radius);


extern void drawFilledRoundedRect(const Point &pos, S32 width, S32 height, const Color &fillColor, 
                                  const Color &outlineColor, S32 radius, F32 alpha = 1.0);
extern void drawFilledRoundedRect(const Point &pos, F32 width, F32 height, const Color &fillColor, 
                                  const Color &outlineColor, F32 radius, F32 alpha = 1.0);

extern void drawArc(const Point &pos, F32 radius, F32 startAngle, F32 endAngle);

extern void drawEllipse(const Point &pos, F32 width, F32 height, F32 angle);
extern void drawEllipse(const Point &pos, S32 width, S32 height, F32 angle);

extern void drawFilledEllipse(const Point &pos, F32 width, F32 height, F32 angle);
extern void drawFilledEllipse(const Point &pos, S32 width, S32 height, F32 angle);

extern void drawPolygon(const Point &pos, S32 sides, F32 radius, F32 angle);

extern void drawStar(const Point &pos, S32 points, F32 radius, F32 innerRadius);
extern void drawFilledStar(const Point &pos, S32 points, F32 radius, F32 innerRadius);

extern void renderHexScale(const Point &center, F32 radius);

extern void drawAngledRay(const Point &center, F32 innerRadius, F32 outerRadius, F32 angle);
extern void drawAngledRayCircle(const Point &center, F32 innerRadius, F32 outerRadius, S32 rayCount, F32 offsetAngle);
extern void drawAngledRayArc(const Point &center, F32 innerRadius, F32 outerRadius, F32 centralAngle, S32 rayCount, F32 offsetAngle);
extern void drawDashedArc(const Point &center, F32 radius, F32 centralAngle, S32 dashCount, F32 dashSpaceCentralAngle, F32 offsetAngle);
extern void drawDashedHollowCircle(const Point &center, F32 innerRadius, F32 outerRadius, S32 dashCount, F32 dashSpaceCentralAngle, F32 offsetAngle);
extern void drawDashedCircle(const Point &center, F32 radius, S32 dashCount, F32 dashSpaceCentralAngle, F32 offsetAngle);
extern void drawHollowArc(const Point &center, F32 innerRadius, F32 outerRadius, F32 centralAngle, F32 offsetAngle);

extern void drawSquare(const Point &pos, F32 radius, bool filled = false);
extern void drawSquare(const Point &pos, S32 radius, bool filled = false);
extern void drawHollowSquare(const Point &pos, F32 radius, const Color *color = NULL);
extern void drawFilledSquare(const Point &pos, F32 radius, const Color *color = NULL);
extern void drawFilledSquare(const Point &pos, S32 radius, const Color *color = NULL);

extern void renderSmallSolidVertex(F32 currentScale, const Point &pos, bool snapping);

extern void renderVertex(char style, const Point &v, S32 number);
extern void renderVertex(char style, const Point &v, S32 number,           F32 scale);
extern void renderVertex(char style, const Point &v, S32 number,           F32 scale, F32 alpha);
extern void renderVertex(char style, const Point &v, S32 number, S32 size, F32 scale, F32 alpha);

void renderLine(const Vector<Point> *points, const Color *color);

extern void drawHorizLine(S32 x1, S32 x2, S32 y);
extern void drawVertLine (S32 x,  S32 y1, S32 y2);
extern void drawHorizLine(F32 x1, F32 x2, F32 y);
extern void drawVertLine (F32 x,  F32 y1, F32 y2);

extern void drawDashedLine(const Point &start, const Point &end, F32 period, F32 dutycycle = 0.5, F32 fractionalOffset = 0);

extern void drawFadingHorizontalLine(S32 x1, S32 x2, S32 yPos, const Color &color);

extern void renderSquareItem(const Point &pos, const Color *c, F32 alpha, const Color *letterColor, char letter);

extern void drawCircle(const Point &pos, S32 radius, const Color *color = NULL, F32 alpha = 1.0);
extern void drawCircle(const Point &pos, F32 radius, const Color *color = NULL, F32 alpha = 1.0);
extern void drawCircle(F32 x, F32 y,     F32 radius, const Color *color = NULL, F32 alpha = 1.0);

extern void drawDivetedTriangle(F32 height, F32 len);
extern void drawGear(const Point &center, S32 teeth, F32 r1, F32 r2, F32 ang1, F32 ang2, F32 innerCircleRadius, F32 angleRadians = 0.0f);


//////////
// Some things for rendering on screen display
extern F32 renderCenteredString(const Point &pos, S32 size, const char *string);
extern F32 renderCenteredString(const Point &pos, F32 size, const char *string);

// Renders the core ship, good for instructions and such
extern void renderShip(ShipShape::ShipShapeType shapeType, const Color *shipColor, const Color &hbc, F32 alpha, F32 thrusts[], F32 health, F32 radius, U32 sensorTime,
                       bool shieldActive, bool sensorActive, bool repairActive, bool hasArmor);

// Renders the ship and all the fixins
extern void renderShip(S32 layerIndex, const Point &renderPos, const Point &actualPos, const Point &vel, 
                       F32 angle, F32 deltaAngle, ShipShape::ShipShapeType shape, const Color *color, const Color &hbc, F32 alpha, 
                       U32 renderTime, const string &shipName, F32 warpInScale, bool isLocalShip, bool isBusy, 
                       bool isAuthenticated, bool showCoordinates, F32 health, F32 radius, S32 team, 
                       bool boostActive, bool shieldActive, bool repairActive, bool sensorActive, 
                       bool hasArmor, bool engineeringTeleport, U32 killStreak, U32 gamesPlayed);

extern void renderSpawnShield(const Point &pos, U32 shieldTime, U32 renderTime);

// Render repair rays to all the repairing objects
extern void renderShipRepairRays(const Point &pos, const Ship *ship, Vector<SafePtr<BfObject> > &repairTargets, F32 alpha);   

extern void renderShipCoords(const Point &coords, bool localShip, F32 alpha);

extern void drawFourArrows(const Point &pos);

extern void renderTeleporter(const Point &pos, U32 type, bool spiralInwards, U32 time, F32 zoomFraction, F32 radiusFraction, F32 radius, F32 alpha, 
                             const Vector<Point> *dests, U32 trackerCount = 100);
extern void renderTeleporterOutline(const Point &center, F32 radius, const Color &color);
extern void renderSpyBugVisibleRange(const Point &pos, const Color &color, F32 currentScale = 1);
extern void renderTurretFiringRange(const Point &pos, const Color &color, F32 currentScale);
extern void renderTurret(const Color &c, const Color &hbc, Point anchor, Point normal, bool enabled, F32 health, F32 barrelAngle, S32 healRate = 0);
extern void renderTurretIcon(const Point &pos, F32 scale, const Color *color);

extern void renderFlag(const Point &pos, const Color *flagColor);
extern void renderFlag(const Point &pos, F32 scale, const Color *flagColor);
extern void renderFlag(F32 x, F32 y, const Color *flagColor);
extern void renderFlag(const Color *flagColor);
extern void renderFlag(const Point &pos, const Color *flagColor, const Color *mastColor, F32 alpha);
extern void doRenderFlag(F32 x, F32 y, F32 scale, const Color *flagColor, const Color *mastColor, F32 alpha);


//extern void renderFlag(Point pos, Color c, F32 timerFraction);
extern void renderSmallFlag(const Point &pos, const Color &c, F32 parentAlpha);
extern void renderFlagSpawn(const Point &pos, F32 currentScale, const Color *color);

extern void renderZone(const Color *c, const Vector<Point> *outline, const Vector<Point> *fill);   

extern void renderLoadoutZone(const Color *c, const Vector<Point> *outline, const Vector<Point> *fill, 
                              const Point &centroid, F32 angle, F32 scaleFact = 1);

extern void renderLoadoutZoneIcon(const Point &center, S32 outerRadius, F32 angleRadians = 0.0f);

extern void renderNavMeshZone(const Vector<Point> *outline, const Vector<Point> *fill,
                              const Point &centroid, S32 zoneId);

class NeighboringZone;
extern void renderNavMeshBorders(const Vector<NeighboringZone> &borders);

extern void renderStars(const Point *stars, const Color *colors, S32 numStars, F32 alphaFrac, Point cameraPos, Point visibleExtent);

extern void drawObjectiveArrow(const Point &nearestPoint, F32 zoomFraction, const Color *outlineColor, 
                               S32 canvasWidth, S32 canvasHeight, F32 alphaMod, F32 highlightAlpha);

extern void renderScoreboardOrnamentTeamFlags(S32 xpos, S32 ypos, const Color *color, bool teamHasFlag);


// Some things we use internally, but also need from UIEditorInstructions for consistency
extern const Color BORDER_FILL_COLOR;
extern const F32 BORDER_FILL_ALPHA;
extern const F32 BORDER_WIDTH;
extern float gDefaultLineWidth;

extern void renderPolygonOutline(const Vector<Point> *outline);
extern void renderPolygonOutline(const Vector<Point> *outlinePoints, const Color *outlineColor, F32 alpha = 1, F32 lineThickness = gDefaultLineWidth);
extern void renderPolygonFill(const Vector<Point> *fillPoints, const Color *fillColor = NULL, F32 alpha = 1);
extern void renderPolygon(const Vector<Point> *fillPoints, const Vector<Point> *outlinePoints,
                          const Color *fillColor, const Color *outlineColor, F32 alpha = 1);

extern void renderGoalZone(const Color &c, const Vector<Point> *outline, const Vector<Point> *fill);     // No label version
extern void renderGoalZone(const Color &c, const Vector<Point> *outline, const Vector<Point> *fill, Point centroid, F32 labelAngle,
                           bool isFlashing, F32 glowFraction, S32 score, F32 flashCounter);
extern void renderGoalZoneIcon(const Point &center, S32 radius, F32 angleRadians = 0.0f);


extern void renderNexus(const Vector<Point> *outline, const Vector<Point> *fill, Point centroid, F32 labelAngle, 
                        bool open, F32 glowFraction);

extern void renderNexus(const Vector<Point> *outline, const Vector<Point> *fill, bool open, F32 glowFraction);
extern void renderNexusIcon(const Point &center, S32 radius, F32 angleRadians = 0.0f);


extern void renderSlipZone(const Vector<Point> *bounds, const Vector<Point> *boundsFill, const Point &centroid);
extern void renderSlipZoneIcon(const Point &center, S32 radius, F32 angleRadians = 0.0f);

extern void renderPolygonLabel(const Point &centroid, F32 angle, F32 size, const char *text, F32 scaleFact = 1);

extern void renderProjectile(const Point &pos, U32 style, U32 time);
extern void renderProjectileRailgun(const Point &pos, const Point &velocity, U32 time);
extern void renderSeeker(const Point &pos, U32 style, F32 angleRadians, F32 speed, U32 timeRemaining);

extern void renderMine(const Point &pos, bool armed, bool visible);
extern void renderGrenade(const Point &pos, U32 style, F32 lifeLeft);
extern void renderSpyBug(const Point &pos, const Color &teamColor, bool visible);

extern void renderRepairItem(const Point &pos);
extern void renderRepairItem(const Point &pos, bool forEditor, const Color *overrideColor, F32 alpha);

extern void renderEnergyItem(const Point &pos); 

extern void renderWallFill(const Vector<Point> *points, const Color &fillColor, bool polyWall);
extern void renderWallFill(const Vector<Point> *points, const Color &fillColor, const Point &offset, bool polyWall);

extern void renderEnergyItem(const Point &pos, bool forEditor);
extern void renderEnergySymbol();                                   // Render lightning bolt symbol
extern void renderEnergySymbol(const Point &pos, F32 scaleFactor);  // Another signature

// Wall rendering
void renderWallEdges(const Vector<Point> &edges, const Color &outlineColor, F32 alpha = 1.0);
void renderWallEdges(const Vector<Point> &edges, const Point &offset, const Color &outlineColor, F32 alpha = 1.0);

//extern void renderSpeedZone(Point pos, Point normal, U32 time);
void renderSpeedZone(const Vector<Point> &pts, U32 time);

void renderTestItem(const Point &pos, S32 size, F32 alpha = 1);
//void renderTestItem(const Point &pos, S32 size, F32 alpha = 1);
void renderTestItem(const Vector<Point> &points, F32 alpha = 1);

void renderAsteroid(const Point &pos, S32 design, F32 scaleFact, const Color *color = NULL, F32 alpha = 1);
void renderDefaultAsteroid(const Point &pos, S32 design, F32 scaleFact, F32 alpha = 1);
void renderAsteroidForTeam(const Point &pos, S32 design, F32 scaleFact, const Color *color, F32 alpha);

void renderAsteroidSpawn(const Point &pos, S32 time, const Color* color);
void renderAsteroidSpawnEditor(const Point &pos, const Color* color, F32 scale = 1.0);

void renderResourceItem(const Vector<Point> &points, F32 alpha = 1);
//void renderResourceItem(const Point &pos, F32 scaleFactor, const Color *color, F32 alpha);

struct PanelGeom;
void renderCore(const Point &pos, const Color *coreColor, const Color &hbc, U32 time, 
                PanelGeom *panelGeom, F32 panelHealth[10], F32 panelStartingHealth);

void renderCoreSimple(const Point &pos, const Color *coreColor, S32 width);

void renderSoccerBall(const Point &pos, F32 size);
void renderSoccerBall(const Point &pos);

void renderTextItem(const Point &pos, const Point &dir, F32 size, const string &text, const Color *color);

// Editor support items
extern void renderPolyLineVertices(BfObject *obj, bool snapping, F32 currentScale);
extern void renderGrid(F32 currentScale, const Point &offset, const Point &origin, F32 gridSize, bool fadeLines, bool showMinorGridLines);

extern void renderForceFieldProjector(const Point &pos, const Point &normal, const Color *teamColor, bool enabled, S32 healRate);
extern void renderForceFieldProjector(const Vector<Point> *geom, const Point &pos, const Color *teamColor, bool enabled, F32 health, S32 healRate = 0);
extern void renderForceField(const Point &start, const Point &end, const Color *c, bool fieldUp, F32 health = 1.0, U32 time = 0);

extern void renderBitfighterLogo(S32 yPos, F32 scale, U32 mask = 1023);
extern void renderBitfighterLogo(const Point &pos, F32 size, U32 letterMask = 1023);
extern void renderStaticBitfighterLogo();

extern void renderBadge(F32 x, F32 y, F32 rad, MeritBadges badge);

extern void renderWalls(const GridDatabase *wallSegmentDatabase, const Vector<Point> &wallEdgePoints, 
                        const Vector<Point> &selectedWallEdgePoints, const Color &outlineColor, 
                        const Color &fillColor, F32 currentScale, bool dragMode, bool drawSelected,
                        const Point &selectedItemOffset, bool previewMode, bool showSnapVertices, F32 alpha);


extern void renderWallOutline(WallItem *wallItem, const Vector<Point> *outline, const Color *color, 
                              F32 currentScale, bool snappingToWallCornersEnabled, bool renderVertices = false);

extern void drawLetter(char letter, const Point &pos, const Color &color, F32 alpha);
extern void renderSpawn(const Point &pos, F32 scale, const Color *color);
extern void renderFlightPlan(const Point &from, const Point &to, const Vector<Point> &flightPlan);
extern void renderHeavysetArrow(const Point &pos, const Point &dest, const Color &color, bool isSelected, bool isLitUp);
extern void renderTeleporterEditorObject(const Point &pos, S32 radius, const Color &color);

extern void renderFilledPolygon(const Point &pos, S32 points, S32 radius, const Color &fillColor);
extern void renderFilledPolygon(const Point &pos, S32 points, S32 radius, const Color &fillColor, const Color &outlineColor);
};

#else

// for ZAP_DEDICATED, we will just define blank functions, and don't compile gameObjectRender.cpp
// WHAT A HACK
#define renderSoccerBall
#define renderNexus
#define renderTextItem
#define renderSpeedZone
#define renderSlipZone
#define renderProjectile
#define renderProjectileRailgun
#define renderGrenade
#define renderMine
#define renderSpyBug
#define renderLoadoutZone
#define renderZone
#define renderGoalZone
#define renderForceFieldProjector
#define renderForceField
#define renderTurret
#define renderTurretIcon
#define renderSquareItem
#define renderNavMeshZone
#define renderNavMeshBorders
#define renderRepairItem
#define renderEnergyItem
#define renderAsteroid
#define renderDefaultAsteroid
#define renderAsteroidForTeam
#define renderCore
#define renderTestItem
#define renderResourceItem
#define renderFlag
#define renderWallFill
#define renderWallEdges
#define renderPolygonFill
#define renderPolygonOutline
#define drawCircle
#define drawSquare

#endif
#endif
