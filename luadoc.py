# This script preprocesses our C++ code and generates fodder for Doxygen which makes pretty documentation
# for our Lua scripters out there

# Works with doxygen verison 1.9.0

# HINT: Scanning the Bitfighter source code for the string "METHOD(CLASS," will locate all implemented Lua methods.


# TODO: UIMenuItems_cpp.h

import os
from glob import glob
import re
import subprocess
from typing import List, Any, Dict, Optional, Tuple
from lxml import etree
from datetime import datetime
import shlex

# Need to be in the doc directory
doc_dir = "doc"
outpath = os.path.join(os.path.dirname(__file__), doc_dir)

if not os.path.exists(outpath):
    raise Exception("Could not change to doc folder!")

# Relative path where intermediate outputs will be written
outpath = os.path.join(outpath, "temp-doxygen")

# Create it if it doesn't exist
if not os.path.exists(outpath):
    os.makedirs(outpath)


doxygen_cmd = R"C:\Program Files\doxygen\bin\doxygen.exe"


# These are items we collect and build up over all pages, and they will get written to a special .h file at the end
mainpage = []
otherpage = []
enums = []

FUNCS_HEADER_MARKER = "int DummyArg"

NBSP = "&#160;"


# These flags are ignored if DEBUG_MODE is False
# These flags are used to let you run a subset of the process while debugging.
# TODO: Document this better
DEBUG_MODE = False              # Set to False for production mode

DEBUG_PREPROCESS = False
DEBUG_DOXYGEN = False
DEBUG_POST_PROCESS = True      # Only do post processing stage; need to copy files from html to html-final


# Adjust these above only; do not change the settings below
if not DEBUG_MODE:
    DEBUG_PREPROCESS = True
    DEBUG_DOXYGEN = True
    DEBUG_POST_PROCESS = True
else:
    print("Running in debug mode; if this is a production run, be sure to set DEBUG_MODE to False!")


def main():
    if DEBUG_PREPROCESS:
        preprocess()           # --> Writes files to temp-doxygen

    if DEBUG_DOXYGEN:
        run_doxygen()          # --> Writes files to html

    if DEBUG_POST_PROCESS:
        postprocess_classes()
          # --> Overwrites files in html
        # postprocess_enums()
    pass


class CollectingMode:
    LONG_COMMENT = "LONG_COMMENT"
    MAIN_PAGE = "MAIN_PAGE"
    OTHER_PAGE = "OTHER_PAGE"
    METHODS = "METHODS"
    STATIC_METHODS = "STATIC_METHODS"
    LUA_ENUM = "LUA_ENUM"
    EXPECTING_LUA_ENUM_COMMENT = "EXPECTING_LUA_ENUM_COMMENT"
    LUA_ENUM_COMMENT = "LUA_ENUM_COMMENT"
    CPP_ENUM_DEFINE = "CPP_ENUM_DEFINE"
    LUA_CONST = "LUA_CONST"
    NONE = "NONE"


