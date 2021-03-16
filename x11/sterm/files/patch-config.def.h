--- config.def.h.orig	2020-06-19 09:29:45 UTC
+++ config.def.h
@@ -1,3 +1,4 @@
+char *externalpipe_sigusr1[] = {"/bin/sh", "-c", "externalpipe_buffer.sh st_strings_read"};
 /* See LICENSE file for copyright and license details. */
 
 /*
@@ -5,7 +6,20 @@
  *
  * font: see http://freedesktop.org/software/fontconfig/fontconfig-user.html
  */
-static char *font = "Liberation Mono:pixelsize=12:antialias=true:autohint=true";
+static char *font = "Source Code Pro:size=12:antialias=true:autohint=true";
+/* Spare fonts */
+static char *font2[] = {
+  "JoyPixels:pixelsize=14:antialias=true:autohint=true"
+  "Inconsolata for Powerline:pixelsize=14:antialias=true:autohint=true",
+  "Hack Nerd Font Mono:pixelsize=14:antialias=true:autohint=true",
+};
+
+
+/* disable bold, italic and roman fonts globally */
+int disablebold = 0;
+int disableitalic = 0;
+int disableroman = 0;
+
 static int borderpx = 2;
 
 /*
@@ -68,6 +82,18 @@ static unsigned int blinktimeout = 800;
 static unsigned int cursorthickness = 2;
 
 /*
+ * 1: render most of the lines/blocks characters without using the font for
+ *    perfect alignment between cells (U2500 - U259F except dashes/diagonals).
+ *    Bold affects lines thickness if boxdraw_bold is not 0. Italic is ignored.
+ * 0: disable (render all U25XX glyphs normally from the font).
+ */
+const int boxdraw = 1;
+const int boxdraw_bold = 1;
+
+/* braille (U28XX):  1: render as adjacent "pixels",  0: use font */
+const int boxdraw_braille = 1;
+
+/*
  * bell volume. It must be a value between -100 and 100. Use 0 for disabling
  * it
  */
@@ -93,53 +119,56 @@ char *termname = "st-256color";
  */
 unsigned int tabspaces = 8;
 
+/* bg opacity */
+float alpha = 0.93;
+
 /* Terminal colors (16 first used in escape sequence) */
 static const char *colorname[] = {
-	/* 8 normal colors */
-	"black",
-	"red3",
-	"green3",
-	"yellow3",
-	"blue2",
-	"magenta3",
-	"cyan3",
-	"gray90",
+  /* 8 normal colors */
+  [0] = "#282828", /* hard contrast: #1d2021 / soft contrast: #32302f */
+  [1] = "#cc241d", /* red     */
+  [2] = "#98971a", /* green   */
+  [3] = "#d79921", /* yellow  */
+  [4] = "#458588", /* blue    */
+  [5] = "#b16286", /* magenta */
+  [6] = "#689d6a", /* cyan    */
+  [7] = "#a89984", /* white   */
 
-	/* 8 bright colors */
-	"gray50",
-	"red",
-	"green",
-	"yellow",
-	"#5c5cff",
-	"magenta",
-	"cyan",
-	"white",
-
-	[255] = 0,
-
-	/* more colors can be added after 255 to use with DefaultXX */
-	"#cccccc",
-	"#555555",
+  /* 8 bright colors */
+  [8]  = "#928374", /* black   */
+  [9]  = "#fb4934", /* red     */
+  [10] = "#b8bb26", /* green   */
+  [11] = "#fabd2f", /* yellow  */
+  [12] = "#83a598", /* blue    */
+  [13] = "#d3869b", /* magenta */
+  [14] = "#8ec07c", /* cyan    */
+  [15] = "#ebdbb2", /* white   */
 };
 
-
 /*
  * Default colors (colorname index)
- * foreground, background, cursor, reverse cursor
+ * foreground, background, cursor
  */
-unsigned int defaultfg = 7;
+unsigned int defaultfg = 15;
 unsigned int defaultbg = 0;
