--- config.def.h.orig	2019-02-02 12:55:28 UTC
+++ config.def.h
@@ -1,23 +1,53 @@
 /* See LICENSE file for copyright and license details. */
 
+#include <X11/XF86keysym.h>
+
 /* appearance */
-static const unsigned int borderpx  = 1;        /* border pixel of windows */
+static const unsigned int borderpx  = 3;        /* border pixel of windows */
+static const unsigned int gappx     = 6;        /* gaps between windows */
 static const unsigned int snap      = 32;       /* snap pixel */
+static const unsigned int systraypinning = 1;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
+static const unsigned int systrayspacing = 2;   /* systray spacing */
+static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
+static const int showsystray        = 1;     /* 0 means no systray */
 static const int showbar            = 1;        /* 0 means no bar */
 static const int topbar             = 1;        /* 0 means bottom bar */
-static const char *fonts[]          = { "monospace:size=10" };
-static const char dmenufont[]       = "monospace:size=10";
-static const char col_gray1[]       = "#222222";
-static const char col_gray2[]       = "#444444";
-static const char col_gray3[]       = "#bbbbbb";
-static const char col_gray4[]       = "#eeeeee";
-static const char col_cyan[]        = "#005577";
+static const char *fonts[]          = { "Source Code Pro:size=12:antialias=true:autohint=true" };
+static const char dmenufont[]       = "Source Code Pro:size=12:antialias=true:autohint=true";
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
 static const char *colors[][3]      = {
 	/*               fg         bg         border   */
-	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
-	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
+	[SchemeNorm] = { col_cream0, col_gray1, col_gray2 },
+	[SchemeSel]  = { col_cream1, col_cyan1, col_yellow },
 };
 
+typedef struct {
+	const char *name;
+	const void *cmd;
+} Sp;
+const char *spcmd1[] = {"st", "-n", "spterm", "-g", "120x34", NULL };
+const char *spcmd2[] = {"st", "-n", "spfm", "-g", "144x41", "-e", "ranger", NULL };
+const char *spcmd3[] = {"st", "-n", "spcalc", "-g", "80x30", "-e", "qalc", NULL };
+static Sp scratchpads[] = {
+	/* name          cmd  */
+	{"spterm",      spcmd1},
+	{"spranger",    spcmd2},
+	{"spcalc",      spcmd3},
+};
+
 /* tagging */
 static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
 
@@ -26,29 +56,41 @@ static const Rule rules[] = {
 	 *	WM_CLASS(STRING) = instance, class
 	 *	WM_NAME(STRING) = title
 	 */
-	/* class      instance    title       tags mask     isfloating   monitor */
-	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
-	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
+	/* class             instance          title           tags mask  iscenter  isfloating  monitor */
+	{ "Gimp",            NULL,             NULL,           0,         0,         1,         -1 },
+	{ "Google-chrome", "google-chrome",    NULL,           1 << 1,    0,         0,         -1 },
+	{ "firefox",         "Navigator",      NULL,           1 << 1,    0,         0,         -1 },
+	{ "Emacs",           NULL,             NULL,           1 << 2,    0,         0,         -1 },
+	{ "Signal",          "signal",         NULL,           1 << 3,    0,         0,         -1 },
+	{ "Microsoft Teams - Preview", NULL,   NULL,           1 << 4,    0,         0,         -1 },
+	{ "Vmware",          "vmware",         NULL,           1 << 5,    0,         0,         -1 },
+	{ NULL,              "youtube-mpv",    NULL,           0,         0,         1,         -1 },
+	{ "St",              NULL,             NULL,           0,         0,         0,         -1 },
+	{ "st-256color",     NULL,             NULL,           0,         0,         0,         -1 },
+	{ NULL,              NULL,             "Event Tester", 0,         0,         1,         -1 }, /* xev */
+	{ NULL,              "spterm",         NULL,           SPTAG(0),  0,         1,         -1 },
+	{ NULL,              "spfm",           NULL,           SPTAG(1),  0,         1,         -1 },
+	{ NULL,              "spcalc",         NULL,           SPTAG(2),  0,         1,         -1 },
 };
 
 /* layout(s) */
 static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
 static const int nmaster     = 1;    /* number of clients in master area */
-static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
+static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
 
 static const Layout layouts[] = {
 	/* symbol     arrange function */
 	{ "[]=",      tile },    /* first entry is default */
-	{ "><>",      NULL },    /* no layout function means floating behavior */
 	{ "[M]",      monocle },
+	{ "><>",      NULL },    /* no layout function means floating behavior */
 };
 
 /* key definitions */
 #define MODKEY Mod1Mask
 #define TAGKEYS(KEY,TAG) \
-	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
+	{ MODKEY,                       KEY,      comboview,      {.ui = 1 << TAG} }, \
 	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