def preprocess():
        # Collect some file names to process
    files = []

    files.extend(glob(os.path.join(outpath, "../../zap/*.cpp")))               # Core .cpp files
    files.extend(glob(os.path.join(outpath, "../../zap/*.h")))                 # Core .h files
    files.extend(glob(os.path.join(outpath, "../../lua/lua-vec/src/*.c")))     # Lua-vec .c files  -- none here anymore... can probably delete this line
    files.extend(glob(os.path.join(outpath, "../../resource/scripts/*.lua")))  # Some Lua scripts
    files.extend(glob(os.path.join(outpath, "../static/*.txt")))               # our static pages for general information and task-specific examples


    # if DEBUG_MODE:
    #     files = [
    #         R"C:\dev\bitfighter\zap\flagItem.h",
    #         R"C:\dev\bitfighter\zap\flagItem.cpp",
    #         # R"C:\dev\bitfighter\resource\scripts\luavec.lua"
    #     ]

    # Loop through all the files we found above...
    for file_cnt, file in enumerate(files):
        # print(f"Processing {os.path.basename(file)}...", end="", flush=True)
        update_progress(file_cnt / len(files), os.path.basename(file))

        enumIgnoreColumn: Optional[int] = None
        enumName = ""
        enumColumn: int = -1

        collecting_mode = CollectingMode.NONE
        collecting_class = ""

        descrColumn: Optional[int] = None
        encounteredDoxygenCmd = False

        luafile = file.endswith(".lua")   # Are we processing a .lua file?

        methods = []
        staticMethods = []
        globalfunctions = []
        comments = []
        classes = {}         # Will be a dict of arrays

        shortClassDescr = ""
        longClassDescr  = ""

        const_declaration = ""

        with open(file, "r") as filex:
            # Visit each line of the source cpp file
            for line in filex:

                # If we are processing a .lua file, replace @luaclass with @luavclass for simplicity
                if luafile:
                    line = line.replace("@luaclass", "@luavclass")

                #####
                # LUA BASE CLASSES in C++ code
                #####

                # REGISTER_LUA_CLASS(LuaPlayerInfo);
                match = re.search(r"REGISTER_LUA_CLASS *\( *(.+?) *\)", line)
                if match:
                    classname = cleanup_classname(match.groups()[0])        # ==> PlayerInfo
                    if classname not in classes:
                        classes[classname] = []

                    classes[classname].insert(0, f"{shortClassDescr}\n{longClassDescr}\nclass {classname} {{ \n")
                    continue

                #####
                # LUA SUBCLASSES in C++ code
                #####
                match = re.search(r"REGISTER_LUA_SUBCLASS *\( *(.+?) *, *(.+?) *\)", line)      # REGISTER_LUA_SUBCLASS(WallItem, BfObject);
                if match:
                    classname = cleanup_classname(match.groups()[0])   # ==> WallItem
                    parent = cleanup_classname(match.groups()[1])      # ==> BfObject
                    if classname not in classes:
                        classes[classname] = []
                    classes[classname].insert(0, f"{shortClassDescr}\n{longClassDescr}\nclass {classname} : public {parent} {{ \n ")
                    shortClassDescr = ""
                    longClassDescr = ""
                    continue


                #####
                # LUA METHODS block in C++ code
                #####

                def make_method_line(method_name: str) -> str:
                    return f"void {method_name}() {{ }}\n"

                match = re.search(r"define +LUA_METHODS *\(CLASS, *METHOD\)", line)
                if match:
                    collecting_mode = CollectingMode.METHODS
                    continue

                if collecting_mode == CollectingMode.METHODS:
                    match = re.search(r"METHOD *\( *CLASS, *(.+?) *,", line)                 # Signals class declaration... methods will follo
                    if match:

                        # Special case; suppress this item until the code is written
                        # Block ID 8675309
                        if match.groups()[0].strip() == "clone":
                            continue

                        methods.append(match.groups()[0])
                        continue

                    # GENERATE_LUA_METHODS_TABLE(LuaPlayerInfo, LUA_METHODS);
                    match = re.search(r"GENERATE_LUA_METHODS_TABLE *\( *(.+?) *,", line)      # Signals we have all methods for this class, gives us class name; now generate cod
                    if match:
                        classname = cleanup_classname(match.groups()[0])    # ==> PlayerInfo

                        for method in methods:
                            if classname not in classes:
                                classes[classname] = []
                            classes[classname].append(make_method_line(method))

                        methods = []
                        collecting_mode = CollectingMode.NONE
                        continue

                match = re.search(r"define +LUA_STATIC_METHODS *\( *METHOD *\)", line)
                if match:
                    collecting_mode = CollectingMode.STATIC_METHODS
                    continue


                if collecting_mode == CollectingMode.STATIC_METHODS:
                    match = re.search(r"METHOD *\( *(.+?) *,", line)                 # Signals class declaration... methods will follow
                    if match:
                        method = match.groups()[0]
                        staticMethods.append(method + "\n")
                        continue

                    # GENERATE_LUA_STATIC_METHODS_TABLE(Geom, LUA_STATIC_METHODS);
                    match = re.search(r"GENERATE_LUA_STATIC_METHODS_TABLE *\( *(.+?) *,", line)  # Signals we have all methods for this class, gives us class name; now generate code

                    if match:
                        classname = cleanup_classname(match.groups()[0])        # ==> Geom

                        if classname not in classes:
                            classes[classname] = []

                        # This becomes our "constructor"
                        classes[classname].insert(0, f"{shortClassDescr}\n{longClassDescr}\nclass {classname} {{ \n")

                        collecting_mode = CollectingMode.NONE
                        continue

                #####
                # LONG COMMENTS (anything starting with "/**" in C++ or "--[[" in Lua)
                # These tokens signal we're entering the beginning of a long comment block we need to pay attention to
                comment_pattern = r"--\[\[" if luafile else r"/\*\*"
                if re.search(comment_pattern, line):
                    collecting_mode = CollectingMode.LONG_COMMENT
                    comments.append("/*!\n")
                    continue


                if collecting_mode not in (CollectingMode.NONE, CollectingMode.EXPECTING_LUA_ENUM_COMMENT):
                    end_comment_found = re.search(r"--\]\]" if luafile else r"\*/", line)

                    if end_comment_found:
                        if collecting_mode == CollectingMode.LUA_CONST:
                            classes[collecting_class].append("*/\n")
                            classes[collecting_class].append(const_declaration + "\n")
                            collecting_mode = CollectingMode.NONE
                            continue


                        if collecting_mode == CollectingMode.LONG_COMMENT:
                            if end_comment_found:
                                comments.append("*/\n")
                                collecting_mode = CollectingMode.NONE
                                encounteredDoxygenCmd = False
                                continue

                        if collecting_mode == CollectingMode.LUA_ENUM:
                            collecting_mode = CollectingMode.EXPECTING_LUA_ENUM_COMMENT
                            continue

                        collecting_mode = CollectingMode.NONE
                        continue


                if collecting_mode == CollectingMode.LONG_COMMENT:
                    if re.search(r"@mainpage\s", line):
                        mainpage.append(line)
                        collecting_mode = CollectingMode.MAIN_PAGE
                        encounteredDoxygenCmd = True
                        continue

                    if re.search(r"@page\s", line):
                        otherpage.append(line)
                        collecting_mode = CollectingMode.OTHER_PAGE
                        encounteredDoxygenCmd = True
                        continue

                    if re.search(r"\*\s+\\par", line):       # Match * \par => Paragraph
                        encounteredDoxygenCmd = True
                        continue

                if collecting_mode not in (CollectingMode.NONE, CollectingMode.EXPECTING_LUA_ENUM_COMMENT, CollectingMode.CPP_ENUM_DEFINE):
                    # Check for some special custom tags...

                    #####
                    # @LUAENUM in C++ code
                    #####
                    # Check the regexes here: http://www.regexe.com
                    # Handle Lua enum defs: "@luaenum ObjType(2)" or "@luaenum ObjType(2,1,4)"  1, 2, or 3 nums are ok
                    #                                $1        $2           $4           $6
                    match = re.search(r"@luaenum\s+(\w+)\s*\((\d+)\s*(,\s*(\d+)\s*(,\s*(\d+))?)?\s*\)", line)
                    if match:
                        collecting_mode = CollectingMode.LUA_ENUM
                        enumName = match.groups()[0]
                        enumColumn = int(match.groups()[1])
                        descrColumn      = None if match.groups()[3] is None else int(match.groups()[3])   # Optional
                        enumIgnoreColumn = None if match.groups()[5] is None else int(match.groups()[5])   # Optional
                        encounteredDoxygenCmd = True

                        enums.append(f"/**\n * @defgroup {enumName}Enum {enumName}\n")

                        continue
                        # Parsing:
                        # /**
                        #   * @luaenum ClipType(1,1)
                        #   * Supported operations for clipPolygon. For a description of the operations...
                        #   */
                        # #define CLIP_TYPE_TABLE \
                        #     CLIP_TYPE_ITEM( Intersection, ClipperLib::ctIntersection ) \
                        #     CLIP_TYPE_ITEM( Union,        ClipperLib::ctUnion) \
                        #     CLIP_TYPE_ITEM( Difference,   ClipperLib::ctDifference ) \
                        #     CLIP_TYPE_ITEM( Xor,          ClipperLib::ctXor ) \


                    # Look for @geom, and replace with @par Geometry \n
                    match = re.search(r"@geom\s+(.*)$", line)
                    if match:
                        # print(f"line = {line}")
                        body = match.groups()[0]
                        # print(f"body = {body}")
                        comments.append(f"\\par Geometry\n{body}\n")
                        continue

                    #####
                    # @LUAFUNCSHEADER delcaration in C++ code
                    # Look for  "* @luafuncsheader <class>" followed by a block of text.  class is the class we're documenting, obviously.
                    # This is a magic item that lets us, through extreme hackery, inject a comment at the top of the functions list (as is done with Ship)
                    # Currently only support one per file.  That should be enough.
                    match = re.search(r"^\s*\*\s*@luafuncsheader\s+(\w+)", line)        # @luafuncsheader Ship
                    if match:
                        # Now we need to do something that will make it through Doxygen and emerge in a recognizable form for the postprocessor to work on
                        # We'll use a dummy function and some dummy documentation.  We'll fix it in post!
                        classname = cleanup_classname(match.groups()[0])        # ==> Ship
                        if classname not in classes:
                            classes[classname] = []

                        classes[classname].append(f"private:\nvoid {classname} ({FUNCS_HEADER_MARKER}) {{ }}\n")    # Make it look like a private constructor to discourage
                                                                                                                        # Doxygen from inserting it into child classes

                        # And the dummy documentation -- the encounteredDoxygenCmd tells us to keep reading until we end the comment block
                        comments.append(f"\\fn {classname}::{classname}({FUNCS_HEADER_MARKER})\n")
                        encounteredDoxygenCmd = True

                        continue

                    #####
                    # @LUACONST delcaration in C++ or Lua code
                    #####
                    # Transform:
                    #
                    # --[[
                    # @luaconst point.one
                    # @brief Constant representing the point (1, 1).
                    #
                    # Using this constant is marginally more efficient than defining it yourself.
                    # --]]
                    # point.one = point.new(1,1)
                    #
                    # into:
                    #
                    # /**
                    # __FULLNAME: point.zero
                    # @brief Constant representing the point (0, 0).
                    #
                    # Using this constant is marginally more efficient than defining it yourself.
                    # */
                    # point zero;

                    match = re.search(r"^\s*\*?\s*@luaconst\s+(.*)$", line)
                    if match:
                        fullname = match.groups()[0].strip()   # Arg to @luaconst
                        collecting_class, name = re.split("[.:]", fullname.replace("::", ":"))   # Split out classname regardless of whether separator is :, ::, or .

                        classes[collecting_class].append(f"/**\n__FULLNAME: {fullname}\n")

                        const_declaration = f"{collecting_class} {name};"

                        collecting_mode = CollectingMode.LUA_CONST
                        encounteredDoxygenCmd = True

                        continue

                    #####
                    # @LUAFUNC delcaration in C++ code
                    #####
                    # Look for: "* @luafunc static table Geom::triangulate(mixed polygons)"  [retval (table) and args are optional]

                    #   staticness -> "static"
                    #   voidlessRetval -> "table"
                    #   classname -> "Geom"
                    #   method -> "triangulate"
                    #   args -> "mixed polygons"
                    match = re.search(r"^\s*\*?\s*@luafunc\s+(.*)$", line)
                    if match:
                        # In C++ code, we use "::" to separate classes from functions (class::func); in Lua, we use "." or ":" (class.func).
                        sep = "[.:]" if luafile else "::"

                        # @luafunc bool Ship::isAlive()
                        #                                      $1          $2       $3                 $4    $5
                        match = re.search(r".*?@luafunc\s+(static\s+)?(\w+\s+)?(?:(\w+)" + sep + r")?(.+?)\((.*)\)", line)     # Grab retval, class, method, and args from line

                        staticness = match.groups()[0].strip() if match.groups()[0] else ""
                        retval = match.groups()[1] if match.groups()[1] else "void"         # Retval is optional, use void if omitted
                        voidlessRetval = match.groups()[1] or ""
                        classname  = cleanup_classname(match.groups()[2]) or "global"       # If no class is given the function is assumed to be global
                        method = match.groups()[3]
                        args   = match.groups()[4]     # Args are optional

                        if not method:
                            raise Exception(f"Couldn't get method name from {line}\n")   # Must have a method; should never happen


                        if args:        # point pos, int teamIndex
                            parts = args.split(",")
                            arg_parts = []
                            for part in parts:
                                part = part.strip()
                                if " " in part:         # e.g. BarrierItem has a PolyGeom that this doesn't like
                                    arg_parts.append(part.split(" ")[1])
                                else:
                                    arg_parts.append(part)
                                    # print(part)
                            classless_arg_list = ", ".join(arg_parts)
                        else:
                            classless_arg_list = ""

                        # retval =~ s|\s+$||      # Trim any trailing spaces from $retval

                        # Use voidlessRetval to avoid having "void" show up where we'd rather omit the return type altogether
                        comments.append(f" \\fn {voidlessRetval} {classname}::{method}({args})\n")

                        # Here we generate some boilerplate standard code and such
                        is_constructor = classname == method
                        if is_constructor:      # Constructors come in the form of class::method where class and method are the same

                            # SPECIAL CASE HANDLER!
                            # Because the point class is lowercase, the default example we generate below won't work.  Insert a different one.
                            if classname == "point":
                                comments.append(f"\\brief Constructor.\n\nExample:\n@code\npt = point.new(100, 300)\n" +
                                                f"testitem = TestItem.new(pt)\nlevelgen:addItem(testitem)\n@endcode\n\n")
                            else:
                                comments.append(f"\\brief Constructor.\n\nExample:\n@code\n{classname.lower()} = {classname}.new({classless_arg_list})\n...\n" +
                                                f"levelgen:addItem({classname.lower()})\n@endcode\n\n")

                        # Find an earlier definition and delete it (if it still exists); but not if it's a constructor.
                        # This might have come from an earlier GENERATE_LUA_METHODS_TABLE block.
                        # We do this in order to provide more complete method descriptions if they are found subsequently.  I think.
                        else:
                            try:
                                if classname in classes:
                                    classes[classname].remove(make_method_line(method))
                            except ValueError:
                                pass    # Item is not in the list; this is fine, nothing to do.

                        # Add our new sig to the list
                        if classname:
                            if classname not in classes:
                                classes[classname] = []

                            classes[classname].append(f"public:\n{staticness} {retval} {method}({args}) {{ /* From '{line.strip()}' */ }}\n")
                        else:
                            globalfunctions.append(f"{retval} {method}({args}) {{ /* From '{line}' */ }}\n")

                        encounteredDoxygenCmd = True

                        continue


                    match = re.search(r"@luaclass\s+(\w+)\s*$", line)        # Description of a class defined in a header file
                    if match:
                        comments.append(f" \\class {cleanup_classname(match.groups()[0])}\n")

                        encounteredDoxygenCmd = True

                        continue


                    match = re.search(r"@luavclass\s+(\w+)\s*$", line)       # Description of a virtual class, not defined in any C++ code
                    if match:
                        classname = cleanup_classname(match.groups()[0])
                        comments.append(f" \\class {classname}\n")

                        if classname not in classes:
                            classes[classname] = []
                        classes[classname].append(f"class {cleanup_classname(match.groups()[0])} {{\n")
                        # classes[classname].append("public:\n")

                        encounteredDoxygenCmd = True

                        continue

                    match = re.search(r"@descr\s+(.*)$", line)
                    if match:
                        comments.append(f"\n {cleanup_classname(match.groups()[0])}\n")
                        encounteredDoxygenCmd = True
                        continue

                    # Otherwise keep the line unaltered and put it in the appropriate array
                    if collecting_mode == CollectingMode.MAIN_PAGE:
                        mainpage.append(line)
                    elif collecting_mode == CollectingMode.OTHER_PAGE:
                        otherpage.append(line)
                    elif collecting_mode == CollectingMode.LUA_CONST:
                        classes[collecting_class].append(line)
                    elif collecting_mode in (CollectingMode.LUA_ENUM, CollectingMode.CPP_ENUM_DEFINE):
                        enums.append(line)
                    elif encounteredDoxygenCmd:
                        comments.append(line)       # @code ends up here

                    continue

                # Starting with an enum def that looks like this:
                # /**
                #  * @luaenum Weapon(2[,n])  <=== collecting_mode = CollectingMode.LUA_ENUM
                #                                 2 refers to 0-based index of column containing Lua enum name,
                #                                 n refers to column specifying whether to include this item
                #  * The Weapon enum can be used to represent a weapon in some functions.
                #  */                       <=== collecting_mode = CollectingMode.EXPECTING_LUA_ENUM_COMMENT
                #
                #  #define WEAPON_ITEM_TABLE \          <=== collecting_mode = CollectingMode.CPP_ENUM_DEFINE
                #    WEAPON_ITEM(WeaponPhaser,     "Phaser",      "Phaser",     100,   500,   500,  600, 1000, 0.21f,  0,       false, ProjectilePhaser ) \
                #    WEAPON_ITEM(WeaponBounce,     "Bouncer",     "Bouncer",    100,  1800,  1800,  540, 1500, 0.15f,  0.5f,    false, ProjectileBounce ) \
                #
                # Make this:
                # /** @defgroup WeaponEnum Weapon
                #  *  The Weapons enum has values for each type of weapon in Bitfighter.
                #  *  @{
                #  *  @section Weapon
                #  * __Weapon__
                #  * * %Weapon.%Phaser
                #  * * %Weapon.%Bouncer
                #  @}

                if collecting_mode in (CollectingMode.EXPECTING_LUA_ENUM_COMMENT, CollectingMode.CPP_ENUM_DEFINE):

                    # If we get here we presume the @luaenum comment has been closed, and the next #define we see will begin the enum itself
                    # Enum will continue until we hit a line with no trailing \
                    match = re.search(r"#\s*define", line)
                    if match:
                        enums.append("@{\n")
                        enums.append(f"# {enumName}\n")   # Add the list header
                        collecting_mode = CollectingMode.CPP_ENUM_DEFINE
                        continue

                    if collecting_mode == CollectingMode.EXPECTING_LUA_ENUM_COMMENT:
                        continue     # Skip lines until we hit a #define

                    ends_in_backslash = re.search(r"\\[\r\n]*$", line)      # Examine line before it gets munged below

                    # There are a lot of reasons we might not do work on a line; if we detect any, set skip to True.  But
                    # even for those lines, we need to test whether we're exiting the CPP comment block.
                    skip = False

                    # If we're here, collecting_mode == CollectingMode.CPP_ENUM_DEFINE, and we're processing an enum definition line
                    line = re.sub(r"/\*.*?\*/", "", line)   # Remove embedded comments
                    if re.search(r"^\W*\\$", line):         # Skip any lines that have no \w chars, as long as they end in a backslash
                        skip = True

                    # Skip blank lines, or those that look like they are starting with a comment
                    if re.search(r"^\s*$", line) or re.search(r"^\s*//", line) or re.search(r"\s*/\*", line):
                        skip = True

                    # Extract the good bits (but with arbitrary whitespace between items):
                    #   WeaponPhaser, "Phaser", "Phaser", 100, 500, 500, 600, 1000, 0.21f, 0, false, ProjectilePhaser
                    match = re.search(r"[^(]+\((.+)\)", line)
                    if not match:
                        skip = True

                    if not skip:
                        line = match.groups()[0]
                        line = line.replace(",", ", ")      # ==> Make sure this will split properly: MODULE_ITEM(ModuleEngineer,"Engineer",

                        # words = re.search(r'("[^"]+"|[^,]+)(?:,\s*)?', line)
                        words = shlex.split(line)         # Splits without breaking up quoted strings

                        # Skip items marked as not to be shared with Lua...
                        # See #define TYPE_NUMBER_TABLE for example. BotNavMeshZoneTypeNumber is not shared.
                        if enumIgnoreColumn is None or words[enumIgnoreColumn].strip(",") == "true":

                            enumDescr = words[descrColumn] if descrColumn is not None else ""

                            # # Clean up descr -- remove leading and traling non-word characters... i.e. junk  (probably no longer needed, but harmless)
                            # enumDescr = re.sub(r'^\W*"', "", enumDescr)         # Remove leading junk
                            # enumDescr = re.sub(r'"\W*$', "", enumDescr)         # Remove trailing junk


                            # Suppress any words that might trigger linking by prepending a "%" to them
                            descr_words = enumDescr.split()

                            enumDescr = " %".join(descr_words).strip(",").replace("%`", "`")     # Also normalizes interior spaces, if that's an issue
                            enumDescr = enumDescr.replace(R"\link %", R"\link ")                 # Undo quoting of \links.  e.g. \link %ForceFieldProjector
                            enumDescr = enumDescr.replace("%(", "(")                             # Undo quoting of ()s...  they get b0rked
                            enumDescr = enumDescr.replace("%->", "->")                           # Undo quoting of arrows...
                            # TODO --> Replace %([^a-zA-Z]) with \1 ?

                            # This bit really only applies to the Event enum
                            # Selectively remove %s to convert this:
                            #   %onShipKilled(Ship %ship, %BfObject %damagingObject, %BfObject %shooter)
                            # to this:
                            #   %onShipKilled(Ship %ship, BfObject %damagingObject, BfObject %shooter)
                            # This will make our classes linkable (just omitting the % above does not work for unknown reasons)
                            match = re.match(r"^(.*)\((.+?)\)(.*)$", enumDescr)   # ["%onShipKilled", "Ship %ship, %BfObject %damagingObject, %BfObject %shooter", ""]
                            if match:
                                arglist = ""
                                args = match.groups()[1].split(",")    # Ship %ship, %BfObject %damagingObject, %BfObject %shooter
                                for arg in args:
                                    a = arg.split()     # ["Ship", "%ship"]   //  ["%BfObject", "%damagingObject"]  //   ["%BfObject", "%shooter"]
                                    arglist = a[0].replace("%", "") + " " + a[1]
                                enumDescr = f"{match.groups()[0]}({arglist}){match.groups()[2]}"

                            enumval = words[enumColumn].replace(",", "")
                            # enumval = re.sub(r'[\s"\)\\]*', "", enumval)         # Strip out quotes and whitespace and other junk

                            # continue unless $enumval;      # Skip empty enumvals... should never happen, but does
                            # `s cause text to be wrapped in <code> tags
                            enums.append(f" * * `%{enumName}.%{enumval}`<br>\n{enumDescr}<br>\n");    # Produces:  * * %Weapon.Triple  Triple

                            # no next here, always want to do the termination check below

                    if not ends_in_backslash:  # Line has no terminating \, it's the last of its kind!
                        enums.append("@}\n");       # Close doxygen group block
                        enums.append("*/\n\n");     # Close comment
                        if CollectingMode.CPP_ENUM_DEFINE:
                            collecting_mode = CollectingMode.NONE          # This enum is complete!

                    continue

        # Done reading this file, time to write!

        # If we added any lines to keepers, write it out... otherwise skip it!
        if globalfunctions or comments or classes:
            base, ext = os.path.splitext(os.path.basename(file))
            ext = ext.replace(".", "")

            # Write the simulated .h file

            outfile = os.path.join(outpath, f"{base}__{ext}.h")

            with open(outfile, "w") as filex:
                filex.write("// This file was generated automatically from the C++ source to feed doxygen.  It will be overwritten.\n\n")
                filex.write(f"// Generated {datetime.now()}\n\n")


                for key in classes:
                    filex.write("".join(classes[key]));      # Main body of class
                    filex.write(f"}}; // {key}\n")  # Close the class

                filex.write("\n\n// What follows is a dump of the globalfunctions list\n\n")
                filex.write("namespace global {\n" + "\n".join(globalfunctions) + "}\n")

                filex.write("\n\n// What follows is a dump of the comments list\n\n")
                filex.write("".join(comments))
            # print(" done.")
        else:
            # print(" nothing to do.")
            pass

    # Finally, write our main page data... some of this was collected from multiple files.
    with open(os.path.join(outpath, "main_page_content.h"), "w") as filex:
        filex.write("// This file was generated automatically from the C++ source to feed doxygen.  It will be overwritten.\n\n")
        filex.write(f"// Generated {datetime.now()}\n\n")

        filex.write("/**\n")
        filex.write("".join(mainpage) + "\n")       # This section needs to come first, apparently
        filex.write("".join(otherpage) + "\n")
        filex.write("*/\n")

        filex.write("".join(enums))

    update_progress(1)


