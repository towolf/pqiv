PQIV README
===========

About pqiv
----------

Originally, PQIV was written as a drop-in replacement for QIV
(http://spiegl.de/qiv/), as QIV was unmaintained and used imlib, a deprecated
library, which was to be removed from Gentoo Linux. The first release was not
more than a Python script, hence the name. After some month I realized that
nobody would do a better version, so I did it myself.  Now, PQIV is a (modulo
some small extras) full featured clone of QIV written in C using GTK-2 and
GLIB-2.

When Debian decided to do the same step regarding imlib, a new developer stepped
in for QIV's klografx.net team and updated QIV to use GDK-2 and imlib-2. As of
May 2009, both programs are useable again and their features are likely to
diverge.


Features
--------

Originating from qiv:
 * moving & zooming image in fullscreen mode.
 * fullscreen viewing with a great statusbar
 * external "qiv-command" program support
 * real transparency
 * maxpect (zoom to screen size while preserving aspect ratio)
 * scale_down (scale down to big images to fit screen size)
 * slideshow (with random order if you want)
 * flip horizontal/vertical, rotate left/right
 * jump to image number x
Unique features:
 * fade between images
 * better transparency handling
 * ability to place the window at arbitrary positions on the desktop and remove
   the title bar
 * define custom keyboard aliases
 * configuration file support
 * external commands support pipelining images
 * search in argument list
 * animation support


Installation
------------

Usual stuff. `./configure && make && make install`
You'll need
 * gtk+-2.0 >= 2.12
 * Corresponding glib, gthread, gpixbuf versions


Thanks
------

This program uses Martin Pool's natsort algorithm <http://sourcefrog.net/projects/natsort/>


Contributors
------------

 * Nir Tzachar
 * Yaakov (Cygwin Ports)
 * David Lindquist
 * Tinoucas
 * John Keeping
 * Rene Saarsoo
 * Alexandros Diamantidis (Reverse-movement-direction code, fixed code typos)
 * Alexander Sulfrian
 * Brandon


Known problems
--------------

 * If you experience problems with file encodings, try setting the environment
   variable `G_FILENAME_ENCODING` to match your file system's encoding
 * It seems that GTK buffers images in memory even after they are freed. If you
   reload images (press 'r') often, the memory consumed by the procress will
   increase. However, it doesn't do so linearly and reaches a limit, so it's
   probably not an issue in pqiv's code. IF you can assert any linear increase
   report that as a bug!


Coding-style
------------

I personally dislike the typical C coding style. Which means I won't ever use
it in pqiv code. Here's what I do:
 * Tabs (width 8) instead of spaces
 * if(..) {\n\t..\n}
   same goes for while/do/..
 * }\nelse {
 * function(\n\tparameters,\n\t..\n)
 * I indent preprocessor stuff whereever I think it might improve readability
 * Variable names are camelCase


Changelog
---------

pqiv 0.12
 * Included patch to correctly fullscreen on Xinerama dual screen setups
   (Thanks to by Alexander Sulfrian)
 * glib 2.30 compatibility fixes (remove gconvert.h include, see Gentoo bug #415325)
 * Keypad support for movement (See Debian bug #671401)

pqiv 0.11
 * Updated configure-script (for enhanced non-linux compatibility)
 * Solved bug in process spawning (Reported by Martin Vaeth)
 * Support for animated images
 * Changed default binary name to pqiv

pqiv 0.10
 * Improved window initialisation code to support systems without a window manager
 * Rewrote code that toggles fullscreen to improve compatibility and speed
   things up
 * Rewrote file sorting, supports shuffle mode now as well
 * Improved handling of ~/.pqivrc
 * -&lt;n&gt; supports inserting the filename at arbitrary positions
 * Replaced most algorithms with corresponding GLib functions

pqiv 0.9
 * Improved reliability in resize/reshape code. Switching to/from fullscreen with
   Beryl should no longer raise problems
 * Improved image translation code in fullscreen (Moving the image with the mouse)
 * Made -R default, as many people complained about the movement direction. Use
   -R to get pre-0.9 behaviour.
 * Added a configure option to name the binary "pqiv" and remove "qiv" stuff
   (to simplify life for Debian maintainers :-)

pqiv 0.8
 * Added autorotation for digicam images
 * Added `off' as an parameter for -P
 * Added -R (reverse movement direction)
 
pqiv 0.7
 * Improved debian compatibility (ie. make deb)
 * Added option -S ("Follow symlinks") and recursion checking code
 * Added "open file" dialog (start qiv without a TTY in stdin &
   without parameters)
 * Fixed various bugs in sorting/directory traversal
 * Added 'j' binding to jump to a file (search by name / number)
 
pqiv 0.6
 * Added -q option
 * Added --no-inotify option to make pqiv compatible with
   !linux systems. (Yaakov)
 * Added -a for aliasing (Idea by David Lindquist)
 * -t scales images up to fill the screen (optional)
 * zoom in fullscreen via mouse 3 & scroll (not realtime)
 * Mousebuttons behave more qiv-like. (Tinoucas)

pqiv 0.5
 * Minor bugfix: Show an error message when no X11 display
   is found
 * Fading between images
 * Advanced command execution
 * More glib calls instead of own functions
 * Added option to select interpolation type
 * Added support for real transparent images
 * Added support for sorting (using natsort)
 * Added support for real transparency
 * Added support for fading between images

pqiv 0.4
 * Updated Makefile to conform to standards
   Thanks to Samuli Suominen <drac at gentoo.org>
 
pqiv 0.4 rc1
 * Source rewritten into c
 * Mouse moves image in fullscreen
 * Configuration file
 * Many bugfixes (i.e. fullscreen on startup)
 * Ability to execute external commands

pqiv &lt;0.4:
 See the old python release for information on that