-	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
+	{ MODKEY|ShiftMask,             KEY,      combotag,       {.ui = 1 << TAG} }, \
 	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },
 
 /* helper for spawning shell commands in the pre dwm-5.0 fashion */
@@ -56,34 +98,42 @@ static const Layout layouts[] = {
 
 /* commands */
 static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
-static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
+static const char *dmenucmd[] = { "dmenu_run_history", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_cream1, "-sb", col_cyan1, "-sf", col_cream1, topbar ? NULL : "-b", NULL };
 static const char *termcmd[]  = { "st", NULL };
+static const char *clipmenucmd[]  = { "clipmenu", NULL };
+static const char *filecmd[]  = { "st", "-e", "ranger", NULL };
 
 static Key keys[] = {
 	/* modifier                     key        function        argument */
-	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
-	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
+	{ MODKEY,                       XK_d,      spawn,          {.v = dmenucmd } },
+	{ MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
 	{ MODKEY,                       XK_b,      togglebar,      {0} },
 	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
 	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
-	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
-	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
+	{ MODKEY|ShiftMask,             XK_i,      incnmaster,     {.i = +1 } },
+	{ MODKEY|ShiftMask,             XK_d,      incnmaster,     {.i = -1 } },
 	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
 	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
-	{ MODKEY,                       XK_Return, zoom,           {0} },
-	{ MODKEY,                       XK_Tab,    view,           {0} },
-	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
-	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
-	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
-	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
-	{ MODKEY,                       XK_space,  setlayout,      {0} },
-	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
-	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
-	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
+	{ MODKEY,                       XK_f,      togglefullscr,  {0} },
+	{ MODKEY,                       XK_space,  zoom,           {0} },
+	{ MODKEY,                       XK_Tab,    comboview,      {0} },
+	{ MODKEY,                       XK_q,      killclient,     {0} },
+	{ MODKEY|ShiftMask,             XK_t,      setlayout,      {.v = &layouts[0]} },
+	{ MODKEY|ShiftMask,             XK_m,      setlayout,      {.v = &layouts[1]} },
+	{ MODKEY|ShiftMask,             XK_f,      setlayout,      {.v = &layouts[2]} },
+	{ MODKEY|ControlMask,           XK_f,      togglefloating, {0} },
+	{ MODKEY,                       XK_s,      togglesticky,   {0} },
+	{ MODKEY,                       XK_0,      comboview,      {.ui = ~0 } },
+	{ MODKEY|ShiftMask,             XK_0,      combotag,       {.ui = ~0 } },
+	{ ControlMask|Mod4Mask,         XK_Left,   shiftview,      {.i = -1 } },
+	{ ControlMask|Mod4Mask,         XK_Right,  shiftview,      {.i = +1 } },
 	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
 	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
 	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
 	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
+	{ MODKEY,                       XK_t,      togglescratch,  {.ui = 0 } },
+	{ MODKEY,                       XK_u,      togglescratch,  {.ui = 1 } },
+	{ 0,                            XF86XK_Calculator,         togglescratch,   {.ui = 2 } },
 	TAGKEYS(                        XK_1,                      0)
 	TAGKEYS(                        XK_2,                      1)
 	TAGKEYS(                        XK_3,                      2)
@@ -93,6 +143,33 @@ static Key keys[] = {
 	TAGKEYS(                        XK_7,                      6)
 	TAGKEYS(                        XK_8,                      7)
 	TAGKEYS(                        XK_9,                      8)
+	{ MODKEY,                       XK_o,      spawn,          SHCMD("org-capture") },
+	{ MODKEY|ShiftMask,             XK_r,      spawn,          {.v = filecmd } },
+	{ MODKEY|ShiftMask,             XK_s,      spawn,          SHCMD("$HOME/bin/dmenussh") },
+	{ MODKEY,                       XK_c,      spawn,          {.v = clipmenucmd } },
+	{ MODKEY|ShiftMask,             XK_v,      spawn,          SHCMD("$HOME/bin/dmenu-mullvad.sh") },
+	{ MODKEY,                       XK_y,      spawn,          SHCMD("$HOME/bin/youtube-watch.sh") },
+	{ MODKEY|ShiftMask,             XK_y,      spawn,          SHCMD("$HOME/bin/yt_clipboard.sh") },
+	{ MODKEY|ShiftMask,             XK_e,      spawn,          SHCMD("emacsclient -c -a 'emacs'") },
+	{ MODKEY|ShiftMask,             XK_w,      spawn,          SHCMD("$BROWSER") },
+	{ 0,        XF86XK_MonBrightnessDown,      spawn,          SHCMD("xbacklight -dec 10") },
+	{ 0,          XF86XK_MonBrightnessUp,      spawn,          SHCMD("xbacklight -inc 10") },
+	{ 0,                 XF86XK_AudioMute,     spawn,          SHCMD("pactl -- set-sink-mute @DEFAULT_SINK@ toggle") },
+	{ 0,          XF86XK_AudioLowerVolume,     spawn,          SHCMD("pactl -- set-sink-volume @DEFAULT_SINK@ -5%") },
+	{ 0,          XF86XK_AudioRaiseVolume,     spawn,          SHCMD("pactl -- set-sink-volume @DEFAULT_SINK@ +5%") },
+	{ MODKEY,            XF86XK_AudioMute,     spawn,          SHCMD("playerctl play-pause") },
+	{ MODKEY,     XF86XK_AudioLowerVolume,     spawn,          SHCMD("playerctl previous") },
+	{ MODKEY,     XF86XK_AudioRaiseVolume,     spawn,          SHCMD("playerctl next") },
+	{ MODKEY,                       XK_F8,     spawn,          SHCMD("$HOME/bin/launch_apps.sh") },
+	{ MODKEY,                       XK_F9,     spawn,          SHCMD("$HOME/bin/screen_script.sh") },
+	{ MODKEY,                       XK_F10,    spawn,          SHCMD("$HOME/bin/screen_script.sh -a") },
+	{ MODKEY,                       XK_F11,    spawn,          SHCMD("$HOME/bin/screen_script.sh -m") },
+	{ MODKEY,                       XK_Home,   spawn,          SHCMD("$HOME/bin/go_home.sh") },
+	{ MODKEY,                       XK_End,    spawn,          SHCMD("$HOME/bin/go_to_work.sh") },
+	{ MODKEY,                       XK_F12,    spawn,          SHCMD("$HOME/bin/connect_airpods.sh") },
+	{ MODKEY|ShiftMask,             XK_l,      spawn,          SHCMD("i3lock-fancy-rapid 5 2 -u") },
+	{ MODKEY|ControlMask,           XK_q,      spawn,          SHCMD("$HOME/bin/logout.sh") },
+	{ MODKEY,          XF86XK_Calculator,      spawn,          SHCMD("systemctl suspend") },
 	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
 };
 
@@ -107,9 +184,90 @@ static Button buttons[] = {
 	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
 	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
 	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
-	{ ClkTagBar,            0,              Button1,        view,           {0} },
+	{ ClkTagBar,            0,              Button1,        comboview,      {0} },
 	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
-	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
+	{ ClkTagBar,            MODKEY,         Button1,        combotag,       {0} },
 	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
 };
 
+static const char *const autostart[] = {
+  "xss-lock", "--", "/usr/bin/i3lock-fancy-rapid", "5", "2", "-u", NULL,
+  "dunst", NULL,
+  "xbanish", NULL,
+  "numlockx", NULL,
+  "nm-applet", NULL,
+  "picom", NULL,
+  "dwmblocks", NULL,
+  NULL /* terminate */
+};
+
+void
+setlayoutex(const Arg *arg)
+{
+	setlayout(&((Arg) { .v = &layouts[arg->i] }));
+}
+
+void
+viewex(const Arg *arg)
+{
+	comboview(&((Arg) { .ui = 1 << arg->ui }));
+}
+
+void
+viewall(const Arg *arg)
+{
+	comboview(&((Arg){.ui = ~0}));
+}
+
+void
+toggleviewex(const Arg *arg)
+{
+	toggleview(&((Arg) { .ui = 1 << arg->ui }));
+}
+
+void
+tagex(const Arg *arg)
+{
+	combotag(&((Arg) { .ui = 1 << arg->ui }));
+}
+
+void
+toggletagex(const Arg *arg)
+{
+	toggletag(&((Arg) { .ui = 1 << arg->ui }));
+}
+
+void
+tagall(const Arg *arg)
+{
+	combotag(&((Arg){.ui = ~0}));
+}
+
+/* signal definitions */
+/* signum must be greater than 0 */
+/* trigger signals using `xsetroot -name "fsignal:<signame> [<type> <value>]"` */
+static Signal signals[] = {
+	/* signum           function */
+	{ "focusstack",     focusstack },
+	{ "setmfact",       setmfact },
+	{ "togglebar",      togglebar },
+	{ "incnmaster",     incnmaster },
+	{ "togglefloating", togglefloating },
+	{ "focusmon",       focusmon },
+	{ "tagmon",         tagmon },
+	{ "zoom",           zoom },
+	{ "view",           comboview },
+	{ "viewall",        viewall },
+	{ "viewex",         viewex },
+	{ "toggleview",     comboview },
+	{ "toggleviewex",   toggleviewex },
+	{ "tag",            combotag },
+	{ "tagall",         tagall },
+	{ "tagex",          tagex },
+	{ "toggletag",      combotag },
+	{ "toggletagex",    toggletagex },
+	{ "killclient",     killclient },
+	{ "quit",           quit },
+	{ "setlayout",      setlayout },
+	{ "setlayoutex",    setlayoutex },
+};