def cleanup_classname(classname: str) -> str:
    """ Do some renaming to avoid awkward names (LuaPlayerInfo ==> PlayerInfo, LuaGameInfo ==> GameInfo, etc.) """
    if classname.startswith("Lua"):
        return classname.replace("Lua", "", 1)
    return classname


def run_doxygen():
    os.chdir("doc")
    subprocess.run(f"{doxygen_cmd} luadocs.doxygen luadocs_html_extra_stylesheet.css")
    os.chdir("..")


def postprocess_enums():
    os.chdir("doc")
    files = glob("./html/*_enum.html")
    for file_ct, file in enumerate(files):
        update_progress(file_ct / len(files), os.path.basename(file))

        with open(file, "r") as infile:
            root = etree.HTML(infile.read())

        format_enums(root)

        if DEBUG_MODE and DEBUG_POST_PROCESS:
            file = file.replace("html", "html-final", 1)

        with open(file, "w") as outfile:
            outfile.write(etree.tostring(root).decode("utf-8"))


    update_progress(1)
    os.chdir("..")


def format_enums(root: Any) -> None:
    # Need to swap where the code styline is applied.  Add some spans with classes while we're here.
    elements = root.xpath(f"//div[@class='contents']/ul/li/code")
    for element in elements:
        element.getparent().insert(1, etree.fromstring(f'<span class="enum-descr">{element.text}</span>'))
        delete_node(element)

    elements = root.xpath(f"//div[@class='contents']/ul/li")
    for element in elements:
        element.insert(0, etree.fromstring(f'<span class="enum-name"><code>{element.text}</code></span>'))
        element.text = ""



    # Remove some annoying BRs
    elements = root.xpath(f"//li/br")
    for element in elements:
        delete_node(element)


