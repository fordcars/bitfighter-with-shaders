--------------------------------------------------------------------------------
-- Copyright Chris Eykamp
-- See LICENSE.txt for full copyright information
--------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-- These functions will be included with every levelgen script automatically.
-- Do not tinker with these unless you are sure you know what you are doing!!
-- And even then, be careful!
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------

--
-- Some aliases and accessibility aids
--

-- Documented in luaLevelGenerator.cpp
function subscribe(...)
    bf:subscribe(...)
end

-- Documented in luaLevelGenerator.cpp
function levelgen:subscribe(...)
    bf:subscribe(...)
end

-- Documented in luaLevelGenerator.cpp
function unsubscribe(...)
    bf:unsubscribe(...)
end

function levelgen:unsubscribe(...)
    bf:unsubscribe(...)
end

function levelgen:sendData(...)
    bf:sendData(...)
end


-- Documented in luaLevelGenerator.cpp
function globalMsg(...)
   levelgen:globalMsg(...)
end


--
-- Add backwards compatibility for some API changes
--
--
-- Deprecation started in 019
--
function levelgen:findObjectById(id)
    return bf:findObjectById(id)
end

function levelgen:addItem(object)
    return bf:addItem(object)
end

function levelgen:pointCanSeePoint(p1, p2)
    printDeprecationWarning("levelgen:pointCanSeePoint(p1, p2)", "bf:pointCanSeePoint(p1, p2)")
    return bf:pointCanSeePoint(p1, p2)
end

function GameInfo()
    printDeprecationWarning("GameInfo()", "bf:getGameInfo()")
    return bf:getGameInfo()
end

function TeamInfo(index)
    printDeprecationWarning("TeamInfo(index)", "bf:getGameInfo():getTeam(index)")
    return bf:getGameInfo():getTeam(index)
end

--
-- Deprecation started in 018
--
function Point(x, y)
    printDeprecationWarning("Point(x,y)", "point.new(x,y)")
    return point.new(x, y)
end

--
-- Let the log know that this file was processed correctly
--
-- logprint("Loaded levelgen helper functions")