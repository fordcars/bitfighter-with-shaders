# This script preprocesses our C++ code and generates fodder for Doxygen which makes pretty documentation
# for our Lua scripters out there

use strict;            # Require vars to be declared!
use File::Basename;
use List::Util 'first';
use IO::Handle;

# Need to be in the doc directory
chdir "doc" || die "Could not change to doc folder: $!";

# Relative path where intermediate outputs will be written
my $outpath = "temp-doxygen/";

# Create it if it doesn't exist
if (! -d $outpath) {
   mkdir($outpath) || die "Could not create folder $outpath: $!";
}

# These are items we collect and build up over all pages, and they will get written to a special .h file at the end
my @mainpage = ();
my @enums = ();

# Collect some file names to process
my @files = ();

push(@files, <../zap/*.cpp>);                # Core .cpp files
push(@files, <../zap/*.h>);                  # Core .h files
push(@files, <../lua/lua-vec/src/*.c>);      # Lua-vec .c files
push(@files, <../resource/scripts/*.lua>);   # Some Lua scripts
push(@files, <./static/*.txt>);              # our static pages for general information and task-specific examples

# Loop through all the files we found above...
foreach my $file (@files) {
   print($file);
   print("\n");

   open my $IN, "<", $file || die "Could not open $file for reading: $!";

   my $writeFile = 0;

   # Various modes we could be in
   my $collectingMethods = 0;
   my $collectingStaticMethods = 0;
   # my $collectingLongDescr = 0;
   my $collectingLongComment = 0;
   my $collectingMainPage = 0;
   my $collectingEnum = 0;
   my $descrColumn = 0;
   my $encounteredDoxygenCmd = 0;      # Gets set to 1 the first time we encounter a @cmd in a file

   my $luafile = $file =~ m|\.lua$|;   # Are we processing a .lua file?

   my $enumColumn;
   my $enumIgnoreColumn;
   my $enumName;
   my $enumDescr;

   my @methods = ();
   my @staticMethods = ();
   my @globalfunctions = ();
   my @comments = ();
   my %classes = ();        # Will be a hash of arrays

   my $shortClassDescr = "";
   my $longClassDescr  = "";

   my %alreadyWroteHeader = ();

   # Visit each line of the source cpp file
   foreach my $line (<$IN>) {

      # If we are processing a .lua file, we want to remap @luaclass to @luavclass for simplicity
      if($luafile) {
         $line =~ s|\@luaclass\s|\@luavclass |;
      }

      # Lua base classes
      if( $line =~ m|REGISTER_LUA_CLASS *\( *(.+?) *\)| ) {
         my $class = $1;
         unshift(@{$classes{$class}}, "$shortClassDescr\n$longClassDescr\nclass $class { \n public:\n");   # unshift adds element to beginning of array
         $writeFile = 1;
         next;
      }

      # Lua subclasses
      if( $line =~ m|REGISTER_LUA_SUBCLASS *\( *(.+?) *, *(.+?) *\)| ) {
         my $class = $1;
         my $parent = $2;
         unshift(@{$classes{$class}}, "$shortClassDescr\n$longClassDescr\nclass $class : public $parent { \n public:\n");
         $writeFile = 1;
         $shortClassDescr = "";
         $longClassDescr = "";
         next;
      }

      if( $line =~ m|define +LUA_METHODS *\(CLASS, *METHOD\)| ) {
         $collectingMethods = 1;
         next;
      }

      if( $collectingMethods ) {
         if( $line =~ m|METHOD *\( *CLASS, *(.+?) *,| ) {                 # Signals class declaration... methods will follow
            my $method = $1;
            push(@methods, $method);
            next;
         }

         if( $line =~ m|GENERATE_LUA_METHODS_TABLE *\( *(.+?) *,| ) {     # Signals we have all methods for this class, gives us class name; now generate code
            my $class = $1;

            foreach my $method (@methods) {
               push(@{$classes{$class}}, "void $method() { }\n");
            }

            @methods = ();
            $collectingMethods = 0;
            next;
         }
      }

      if( $line =~ m|define +LUA_STATIC_METHODS *\( *METHOD *\)| ) {
         $collectingStaticMethods = 1;
         next;
      }

      if( $collectingStaticMethods ) {
         if( $line =~ m|METHOD *\( *(.+?) *,| ) {                 # Signals class declaration... methods will follow
            my $method = $1;
            push(@staticMethods, $method);
            next;
         }

         if( $line =~ m|GENERATE_LUA_STATIC_METHODS_TABLE *\( *(.+?) *,| ) {     # Signals we have all methods for this class, gives us class name; now generate code
            my $class = $1;

            unshift(@{$classes{$class}}, "$shortClassDescr\n$longClassDescr\nclass $class { \n public:\n");   # unshift adds element to beginning of array
            $writeFile = 1;

            foreach my $method (@staticMethods) {
               my $index = first { ${$classes{$class}}[$_] =~ m|\s$method\(| } 0..$#{$classes{$class}};

               # Ignore methods that already have explicit documentation
               if($index eq "") {
                  push(@{$classes{$class}}, "static void $method() { }\n");
               }
            }

            @staticMethods = ();
            $collectingStaticMethods = 0;
            next;
         }
      }

      # /** signals the beginning of a long comment block we need to pay attention to
      # In Lua files we can also use --[[
      if( !$luafile && $line =~ m|/\*\*| || $luafile && $line =~ m|--\[\[| ) {
         $collectingLongComment = 1;
         push(@comments, "/*!\n");
         next;
      }

      if( $collectingLongComment ) {

         # Look for closing */ or --]] to terminate our long comment
         if( !$luafile && $line =~ m|\*/| || $luafile && $line =~ m|--\]\]| ) {
            push(@comments, "*/\n");
            $collectingLongComment = 0;
            $collectingMainPage    = 0;
            $encounteredDoxygenCmd = 0;
            next;
         }

         # $line =~ s|^\ *\* *||;  # Strip off leading *s and spaces

         if( $line =~ m|\@mainpage\s| or $line =~ m|\@page\s|) {
            push(@mainpage, $line);
            $collectingMainPage    = 1;
            $encounteredDoxygenCmd = 1;
            next;
         }

         # Check for some special custom tags...

         # Check the regexes here: http://www.regexe.com
         # Handle Lua enum defs: "@luaenum ObjType(2)" or "@luaenum ObjType(2,1,4)"  1, 2, or 3 nums are ok
         #                            $1        $2           $4           $6
         if( $line =~ m|\@luaenum\s+(\w+)\s*\((\d+)\s*(,\s*(\d+)\s*(,\s*(\d+))?)?\s*\)| ) {
            $collectingEnum = 1;
            $enumName = $1;
            $enumColumn = $2;
            $descrColumn      = $4 eq "" ? -1 : $4;      # Optional
            $enumIgnoreColumn = $6 eq "" ? -1 : $6;      # Optional
            $encounteredDoxygenCmd = 1;

            push(@enums, "/**\n  * \@defgroup $enumName"."Enum $enumName\n");

            next;
         }

         # Look for @geom, and replace with @par Geometry \n
         if( $line =~ m|\@geom\s+(.*)$| ) {
             print($line);
             my $body = $1;
             print($body);
             push(@comments,"\\par Geometry\n$body");
             next;
         }

         # Look for:  * @luafunc  retval BfObject::getClassID(p1, p2); retval and p1/p2 are optional
         if( $line =~ m|\@luafunc\s+(.*)$| ) {
            # In C++ code, we use "::" to separate classes from functions (class::func); in Lua, we use "." (class.func).
            my $sep = ($file =~ m|\.lua$|) ? "[.:]" : "::";

            #                          $1          $2          $3         $4     $5     (warning: $1 grabs extra spaces, trimmed below)
            $line =~ m|.*?\@luafunc\s+(static\s+)?(\w+\s+)?(?:(\w+)$sep)?(.+?)\((.*)\)|;    # Grab retval, class, method, and args from $line

            my $staticness = $1;
            my $retval = $2 eq "" ? "void" : $2;                              # Retval is optional, use void if omitted
            my $voidlessRetval = $2;
            my $class  = $3; # If no class is given the function is assumed to be global
            my $method = $4 || die "Couldn't get method name from $line\n";  # Must have a method
            my $args   = $5;                                                  # Args are optional

            $retval =~ s|\s+$||;     # Trim any trailing spaces from $retval

            # Use voidlessRetval to avoid having "void" show up where we'd rather omit the return type altogether
            my $prefix = $class || "global";
            push(@comments, " \\fn $voidlessRetval $prefix" . "::" . "$method($args)\n");

            # Here we generate some boilerplate standard code and such
            if($class eq $method) {
               my $lcClass = lc $class;
               push(@comments, "\\brief Constructor.\n\nExample:\@code\n$lcClass = $class.new($args)\n...\nlevelgen:addItem($lcClass)\@endcode\n\n");
            }


            # Find the original class definition and delete it (if it still exists); but not if it's a constructor.
            # We do this in order to provide more complete method descriptions if they are found subsequently.  I think.
            # We can detect constructors because they come in the form of $class::$method where class and method are the same.
            if($class ne $method) {
               my $index = first { ${$classes{$class}}[$_] =~ m|(static\s+)?void $method\(\)| } 0..$#{$classes{$class}};
               if($index ne "") {
                  splice(@{$classes{$class}}, $index, 1);       # Delete element at $index
               }
            }

            chomp($line);     # Remove trailing \n

            # Add our new sig to the list
            if($class) {
               $line =~ s/^\s+//;
               push(@{$classes{$class}}, "$staticness $retval $method($args) { /* From '$line' */ }\n");
            } else {
               push(@globalfunctions, "$retval $method($args) { /* From '$line' */ }\n");
            }

            $writeFile = 1;

            $encounteredDoxygenCmd = 1;

            next;
         }

         if( $line =~ m|\@luaclass\s+(\w+)\s*$| ) {       # Description of a class defined in a header file
            push(@comments, " \\class $1\n");

            $encounteredDoxygenCmd = 1;

            next;
         }

         if( $line =~ m|\@luavclass\s+(\w+)\s*$| ) {      # Description of a virtual class, not defined in any C++ code
            my $class = $1;
            push(@comments, " \\class $class\n");

            push(@{$classes{$class}}, "class $1 {\n");
            push(@{$classes{$class}}, "public:\n");

            $writeFile = 1;
            $encounteredDoxygenCmd = 1;

            next;
         }

         if( $line =~ m|\@descr\s+(.*)$| ) {
            push(@comments, "\n $1\n");
            $encounteredDoxygenCmd = 1;
            next;
         }


         # Otherwise keep the line unaltered and put it in the appropriate array
         if($collectingMainPage) {
            push(@mainpage, $line);
         } elsif($collectingEnum) {
            push(@enums, $line);
         } else {
            push(@comments, $line) if $encounteredDoxygenCmd;
         }

         next;
      }

      # Starting with an enum def that looks like this:
      # /**
      #  * @luaenum Weapon(2[,n])  <=== 2 refers to 0-based index of column containing Lua enum name, n refers to column specifying whether to include this item
      #  * The Weapon enum can be used to represent a weapon in some functions.
      #  */
      #  #define WEAPON_ITEM_TABLE \
      #    WEAPON_ITEM(WeaponPhaser,     "Phaser",      "Phaser",     100,   500,   500,  600, 1000, 0.21f,  0,       false, ProjectilePhaser ) \
      #    WEAPON_ITEM(WeaponBounce,     "Bouncer",     "Bouncer",    100,  1800,  1800,  540, 1500, 0.15f,  0.5f,    false, ProjectileBounce ) \
      #
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

      if($collectingEnum) {
         # If we get here we presume the @luaenum comment has been closed, and the next #define we see will begin the enum itself
         # Enum will continue until we hit a line with no trailing \
         if( $line =~ m|#\s*define| ) {
            push(@enums, "\@\{\n");
            push(@enums, "# $enumName\n");# Add the list header
            $collectingEnum = 2;
            next;
         }

         next if $collectingEnum == 1;    # Skip lines until we hit a #define

         # If we're here, collectingEnum == 2, and we're processing an enum definition line
         $line =~ s|/\*.*?\*/||g;         # Remove embedded comments
         next if($line =~ m|^\W*\\$|);    # Skip any lines that have no \w chars, as long as they end in a backslash


         # Skip blank lines, or those that look like they are starting with a comment
         unless( $line =~ m|^\s*$| or $line =~ m|^\s*//| or $line =~ m|\s*/\*| ) {
            $line =~ m/[^(]+\((.+)\)/;
            my $string = $1;
            my @words = $string =~ m/("[^"]+"|[^,]+)(?:,\s*)?/g;

            # Skip items marked as not to be shared with Lua... see #define TYPE_NUMBER_TABLE for example
            next if($enumIgnoreColumn != -1 && $words[$enumIgnoreColumn] eq "false");

            $enumDescr = $descrColumn != -1 ? $words[$descrColumn] : "";

            # Clean up descr -- remove leading and traling non-word characters... i.e. junk
            $enumDescr =~ s|^\W*"||;         # Remove leading junk
            $enumDescr =~ s|"\W*$||;         # Remove trailing junk


            # Suppress any words that might trigger linking
            $enumDescr =~ s|\s(\w+)| %\1|g;


            my $enumval = $words[$enumColumn];
            $enumval =~ s|[\s"\)\\]*||g;         # Strip out quotes and whitespace and other junk

            # next unless $enumval;      # Skip empty enumvals... should never happen, but does

            push(@enums, " * * \%" . $enumName . ".\%" . $enumval . " <br>\n `" . $enumDescr."` <br>\n");    # Produces:  * * %Weapon.Triple  Triple
            # no next here, always want to do the termination check below
         }


         if( $line !~ m|\\\r*$| ) {          # Line has no terminating \, it's the last of its kind!
            push(@enums, "\@\}\n");       # Close doxygen group block
            push(@enums, "*/\n\n");       # Close comment

            $collectingEnum = 0;          # This enum is complete!
         }

         next;
      }
   }

   # If we added any lines to keepers, write it out... otherwise skip it!
   if($writeFile)
   {
      my ($name, $path, $suffix) = fileparse($file, (".cpp", ".h", ".lua"));

      # Write the simulated .h file
      $suffix =~ s|\.||;     # Strip the "." that fileparse sticks on the font of $suffix
      my $outfile = $outpath . $name . "__" . $suffix . ".h";
      open my $OUT, '>', $outfile || die "Can't open $outfile for writing: $!";

      print $OUT "// This file was generated automatically from the C++ source to feed doxygen.  It will be overwritten.\n\n\n";

      foreach my $key ( keys %classes ) {
         if(@{$classes{$key}}) {
            print $OUT @{$classes{$key}};    # Main body of class
            print $OUT "}; // $key\n";       # Close the class
         }
      }

      print $OUT "\n\n// What follows is a dump of the \@globalfunctions varaible\n\n";

      print $OUT "namespace global {\n";
      foreach ( @globalfunctions ) {
         # print each global function declaration
         print $OUT $_;
      }
      print $OUT "}\n";

      print $OUT "\n\n// What follows is a dump of the \@comments varaible\n\n";
      print $OUT @comments;

      close $OUT;
   }
   close $IN;
}