def get_class_files() -> List[str]:
    files = glob("./html/class_*.html")
    files.extend(glob("./html/classpoint*.html"))
    files.extend(glob("./html/classglobal*.html"))

    return files


def postprocess_classes():
    """ Post-process the generated doxygen stuff """

    print("Fixing doxygen output...")

    os.chdir("doc")
    class_urls = {}     # Map of class to url that describes it

    # First pass
    files = get_class_files()
    files.extend(glob("./html/group__*.html"))


    # Rip through them first to build a map of all the class --> urls so we can create links
    for file_ct, file in enumerate(files):
        update_progress(file_ct / len(files), os.path.basename(file))
        with open(file, "r") as infile:
            root = etree.HTML(infile.read())
            class_urls.update(get_class_urls(root, file))
    update_progress(1)


    # Second pass
    files = get_class_files()

    # if DEBUG_MODE:
    #     files = glob("./html/*point*.html")
    #     # files = [
    #     #     "./html/flagItem.h",
    #     #     "./html/flagItem.cpp",
    #     # ]


    for file_ct, file in enumerate(files):
        update_progress(file_ct / len(files), os.path.basename(file))
        dirty_html = ""
        cleaned_html = ""

        with open(file, "r") as infile:

            # Do some quick text cleanup as we read the file, but before we feed it to etree
            for line in infile:
                dirty_html += line
                cleaned_html += cleanup_html(line)


        # Do some fancier parsing with something that understands the document structure
        root = etree.HTML(cleaned_html)

        remove_first_two_more_links(root)   # They're annoying
        replace_text(root, "Public Member Function", "Member Function")     # Remove the word "Public" (it's confusing!)
        replace_text(root, "Member Data Documentation", "Constants")        # Fits better with the context
        replace_text(root, "Public Attributes", "Constants")                # Fits better with the context

        replace_text(root, "More...", "[details]")

        handle_dummy_constructor_element(root)          # Has to go before remove_destructor_text, which alters one of our markers

        # Clear the type declaration for function return types in the upper portion of the class definitions
        elements = root.xpath(f"//table[@class='memberdecls']//td[@class='memItemLeft']")
        for element in elements:
            element.clear()


        remove_space_after_method_name(root)
        remove_types_from_method_declarations_section(root)
        remove_destructor_text(root)
        cleanup_member_details(root, class_urls)
        cleanup_constant_defs(root)


        if DEBUG_MODE and DEBUG_POST_PROCESS:
            file = file.replace("html", "html-final", 1)
        # if not os.path.exists(os.path.dirname(file)):
        #     os.mkdir(os.path.dirname(file))

        with open(file, "w") as outfile:
            outfile.write(etree.tostring(root).decode("utf-8"))

    update_progress(1)
    os.chdir("..")


