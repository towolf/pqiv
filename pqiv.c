/**
 * vim:ft=c:fileencoding=utf-8:fdm=marker:tabstop=8
 *
 * pqiv - pretty quick image viewer
 * Copyright (c) Phillip Berndt, 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License ONLY.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 */
#define RELEASE "0.9"

/* Includes {{{ */
#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gconvert.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <regex.h> 
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/mman.h>
#ifndef NO_SORTING
#include "lib/strnatcmp.h"
#endif
#ifndef NO_INOTIFY
#include <sys/inotify.h>
#endif
/* }}} */
/* Definitions {{{ */
/* Compile time settings */
#define DRAG_MAX_TIME 150
#define SECONDS_TILL_LOADING_INFO 5 /* Undef to disable */
#define KENBURNS_SPEED_ADJUST 40 /* Kenburns effect calls in ms */

/* libc stuff */
extern char *optarg;
extern int optind, opterr, optopt;
extern char **environ;

/* GTK stuff */
static GtkWidget *window = NULL;
static GtkWidget *imageWidget = NULL;
static GtkWidget *fixed;
static GtkWidget *infoLabel;
static GtkWidget *infoLabelBox;
static GtkWidget *mouseEventBox;
static GdkPixbuf *currentImage = NULL;
static GdkPixbuf *scaledImage = NULL;
static GdkPixbuf *memoryArgImage = NULL;
static char emptyCursor[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* Inline images: The chess board like one for transparent images and
 * the application's icon */
static char *chessBoard = 
		"GdkP"
		"\0\0\0\263" "\2\1\0\2" "\0\0\0@" "\0\0\0\20" "\0\0\0\20"
		"\210jjj\377\210\233\233\233\377\210jjj\377\210\233\233\233\377\210jj"
		"j\377\210\233\233\233\377\210jjj\377\210\233\233\233\377\210jjj\377\210"
		"\233\233\233\377\210jjj\377\210\233\233\233\377\210jjj\377\210\233\233"
		"\233\377\210jjj\377\220\233\233\233\377\210jjj\377\210\233\233\233\377"
		"\210jjj\377\210\233\233\233\377\210jjj\377\210\233\233\233\377\210jj"
		"j\377\210\233\233\233\377\210jjj\377\210\233\233\233\377\210jjj\377\210"
		"\233\233\233\377\210jjj\377\210\233\233\233\377\210jjj\377";
static char *appIcon =
		"GdkP"
		"\0\0\1\15" "\2\1\0\2" "\0\0\0@" "\0\0\0\20" "\0\0\0\20"
		"\244\0\0\0\0\210\377\226\0\377\207\0\0\0\0\212\377\226\0\377\206\0\0"
		"\0\0\212\377\226\0\377\205\0\0\0\0\205\377\226\0\377\204\0\0\0\0\203"
		"\377\226\0\377\204\0\0\0\0\204\377\226\0\377\206\0\0\0\0\202\377\226"
		"\0\377\204\0\0\0\0\203\377\226\0\377\207\0\0\0\0\202\377\226\0\377\204"
		"\0\0\0\0\203\377\226\0\377\202\0\0\0\0\202\377\226\0\377\204\0\0\0\0"
		"\1\377\226\0\377\204\0\0\0\0\203\377\226\0\377\202\0\0\0\0\203\377\226"
		"\0\377\202\0\0\0\0\202\377\226\0\377\205\0\0\0\0\202\377\226\0\377\203"
		"\0\0\0\0\203\377\226\0\377\1\0\0\0\0\202\377\226\0\377\206\0\0\0\0\202"
		"\377\226\0\377\203\0\0\0\0\204\377\226\0\377\207\0\0\0\0\211\377\226"
		"\0\377\211\0\0\0\0\202\377\226\0\377\203\0\0\0\0\203\377\226\0\377\216"
		"\0\0\0\0\202\377\226\0\377\222\0\0\0\0";

/* Structure for file list building */
static struct file {
	char *fileName;
	int nr;
	struct file *next;
	struct file *prev;
} firstFile;
static struct file *currentFile = &firstFile;
static struct file *lastFile = &firstFile;
GtkFileFilter *fileFormatsFilter;
GTree* recursionCheckTree = NULL;

static GTimeVal  programStart;
#ifdef SECONDS_TILL_LOADING_INFO
static int       loadFilesChecked = 0;
#endif

/* Program settings */
static gboolean isFullscreen = FALSE;
static char infoBoxVisible = 0;
static float scaledAt;
static float zoom;
static enum autoScaleSetting { OFF = 0, ON, ALWAYS } autoScale = ON;
static int moveX, moveY;
static int slideshowInterval = 3;
static gboolean slideshowEnabled = 0;
static int slideshowID = 0;
static char aliases[128];
#ifndef NO_INOTIFY
static int inotifyFd = 0;
static int inotifyWd = -1;
#endif

/* Program options */
static gboolean optionHideInfoBox = FALSE;
#ifndef NO_INOTIFY
static gboolean optionUseInotify = FALSE;
#endif
static gboolean optionFollowSymlinks = FALSE;
static float optionInitialZoom = 1;
static int optionWindowPosition[3] = {-1, -1, -1};
static char optionHideChessboardLevel = 0;
static char optionReverseMovement = TRUE;
#ifndef NO_COMMANDS
static char *optionCommands[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
#endif
static GdkInterpType optionInterpolation = GDK_INTERP_BILINEAR;

#ifndef NO_FADING
/* Stuff for fading between images */
static gboolean optionFadeImages = FALSE;
struct fadeInfo {
	GdkPixbuf *pixbuf;
	int alpha;
};
#endif

#ifndef NO_KENBURNS
/* Kenburns effect */
static enum { NO = 0, YES, WITH_SCALING } optionUseKenburns;
struct {
	int id;
	int status;
	float addX, addY;
	float currX, currY;

	float sizeX, sizeY;
	float addSizeX, addSizeY;
} kenburnsData;
#endif

/* Function prototypes */
gboolean reloadImage();
void autoScaleFactor();
void resizeAndPosWindow();
void displayImage();
/* }}} */
/* Error, debug and info message stuff {{{ */
/* Debugging {{{ */
#ifdef DEBUG
	#define DEBUG1(text) g_printerr("(%04d) %-20s %s\n", __LINE__, G_STRFUNC, text);
	#define DEBUG2(text, param) g_printerr("(%04d) %-20s %s %s\n", __LINE__, G_STRFUNC, text, param);
	#define DEBUG2d(text, param) g_printerr("(%04d) %-20s %s %d\n", __LINE__, G_STRFUNC, text, param);
	#define G_ENABLE_DEBUG
#else
	#define DEBUG1(text);
	#define DEBUG2(text, param);
	#define DEBUG2d(text, param);
#endif
#define die(text) g_printerr("%s\n", text); exit(1);
/* }}} */
/* Info text (Yellow label & title) {{{ */
char *infoText;
void setInfoText(char *text) {
	/**
	 * Set the info text in the yellow box and the title
	 * line
	 * Will look like "pqiv: file (size) zoom [nr/count] (text)"
	 * If text is NULL the "(text)" part will be stripped
	 */
	GString *newText = g_string_new(NULL);
	char *displayName;
	displayName = g_filename_display_name(currentFile->fileName);
	if(text == NULL) {
		g_string_printf(newText, "pqiv: %s (%dx%d) %d%% [%d/%d]",
			displayName,
			gdk_pixbuf_get_width(scaledImage),
			gdk_pixbuf_get_height(scaledImage), (int)(zoom * 100), currentFile->nr + 1,
			lastFile->nr + 1);
	}
	else {
		g_string_printf(newText, "pqiv: %s (%dx%d) %d%% [%d/%d] (%s)",
			displayName,
			gdk_pixbuf_get_width(scaledImage),
			gdk_pixbuf_get_height(scaledImage), (int)(zoom * 100), currentFile->nr + 1,
			lastFile->nr + 1, text);
	}
	g_free(displayName);
	gtk_window_set_title(GTK_WINDOW(window), newText->str);
	gtk_label_set_text(GTK_LABEL(infoLabel), newText->str + 6);
	g_string_free(newText, TRUE);
}
/* }}} */
void helpMessage(char claim) { /* {{{ */
	/**
	 * Display help message
	 * If claim != NULL complain about that option.
	 */

	/* Perl code to get bindings - simply execute this c file with perl -x {{{
#!perl
		$b = $o = "";
		open(STDIN, "<", $0);
		while(<>) {
			m/#(?:ifn?def \w+|endif)/ and $ab .= "\t\t".$&."\n";
			m/ADD: (.+?)[{$}\*]/ and $$l .= "\t\t\"".(" "x16)."$1\\n\"\n";
			if(m/(BIND|OPTION): (.+?): (.+?)[{$\*]/) {
				$l = ($1 eq "BIND" ? \$b : \$o);
				$$l .= $ab."\t\t\" ".$2.(" "x(15-length($2))).$3."\\n\"\n";
				$ab = "";
				$$l =~ s/\#ifn?def\s\w+\s*\#endif\s* //x;
			}
		}
		$$_ =~ s/#ifn?def \w+(?:.(?!#endif))+$/$&\t\t#endif\n/s for (\$o, \$b);
		print q(		"options:\n")."\n$o\n\t\t\"\\n\"\n".q(		"key bindings:\n")."\n$b\n";
		__END__

		}}}		
	 */
	g_print("usage: pqiv [options] <files or folders>\n"
		"(p)qiv version " RELEASE " by Phillip Berndt\n"
		"\n");
	if(claim != 0) {
		g_print("I don't understand the meaning of %c\n\n", claim);
	}
	g_print(
		/* Autogenerated! See above */
                "options:\n"
                " -i             Hide info box \n"
                " -f             Start in fullscreen mode \n"
                #ifndef NO_FADING
                " -F             Fade between images \n"
                #endif
                " -s             Activate slideshow \n"
		" -S             Follow symlinks \n"
                #ifndef NO_SORTING
                " -n             Sort all files in natural order \n"
                #endif
                " -d n           Slideshow interval \n"
                " -t             Scale images up to fill the whole screen \n"
                "                Use twice to deactivate scaling completely \n"
                " -r             Read additional filenames (not folders) from stdin \n"
                " -c             Disable the background for transparent images \n"
                "                See manpage for what happens if you use this option more than once \n"
                #ifndef NO_INOTIFY
                " -w             Watch files for changes \n"
                #endif
                " -z n           Set initial zoom level \n"
                " -p             Interpolation quality level (1-4, defaults to 3) \n"
                " -P             Set initial window position. Use: \n"
                "                x,y   to place the window \n"
                "                'off' will deactivate window positioning \n"
                "                Default behaviour is to center the window \n"
		" -R             Reverse meaning of cursor keys and Page Up/Down \n"
                " -a nf          Define n as a keyboard alias for f \n"
		#ifndef NO_KENBURNS
		" -k             Use kenburns effect in fullscreen slideshows \n"
		"                Use twice to activate scaling (Slow!) \n"
		#endif
                #ifndef NO_COMMANDS
                " -<n> s         Set command number n (1-9) to s \n"
                "                See manpage for advanced commands (starting with > or |) \n"
                " -q             Use the qiv-command script for commands \n"
                #endif

                "\n"
                "key bindings:\n"
                " Backspace      Previous image \n"
                " PgUp           Jump 10 images forewards \n"
                " PgDn           Jump 10 images backwards \n"
                " Escape         Quit \n"
                " Cursor keys    Move (Fullscreen) \n"
                " Space          Next image \n"
                " f              Fullscreen \n"
                " r              Reload \n"
                " +              Zoom in \n"
                " -              Zoom out \n"
                " 0              Autoscale down \n"
                " q              Quit \n"
                " t              Toggle autoscale \n"
                " l              Rotate left \n"
                " k              Rotate right \n"
                " h              Horizontal flip \n"
                " v              Vertical flip \n"
                " i              Show/hide info box \n"
                " s              Slideshow toggle \n"
                " a              Hardlink current image to .qiv-select/ \n"
                #ifndef NO_COMMANDS
                " <n>            Run command n (1-3) \n"
                #endif
                " Drag & Drop    Move image (Fullscreen) and decoration switch \n"
                " Button 3/Drag  Zoom in and out \n"
                " Button 2       Quit \n"
                " Button 1/3     Next/previous image \n"
                " Scroll         Next/previous image \n"
		);
	exit(0);
} /* }}} */
/* }}} */
/* File loading and file structure {{{ */
char *mgetcwd() { /* {{{ */
	/**
	 * Behaves like the non-standard GNU getcwd(NULL) call
	 */
	int length = 0;
	char *retVal = NULL;
	while(retVal == NULL) {
		length += 1024;
		retVal = (char*)g_malloc(length);
		getcwd(retVal, length);
		if(retVal == NULL) {
			if(errno == ERANGE) {
				free(retVal);
				length += 1024;
			}
			else {
				die("Failed to get current working directory.");
			}
		}
	}
	return retVal;
} /* }}} */
void loadFilesAddFile(char *file) { /*{{{*/
	/**
	 * Append a file to the list of files
	 */
	if(firstFile.fileName != NULL) {
		lastFile->next = g_new(struct file, 1);
		lastFile->next->nr = lastFile->nr + 1;
		lastFile->next->prev = lastFile;
		lastFile = lastFile->next;
	}
	lastFile->fileName = (char *)g_malloc(strlen(file) + 1);
	strcpy(lastFile->fileName, file);
	lastFile->next = NULL;
} /*}}}*/
void loadFilesHelper(DIR *cwd) { /*{{{*/
	/**
	 * Traverse through a directory and load every
	 * image into the list of files.
	 *
	 * Is called by loadFiles(). If you intend to use
	 * this function keep in mind that you should chdir
	 * to the directory cwd before calling this function.
	 */
	#ifdef SECONDS_TILL_LOADING_INFO
	GTimeVal currentTime;
	#endif
	struct dirent *dirp;
	GtkFileFilterInfo validFileTester;
	DIR *ncwd;
	char *completeName;
	char *cwdName;
	gchar *lowerName;
	cwdName = mgetcwd();
	if(optionFollowSymlinks == TRUE) {
		/* Binary tree for recursion checking */
		if(g_tree_lookup(recursionCheckTree, cwdName) != NULL) {
			DEBUG2("Recursion detected, will not traverse into ", cwdName);
			g_free(cwdName);
			return;
		}
		g_tree_insert(recursionCheckTree, cwdName, (void*)1);
	}

	/* Display information message */
	#ifdef SECONDS_TILL_LOADING_INFO
	if(loadFilesChecked == 0) {
		if(!isatty(1)) {
			loadFilesChecked = 1;
		}
		else {
			g_get_current_time(&currentTime);
			if(programStart.tv_sec + SECONDS_TILL_LOADING_INFO < currentTime.tv_sec) {
				loadFilesChecked = 2;
				g_print("\033" "7");
			}
		}
	}
	else if(loadFilesChecked == 2) {
		g_print("\033" "8\033" "7\033[2K\r%08d images so far, browsing %s", lastFile->nr, cwdName);
	}
	#endif

	validFileTester.contains = GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_DISPLAY_NAME;
	while((dirp = readdir(cwd)) != NULL) {
		if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
			continue;
		}

		/* Try to open (possible) directory */
		ncwd = opendir(dirp->d_name);
		if(ncwd != NULL) {
			/* Check for symlink */
			if(optionFollowSymlinks == FALSE &&
				g_file_test(dirp->d_name, G_FILE_TEST_IS_SYMLINK)) {
				DEBUG2("Will not traverse into symlinked directory %s\n", dirp->d_name);
				closedir(ncwd);
				continue;
			}
			/* Call this function recursive on the subdirectory */
			if(chdir(dirp->d_name) == 0) {
				loadFilesHelper(ncwd);
			}
			chdir(cwdName);
			closedir(ncwd);
		}
		else {
			/* Is a file. Check if it is an image */
			lowerName = g_ascii_strdown(dirp->d_name, strlen(dirp->d_name));
			validFileTester.filename = validFileTester.display_name = lowerName;
			if(gtk_file_filter_filter(fileFormatsFilter, &validFileTester)) {
				/* Load file if readable */
				if(g_access(dirp->d_name, R_OK) == 0) {
					completeName = (char*)g_malloc(
						strlen(dirp->d_name) + strlen(cwdName) + 2);
					sprintf(completeName, "%s/%s", cwdName, dirp->d_name);
					loadFilesAddFile(completeName);
					g_free(completeName);
				}
			}
			g_free(lowerName);
		}
	}
	if(optionFollowSymlinks == FALSE) {
		/* Free cwdName unless it is stored in the
		 * binary tree (which does not copy the data)
		 */
		g_free(cwdName);
	}
} /*}}}*/
gboolean loadFiles(char **iterator) { /*{{{*/
	/**
	 * Load all files. The parameter "iterator"
	 * is an array of char*-filenames.
	 */
	int i;
	DIR *cwd;
	char *ocwd;
	char *buf;
	GError *loadError = NULL;
	GdkPixbufLoader *memoryImageLoader = NULL;

	/* Load files */
	DEBUG1("Load files");
	while(*iterator != 0) {
		if(strcmp(*iterator, "-") == 0) {
			/* Load image from stdin {{{ */
			if(memoryArgImage != NULL) {
				g_printerr("You can't specify more than one image to "
					"be read from stdin.\n");
				iterator++;
				continue;
			}
			memoryImageLoader = gdk_pixbuf_loader_new();
			buf = (char*)g_malloc(1024);
			while(TRUE) {
				i = fread(buf, 1, 1024, stdin);
				if(i == 0) {
					break;
				}
				if(gdk_pixbuf_loader_write(memoryImageLoader, (unsigned char*)buf, i,
					&loadError) == FALSE) {
					g_printerr("Failed to load the image from stdin: %s\n", 
						loadError->message);
					loadError->message = NULL;
					g_error_free(loadError);
					g_object_unref(memoryImageLoader);
					loadError = NULL;
					memoryArgImage = (GdkPixbuf*)1; /* Ignore further
						attempts to load an image from stdin */
					break;
				}
			}
			if(gdk_pixbuf_loader_close(memoryImageLoader, &loadError) == TRUE) {
				memoryArgImage = gdk_pixbuf_copy(
					gdk_pixbuf_loader_get_pixbuf(memoryImageLoader));
				g_object_unref(memoryImageLoader);
				loadFilesAddFile("-");
			}
			else {
				g_printerr("Failed to load the image from stdin: %s\n",
					loadError->message);
				g_error_free(loadError);
				g_object_unref(memoryImageLoader);
				memoryArgImage = (GdkPixbuf*)1; /* Ignore further attempts
					to load an image from stdin */
			}
			g_free(buf);
			iterator++;
			continue;
			/* }}} */
		}
		cwd = opendir(*iterator);
		if(cwd != NULL) {
			/* Use loadFilesHelper for directories */
			ocwd = mgetcwd();
			if(chdir(*iterator) == 0) {
				loadFilesHelper(cwd);
			}
			chdir(ocwd);
			g_free(ocwd);
			closedir(cwd);
		}
		else {
			if(g_access(*iterator, R_OK) == 0) {
				loadFilesAddFile(*iterator);
			}
		}
		iterator++;
	}
	return FALSE;
} /*}}}*/
gint windowCloseOnlyCb(GtkWidget *widget, GdkEventKey *event, gpointer data) { /*{{{*/
	/**
	 * Callback function:
	 * Close window when q is pressed
	 */
	if(event->keyval == 'q') {
		gtk_widget_destroy(widget);
	}
	return 0;
} /* }}} */
gboolean storeImageCb(const char *buf, gsize count, GError **error, gpointer data) { /*{{{*/
	/**
	 * Write "buf" to the file pointed to by "data"
	 */
	if(write(*(int*)data, buf, count) != -1) {
		return TRUE;
	}
	else {
		return FALSE;
	}
} /*}}}*/
int copyFile(char *src, char *dst) { /*{{{*/
	int fsrc, fdst, rds;
	char buffer[1024];

	fsrc = open(src, O_RDONLY);
	if(fsrc == -1) {
		return -1;
	}
	fdst = open(dst, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if(fdst == -1) {
		close(fsrc);
		return -1;
	}

	while( ((rds = read(fsrc, buffer, 1024)) > 0) &&
		(rds = write(fdst, buffer, rds) != -1));
	
	close(fsrc);
	close(fdst);

	if(rds == -1) {
		unlink(dst);
	}

	return rds;
} /*}}}*/
#ifndef NO_COMMANDS
void runProgram(char *command) { /*{{{*/
	/**
	 * Execute program "command" on the current
	 * image. If command starts with ">" or "|"
	 * behaves differently, you should read through
	 * the code before using this function ;)
	 */
	char *buf4, *buf3, *buf2, *buf;
	GtkWidget *tmpWindow, *tmpScroller, *tmpText;
	FILE *readInformation;
	GString *infoString;
	gsize uniTextLength;
	GdkPixbufLoader *memoryImageLoader = NULL;
	GdkPixbuf *tmpImage = NULL;
	int i, child;
	GError *loadError = NULL;
	int tmpFileDescriptorsTo[] = {0, 0};
	int tmpFileDescriptorsFrom[] = {0, 0};
	if(command[0] == '>') {
		/* Pipe information {{{ */
		command = &command[1]; /* Does always exist as command is at least ">\0" */
		buf2 = g_shell_quote(currentFile->fileName);
		buf = (char*)g_malloc(strlen(command) + 2 + strlen(buf2));
		sprintf(buf, "%s %s", command, buf2);
		readInformation = popen(buf, "r");
		if(readInformation == NULL) {
			g_printerr("Command execution failed for %s\n", command);
			g_free(buf);
			g_free(buf2);
			return;
		}
		infoString = g_string_new(NULL);
		buf3 = (char*)g_malloc(1024);
		while(fgets(buf3, 1024, readInformation) != NULL) {
			g_string_append(infoString, buf3);
		}
		pclose(readInformation);
		g_free(buf);
		g_free(buf2);
		g_free(buf3);
	
		/* Display information in a window */
		tmpWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(tmpWindow), command);
		gtk_window_set_position(GTK_WINDOW(tmpWindow), GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_modal(GTK_WINDOW(tmpWindow), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(tmpWindow), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(tmpWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_widget_set_size_request(tmpWindow, 400, 480);
		g_signal_connect(tmpWindow, "key-press-event",
			G_CALLBACK(windowCloseOnlyCb), NULL);
		tmpScroller = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(tmpWindow), tmpScroller);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tmpScroller),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		tmpText = gtk_text_view_new();
		gtk_container_add(GTK_CONTAINER(tmpScroller), tmpText); 
		gtk_text_view_set_editable(GTK_TEXT_VIEW(tmpText), FALSE);
		if(g_utf8_validate(infoString->str, infoString->len, NULL) == FALSE) {
			buf4 = g_convert(infoString->str, infoString->len, "utf8", "iso8859-1",
				NULL, &uniTextLength, NULL);
			gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tmpText)),
				buf4, uniTextLength);
			g_free(buf4);
		}
		else {
			gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tmpText)),
				infoString->str, infoString->len);
		}
		gtk_widget_show(tmpText);
		gtk_widget_show(tmpScroller);
		gtk_widget_show(tmpWindow);
		g_string_free(infoString, TRUE);
		/* }}} */
	}
	else if(command[0] == '|') {
		/* Pipe data {{{ */
		command = &command[1];
		/* Create a pipe */
		if(pipe(tmpFileDescriptorsTo) == -1) {
			g_printerr("Failed to create pipes for data exchange\n");
			setInfoText("Failure");
			return;
		}
		if(pipe(tmpFileDescriptorsFrom) == -1) {
			g_printerr("Failed to create pipes for data exchange\n");
			setInfoText("Failure");
			close(tmpFileDescriptorsTo[0]);
			close(tmpFileDescriptorsTo[1]);
			return;
		}
		/* Spawn child process */
		if((child = fork()) == 0) {
			dup2(tmpFileDescriptorsFrom[1], STDOUT_FILENO);
			dup2(tmpFileDescriptorsTo[0], STDIN_FILENO);
			close(tmpFileDescriptorsFrom[0]); close(tmpFileDescriptorsFrom[1]);
			close(tmpFileDescriptorsTo[0]); close(tmpFileDescriptorsTo[1]);
			_exit(system(command));
		}
		/* Store currentImage to the child process'es stdin */
		if(fork() == 0) {
			close(tmpFileDescriptorsFrom[0]); close(tmpFileDescriptorsFrom[1]);
			close(tmpFileDescriptorsTo[0]); 
			if(gdk_pixbuf_save_to_callback(currentImage, storeImageCb,
					&tmpFileDescriptorsTo[1],
					"png", NULL, NULL) == FALSE) {
				g_printerr("Failed to save image\n");
				close(tmpFileDescriptorsFrom[0]); close(tmpFileDescriptorsFrom[1]);
				close(tmpFileDescriptorsTo[0]); close(tmpFileDescriptorsTo[1]);
				setInfoText("Failure");
				_exit(1);
			}
			close(tmpFileDescriptorsTo[1]);
			_exit(0);
		}
		fsync(tmpFileDescriptorsTo[1]);
		close(tmpFileDescriptorsFrom[1]); close(tmpFileDescriptorsTo[0]);
		close(tmpFileDescriptorsTo[1]);
		/* Load new image from the child processes stdout */
		memoryImageLoader = gdk_pixbuf_loader_new();
		buf = (char*)g_malloc(1024);
		while(TRUE) {
			if((i = read(tmpFileDescriptorsFrom[0], buf, 1024)) < 1) {
				break;
			}
			if(gdk_pixbuf_loader_write(memoryImageLoader, (unsigned char*)buf,
				i, &loadError) == FALSE) {
				kill(child, SIGTERM);
				g_printerr("Failed to load output image: %s\n", loadError->message);
				g_error_free(loadError);
				g_object_unref(memoryImageLoader);
				close(tmpFileDescriptorsFrom[0]);
				g_free(buf);
				setInfoText("Failure");
				return;
			}
		}
		close(tmpFileDescriptorsFrom[0]);
		g_free(buf);
		wait(NULL);

		if(gdk_pixbuf_loader_close(memoryImageLoader, NULL) == TRUE) {
			g_object_unref(currentImage);
			tmpImage = gdk_pixbuf_loader_get_pixbuf(memoryImageLoader);
			currentImage = gdk_pixbuf_copy(tmpImage);
			g_object_unref(memoryImageLoader);
			scaledAt = -1;
			autoScaleFactor();
			resizeAndPosWindow();
			displayImage();
			setInfoText("Success");
		}
		else {
			g_object_unref(memoryImageLoader);
			setInfoText("Failure");
		}
		/* }}} */
	}
	else {
		/* Run program {{{ */
		if(fork() == 0) {
			buf2 = g_shell_quote(currentFile->fileName);
			buf = (char*)g_malloc(strlen(command) + 2 + strlen(buf2));
			sprintf(buf, "%s %s", command, buf2);
			system(buf);
			g_free(buf);
			g_free(buf2);
			exit(0);
		} /* }}} */
	}
} /*}}}*/
#endif
#ifndef NO_INOTIFY
void inotifyCb(gpointer data, gint source_fd, GdkInputCondition condition) { /*{{{*/
	/**
	 * Callback for inotify, gets called if the current image
	 * has been modified
	 */
	GdkEventKey keyEvent;
	struct inotify_event *event = g_new(struct inotify_event, 1);
	DEBUG1("Inotify cb");

	read(inotifyFd, event, sizeof(struct inotify_event));
	g_assert(event->len == 0); /* Should not happen according to manpage
				    * The filename flag is only filled for
				    * directory watches
				    */
	if(event->mask != IN_CLOSE_WRITE) {
		g_free(event);
		return;
	}
	g_free(event);

	/* We have to emulate a keypress because of some buggy wms */
	memset(&keyEvent, 0, sizeof(GdkEventKey));
	keyEvent.type = GDK_KEY_PRESS;
	keyEvent.window = window->window;
	keyEvent.time = time(NULL);
	keyEvent.keyval = 114;
	keyEvent.hardware_keycode = 27;
	keyEvent.length = 1;
	keyEvent.string = "r";
	gdk_event_put((GdkEvent*)(&keyEvent));
} /*}}}*/
#endif
/* File sorting {{{ */
#ifndef NO_SORTING
int sortFilesCompare(const void *f1, const void *f2) {
	/**
	 * Compare function for qsort
	 *
	 * This is quite complex oO
	 * qsort gives me pointers to the array elements,
	 * which are pointers themselves
	 */
	return strnatcasecmp(
		(*(struct file **)f1)->fileName,
		(*(struct file **)f2)->fileName
		);
}
void sortFiles() {
	/**
	 * Sort the file list
	 */

	/* TODO
	 * Find a better method for this. Atm the code fills an array with
	 * all elements, qsorts it and relinks all elements. It should be
	 * possible to avoid the array or at least the linking without
	 * having to write a new qsort implementation. Maybe something with
	 * the string pointers?!
	 */
	DEBUG1("Sort");
	int i = 0;
	int fileCount = lastFile->nr;
	/* Create a copy of the firstFile structure (which is not dynamically
	 * allocated) */
	struct file *tmpStore = g_new(struct file, 1);
	memcpy(tmpStore, &firstFile, sizeof(struct file));
	struct file *iterator = tmpStore;
	if(iterator->next != NULL) {
		iterator->next->prev = iterator;
	}
	/* Create an array of pointers to all files */
	struct file **files = g_new(struct file*, fileCount + 1);
	do {
		files[i++] = iterator;
	} while((iterator = iterator->next) != NULL);
	/* qsort that array */
	qsort(files, lastFile->nr + 1, sizeof(struct file*), sortFilesCompare);
	/* Renumber and relink the list */
	lastFile = files[fileCount];
	for(i=0; i<=fileCount; i++) {
		files[i]->nr = i;
		files[i]->prev = (i == 0 ? NULL : files[i-1]);
		files[i]->next = (i == fileCount ? NULL : files[i+1]);
	}
	/* Copy the firstFile structure back */
	memcpy(&firstFile, files[0], sizeof(struct file));
	if(firstFile.next != NULL) {
		files[1]->prev = &firstFile;
	}
	/* Free the temporary copy */
	g_free(files[0]);
}
#endif
/* }}} */
/*}}}*/
/* Load images and modify them {{{ */
gboolean loadImage() { /*{{{*/
	/**
	 * Load an image and display it
	 */
	GdkPixbuf *tmpImage;
	GdkPixbuf *chessBoardBuf;
	GError *error = NULL;
	int i, n, o, p;

	DEBUG2("loadImage", currentFile->fileName);
	if(strcmp(currentFile->fileName, "-") == 0) {
		/* Load memory image */
		if(memoryArgImage == NULL) {
			return FALSE;
		}
		tmpImage = g_object_ref(memoryArgImage);
	}
	else {
		/* Load from file */
		tmpImage = gdk_pixbuf_new_from_file(currentFile->fileName, &error);
		if(!tmpImage) {
			g_printerr("Failed to load %s: %s\n", currentFile->fileName, error->message);
			g_error_free(error);
			return FALSE;
		}
	}
	if(currentImage != NULL) {
		DEBUG1("Free old image");
		g_object_unref(currentImage);
	}
	if(optionHideChessboardLevel == 0 && gdk_pixbuf_get_has_alpha(tmpImage)) {
		/* Draw chessboard for transparent images */
		DEBUG1("Creating chessboard");
		chessBoardBuf = gdk_pixbuf_new_from_inline(159, (const guint8 *)chessBoard,
			FALSE, NULL);
		currentImage = gdk_pixbuf_copy(tmpImage);
		o = gdk_pixbuf_get_width(currentImage);
		p = gdk_pixbuf_get_height(currentImage);
		for(i=0; i<=o; i+=16) {
			for(n=0; n<=p; n+=16) {
				gdk_pixbuf_copy_area(chessBoardBuf, 0, 0, (o-i<16)?o-i:16,
					(p-n<16)?p-n:16, currentImage, i, n);
			}
		}
		/* Copy image into chessboard */
		gdk_pixbuf_composite(tmpImage, currentImage, 0, 0, o, p, 0, 0, 1, 1,
			GDK_INTERP_BILINEAR, 255);
		g_object_unref(tmpImage);
		g_object_unref(chessBoardBuf);
	}
	else {
		/* If the image has no alpha channel, just display the image */
		currentImage = tmpImage;
	}

	/* Rotate image */
	#if GDK_PIXBUF_MAJOR >= 2 && GDK_PIXBUF_MINOR >= 12
	if(gdk_pixbuf_get_option(currentImage, "orientation") != NULL) {
		tmpImage = gdk_pixbuf_apply_embedded_orientation(currentImage);
		g_object_unref(currentImage);
		currentImage = tmpImage;
	}
	#else
		#warning "Automatic image rotation requires GDK Pixbuf >= 2.12. Feature disabled!"
	#endif

	#ifndef NO_INOTIFY
	/* Update inotify watch to manage automatic reloading {{{ */ 
	if(optionUseInotify == TRUE) {
		DEBUG1("Updating inotify fd");
		if(inotifyWd != -1) {
			inotify_rm_watch(inotifyFd, inotifyWd);
			inotifyWd = -1;
		}
		inotifyWd = inotify_add_watch(inotifyFd, currentFile->fileName, IN_CLOSE_WRITE);
	} /* }}} */
	#endif

	/* Reset settings */
	zoom = 1;
	moveX = moveY = 0;
	scaledAt = -1;
	return TRUE;
} /*}}}*/
inline void scale() { /*{{{*/
	/**
	 * Scale image according to the value of "zoom"
	 * This function uses "scaledAt" and "scaledImage"
	 * for caching.
	 */
	int imgx, imgy;

	if(scaledAt != zoom) {
		DEBUG1("Scale");
		imgx = gdk_pixbuf_get_width(currentImage);
		imgy = gdk_pixbuf_get_height(currentImage);
		scaledAt = zoom;
		if(scaledImage != NULL) {
			DEBUG1("Free");
			g_object_unref(scaledImage);
			scaledImage = NULL;
		}
		if(imgx > 10 && imgy > 10 && (imgx * zoom < 10 || imgy * zoom < 10)) {
			/* Don't zoom below 10x10 pixels (except for images that actually are that small) */
			scaledImage = gdk_pixbuf_scale_simple(currentImage, 10, 10,
				GDK_INTERP_BILINEAR);
		}
		else {
			scaledImage = gdk_pixbuf_scale_simple(currentImage, (int)(imgx * zoom),
				(int)(imgy * zoom), optionInterpolation);
		}
		if(scaledImage == NULL) {
			die("Failed to scale image");
		}
	}
} /*}}}*/
void forceAutoScaleFactor(enum autoScaleSetting upDown) { /*{{{*/
	/**
	 * Set "zoom" to an optimal value according
	 * to the window/screen size
	 */
	int imgx, imgy, scrx, scry, rem;
	GdkScreen *screen;

	screen = gtk_widget_get_screen(window);
	imgx = gdk_pixbuf_get_width(currentImage);
	imgy = gdk_pixbuf_get_height(currentImage);
	scrx = gdk_screen_get_width(screen);
	scry = gdk_screen_get_height(screen);

	if(isFullscreen == TRUE) {
		rem = 0;
	}
	else {
		rem = 125;
	}

	zoom = 1;
	if(upDown == ALWAYS) {
		/* Scale images smaller than the screen up */
		zoom = (scrx - rem) * 1.0f / imgx;
		if(imgy * zoom > scry - rem) {
			zoom = (scry - rem) * 1.0f / imgy;
		}
	}
	else {
		/* Scale images bigger than the screen down */
		if(imgx > scrx - rem) {
			zoom = (scrx - rem) * 1.0f / imgx;
		}
		if(imgy * zoom > scry - rem) {
			zoom = (scry - rem) * 1.0f / imgy;
		}
	}

	/* Now do actually scale */
	scale();
} /*}}}*/
void autoScaleFactor() { /*{{{*/
	/**
	 * Wrapper for forceAutoScaleFactor;
	 * will call forceAutoScaleFactor only if
	 * autoScale is on
	 */
	DEBUG1("autoScaleFactor");

	if(autoScale == OFF) {
		zoom = optionInitialZoom;
		scale();
		return;
	}
	
	forceAutoScaleFactor(autoScale);
} /*}}}*/
void scaleBy(float add) { /*{{{*/
	/*
	 * Change zoom factor by "add"
	 */
	DEBUG1("Scale by");
	zoom += add;
	scale();
} /*}}}*/
void flip(gboolean horizontal) { /*{{{*/
	/*
	 * Flip the image
	 * You'll have to regenerate the scaled image!
	 */
	GdkPixbuf *tmp;
	DEBUG1("flip");
	tmp = gdk_pixbuf_flip(currentImage, horizontal);
	g_object_unref(currentImage);
	currentImage = tmp;

	scaledAt = -1;
} /*}}}*/
void rotate(gboolean left) { /*{{{*/
	/*
	 * Rotate the image
	 * You'll have to regenerate the scaled image!
	 */
	GdkPixbuf *tmp;
	DEBUG1("Rotate");
	tmp = gdk_pixbuf_rotate_simple(currentImage, left == TRUE ? 90 : 270);
	g_object_unref(currentImage);
	currentImage = tmp;

	scaledAt = -1;
} /*}}}*/
void jumpFiles(int num) { /* {{{ */
	int i, n, count, forwards;

	if(num < 0) {
		count = -num;
		forwards = FALSE;
	} else {
		count = num;
		forwards = TRUE;
	}

	i = currentFile->nr;
	do {
		for(n=0; n<count; n++) {
			if(forwards) {
				currentFile = currentFile->next;
				if(currentFile == NULL) {
					currentFile = &firstFile;
				}
			} else {
				currentFile = currentFile->prev;
				if(currentFile == NULL) {
					currentFile = lastFile;
				}
			}
		}
	} while((!reloadImage()) && i != currentFile->nr);
} /* }}} */
/* }}} */
/* Draw image to screen {{{ */
#ifndef NO_COMPOSITING
gint exposeCb(GtkWidget *widget, GdkEventExpose *event, gpointer data) { /*{{{*/
	/**
	 * Taken from API documentation on developer.gnome.org
	 * For real alpha :)
	 */
	cairo_t *cr;
	DEBUG1("Expose");
	cr = gdk_cairo_create(widget->window);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	gdk_cairo_region(cr, event->region);
	cairo_fill(cr);
	cairo_destroy(cr);
	return FALSE;
} /*}}}*/
/* Screen changed callback (for transparent window) {{{ */
static void alphaScreenChangedCb(GtkWidget *widget, GdkScreen *old_screen, gpointer userdata) {
	GdkScreen *screen;
	GdkColormap *colormap;
	DEBUG1("Screen changed");
	
	screen = gtk_widget_get_screen(widget);
	colormap = gdk_screen_get_rgba_colormap(screen);
	if(!colormap) {
		g_printerr("Sorry, alpha channels are not supported on this screen.\n");
		die("Try again without -cc or activate compositing\n");
	}
	gtk_widget_set_colormap(widget, colormap);
}
/* }}} */
#endif
#ifndef NO_FADING
/* Fading images {{{ */
gboolean fadeOut(gpointer data) { /*{{{*/
	/**
	 * Timeout callback for fading between two images
	 */
	struct fadeInfo *fadeStruct = (struct fadeInfo *)data;
	int imgx, imgy;
	GdkPixbuf *fadeBuf;
	imgx = gdk_pixbuf_get_width(fadeStruct->pixbuf);
	imgy = gdk_pixbuf_get_height(fadeStruct->pixbuf);

	fadeStruct->alpha -= 20;
	if(fadeStruct->alpha > 0 && imgx == gdk_pixbuf_get_width(scaledImage) &&
		imgy == gdk_pixbuf_get_height(scaledImage)) {
		
		fadeBuf = gdk_pixbuf_copy(scaledImage);
		gdk_pixbuf_composite(fadeStruct->pixbuf, fadeBuf, 0,
			0, imgx, imgy, 0, 0, 1, 1,
			GDK_INTERP_NEAREST, fadeStruct->alpha);
		gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), fadeBuf);
		g_object_unref(fadeBuf);
		return TRUE;
	}
	else {
		gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), scaledImage);
		g_object_unref(fadeStruct->pixbuf);
		g_free(fadeStruct);
		return FALSE;
	}
} /*}}}*/
void fadeImage(GdkPixbuf *image) { /*{{{*/
	/**
	 * Initialize image fading. Fade from "image" to
	 * currentImage
	 */
	struct fadeInfo *fadeStruct;
	fadeStruct = g_new(struct fadeInfo, 1);
	if(gdk_pixbuf_get_width(image) != gdk_pixbuf_get_width(scaledImage) ||
		gdk_pixbuf_get_height(image) != gdk_pixbuf_get_height(scaledImage)) {
		g_free(fadeStruct);
		displayImage();
		return;
	}
	DEBUG1("Fade");
	if(scaledAt != zoom) {
		scale();
	}
	fadeStruct->pixbuf = image;
	fadeStruct->alpha = 255;
	gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), image);
	g_timeout_add(50, fadeOut, fadeStruct);
} /*}}}*/
/* }}} */
#endif
void setFullscreen(gboolean fullscreen) { /*{{{*/
	/**
	 * Change to fullscreen view (and back)
	 */
	GdkCursor *cursor;
	GdkPixmap *source;
	GdkScreen *screen;
	int scrx, scry;

	DEBUG1("Fullscreen");
	if(fullscreen == TRUE) {
		/* This is needed because of crappy window managers :/ */
		screen = gtk_widget_get_screen(window);
		scrx = gdk_screen_get_width(screen);
		scry = gdk_screen_get_height(screen);
		gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
		while(gtk_events_pending()) {
			gtk_main_iteration();
		}
		gdk_window_fullscreen(window->window);
		while(gtk_events_pending()) {
			gtk_main_iteration();
		}
		gtk_widget_set_size_request(window, scrx, scry);
		/* This is done by event cb now
		 * gtk_window_set_resizable(GTK_WINDOW(window), FALSE);*/

		/* Hide cursor */
		source = gdk_bitmap_create_from_data (NULL, emptyCursor,
                                       16, 16);
		cursor = gdk_cursor_new_from_pixmap (source, source, (GdkColor*)emptyCursor,
			(GdkColor*)emptyCursor, 8, 8);
		gdk_pixmap_unref(source);
		gdk_window_set_cursor(window->window, cursor);
		gdk_cursor_unref(cursor);
	}
	else {
		gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
		while(gtk_events_pending()) {
			gtk_main_iteration();
		}
		gdk_window_unfullscreen(window->window);
		while(gtk_events_pending()) {
			gtk_main_iteration();
		}
		/*gtk_window_set_resizable(GTK_WINDOW(window), FALSE);*/
		gdk_window_set_cursor(window->window, NULL);
	}
	scaledAt = -1;
	isFullscreen = fullscreen;
} /*}}}*/
gboolean toFullscreenCb(gpointer data) { /*{{{*/
	/**
	 * Callback for the "go to fullscreen on load"
	 * function (which must be called a few ms. after
	 * starting the program oO)
	 */
	GdkEventKey keyEvent;
	DEBUG1("Switch to fullscreen (callback)");

	/* We have to emulate a keypress because of some buggy wms */
	memset(&keyEvent, 0, sizeof(GdkEventKey));
	keyEvent.type = GDK_KEY_PRESS;
	keyEvent.window = window->window;
	keyEvent.time = time(NULL);
	keyEvent.keyval = 102;
	keyEvent.hardware_keycode = 41;
	keyEvent.length = 1;
	keyEvent.string = "f";
	gdk_event_put((GdkEvent*)(&keyEvent));

	return FALSE;
} /* }}} */
void resizeAndPosWindow() { /*{{{*/
	/**
	 * Resize the window and place it centered
	 * on the screen
	 */
	int imgx, imgy, scrx, scry;
	GdkScreen *screen;
	DEBUG1("Resize");

	imgx = gdk_pixbuf_get_width(scaledImage);
	imgy = gdk_pixbuf_get_height(scaledImage);
	screen = gtk_widget_get_screen(window);
	scrx = gdk_screen_get_width(screen);
	scry = gdk_screen_get_height(screen);

	gtk_widget_set_size_request(imageWidget, imgx, imgy);

	if(!isFullscreen) {
		/* In window mode, resize and reposition window */
		gtk_widget_set_size_request(mouseEventBox, imgx, imgy);
		gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
		while(gtk_events_pending()) {
			gtk_main_iteration();
		}
		gtk_widget_set_size_request(window, imgx, imgy);
		while(gtk_events_pending()) {
			gtk_main_iteration();
		}
		/*gtk_window_set_resizable(GTK_WINDOW(window), FALSE);*/
		if(optionWindowPosition[2] == -1) {
			gtk_window_move(GTK_WINDOW(window), (scrx - imgx) / 2, (scry - imgy) / 2);
		}
		else if(optionWindowPosition[2] == 1) {
			gtk_window_move(GTK_WINDOW(window), optionWindowPosition[0], 
				optionWindowPosition[1]);
		}
		gtk_fixed_move(GTK_FIXED(fixed), imageWidget, 0, 0);
	}
	else {
		/* Fullscreen mode: Center image widget */
		gtk_widget_set_size_request(mouseEventBox, scrx, scry);
		gtk_fixed_move(GTK_FIXED(fixed), imageWidget, (scrx - imgx) / 2 + moveX,
			(scry - imgy) / 2 + moveY);
	}
} /*}}}*/
void displayImage() { /*{{{*/
	/**
	 * Display the image (Call "scale" and then draw
	 * the scaled image)
	 */
	DEBUG1("Display");
	if(scaledAt != zoom) {
		scale();
	}
	/* Draw image */
	gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), scaledImage);
} /*}}}*/
gboolean reloadImage() { /*{{{*/
	/**
	 * In fact, not only reload but also used to load images
	 */
	/* The additional ref is used for fading */
	GdkPixbuf *oldImage = g_object_ref(scaledImage);
	if(!loadImage()) {
		g_object_unref(oldImage);
		return FALSE;
	}

	autoScaleFactor();
	resizeAndPosWindow();
	
	#ifndef NO_FADING
	if(optionFadeImages == TRUE) {
		fadeImage(oldImage);
	} else {
		displayImage();
		g_object_unref(oldImage);
	}
	#else
	displayImage();
	g_object_unref(oldImage);
	#endif

	setInfoText(NULL);
	return TRUE;
} /*}}}*/
/* }}} */
/* Slideshow {{{ */
gboolean slideshowCb(gpointer data) { /*{{{*/
	/**
	 * Callback for slideshow mode,
	 * emulates a keypress of the spacebar 
	 */
	GdkEventKey keyEvent;
	DEBUG1("Slideshow next");

	/* We have to emulate a keypress because of some buggy wms */
	memset(&keyEvent, 0, sizeof(GdkEventKey));
	keyEvent.type = GDK_KEY_PRESS;
	keyEvent.window = window->window;
	keyEvent.time = time(NULL);
	keyEvent.keyval = 32;
	keyEvent.hardware_keycode = 65;
	keyEvent.length = 1;
	keyEvent.string = "x";
	gdk_event_put((GdkEvent*)(&keyEvent));

	/*
	 * We can't use true here because some images could need a long time to load
	 * This makes the whole slide show thing MUCH more complicated
	 */
	slideshowID = 0;
	return FALSE;
} /*}}}*/
#ifndef NO_KENBURNS
gboolean kenburnsCb(gpointer data) { /* {{{ */
	int i, imgx, imgy, scrx, scry;
	float myscale;
	GdkScreen *screen;
	GdkPixbuf *myScaledImage;

	if(slideshowEnabled == FALSE) {
		kenburnsData.id = 0;
		reloadImage();
		return FALSE;
	}

	if(kenburnsData.status == 0) {
		DEBUG1("Kenburns: New image");
		i = currentFile->nr;
		do {
			currentFile = currentFile->next;
			if(currentFile == NULL) {
				currentFile = &firstFile;
			}
		} while((!loadImage()) && i != currentFile->nr);

		imgx = gdk_pixbuf_get_width(currentImage);
		imgy = gdk_pixbuf_get_height(currentImage);
		screen = gtk_widget_get_screen(window);
		scrx = gdk_screen_get_width(screen);
		scry = gdk_screen_get_height(screen);

		if(imgx > scrx) {
			kenburnsData.addX = (scrx - imgx) / (slideshowInterval * 1000 / KENBURNS_SPEED_ADJUST);
			if(rand() > RAND_MAX / 2) {
				kenburnsData.addX = -1 * kenburnsData.addX;
				kenburnsData.currX = (scrx - imgx);
			}
		}
		else {
			kenburnsData.addX = 0;
			kenburnsData.currX = (scrx - imgx) / 2;
		}
		if(imgy > scry) {
			kenburnsData.addY = (scry - imgy) / (slideshowInterval * 1000 / KENBURNS_SPEED_ADJUST);
			if(rand() > RAND_MAX / 2) {
				kenburnsData.addY = -1 * kenburnsData.addY;
				kenburnsData.currY = (scry - imgy);
			}
		}
		else {
			kenburnsData.addY = 0;
			kenburnsData.currY = (scry - imgy) / 2;
		}

		if(optionUseKenburns == WITH_SCALING) {
			myscale = 1 + rand() * 0.5 / RAND_MAX;
			kenburnsData.addSizeX = imgx * (myscale - 1) / (slideshowInterval * 1000 / KENBURNS_SPEED_ADJUST);
			kenburnsData.addSizeY = imgy * (myscale - 1) / (slideshowInterval * 1000 / KENBURNS_SPEED_ADJUST);

			/* Scaling down should be faster */
			myScaledImage = gdk_pixbuf_scale_simple(currentImage, (int)(imgx * myscale), (int)(imgy * myscale), GDK_INTERP_NEAREST);
			g_object_unref(currentImage);
			currentImage = myScaledImage;

			if(rand() > RAND_MAX / 2) {
				kenburnsData.sizeX = imgx * myscale;
				kenburnsData.sizeY = imgy * myscale;
				kenburnsData.addSizeY *= -1;
				kenburnsData.addSizeX *= -1;
			}
			else {
				kenburnsData.sizeX = imgx;
				kenburnsData.sizeY = imgy;
			}

		
			myScaledImage = gdk_pixbuf_scale_simple(currentImage, (int)(kenburnsData.sizeX), (int)(kenburnsData.sizeY), GDK_INTERP_NEAREST);
			gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), myScaledImage);
			gtk_widget_set_size_request(imageWidget, (int)(kenburnsData.sizeX), (int)(kenburnsData.sizeY));
			g_object_unref(myScaledImage);
		}
		else {
			gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), currentImage);
			gtk_widget_set_size_request(imageWidget, imgx, imgy);
		}

		gtk_widget_set_size_request(mouseEventBox, scrx, scry);
		setInfoText(NULL);
	}

	if(optionUseKenburns == WITH_SCALING) {
		kenburnsData.sizeX += kenburnsData.addSizeX;
		kenburnsData.sizeY += kenburnsData.addSizeY;
		myScaledImage = gdk_pixbuf_scale_simple(currentImage, (int)(kenburnsData.sizeX), (int)(kenburnsData.sizeY), GDK_INTERP_NEAREST);
		gtk_image_set_from_pixbuf(GTK_IMAGE(imageWidget), myScaledImage);
		gtk_widget_set_size_request(imageWidget, (int)(kenburnsData.sizeX), (int)(kenburnsData.sizeY));
		g_object_unref(myScaledImage);
	}

	gtk_fixed_move(GTK_FIXED(fixed), imageWidget, (int)kenburnsData.currX, (int)kenburnsData.currY);
	kenburnsData.currX += kenburnsData.addX;
	kenburnsData.currY += kenburnsData.addY;

	kenburnsData.status = (kenburnsData.status + 1) % (slideshowInterval * 1000 / KENBURNS_SPEED_ADJUST);
	return TRUE;
} /* }}} */
#endif
inline void slideshowDo() { /*{{{*/
	/**
	 * Activate/deactivate slideshow
	 */
	DEBUG1("Slideshow switch");
	if(slideshowEnabled == FALSE) {
		return;
	}
	if(slideshowID != 0) {
		g_source_remove(slideshowEnabled);
		slideshowID = 0;
	}

	#ifndef NO_KENBURNS
	if(kenburnsData.id != 0) {
		g_source_remove(kenburnsData.id);
		kenburnsData.id = 0;
	}

	if(isFullscreen == TRUE && optionUseKenburns) {
		memset(&kenburnsData, 0, sizeof(kenburnsData));
		kenburnsData.id = g_timeout_add(KENBURNS_SPEED_ADJUST, kenburnsCb, NULL);
	}
	else {
	#endif
		slideshowID = g_timeout_add(slideshowInterval * 1000, slideshowCb, NULL);
	#ifndef NO_KENBURNS
	}
	#endif
} /*}}}*/
/* }}} */
/* Jump dialog  {{{ */
gboolean doJumpDialog_searchListFilter(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) { /* {{{ */
	/**
	 * List filter function for the jump dialog
	 */
	GValue colData;
	char *entryText = (char*)gtk_entry_get_text(GTK_ENTRY(data)); /* (Must not be freed here) */
	char *compareIn, *compareSearch;
	gboolean retVal;

	if(entryText[0] == 0) {
		return TRUE;
	}

	memset(&colData, 0, sizeof(GValue));
	gtk_tree_model_get_value(model, iter, 1, &colData);
	compareIn = (char*)g_value_get_string(&colData); /* (Must not be freed here) */
	compareIn = (char*)g_ascii_strdown(compareIn, strlen(compareIn));
	compareSearch = (char*)g_ascii_strdown(entryText, strlen(entryText));
	retVal = strstr(compareIn, compareSearch) != NULL;
	g_free(compareIn);
	g_free(compareSearch);
	g_value_unset(&colData);

	return retVal;
} /* }}} */
gint doJumpDialog_entryChangedCallback(GtkWidget *entry, gpointer data) { /*{{{*/
	/**
	 * Refilter the list when the entry text is changed
	 */
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(data));
	return FALSE;
} /* }}} */
gint doJumpDialog_exitOnEnter(GtkWidget *widget, GdkEventKey *event, gpointer data) { /*{{{*/
	/**
	 * If return is pressed exit the dialog
	 */
	if(event->keyval == GDK_Return) {
		gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
		return TRUE;
	}
	return FALSE;
} /* }}} */
gint doJumpDialog_exitOnDblClk(GtkWidget *widget, GdkEventButton *event, gpointer data) { /*{{{*/
	/**
	 * If the user doubleclicks into the list box, exit
	 * the dialog
	 */
	if(event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
		return TRUE;
	}
	return FALSE;
} /* }}} */
void doJumpDialog_openImage(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) { /* {{{ */
	/**
	 * "for each" function for the list of the jump dialog
	 * (there can't be more than one selected image)
	 * Loads the image
	 */
	GValue colData;
	GtkWidget *dlgWindow;
	struct file *oldIndex;
	int jumpTo;
	memset(&colData, 0, sizeof(GValue));
	gtk_tree_model_get_value(model, iter, 0, &colData);
	jumpTo = g_value_get_int(&colData); /* (Must not be freed here) */
	g_value_unset(&colData);

	/* Jump to that image */
	oldIndex = currentFile;
	currentFile = &firstFile;
	while(jumpTo-- > 1) {
		currentFile = currentFile->next;
	}
	if(!reloadImage()) {
		dlgWindow = gtk_message_dialog_new(
			GTK_WINDOW(window),
			GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"Error jumping to image %s",
			currentFile->fileName);
		gtk_window_set_title(GTK_WINDOW(dlgWindow), "pqiv: Error");
		currentFile = oldIndex;
		gtk_dialog_run(GTK_DIALOG(dlgWindow));
		gtk_widget_destroy(dlgWindow);
	}
} /* }}} */
inline void doJumpDialog() { /* {{{ */
	/**
	 * Show the jump dialog to jump directly
	 * to an image
	 */
	GtkWidget *dlgWindow;
	GtkWidget *searchEntry;
	GtkWidget *searchListBox;
	GtkWidget *scrollBar;
	GtkListStore *searchList;
	GtkTreeModel *searchListFilter;
	GtkTreeIter searchListIter;
	GtkTreePath *gotoActivePath;
	GtkCellRenderer *searchListRenderer0;
	GtkCellRenderer *searchListRenderer1;
	struct file *tmpFileIndex;
	char *tmpStr;
	DEBUG1("Jump dialog");

	/* Create dialog box */
	dlgWindow = gtk_dialog_new_with_buttons("pqiv: Jump to image",
		GTK_WINDOW(window),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
		GTK_STOCK_OK,
		GTK_RESPONSE_ACCEPT,
		NULL);
	
	searchEntry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlgWindow)->vbox),
		searchEntry,
		FALSE,
		TRUE,
		0);

	/* Build list for searching */
	searchList = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
	tmpFileIndex = &firstFile;
	do {
		gtk_list_store_append(searchList, &searchListIter);

		tmpStr = g_filename_display_name(tmpFileIndex->fileName);
		gtk_list_store_set(searchList, &searchListIter,
			0, tmpFileIndex->nr + 1,
			1, tmpStr,
			-1);
		g_free(tmpStr);
	} while((tmpFileIndex = tmpFileIndex->next) != NULL);

	searchListFilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(searchList), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(searchListFilter),
		doJumpDialog_searchListFilter,
		searchEntry,
		NULL);
	
	/* Create tree view */
	searchListBox = gtk_tree_view_new_with_model(GTK_TREE_MODEL(searchListFilter));
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(searchListBox), 0);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(searchListBox), TRUE);
	searchListRenderer0 = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(searchListBox),
		-1,
		"#",
		searchListRenderer0,
		"text", 0,
		NULL);
	searchListRenderer1 = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(searchListBox),
		-1,
		"File name",
		searchListRenderer1,
		"text", 1,
		NULL);
	scrollBar = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrollBar),
		searchListBox);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dlgWindow)->vbox),
		scrollBar,
		TRUE,
		TRUE,
		0);

	/* Jump to active image */
	gotoActivePath = gtk_tree_path_new_from_indices(currentFile->nr, -1);
	gtk_tree_selection_select_path(
		gtk_tree_view_get_selection(GTK_TREE_VIEW(searchListBox)),
		gotoActivePath);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(searchListBox),
		gotoActivePath,
		NULL,
		FALSE, 0, 0);
	gtk_tree_path_free(gotoActivePath);

	/* Show dialog */
	g_signal_connect(searchEntry, "changed",
		G_CALLBACK(doJumpDialog_entryChangedCallback), searchListFilter);
	g_signal_connect(searchListBox, "key-press-event",
		G_CALLBACK(doJumpDialog_exitOnEnter), dlgWindow);
	g_signal_connect(searchListBox, "button-press-event",
		G_CALLBACK(doJumpDialog_exitOnDblClk), dlgWindow);
	gtk_widget_set_size_request(dlgWindow, 640, 480);
	gtk_widget_show_all(dlgWindow);

	if(gtk_dialog_run(GTK_DIALOG(dlgWindow)) == GTK_RESPONSE_ACCEPT) {
		gtk_tree_selection_selected_foreach(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(searchListBox)),
			doJumpDialog_openImage,
			NULL);
	}

	/* What about searchListRenderern? I don't
	 * know how (and whether) to free them :/ */
	gtk_widget_destroy(dlgWindow);
	g_object_unref(searchList);
	g_object_unref(searchListFilter);
	
} /* }}} */
/* }}} */
/* Keyboard & mouse event handlers {{{ */
gboolean mouseScrollEnabled = FALSE;
gboolean keyboardCb(GtkWidget *widget, GdkEventKey *event, gpointer data) { /*{{{*/
	/**
	 * Callback for keyboard events
	 */
	int i = 0, n = 0;
	float savedZoom;
	char *buf, *buf2;
	#ifdef DEBUG
		g_print("(%04d) %-20s Keyboard: '%c' (%d), %d\n",
			__LINE__, G_STRFUNC,
			event->keyval,
			(int)event->keyval,
			event->hardware_keycode
		);
	#endif

	if(optionHideChessboardLevel > 3) {
		return 0;
	}

	if(event->keyval < 128 && aliases[event->keyval] != 0) {
		/* Apply aliases */
		#ifdef DEBUG
		g_print("(%04d) %-20s Rewrite '%c' to '%c'\n",
			__LINE__, G_STRFUNC,
			event->keyval,
			aliases[event->keyval]
		);
		#endif
		event->keyval = aliases[event->keyval];
	}

	switch(event->keyval) {
		/* BIND: Backspace: Previous image {{{ */
		case GDK_BackSpace:
			i = currentFile->nr;
			do {
				currentFile = currentFile->prev;
				if(currentFile == NULL || currentFile->fileName == NULL) {
					currentFile = lastFile;
				}
			} while((!reloadImage()) && i != currentFile->nr);
			break;
			/* }}} */
		/* BIND: PgUp: Jump 10 images forwards {{{ */
		case GDK_Page_Up:
			jumpFiles(optionReverseMovement ? -10 : 10);
			break;
			/* }}} */
		/* BIND: PgDn: Jump 10 images backwards {{{ */
		case GDK_Page_Down:
			jumpFiles(optionReverseMovement ? 10: -10);
			break;
			/* }}} */
		/* BIND: Escape: Quit {{{ */
		case GDK_Escape:
			gtk_main_quit();
			break;
			/* }}} */
		/* BIND: Cursor keys: Move (Fullscreen) {{{ */
		case GDK_Down:
		case GDK_Up:
		case GDK_Right:
		case GDK_Left:
			if(isFullscreen) {
				if(event->keyval == GDK_Down) {
					i = (event->state & GDK_CONTROL_MASK ? 50 : 10);
				} else if(event->keyval == GDK_Up) {
					i = -(event->state & GDK_CONTROL_MASK ? 50 : 10);
				} else if(event->keyval == GDK_Right) {
					n = (event->state & GDK_CONTROL_MASK ? 50 : 10);
				} else if(event->keyval == GDK_Left) {
					n = -(event->state & GDK_CONTROL_MASK ? 50 : 10);
				}

				if(optionReverseMovement) {
					moveX -= n;
					moveY -= i;
				}
				else {
					moveX += n;
					moveY += i;
				}

				resizeAndPosWindow();
				displayImage();
			}
			break;
			/* }}} */
		/* BIND: Space: Next image {{{ */
		case GDK_space:
			jumpFiles(1);
			if(event->string[0] == 'x') {
				/* This can't occour normaly but will when called by the
				 * slideshow code
				 */
				slideshowDo();
			}
			break;
			/* }}} */
		/* BIND: j: Jump to image {{{ */
		case GDK_j:
			doJumpDialog();
			break;
			/* }}} */
		/* BIND: f: Fullscreen {{{ */
		case GDK_f:
			if(optionHideChessboardLevel < 2) {
				moveX = moveY = 0;
				setFullscreen(!isFullscreen);
				/*autoScaleFactor();
				resizeAndPosWindow();
				displayImage();*/
			}
			break;
			/* }}} */
		/* BIND: r: Reload {{{ */
		case GDK_r:
			/* If in -cc mode, preserve zoom level */
			if(optionHideChessboardLevel > 1) {
				savedZoom = zoom;
				if(!loadImage()) {
					return 0;
				}
				zoom = savedZoom;
				displayImage();
				setInfoText(NULL);
			}
			else {
				reloadImage();
			}
			break;
			/* }}} */
		/* BIND: +: Zoom in {{{ */
		case GDK_plus:
			scaleBy(event->state & GDK_CONTROL_MASK ? .2 : .05);
			resizeAndPosWindow();
			displayImage();
			setInfoText("Zoomed in");
			break;
			/* }}} */
		/* BIND: -: Zoom out {{{ */
		case GDK_minus:
			scaleBy(-(event->state & GDK_CONTROL_MASK ? .2 : .05));
			resizeAndPosWindow();
			displayImage();
			setInfoText("Zoomed out");
			break;
			/* }}} */
		/* BIND: 0: Autoscale down {{{ */
		case GDK_0:
			forceAutoScaleFactor(ON);
			moveX = moveY = 0;
			resizeAndPosWindow();
			displayImage();
			setInfoText("Autoscaled");
			break;
			/* }}} */
		/* BIND: q: Quit {{{ */
		case GDK_q:
			gtk_main_quit();
			break;
		/* }}} */
		/* BIND: t: Toggle autoscale {{{ */
		case GDK_t:
			moveX = moveY = 0;
			autoScale = (autoScale + 1) % 3;

			autoScaleFactor();
			resizeAndPosWindow();
			displayImage();

			if(autoScale == ON) {
				setInfoText("Autoscale on");
			}
			else if(autoScale == ALWAYS) {
				setInfoText("Autoscale on with scaleup enabled");
			}
			else {
				setInfoText("Autoscale off");
			}
			
			break;
		/* }}} */
		/* BIND: l: Rotate left {{{ */
		case GDK_l:
			setInfoText("Rotated left");
			rotate(TRUE);
			autoScaleFactor();
			resizeAndPosWindow();
			displayImage();
			break;
			/* }}} */
		/* BIND: k: Rotate right {{{ */
		case GDK_k:
			setInfoText("Rotated right");
			rotate(FALSE);
			autoScaleFactor();
			resizeAndPosWindow();
			displayImage();
			break;
			/* }}} */
		/* BIND: h: Horizontal flip {{{ */
		case GDK_h:
			setInfoText("Flipped horizontally");
			flip(TRUE);
			displayImage();
			break;
			/* }}} */
		/* BIND: v: Vertical flip {{{ */
		case GDK_v:
			setInfoText("Flipped vertically");
			flip(FALSE);
			displayImage();
			break;
			/* }}} */
		/* BIND: i: Show/hide info box {{{ */
		case GDK_i:
			setInfoText(NULL);
			infoBoxVisible = !infoBoxVisible;
			if(infoBoxVisible == TRUE) {
				gtk_widget_show(infoLabelBox);
			}
			else {
				gtk_widget_hide(infoLabelBox);
			}
			break;
			/* }}} */
		/* BIND: s: Slideshow toggle {{{ */
		case GDK_s:
			if(slideshowEnabled == TRUE) {
				setInfoText("Slideshow disabled");
				slideshowEnabled = FALSE;
			}
			else {
				setInfoText("Slideshow enabled");
				slideshowEnabled = TRUE;
				slideshowDo();
			}
			break;
			/* }}} */
		/* BIND: a: Hardlink current image to .qiv-select/ {{{ */
		case GDK_a:
			mkdir("./.qiv-select", 0755);
			buf2 = basename(currentFile->fileName); /* Static memory, do not free */
			buf = (char*)g_malloc(strlen(buf2) + 15);
			sprintf(buf, "./.qiv-select/%s", buf2);
			if(link(currentFile->fileName, buf) != 0) {
				/* Failed to link image, try copying it */
				if(copyFile(currentFile->fileName, buf) != 0) {
					setInfoText("Failed to save hardlink");
				}
				else {
					setInfoText("Hardlink failed, but copied file");
				}
			}
			else {
				setInfoText("Hardlink saved");
			}
			g_free(buf);
			break;
			/* }}} */
		#ifndef NO_COMMANDS
		/* BIND: <n>: Run command n (1-3) {{{ */
		case GDK_1: case GDK_2: case GDK_3: case GDK_4:
		case GDK_5: case GDK_6: case GDK_7: case GDK_8:
		case GDK_9:
			g_assert(GDK_9 - GDK_0 == 9); /* I hope this will always work.. */
			i = event->keyval - GDK_0; 
			if(optionCommands[i] != NULL) {
				buf = (char*)g_malloc(15);
				sprintf(buf, "Run command %c", event->keyval);
				setInfoText(buf);
				while(gtk_events_pending()) {
					gtk_main_iteration();
				}
				g_free(buf);
				runProgram(optionCommands[i]);
			}
			break;
			/* }}} */
		#endif
	}
	return 0;
} /*}}}*/
/* BIND: Drag & Drop: Move image (Fullscreen) and decoration switch {{{ */
gboolean mouseButtonCb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
	/**
	 * Callback for mouse events
	 */
	GdkScreen *screen; int scrx, scry, i;
	static guint32 timeOfButtonPress;
	if(event->button == 1) {
		/* Button 1 for scrolling */
		if(event->type == GDK_BUTTON_PRESS && (isFullscreen == TRUE 
			#ifndef NO_COMPOSITING
			|| optionHideChessboardLevel == 3
			#endif
			)) {
			if(isFullscreen == TRUE) {
				screen = gtk_widget_get_screen(window);
				scrx = gdk_screen_get_width(screen) / 2;
				scry = gdk_screen_get_height(screen) / 2;
				gdk_display_warp_pointer(gdk_display_get_default(),
					gdk_display_get_default_screen(gdk_display_get_default()), scrx, scry);
			}
			mouseScrollEnabled = TRUE;
		}
		#ifndef NO_COMPOSITING
		else if(event->type == GDK_BUTTON_PRESS && isFullscreen == FALSE && 
			optionHideChessboardLevel == 2) {
			/* Change decoration when started with -cc */
			gtk_window_set_decorated(GTK_WINDOW(window), 
				!gtk_window_get_decorated(GTK_WINDOW(window)));
		}
		#endif
		else if(event->type == GDK_BUTTON_RELEASE) {
			mouseScrollEnabled = FALSE;
		}
	}
	if(event->button == 3 && isFullscreen == TRUE) {
		/* BIND: Button 3/Drag: Zoom in and out in fullscreen */
		screen = gtk_widget_get_screen(window);
		scrx = gdk_screen_get_width(screen) / 2;
		scry = gdk_screen_get_height(screen) / 2;

		if(event->type == GDK_BUTTON_PRESS) {
			gdk_display_warp_pointer(gdk_display_get_default(),
				gdk_display_get_default_screen(gdk_display_get_default()),
				scrx, scry);
		}
		else if(event->type == GDK_BUTTON_RELEASE) {
			DEBUG1("Scale (Mousezoom)");
			scaleBy((float)(event->y - scry) * -0.001);
			resizeAndPosWindow();
			displayImage();
			setInfoText(NULL);
		}
	}
	/* BIND: Button 2: Quit */
	if(event->button == 2 && event->type == GDK_BUTTON_RELEASE
		#ifndef NO_COMPOSITING
		&& optionHideChessboardLevel != 4 && optionHideChessboardLevel != 2
		#endif
		) {
		gtk_main_quit();
	}
	/* BIND: Button 1/3: Next/previous image */
	if((event->button == 1 || event->button == 3)
		#ifndef NO_COMPOSITING
		&& optionHideChessboardLevel != 4 && optionHideChessboardLevel != 2
		#endif
		) {
		if(event->type == GDK_BUTTON_PRESS) {
			timeOfButtonPress = event->time;
		}
		else if(event->type == GDK_BUTTON_RELEASE && event->time - timeOfButtonPress <= 
			DRAG_MAX_TIME) {
			jumpFiles(event->button == 1 ? 1 : -1);
		}
	}
	return FALSE;
}
gint mouseMotionCb(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
	/**
	 * Callback for mouse motion
	 * (Moving & zooming)
	 */
	GdkScreen *screen; int scrx, scry;
	if(mouseScrollEnabled == FALSE) {
		return 0;
	}
	if(isFullscreen == TRUE) {
		screen = gtk_widget_get_screen(window);
		scrx = gdk_screen_get_width(screen) / 2;
		scry = gdk_screen_get_height(screen) / 2;

		// Don't perform too small movement (Improves performance a lot!)
		if(abs(event->x - scrx) + abs(event->y - scry) < 4) {
			return 0;
		}

		if(optionReverseMovement) {
			moveX -= event->x - scrx;
			moveY -= event->y - scry;
		}
		else {
			moveX += event->x - scrx;
			moveY += event->y - scry;
		}
		gdk_display_warp_pointer(gdk_display_get_default(),
			gdk_display_get_default_screen(gdk_display_get_default()), scrx, scry);

		resizeAndPosWindow();
		// displayImage();
	}
	#ifndef NO_COMPOSITING
	else if(optionHideChessboardLevel == 3) {
		/* Move when started with -ccc */
		gtk_window_get_position(GTK_WINDOW(window), &scrx, &scry);
		scrx -= gdk_pixbuf_get_width(scaledImage) / 2;
		scry -= gdk_pixbuf_get_height(scaledImage) / 2;
		gtk_window_move(GTK_WINDOW(window), (int)(scrx + event->x), (int)(scry + event->y));
	}
	#endif
	return 0;
}
/* }}} */
/* BIND: Scroll: Next/previous image {{{ */
gboolean mouseScrollCb(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
	/**
	 * Callback for scroll wheel of the mouse
	 */
	int i;
	if(event->direction > 1) {
		/* Only vertical scrolling */
		return 0;
	}
	i = currentFile->nr;
	do {
		currentFile = event->direction == 0 ? currentFile->next : currentFile->prev;
		if(currentFile == NULL) {
			currentFile = event->direction == 0 ? &firstFile : lastFile;
		}
	} while((!reloadImage()) && i != currentFile->nr);
	return FALSE;
}
/* }}} */
/* }}} */
/* Event handlers for resize stuff {{{ */
gboolean configureCb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	DEBUG1("Received configure-event");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	return FALSE;
}
gboolean screenChangedCb(GtkWidget *widget, GdkScreen *previous_screen, gpointer data) {
	DEBUG1("Received screen-changed-event");
	autoScaleFactor();
	resizeAndPosWindow();
	displayImage();
	return FALSE;
}
gboolean windowStateCb(GtkWidget *widget, GdkEventWindowState *event, gpointer data) {
	DEBUG1("Received window-state-event");
	if(event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
		autoScaleFactor();
		resizeAndPosWindow();
		displayImage();
	}
	return FALSE;
}