# Finally, write our main page data
my $outfile = $outpath . "main_page_content.h";
open my $OUT, '>', $outfile || die "Can't open $outfile for writing: $!";

print $OUT "// This file was generated automatically from the C++ source to feed doxygen.  It will be overwritten.\n\n\n";

print $OUT "/**\n";
print $OUT @mainpage;
print $OUT "*/\n";

print $OUT @enums;

close $OUT;

# Finally, launch doxygen
chdir("doc");
system("doxygen luadocs.doxygen");


# Post-process the generated doxygen stuff
# Let's try this with Tie::File... it looks pretty neat!
# Chcek the docs at http://search.cpan.org/~toddr/Tie-File-0.98/lib/Tie/File.pm

print "Fixing doxygen output...\n";
@files = ();

push(@files, <./html/class_*.html>);    # Files containing the offending character

# Loop through all the files we found above...
foreach my $file (@files) {
   my @outlines;
   STDOUT->printflush(".");
   open my $IN, "<", $file || die "Could not open $file for reading: $!";

   # Remove the offending &nbsp that makes some )s appear in the wrong place
   foreach my $line (<$IN>) {
      $line =~ s|(<td class="paramtype">.+)&#160;(</td>)|\1\2|;
      push(@outlines, $line);
   }

   close $IN;

   open my $OUT, '>', $file || die "Can't open $file for writing: $!";
   foreach my $line (@outlines) {
      print $OUT $line;
   }

   close $OUT;
}

print "\n";