def delete_node(element: Any) -> None:
    element.getparent().remove(element)


def handle_dummy_constructor_element(root: Any) -> None:
    """ This is a lot of work to go through to get a warning placed at the top of selected class method lists! """

    elements_to_delete = []

    # Find rows in our Member Functions table that refer to the DummyConstructor
    # Looking for rows in tables of class memberdelcs that contain a td that contains a link for DummyConstructor
    elements = root.xpath(f"//table[@class='memberdecls'][descendant::td[contains(text(),'{FUNCS_HEADER_MARKER}')]]")
    if elements:
        elements_to_delete.append(elements[0])
        # Deletes:
        # <table class="memberdecls">
        #     <tr class="heading">
        #         <td colspan="2"><h2 class="groupheader"><a name="pri-methods" id="pri-methods"></a>Private Member Functions</h2></td>
        #     </tr>
        #     <tr class="memitem:a2b298db4d6febba2e202fd0f3822f607">
        #         <td class="memItemLeft" align="right" valign="top">void&#160;</td>
        #         <td class="memItemRight" valign="bottom"><a class="el" href="class_ship.html#a2b298db4d6febba2e202fd0f3822f607">Ship</a> (int DummyArg)</td>
        #     </tr>
        #     <tr class="separator:a2b298db4d6febba2e202fd0f3822f607">
        #         <td class="memSeparator" colspan="2">&#160;</td>
        #     </tr>
        # </table>

    elements = root.xpath(f"//h2[@class='memtitle' and contains(text(), '{FUNCS_HEADER_MARKER}')]")
    if elements:
        elements_to_delete.append(elements[0])
        # <h2 class="memtitle">
        #   <span class="permalink"><a href="#a515b33956a05bb5a042488cf4f361019">&#9670;&#160;</a></span> DummyConstructor()
        # </h2>



    xpath = f"div[@class='memitem'][descendant::em[contains(text(),'{FUNCS_HEADER_MARKER.split()[1]}')]]"   # Split to get rid of datatype
    elements = root.xpath(f"//{xpath}")
    if elements:
        elements_to_delete.append(elements[0].getprevious())
        elements_to_delete.append(elements[0])
        # Deletes:
        # <h2 class="memtitle"><span class="permalink"><a href="#a2b298db4d6febba2e202fd0f3822f607">&#9670;&#160;</a></span>Ship()</h2>
        # <div class="memitem">
        #     <div class="memproto">
        #         <table class="mlabels">
        #             <tr>
        #                 <td class="mlabels-left">
        #                     <table class="memname">
        #                         <tr>
        #                             <td class="memname">Ship::Ship </td>
        #                             <td>(</td>
        #                             <td class="paramtype">int</td>
        #                             <td class="paramname"><em>DummyArg</em></td>
        #                             <td>)</td>
        #                             <td></td>
        #                         </tr>
        #                     </table>
        #                 </td>
        #                 <td class="mlabels-right"> <span class="mlabels"><span class="mlabel">private</span></span> </td>
        #             </tr>
        #         </table>
        #     </div>
        #     <div class="memdoc">
        #         <dl class="section warning">
        #             <dt>Warning</dt>
        #             <dd>There is no Lua constructor for Ships; they cannot be created from a script. Sorry! <br />
        #                 You might
        #                 be able to do what you want by spawning a <a class="el" href="class_robot.html">Robot</a>. </dd>
        #         </dl>
        #     </div>
        # </div>

        # Retrieve our description, which can be found in the last element of our delete list
        elements = elements_to_delete[-1].xpath(f"./div[@class='memdoc']")
        if elements:
            new_content = etree.tostring(elements[0]).decode("utf-8")

            parent = root.xpath(f"//table[@class='memberdecls']")[0].getprevious()
            new_elements = [
                etree.fromstring(f'<div class="memfunc-banner">{new_content}</div>'),
            ]

            new_elements.reverse()
            for new_element in new_elements:
                parent.insert(1, new_element)


    # Delete any items we've marked for deletion
    for element in elements_to_delete:
        delete_node(element)

    # Delete "Constructor Documentation" header, if it's empty
    elements = root.xpath(f"//h2[@class='groupheader' and contains(text(), 'Member Function Documentation')]/preceding-sibling::*[1]/preceding-sibling::*[1][@class='groupheader' and contains(text(), 'Destructor Documentation')]")

    if elements:
        delete_node(elements[0])
        # Deletes:
        # <h2 class="groupheader">Constructor &amp; Destructor Documentation</h2>