/* }}} */

int main(int argc, char *argv[]) {
/* Variable definitions {{{ */
	GdkColor color;
	GtkWidget *fileChooser;
	char option;
	char **envP;
	char *fileName;
	char *buf;
	gboolean optionFullScreen = FALSE;
	#ifndef NO_SORTING	
	gboolean optionSortFiles = FALSE;
	#endif
	gboolean optionReadStdin = FALSE;
	FILE *optionsFile;
	char **options;
	char **parameterIterator;
	int optionCount = 1, i;
	char **fileFormatExtensionsIterator;
	GSList *fileFormatsIterator;
	GString *stdinReader;
/* }}} */
/* glib & threads initialization {{{ */
	DEBUG1("Debug mode enabled");
	g_type_init();
	g_thread_init(NULL);
	gdk_threads_init();
	if(gtk_init_check(&argc, &argv) == FALSE) {
		die("Failed to open X11 Display.");
	}
	g_get_current_time(&programStart);
/* }}} */
	/* Command line and configuration parsing {{{ */
	envP = environ;
	options = (char**)g_malloc((argc + 251) * sizeof(char*));
	options[0] = argv[0];
	#ifndef NO_CONFIG_FILE
	while((buf = *envP++) != NULL) {
		if(strncmp(buf, "HOME=", 5) != 0) {
			continue;
		}
		fileName = (char*)g_malloc(strlen(buf + 5) + 9);
		sprintf(fileName, "%s/.pqivrc", buf + 5);
		optionsFile = fopen(fileName, "r");
		
		if(optionsFile) {
			while((option = fgetc(optionsFile)) != EOF) {
				if(optionCount > 250) {
					die("Too many options; your configuration file "
						"is restricted to 250 options");
				}
				if(option < 33) {
					/* Ignore control characters / whitespace */
					continue;
				}
				options[optionCount] = (char*)g_malloc(1024);
				i = 0;
				do {
					if(option < 33) {
						break;
					}
					options[optionCount][i++] = option;
					if(i == 1024) {
						die("An option in ~/.pqivrc is too long");
					}
				} while((option = fgetc(optionsFile)) != EOF);
				options[optionCount][i] = 0;
				options[optionCount] = g_realloc(options[optionCount], i);
				optionCount++;
			}
			fclose(optionsFile);
		}
		g_free(fileName);
		break;
	}
	#endif
	for(i=1; i<argc; i++) {
		g_assert(optionCount <= (argc + 250));
		options[optionCount] = argv[i];
		optionCount++;
	}
	options[optionCount] = 0;

	memset(aliases, 0, sizeof(aliases));
	opterr = 0;
	while((option = getopt(optionCount, options, "ifFsSRnthrcwqkz:P:p:d:a:1:2:3:4:5:6:7:8:9:")) > 0) {
		switch(option) {
			/* OPTION: -i: Hide info box */
			case 'i':
				optionHideInfoBox = TRUE;	
				break;
			/* OPTION: -f: Start in fullscreen mode */
			case 'f':
				optionFullScreen = TRUE;
				break;
			#ifndef NO_FADING
			/* OPTION: -F: Fade between images */
			case 'F':
				optionFadeImages = TRUE;
				break;
			#endif
			/* OPTION: -s: Activate slideshow */
			case 's':
				slideshowEnabled = TRUE;
				break;
			/* OPTION: -S: Follow symlinks */
			case 'S':
				optionFollowSymlinks = TRUE;
				break;
			/* OPTION: -R: Reverse meaning of cursor keys and Page Up/Down */
			case 'R':
				optionReverseMovement = !optionReverseMovement;
				break;
			#ifndef NO_SORTING
			/* OPTION: -n: Sort all files in natural order */
			case 'n':
				optionSortFiles = TRUE;
				break;
			#endif
			/* OPTION: -d n: Slideshow interval */
			case 'd':
				slideshowInterval = atoi(optarg);
				if(slideshowInterval < 1) {
					g_printerr("The interval for the slideshow should "
						"be at least 1 second\n");
					exit(1);
				}
				break;
			/* OPTION: -t: Scale images up to fill the whole screen */
			/* ADD: Use twice to deactivate scaling completely */
			case 't':
				autoScale = (autoScale + 1) % 3;
				break;
			/* OPTION: -r: Read additional filenames (not folders) from stdin */
			case 'r':
				optionReadStdin = TRUE;
				memoryArgImage = (GdkPixbuf*)1; /* Don't allow - files */
				break;
			/* OPTION: -c: Disable the background for transparent images */
			#ifndef NO_COMPOSITING
			/* ADD: See manpage for what happens if you use this option more than once */
			#endif
			case 'c':
				#ifndef NO_COMPOSITING
				if(optionHideChessboardLevel < 5) {
					optionHideChessboardLevel++;
				}
				#else
				optionHideChessboardLevel = 1;
				#endif
				break;
			#ifndef NO_INOTIFY
			/* OPTION: -w: Watch files for changes */
			case 'w':
				optionUseInotify = TRUE;
				break;
			#endif
			/* OPTION: -z n: Set initial zoom level */
			case 'z':
				optionInitialZoom = (float)atof(optarg) / 100;
				if(optionInitialZoom < 0.01f || optionInterpolation > 10) {
					g_printerr("Please choose a senseful zoom level (Between "
						"1 and 1000%%).\n");
					exit(1);
				}
				break;
			/* OPTION: -p: Interpolation quality level (1-4, defaults to 3) */
			case 'p':
				switch(*optarg) {
					case '1': optionInterpolation = GDK_INTERP_NEAREST; break;
					case '2': optionInterpolation = GDK_INTERP_TILES; break;
					case '3': optionInterpolation = GDK_INTERP_BILINEAR; break;
					case '4': optionInterpolation = GDK_INTERP_HYPER; break;
					default:  helpMessage(0);
				}
				break;
			/* OPTION: -P: Set initial window position. Use: */
			/* ADD: x,y   to place the window */
			/* ADD: 'off' will deactivate window positioning */
			/* ADD: Default behaviour is to center the window */
			case 'P':
				if(strncmp(optarg, "off", 4) == 0) {
					optionWindowPosition[2] = 0;
				}
				else {
					buf = index(optarg, ',');
					if(buf == NULL) {
						die("Syntax for -P is 'left,top'");
					}
					*buf = 0;
					buf++;
					optionWindowPosition[0] = atoi(optarg);
					optionWindowPosition[1] = atoi(buf);
					optionWindowPosition[2] = 1;
					buf = NULL;
				}
				break;
			/* OPTION: -a nf: Define n as a keyboard alias for f */
			case 'a':
				if((unsigned char)optarg[0] > 128) {
					die("Can't define aliases for non ASCII characters.");
				}
				aliases[(unsigned int)optarg[0]] = optarg[1];
				break;
			#ifndef NO_KENBURNS
			/* OPTION: -k: Use kenburns effect in fullscreen slideshows */
			/* ADD: Use twice to activate scaling (Slow!) */
			case 'k':
				optionUseKenburns = (optionUseKenburns == NO) ? YES : WITH_SCALING;
				break;
			#endif
			#ifndef NO_COMMANDS
			/* OPTION: -<n> s: Set command number n (1-9) to s */
			/* ADD: See manpage for advanced commands (starting with > or |) */
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8':
			case '9':
				i = option - '0';
				if(optionCommands[i] != NULL) {
					g_free(optionCommands[i]);
				}
				optionCommands[i] = (char*)g_malloc(strlen(optarg) + 1);
				strcpy(optionCommands[i], optarg);
				break;
			/* OPTION: -q: Use the qiv-command script for commands */
			case 'q':
				for(i=0; i<10; i++) {
					if(optionCommands[i] != NULL) {
						g_free(optionCommands[i]);
					}
					optionCommands[i] = (char*)g_malloc(14);
					memcpy(optionCommands[i], "qiv-command 0", 14);
					optionCommands[i][12] += i;
				}
				break;
			#endif
			case '?':
				helpMessage(optopt);
			case 'h':
			default:
				helpMessage(0);
		}
	}
	/* }}} */
	/* Load files {{{ */
	/* Load available file formats */
	fileFormatsFilter = gtk_file_filter_new();
	gtk_file_filter_set_name(fileFormatsFilter, "Images");
	gtk_file_filter_add_pixbuf_formats(fileFormatsFilter);
	fileFormatsIterator = gdk_pixbuf_get_formats();
	do {
		fileFormatExtensionsIterator = gdk_pixbuf_format_get_extensions(
			fileFormatsIterator->data);
		while(*fileFormatExtensionsIterator != NULL) {
			buf = (char*)g_malloc(strlen(*fileFormatExtensionsIterator) + 3);
			sprintf(buf, "*.%s", *fileFormatExtensionsIterator);
			gtk_file_filter_add_pattern(fileFormatsFilter, buf);
			g_free(buf);
			++fileFormatExtensionsIterator;
		}
	} while((fileFormatsIterator = g_slist_next(fileFormatsIterator)) != NULL);
	g_slist_free(fileFormatsIterator);
	parameterIterator = options + optind;
	/* Load files */
	memset(&firstFile, 0, sizeof(struct file));
	if(argv[0] != 0) {
		if(optionFollowSymlinks == TRUE) {
			/* Create tree for recursion checking */
			recursionCheckTree = g_tree_new_full((GCompareDataFunc)strcmp,
				NULL, g_free, NULL);
				/* Free the keys (which are strings) but not the values
				 * (which are pointers to 0x1) upon destruction
				 */
		}
		loadFiles(parameterIterator);
		#ifdef SECONDS_TILL_LOADING_INFO
		if(loadFilesChecked == 2) {
			g_print("\033[2K\r");
		}
		#endif
		if(optionFollowSymlinks == TRUE) {
			/* Destroy it again */
			g_tree_destroy(recursionCheckTree);
		}
	}
	if(optionReadStdin == TRUE) {
		stdinReader = g_string_new(NULL);
		do {
			option = getchar();
			if(option == EOF) {
				break;
			}
			if(option == '\n' || option == '\r') {
				loadFilesAddFile(stdinReader->str);
				g_string_truncate(stdinReader, 0);
				continue;
			}
			g_string_append_c(stdinReader, option);
		} while(TRUE);
		g_string_free(stdinReader, TRUE);
	}
	#ifndef NO_SORTING
	if(optionSortFiles == TRUE) {
		sortFiles();
	}
	#endif
	if(currentFile->fileName == NULL) {
		if(options[optind] == 0) {
			/* No images given */
			if(!isatty(0)) {
				/* If stdin is no TTY, show GTK+ load file dialog */
				fileChooser = gtk_file_chooser_dialog_new("Open image(s)..",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
				gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fileChooser),
					fileFormatsFilter);
				gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(fileChooser),
					TRUE);
				if(gtk_dialog_run(GTK_DIALOG(fileChooser)) != GTK_RESPONSE_ACCEPT) {
					return 1;
				}
				/* Reuse fileFormatsIterator for this file list,
				 * don't let the name confuse you ;) */
				fileFormatsIterator = gtk_file_chooser_get_filenames(
					GTK_FILE_CHOOSER(fileChooser));
				do {
					loadFilesAddFile(fileFormatsIterator->data);
					g_free(fileFormatsIterator->data);
				} while((fileFormatsIterator = g_slist_next(fileFormatsIterator)) != NULL);
				g_slist_free(fileFormatsIterator);
				gtk_widget_destroy(fileChooser);
			}
			else {
				return 0;
			}
		}
		else {
			die("Failed to load any of the images");
		}
	}
	g_free(options);
	while(!loadImage()) {
		currentFile = currentFile->next;
		if(currentFile == NULL) {
			die("Failed to load any of the images");
		}
	}
	/* }}} */
	/* Initialize gtk {{{ */
	/* Create gtk window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	if(!window) {
		die("Failed to create a window");
	}
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_widget_set_size_request(window, 640, 480);
	gtk_window_set_title(GTK_WINDOW(window), "pqiv");
	#ifndef NO_COMPOSITING
	if(optionHideChessboardLevel > 1) {
		gtk_widget_set_app_paintable(window, TRUE);
		alphaScreenChangedCb(window, NULL, NULL);
		gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	}
	#endif
	gtk_window_set_icon(GTK_WINDOW(window),
		gdk_pixbuf_new_from_inline(348, (const guint8 *)appIcon, FALSE, NULL));
	gtk_widget_show(window);
	fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(window), fixed);
	gtk_widget_show(fixed);

	/* Create image widget (with alpha?) */
	imageWidget = gtk_image_new();
	color.red = 0; color.green = 0; color.blue = 0;
	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &color);
	#ifndef NO_COMPOSITING
	if(optionHideChessboardLevel > 1) {
		gtk_widget_set_app_paintable(imageWidget, TRUE);
		alphaScreenChangedCb(imageWidget, NULL, NULL);
	}
	#endif
	gtk_fixed_put(GTK_FIXED(fixed), imageWidget, 0, 0);
	gtk_widget_show(imageWidget);

	/* Info label */
	infoLabelBox = gtk_event_box_new();
	infoLabel = gtk_label_new(NULL);
	color.red = 0xee * 255; color.green = 0xee * 255; color.blue = 0x55 * 255;
	gtk_widget_modify_bg(GTK_WIDGET(infoLabelBox), GTK_STATE_NORMAL, &color);
	gtk_widget_modify_font(infoLabel, pango_font_description_from_string("sansserif 9"));
	gtk_label_set_text(GTK_LABEL(infoLabel), "");
	gtk_misc_set_padding(GTK_MISC(infoLabel), 2, 2);
	gtk_container_add(GTK_CONTAINER(infoLabelBox), infoLabel);
	gtk_fixed_put(GTK_FIXED(fixed), infoLabelBox, 10, 10);
	gtk_widget_show(infoLabel);
	if(optionHideInfoBox != TRUE) {
		infoBoxVisible = TRUE;
		gtk_widget_show(infoLabelBox);
	}

	/* Event box for the mouse */
	mouseEventBox = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(mouseEventBox), FALSE);
	gtk_fixed_put(GTK_FIXED(fixed), mouseEventBox, 0, 0);
	gtk_widget_show(mouseEventBox);

	/* Signalling stuff */
	#ifndef NO_COMPOSITING
	if(optionHideChessboardLevel > 1) {
		g_signal_connect(window, "expose-event",
			G_CALLBACK(exposeCb), NULL);
		g_signal_connect(window, "screen-changed",
			G_CALLBACK(alphaScreenChangedCb), NULL);
		g_signal_connect(imageWidget, "expose-event",
			G_CALLBACK(exposeCb), NULL);
		g_signal_connect(imageWidget, "screen-changed",
			G_CALLBACK(alphaScreenChangedCb), NULL);
	}
	#endif
	g_signal_connect(window, "key-press-event",
		G_CALLBACK(keyboardCb), NULL);
	g_signal_connect(mouseEventBox, "button-press-event",
		G_CALLBACK(mouseButtonCb), NULL);
	g_signal_connect(mouseEventBox, "button-release-event",
		G_CALLBACK(mouseButtonCb), NULL);
	g_signal_connect(mouseEventBox, "scroll-event",
		G_CALLBACK(mouseScrollCb), NULL);
	g_signal_connect(mouseEventBox, "motion-notify-event",
		G_CALLBACK(mouseMotionCb), NULL);
	g_signal_connect(window, "destroy",
		G_CALLBACK(gtk_main_quit),
	        &window);
	g_signal_connect(window, "configure-event",
		G_CALLBACK(configureCb), NULL);
	g_signal_connect(window, "screen-changed",
		G_CALLBACK(screenChangedCb), NULL);
	g_signal_connect(window, "window-state-event",
		G_CALLBACK(windowStateCb), NULL);
	/* }}} */
	/* Initialize other stuff {{{ */

	#ifndef NO_KENBURNS
	/* Initialize kenburns structure */
	memset(&kenburnsData, 0, sizeof(kenburnsData));
	#endif

	#ifndef NO_INOTIFY
	/* Initialize inotify */
	if(optionUseInotify == TRUE) {
		DEBUG1("Using inotify");
		inotifyFd = inotify_init();
		if(inotifyFd == -1) {
			g_printerr("Inotify is not supported on your system\n");
			optionUseInotify = FALSE;
		}
		else {
			gdk_input_add(inotifyFd, GDK_INPUT_READ, inotifyCb, NULL);
			inotifyWd = inotify_add_watch(inotifyFd, currentFile->fileName, IN_CLOSE_WRITE);
		}
	}
	#endif

	/* Hide from taskbar and force to background when started with -ccc */
	#ifndef NO_COMPOSITING
	if(optionHideChessboardLevel > 2) {
		gtk_window_stick(GTK_WINDOW(window));
		gtk_window_set_keep_below(GTK_WINDOW(window), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
		gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
	}
	#endif

	/* }}} */
	/* Load first image {{{ */
	if(optionFullScreen == TRUE) {
		g_timeout_add(200, toFullscreenCb, NULL);
	}
	autoScaleFactor();
	resizeAndPosWindow();
	displayImage();
	if(slideshowEnabled == TRUE) {
		slideshowDo();
	}
	setInfoText(NULL);
	/* }}} */
	gtk_main();
	return 0;
}
