--- config.def.h.orig	2020-09-02 16:31:23 UTC
+++ config.def.h
@@ -2,22 +2,46 @@
 /* Default settings; can be overriden by command line. */
 
 static int topbar = 1;                      /* -b  option; if 0, dmenu appears at bottom     */
+static int fuzzy = 1;                      /* -F  option; if 0, dmenu doesn't use fuzzy matching     */
 /* -fn option overrides fonts[0]; default X11 font or font set */
 static const char *fonts[] = {
-	"monospace:size=10"
+  "Source Code Pro:size=12:antialias=true:autohint=true"
 };
 static const char *prompt      = NULL;      /* -p  option; prompt to the left of input field */
+static const char col_black[]       = "#000000";
+static const char col_white[]       = "#ffffff";
+static const char col_gray1[]       = "#282828";
+static const char col_gray2[]       = "#3C3836";
+static const char col_gray3[]       = "#504945";
+static const char col_gray4[]       = "#665c54";
+static const char col_cream0[]      = "#eddbb2";
+static const char col_cream1[]      = "#fbf1c7";
+static const char col_aqua[]        = "#689d6a";
+static const char col_cyan0[]       = "#458588";
+static const char col_cyan1[]       = "#076678";
+static const char col_green[]       = "#98971a";
+static const char col_orange[]      = "#d65d0e";
+static const char col_purple[]      = "#b16286";
+static const char col_red[]         = "#cc241d";
+static const char col_yellow[]      = "#d79921";
 static const char *colors[SchemeLast][2] = {
 	/*     fg         bg       */
-	[SchemeNorm] = { "#bbbbbb", "#222222" },
-	[SchemeSel] = { "#eeeeee", "#005577" },
-	[SchemeOut] = { "#000000", "#00ffff" },
+	[SchemeNorm] = { col_cream1, col_gray1 },
+	[SchemeSel] = { col_cream1, col_cyan1 },
+	[SchemeSelHighlight] = { col_cream1, col_yellow },
+	[SchemeNormHighlight] = { col_cream1, col_purple },
+	[SchemeOut] = { col_gray1, col_cyan0 },
 };
 /* -l option; if nonzero, dmenu uses vertical list with given number of lines */
 static unsigned int lines      = 0;
+static unsigned int maxhist    = 15;
+static int histnodup           = 1;	/* if 0, record repeated histories */
 
 /*
  * Characters not considered part of a word while deleting words
  * for example: " /?\"&[]"
  */
 static const char worddelimiters[] = " ";
+
+/* -n option; preselected item starting from 0 */
+static unsigned int preselected = 0;