def remove_space_after_method_name(root: Any) -> None:
    # Clear out annoying space just before ().  That is, setGeom (Geometry geom)  ==> setGeom(Geometry geom)
    elements = root.xpath(f"//table[@class='memberdecls']//td[@class='memItemRight']/a[@class='el']")
    for element in elements:
        if element.tail:
            if re.match(r" \(.*", element.tail):         # If it's an open paren followed by something...
                element.tail = element.tail.strip()      # ...strip leading space


def remove_destructor_text(root: Any) -> None:
    """ Lua doesn't really do destructors, so let's get rid of references to them. """
    elements = root.xpath("//h2[@class='groupheader']")
    for element in elements:
        if element.text == "Constructor & Destructor Documentation":
            element.text = "Constructor Documentation"

def remove_types_from_method_declarations_section(root: Any) -> None:
    # Remove types from declarations in upper section.  Several patterns to consider.
    # Pattern 1: Method(geom lineGeom, int speed)
    #   convert to: Method(lineGeom, speed)

    # <table class="memberdecls">
    #   ...
    #   <tr class="memitem:a1dce54ec5f53f8848202ccf0b117f1a2">
    #     <td class="memItemLeft" align="right" valign="top">void&nbsp;</td>
    #     <td class="memItemRight" valign="bottom">
    #       <a class="el" href="class_flag_item.html#a1dce54ec5f53f8848202ccf0b117f1a2">FlagItem</a>
    #    >>> replace the following line with (pos, teamIndex) <<<
    #       (<a class="el" href="classpoint.html">point</a> pos, int teamIndex)
    #     </td>
    #   </tr>


    elements = root.xpath(f"//table[@class='memberdecls']//td[@class='memItemRight']")

    for element in elements:
        # This beast of a statement strips out all the HTML tags, but also ensures there are sufficient spaces between tokens.
        # The following (commented out, nicer) method fails occasionally.
        # signature = etree.tostring(element, method="text", encoding="unicode")
        # Strips HTML tags to get FlagItem(point pos, int teamIndex)
        signature = etree.tostring(
            etree.fromstring(etree.tostring(element, encoding="unicode").replace("<", " <")),
            method="text",
            encoding="unicode",
        ).replace(" , ", ", ").replace(" ( ", "(").strip()   # Repair some of the damage

        match = re.match(r".*\((.*)\)", signature)   # matches --> point pos, int teamIndex
        if match:
            arglist = match.groups()[0].split()
            if arglist and len(arglist) % 2 == 0:    # Even number, meaning every param has a type
                children = element.getchildren()
                for child in children[1:]:           # Delete all but the first child, which will be the linked class name
                    delete_node(child)

                argstr = " ".join(arglist[1::2])        # Concatenate every second item; commas will already be included in the text we're parsing
                children[0].tail = "(" + argstr + ")"   # ['point', 'pos,', 'int', 'teamIndex'] ==> (pos, teamIndex)
            else:
                if arglist:
                    print(f"Warn: Missing types? | {signature} | {arglist}")
        else:
            # print(f"Warn: Couldn't find signature pattern | {signature}")
            pass

    # Pattern 2: Method(<linked type> param)
    # Linked types are in <a class="el"> elements, but we need to hang onto the first one, which is the member name itself
    elements = root.xpath(f"//table[@class='memberdecls']//td[@class='memItemRight']/a[@class='el' and position()>1]")
    for element in elements:
        text = element.tail.strip()
        element.getprevious().tail += text.replace(",", ", ")
        delete_node(element)