-static unsigned int defaultcs = 256;
+static unsigned int defaultcs = 15;
 static unsigned int defaultrcs = 257;
 
 /*
- * Default shape of cursor
- * 2: Block ("█")
- * 4: Underline ("_")
- * 6: Bar ("|")
- * 7: Snowman ("☃")
+ * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps-SP-q.1D81
+ * Default style of cursor
+ * 0: Blinking block
+ * 1: Blinking block (default)
+ * 2: Steady block ("█")
+ * 3: Blinking underline
+ * 4: Steady underline ("_")
+ * 5: Blinking bar
+ * 6: Steady bar ("|")
+ * 7: Blinking st cursor
+ * 8: Steady st cursor
  */
-static unsigned int cursorshape = 2;
+static unsigned int cursorstyle = 1;
+static Rune stcursor = 0x2603; /* snowman (U+2603) */
 
 /*
  * Default columns and rows numbers
@@ -172,9 +201,12 @@ static uint forcemousemod = ShiftMask;
  * Internal mouse shortcuts.
  * Beware that overloading Button1 will disable the selection.
  */
+const unsigned int mousescrollincrement = 4;
 static MouseShortcut mshortcuts[] = {
 	/* mask                 button   function        argument       release */
-	{ XK_ANY_MOD,           Button2, selpaste,       {.i = 0},      1 },
+	{ XK_ANY_MOD,           Button4, kscrollup,      {.i = mousescrollincrement},      0, /* !alt */ -1 },
+	{ XK_ANY_MOD,           Button5, kscrolldown,    {.i = mousescrollincrement},      0, /* !alt */ -1 },
+	{ XK_ANY_MOD,           Button2, clippaste,      {.i = 0},      1 },
 	{ ShiftMask,            Button4, ttysend,        {.s = "\033[5;2~"} },
 	{ XK_ANY_MOD,           Button4, ttysend,        {.s = "\031"} },
 	{ ShiftMask,            Button5, ttysend,        {.s = "\033[6;2~"} },
@@ -191,14 +223,19 @@ static Shortcut shortcuts[] = {
 	{ ControlMask,          XK_Print,       toggleprinter,  {.i =  0} },
 	{ ShiftMask,            XK_Print,       printscreen,    {.i =  0} },
 	{ XK_ANY_MOD,           XK_Print,       printsel,       {.i =  0} },
-	{ TERMMOD,              XK_Prior,       zoom,           {.f = +1} },
-	{ TERMMOD,              XK_Next,        zoom,           {.f = -1} },
+	{ TERMMOD,              XK_Up,          zoom,           {.f = +1} },
+	{ TERMMOD,              XK_Down,        zoom,           {.f = -1} },
 	{ TERMMOD,              XK_Home,        zoomreset,      {.f =  0} },
-	{ TERMMOD,              XK_C,           clipcopy,       {.i =  0} },
-	{ TERMMOD,              XK_V,           clippaste,      {.i =  0} },
-	{ TERMMOD,              XK_Y,           selpaste,       {.i =  0} },
-	{ ShiftMask,            XK_Insert,      selpaste,       {.i =  0} },
+	{ MODKEY|ShiftMask,     XK_Return,      newterm,        {.i =  0} },
+	{ MODKEY|ControlMask,   XK_c,           clipcopy,       {.i =  0} },
+	{ MODKEY|ControlMask,   XK_v,           clippaste,      {.i =  0} },
 	{ TERMMOD,              XK_Num_Lock,    numlock,        {.i =  0} },
+	{ MODKEY|ControlMask,   XK_l,           copyurl,        {.i =  0} },
+	{ MODKEY|ControlMask,   XK_o,           opencopied,     {.v = "xdg-open"} },
+	{ MODKEY|ControlMask,   XK_u,           kscrollup,      {.i = -1} },
+	{ MODKEY|ControlMask,   XK_d,           kscrolldown,    {.i = -1} },
+	{ ShiftMask,            XK_Page_Up,     kscrollup,      {.i = -1} },
+	{ ShiftMask,            XK_Page_Down,   kscrolldown,    {.i = -1} },
 };
 
 /*