def parse_member_name(memname: str) -> Optional[Tuple[str, str, str, str]]:
    """
    Given a string like "int BFObject::doMath", return ["int", "BfObject", "doMath"].
    Some parts may be missing; if passed string does not contain "::", bail and return None.
    """
    if "::" not in memname:    # Not sure this check is necessary
        return None

    memname = memname.strip()
    if memname.startswith("static"):
        static = "static"
    else:
        static = ""

    memname = memname.replace("static", "").strip()

    xclass, fn = memname.split("::")

    if " " in xclass:
        ret_type, xclass = xclass.split(" ")
    else:
        ret_type = ""

    return static, ret_type.strip(), xclass.strip(), fn.strip()


def cleanup_member_details(root: Any, class_urls: Dict[str, str]) -> None:
    """
        Move class names to inherited tag on right side of header;
        this does not capture all of our classes, only the inherited ones.
    """
    inherited_tables = root.xpath("//table[contains(.,'inherited')]")     # Matches entire table below
    # <table class="mlabels">
    #   <tr>
    #     <td class="mlabels-left">
    #         <table class="memname">
    #           <tr>
    #             <td class="memname">BfObject::setGeom </td>                         <=== Move BfObject from here...
    #             <td>(</td>
    #             <td class="paramtype"><a class="el" href="class_geom.html">Geom</a></td>
    #             <td class="paramname"><em>geometry</em></td>
    #             <td>)</td>
    #             <td></td>
    #           </tr>
    #         </table>
    #     </td>
    #     <td class="mlabels-right">
    #         <span class="mlabels"><span class="mlabel">inherited</span></span>      <=== ...to here
    #     </td>
    #   </tr>
    # </table>

    for table in inherited_tables:
        memnames = table.xpath(".//td[@class='memname']/text()")
        if not memnames:
            continue
        assert len(memnames) == 1

        parts = parse_member_name(memnames[0])

        if not parts:
            continue

        static, ret_type, xclass, fn = parts

        # Modify "inherited from" tags off to the right
        inherited_elements = table.xpath(".//span[@class='mlabel' and text()='inherited']")
        if inherited_elements:
            assert len(inherited_elements) == 1

            html = f'inherited from {xclass}'
            if xclass in class_urls:        # Use the URL if we have one
                html = f'<a href="{class_urls[xclass]}">{html}</a>'
            inherited_from = etree.fromstring(html)

            inherited_elements[0].clear()
            inherited_elements[0].insert(1, inherited_from)

    #####
    # Spiff up method signatures (looking for inner tables found above, but this time even ones without adjacent inherited tags)
    memitems = root.xpath("//div[@class='memitem']")

    for memitem in memitems:
        tables = memitem.xpath(".//table[@class='memname']")
        if tables:
            table = tables[0]       # There will be only 1
            memtitle = memitem.getprevious()        # Member Title: Big text title on tab in member descriptions
            # <h2 class="memtitle"><span
            #     class="permalink"><a
            #     href="#a603c2eb87a4ed26c5b3fb06e953d611c">◆&nbsp;</a></span>Asteroid() <span
            #     class="overload">[1/2]</span>
            # </h2>

            memnames = table.xpath(".//td[@class='memname']")
            # <table class="memname">
            #   <tr>
            #     <td class="memname">BfObject::setGeom </td>                       <=== "BfObject::setGeom" to "setGeom"
            #     <td>(</td>
            #     <td class="paramtype"><a class="el" href="class_geom.html">Geom</a></td>
            #     <td class="paramname"><em>geometry</em></td>
            #     <td>)</td>
            #     <td></td>                                                         <=== Add "-> <return type>" to this cell
            #   </tr>
            # </table>

            if memnames:
                fn_decl = "".join(memnames[0].itertext()).strip()       # itertext gives us any text inside of tags

                parts = parse_member_name(fn_decl)
                if not parts:
                    continue

                # Note that by the time we get here, static will always be "" in this call because fn_decl is missing the static
                # keyword (it appears to get stripped off by Doxygen).  So we'll determine if the function is static by using the
                # small static banner over on the right, in an mlabel span.
                static, ret_type, xclass, fn_name = parts     # ret_type xclass::fn_name()

                mlabels = memitem.xpath(".//span[@class='mlabel']")

                if mlabels and mlabels[0].text.strip() == "static":
                    static = "static"



                is_constructor = xclass == fn_name
                if is_constructor:
                    ret_type = xclass        # Constructors return an instance of the class, even if it's not written that way in C++
                    fn_name += ".new"        # This is how you call constructors in Lua
            else:
                assert False
                ret_type, fn_name = "", table.xpath(".//td/text()")[0]
                is_constructor = False

            # If this is a static class, alter the title to show the class to make usage clearer
            if static:
                fn_name = f"{xclass}.{fn_name}"

            cells = table.xpath(".//td")
            cells[0].clear()
            cells[0].text = fn_name       # Replaces "num MoveObject::getAngle" with "getAngle"
            # cells[-1].text = f"-> {ret_type if ret_type else 'nil'}"      # Add return type to end of row

            param_types = table.xpath(".//td[@class='paramtype']")
            param_names = table.xpath(".//td[@class='paramname']")

            # These lists will be imbalanced if there are no args... for some reason doxygen inserts an empty
            # <td class="paramname"></td> tag when there are no args, but does not also include a corresponding
            # <td class="paramtype"></td>.
            argstrs = []
            param_list = []

            def linkify(param_type):
                if param_type in class_urls:
                    return f'<a href="{class_urls[param_type]}" class="el">{param_type}</a>'
                else:
                    return param_type


            if len(param_types) == len(param_names):        # Balanced, so has args
                for i in range(len(param_types)):
                    # Use itertext() to burrow into any inner tags and grab all interior text
                    param_name = "".join(param_names[i].itertext()).replace(",", "").strip()
                    param_type = "".join(param_types[i].itertext()).replace(",", "").strip()

                    # Handle special case: when a constructor (or other fn) has the typeless "geom" as an argument, add the type Geom here.
                    # It would be better to fix the code, but there's a lot of places where this is happening.  If that's the case,
                    # we don't want to delete the param_types node from the DOM, as that is where our "geom" token is, and we want that.
                    if param_name == "" and param_type == "geom":
                        param_name = "geom"
                        param_type = "Geom"
                    else:
                        delete_node(param_types[i])     # Won't be needing this: param_types will be displayed in the line below

                    param_list.append(param_name)
                    param_type = handle_mixed(param_type)

                    argstrs.append(f'<span class="paramname">{param_name}</span>: <span class="paramtype">{linkify(param_type)}</span>')

            # Now back up the tab and the big member name
            method = memtitle.xpath("./span[@class='permalink']")[0]
            method.tail = f"{fn_name}({', '.join(param_list)})"


            if not ret_type or ret_type == "void":
                ret_type = "nothing"
            else:
                ret_type = f'<span class="returntype">{linkify(ret_type)}</span>'

            ret_type = f"returns {ret_type}"

            ret_type = handle_mixed(ret_type)       # Decode mixed_xxx_yyy types used for multiple return types

            # The argline styles are defined in luadocs_html_extra_stylesheet.css
            if argstrs:     # Args and return type
                arg_type = "Arg types: "
                argline = f"{', '.join(argstrs)}{NBSP * 2}|{NBSP * 2}{ret_type}"
            else:           # No args, only return type
                arg_type = ""
                argline = ret_type

            new_row = etree.fromstring(f"""
                <tr class="nofloat argline">
                    <td colspan="{len(cells)}">
                        <span class="argtypes">{arg_type}</span>
                        {argline}
                    </td>
                </tr>
            """)
            table.clear()           # Remove existing rows, which are now a repeat of the data shown in the tab
            table.append(new_row)   # And insert the type information in a new row


def cleanup_constant_defs(root: Any):
    fullnames = root.xpath("//p[text()='__FULLNAME: ']")        # Marker for the correct name for the item in question

    # <h2 class="memtitle">
    #   <span class="permalink">
    #     <a href="#a6ae0a4bf7042d0f70123a44dbabd49fd">◆&nbsp;</a>
    #   </span>one()                         <=== replace one() with point.one (which is extracted below); var memtitle will point at this
    # </h2>
    # <div class="memitem">
    #   <div class="memproto">
    #     <table>
    #       <tbody>
    #         <tr class="nofloat argline">
    #           <td colspan="1">
    #             <span class="argtypes"></span> returns <span class="returntype"><a href="classpoint.html" class="el">point</a></span>
    #           </td>
    #         </tr>
    #       </tbody>
    #     </table>
    #   </div>
    #   <div class="memdoc">

    #     <p>Constant representing the point (1, 1). </p>
    #     <p>__FULLNAME: <a class="el" href="classpoint.html#a6ae0a4bf7042d0f70123a44dbabd49fd"
    #         title="Constant representing the point (1, 1).">point.one</a></p>
    #     <p>Using this constant is marginally more efficient than defining it yourself. </p>

    #   </div>
    # </div>
    #   <p>Constant representing the point (1, 1). </p>
    #   <p>__FULLNAME: <a class="el" href="classpoint.html#a6ae0a4bf7042d0f70123a44dbabd49fd"           <=== fullnames finds these elements
    #           title="Constant representing the point (1, 1).">point.one</a></p>                       <=== point.one is the name we want
    #   <p>Using this constant is marginally more efficient than defining it yourself. </p>

    #   </div>
    # </div>

    for item in fullnames:
        fullname = item.xpath("./a")[0].text

        memtitle = root.xpath(f"//div[@class='memitem'][descendant::p[text()='__FULLNAME: ']/a[text()='{fullname}']]/preceding-sibling::*[1]/span")[0]
        memtitle.tail = fullname

        delete_node(item)


def handle_mixed(argtype: str) -> str:
    """
    Because Lua functions can return multiple types, and C++ fuctions cannot, we'll use a fake datatype called mixed
    to specify multiple types.  In the docs, we specify it as "mixed_xxx_yyy", which should survive the processing steps,
    and here we'll translate to "xxx, yyy".
    """
    if "mixed_" in argtype:
        argtype = argtype.replace("mixed_", " ", 1)
        argtype = argtype.replace("_", ", ")
    return argtype


def remove_first_two_more_links(root: Any):
    elements = root.xpath("//a[text()='More...']")
    for element in elements[:2]:
        element.getparent().remove(element)


# https://stackoverflow.com/questions/65506059/how-can-i-get-the-text-from-this-html-snippet-using-lxml
def replace_text(root: Any, search_str: str, replace_str: str):
    context = etree.iterwalk(root, events=("start", "end"))
    for action, elem in context:
        if elem.text and search_str in elem.text:
            elem.text = elem.text.replace(search_str, replace_str)
        elif elem.tail and search_str in elem.tail:
            elem.tail = elem.tail.replace(search_str, replace_str)


def get_class_urls(root: Any, filename: str) -> Dict[str, str]:        # Dict[class, url]
    """
    Build a list of URLs for each class.  Returns a dict where dict[class] = url.
    """
    class_urls = {}

    # Look for a title in the current doc and add that
    title = root.xpath("//div[@class='title']")[0].text
    match = re.match("(.*) Class Reference", title)
    if match:
        class_urls[match.groups()[0]] = os.path.basename(filename)
    elif " " not in title:      # One word... maybe it's a cass!
        class_urls[title] = os.path.basename(filename)

    return class_urls


def cleanup_html(line: str) -> str:
    """
    These things seem to anger lxml for some reason... let's make it happy.
    These commands are run on the raw file, before lxml starts parsing.
    """
    # Replace these with their equivalents
    line = line.replace("&nbsp;", NBSP)
    line = line.replace("&ndash;", "–")

    # Remove the offending &nbsp that makes some )s appear in the wrong place
    line = re.sub(r'(<td class="paramtype">.+)&#160;(</td>)', r"\1\2", line)        # &#160; is a nobsp char

    line = re.sub(r"/\* @license.*?\*/", "", line)       # These comments cause problems for some reason

    return line


def update_progress(progress: float, msg: str = ""):
    """ Progress is float between 0 and 1; 1 signifies completion, negative signifies abort. """
    barLength = 48 # Modify this to change the length of the progress bar
    status = ""

    if isinstance(progress, int):
        progress = float(progress)
    if not isinstance(progress, float):
        progress = 0
        status = "error: progress var must be float\r\n"
    if progress < 0:
        progress = 0
        status = "Halt.\033[K\n"
    if progress >= 1:
        progress = 1
        status = "Done.\033[K\n"

    blocklen = int(round(barLength * progress))
    text = f"\rProgress: [{'■' * blocklen}{' ' * (barLength - blocklen)}] {progress * 100:.1f}% {status}\033[K"

    if msg:
        text += f" ({msg})"

    print(text, end="", flush=True)



if __name__ == "__main__":
    main()