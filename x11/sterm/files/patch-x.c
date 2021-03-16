--- x.c.orig	2020-06-19 09:29:45 UTC
+++ x.c
@@ -34,6 +34,7 @@ typedef struct {
 	void (*func)(const Arg *);
 	const Arg arg;
 	uint  release;
+	int  altscrn;  /* 0: don't care, -1: not alt screen, 1: alt screen */
 } MouseShortcut;
 
 typedef struct {
@@ -81,8 +82,10 @@ typedef XftGlyphFontSpec GlyphFontSpec;
 typedef struct {
 	int tw, th; /* tty width and height */
 	int w, h; /* window width and height */
+	int hborderpx, vborderpx;
 	int ch; /* char height */
 	int cw; /* char width  */
+	int cyo; /* char y offset */
 	int mode; /* window state/mode flags */
 	int cursor; /* cursor style */
 } TermWindow;
@@ -93,7 +96,7 @@ typedef struct {
 	Window win;
 	Drawable buf;
 	GlyphFontSpec *specbuf; /* font spec buffer used for rendering */
-	Atom xembed, wmdeletewin, netwmname, netwmpid;
+	Atom xembed, wmdeletewin, netwmname, netwmiconname, netwmpid;
 	struct {
 		XIM xim;
 		XIC xic;
@@ -105,6 +108,7 @@ typedef struct {
 	XSetWindowAttributes attrs;
 	int scr;
 	int isfixed; /* is fixed geometry? */
+	int depth; /* bit depth */
 	int l, t; /* left and top offset */
 	int gm; /* geometry mask */
 } XWindow;
@@ -157,6 +161,8 @@ static void xhints(void);
 static int xloadcolor(int, const char *, Color *);
 static int xloadfont(Font *, FcPattern *);
 static void xloadfonts(char *, double);
+static int xloadsparefont(FcPattern *, int);
+static void xloadsparefonts(void);
 static void xunloadfont(Font *);
 static void xunloadfonts(void);
 static void xsetenv(void);
@@ -243,6 +249,12 @@ static char *usedfont = NULL;
 static double usedfontsize = 0;
 static double defaultfontsize = 0;
 
+/* declared in config.h */
+extern int disablebold;
+extern int disableitalic;
+extern int disableroman;
+
+static char *opt_alpha = NULL;
 static char *opt_class = NULL;
 static char **opt_cmd  = NULL;
 static char *opt_embed = NULL;
@@ -251,8 +263,10 @@ static char *opt_io    = NULL;
 static char *opt_line  = NULL;
 static char *opt_name  = NULL;
 static char *opt_title = NULL;
+static char *opt_dir   = NULL;
 
 static int oldbutton = 3; /* button event on startup: 3 = release */
+static int cursorblinks = 0;
 
 void
 clipcopy(const Arg *dummy)
@@ -306,6 +320,7 @@ zoomabs(const Arg *arg)
 {
 	xunloadfonts();
 	xloadfonts(usedfont, arg->f);
+	xloadsparefonts();
 	cresize(0, 0);
 	redraw();
 	xhints();
@@ -331,7 +346,7 @@ ttysend(const Arg *arg)
 int
 evcol(XEvent *e)
 {
-	int x = e->xbutton.x - borderpx;
+	int x = e->xbutton.x - win.hborderpx;
 	LIMIT(x, 0, win.tw - 1);
 	return x / win.cw;
 }
@@ -339,7 +354,7 @@ evcol(XEvent *e)
 int
 evrow(XEvent *e)
 {
-	int y = e->xbutton.y - borderpx;
+	int y = e->xbutton.y - win.vborderpx;
 	LIMIT(y, 0, win.th - 1);
 	return y / win.ch;
 }
@@ -446,6 +461,7 @@ mouseaction(XEvent *e, uint release)
 	for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
 		if (ms->release == release &&
 		    ms->button == e->xbutton.button &&
+		    (!ms->altscrn || (ms->altscrn == (tisaltscr() ? 1 : -1))) &&
 		    (match(ms->mod, state) ||  /* exact or forced */
 		     match(ms->mod, state & ~forcemousemod))) {
 			ms->func(&(ms->arg));
@@ -491,6 +507,29 @@ bpress(XEvent *e)
 }
 
 void
+opencopied(const Arg *arg)
+{
+	const size_t max_cmd = 2048;
+	const char *clip = xsel.clipboard;
+	if(!clip) {
+		fprintf(stderr, "Warning: nothing copied to clipboard\n");
+		return;
+	}
+
+	/* account for space/quote (3) and \0 (1) and & (1) */
+	char cmd[max_cmd + strlen(clip) + 5];
+	strncpy(cmd, (char *)arg->v, max_cmd);
+	cmd[max_cmd] = '\0';
+
+	strcat(cmd, " \"");
+	strcat(cmd, clip);
+	strcat(cmd, "\"");
+	strcat(cmd, "&");
+
+	system(cmd);
+}
+
+void
 propnotify(XEvent *e)
 {
 	XPropertyEvent *xpev;
@@ -673,6 +712,7 @@ setsel(char *str, Time t)
 	XSetSelectionOwner(xw.dpy, XA_PRIMARY, xw.win, t);
 	if (XGetSelectionOwner(xw.dpy, XA_PRIMARY) != xw.win)
 		selclear();
+	clipcopy(NULL);
 }
 
 void
@@ -721,6 +761,9 @@ cresize(int width, int height)
 	col = MAX(1, col);
 	row = MAX(1, row);
 
+	win.hborderpx = (win.w - col * win.cw) / 2;
+	win.vborderpx = (win.h - row * win.ch) / 2;
+
 	tresize(col, row);
 	xresize(col, row);
 	ttyresize(win.tw, win.th);
@@ -734,7 +777,7 @@ xresize(int col, int row)
 
 	XFreePixmap(xw.dpy, xw.buf);
 	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
-			DefaultDepth(xw.dpy, xw.scr));
+			xw.depth);
 	XftDrawChange(xw.draw, xw.buf);
 	xclear(0, 0, win.w, win.h);
 
@@ -794,6 +837,13 @@ xloadcols(void)
 			else
 				die("could not allocate color %d\n", i);
 		}
+
+	/* set alpha value of bg color */
+	if (opt_alpha)
+		alpha = strtof(opt_alpha, NULL);
+	dc.col[defaultbg].color.alpha = (unsigned short)(0xffff * alpha);
+	dc.col[defaultbg].pixel &= 0x00FFFFFF;
+	dc.col[defaultbg].pixel |= (unsigned char)(0xff * alpha) << 24;
 	loaded = 1;
 }
 
@@ -826,6 +876,12 @@ xclear(int x1, int y1, int x2, int y2)
 }
 
 void
+xclearwin(void)
+{
+	xclear(0, 0, win.w, win.h);
+}
+
+void
 xhints(void)
 {
 	XClassHint class = {opt_name ? opt_name : termname,
@@ -838,8 +894,8 @@ xhints(void)
 	sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
 	sizeh->height = win.h;
 	sizeh->width = win.w;
-	sizeh->height_inc = win.ch;
-	sizeh->width_inc = win.cw;
+	sizeh->height_inc = 1;
+	sizeh->width_inc = 1;
 	sizeh->base_height = 2 * borderpx;
 	sizeh->base_width = 2 * borderpx;
 	sizeh->min_height = win.ch + 2 * borderpx;
@@ -1000,9 +1056,13 @@ xloadfonts(char *fontstr, double fontsize)
 	/* Setting character width and height. */
 	win.cw = ceilf(dc.font.width * cwscale);
 	win.ch = ceilf(dc.font.height * chscale);
+	win.cyo = ceilf(dc.font.height * (chscale - 1) / 2);
 
 	FcPatternDel(pattern, FC_SLANT);
-	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
+	if (!disableitalic)
+		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
+	if (!disableroman)
+		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
 	if (xloadfont(&dc.ifont, pattern))
 		die("can't open font %s\n", fontstr);
 
@@ -1019,7 +1079,102 @@ xloadfonts(char *fontstr, double fontsize)
 	FcPatternDestroy(pattern);
 }
 
+int
+xloadsparefont(FcPattern *pattern, int flags)
+{
+	FcPattern *match;
+	FcResult result;
+	
+	match = FcFontMatch(NULL, pattern, &result);
+	if (!match) {
+		return 1;
+	}
+
+	if (!(frc[frclen].font = XftFontOpenPattern(xw.dpy, match))) {
+		FcPatternDestroy(match);
+		return 1;
+	}
+
+	frc[frclen].flags = flags;
+	/* Believe U+0000 glyph will present in each default font */
+	frc[frclen].unicodep = 0;
+	frclen++;
+
+	return 0;
+}
+
 void
+xloadsparefonts(void)
+{
+	FcPattern *pattern;
+	double sizeshift, fontval;
+	int fc;
+	char **fp;
+
+	if (frclen != 0)
+		die("can't embed spare fonts. cache isn't empty");
+
+	/* Calculate count of spare fonts */
+	fc = sizeof(font2) / sizeof(*font2);
+	if (fc == 0)
+		return;
+
+	/* Allocate memory for cache entries. */
+	if (frccap < 4 * fc) {
+		frccap += 4 * fc - frccap;
+		frc = xrealloc(frc, frccap * sizeof(Fontcache));
+	}
+
+	for (fp = font2; fp - font2 < fc; ++fp) {
+	
+		if (**fp == '-')
+			pattern = XftXlfdParse(*fp, False, False);
+		else
+			pattern = FcNameParse((FcChar8 *)*fp);
+	
+		if (!pattern)
+			die("can't open spare font %s\n", *fp);
+	   		
+		if (defaultfontsize > 0) {
+			sizeshift = usedfontsize - defaultfontsize;
+			if (sizeshift != 0 &&
+					FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) ==
+					FcResultMatch) {	
+				fontval += sizeshift;
+				FcPatternDel(pattern, FC_PIXEL_SIZE);
+				FcPatternDel(pattern, FC_SIZE);
+				FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fontval);
+			}
+		}
+	
+		FcPatternAddBool(pattern, FC_SCALABLE, 1);
+	
+		FcConfigSubstitute(NULL, pattern, FcMatchPattern);
+		XftDefaultSubstitute(xw.dpy, xw.scr, pattern);
+	
+		if (xloadsparefont(pattern, FRC_NORMAL))
+			die("can't open spare font %s\n", *fp);
+	
+		FcPatternDel(pattern, FC_SLANT);
+		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
+		if (xloadsparefont(pattern, FRC_ITALIC))
+			die("can't open spare font %s\n", *fp);
+			
+		FcPatternDel(pattern, FC_WEIGHT);
+		FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
+		if (xloadsparefont(pattern, FRC_ITALICBOLD))
+			die("can't open spare font %s\n", *fp);
+	
+		FcPatternDel(pattern, FC_SLANT);
+		FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
+		if (xloadsparefont(pattern, FRC_BOLD))
+			die("can't open spare font %s\n", *fp);
+	
+		FcPatternDestroy(pattern);
+	}
+}
+
+void
 xunloadfont(Font *f)
 {
 	XftFontClose(xw.dpy, f->match);
@@ -1103,12 +1258,24 @@ xinit(int cols, int rows)
 	Window parent;
 	pid_t thispid = getpid();
 	XColor xmousefg, xmousebg;
+	XWindowAttributes attr;
+	XVisualInfo vis;
 
 	if (!(xw.dpy = XOpenDisplay(NULL)))
 		die("can't open display\n");
 	xw.scr = XDefaultScreen(xw.dpy);
-	xw.vis = XDefaultVisual(xw.dpy, xw.scr);
 
+	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0)))) {
+		parent = XRootWindow(xw.dpy, xw.scr);
+		xw.depth = 32;
+	} else {
+		XGetWindowAttributes(xw.dpy, parent, &attr);
+		xw.depth = attr.depth;
+	}
+
+	XMatchVisualInfo(xw.dpy, xw.scr, xw.depth, TrueColor, &vis);
+	xw.vis = vis.visual;
+
 	/* font */
 	if (!FcInit())
 		die("could not init fontconfig.\n");
@@ -1116,13 +1283,16 @@ xinit(int cols, int rows)
 	usedfont = (opt_font == NULL)? font : opt_font;
 	xloadfonts(usedfont, 0);
 
+	/* spare fonts */
+	xloadsparefonts();
+
 	/* colors */
-	xw.cmap = XDefaultColormap(xw.dpy, xw.scr);
+	xw.cmap = XCreateColormap(xw.dpy, parent, xw.vis, None);
 	xloadcols();
 
 	/* adjust fixed window geometry */
-	win.w = 2 * borderpx + cols * win.cw;
-	win.h = 2 * borderpx + rows * win.ch;
+	win.w = 2 * win.hborderpx + cols * win.cw;
+	win.h = 2 * win.vborderpx + rows * win.ch;
 	if (xw.gm & XNegative)
 		xw.l += DisplayWidth(xw.dpy, xw.scr) - win.w - 2;
 	if (xw.gm & YNegative)
@@ -1137,19 +1307,15 @@ xinit(int cols, int rows)
 		| ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;
 	xw.attrs.colormap = xw.cmap;
 
-	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0))))
-		parent = XRootWindow(xw.dpy, xw.scr);
 	xw.win = XCreateWindow(xw.dpy, parent, xw.l, xw.t,
-			win.w, win.h, 0, XDefaultDepth(xw.dpy, xw.scr), InputOutput,
+			win.w, win.h, 0, xw.depth, InputOutput,
 			xw.vis, CWBackPixel | CWBorderPixel | CWBitGravity
 			| CWEventMask | CWColormap, &xw.attrs);
 
 	memset(&gcvalues, 0, sizeof(gcvalues));
 	gcvalues.graphics_exposures = False;
-	dc.gc = XCreateGC(xw.dpy, parent, GCGraphicsExposures,
-			&gcvalues);
-	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h,
-			DefaultDepth(xw.dpy, xw.scr));
+	xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h, xw.depth);
+	dc.gc = XCreateGC(xw.dpy, xw.buf, GCGraphicsExposures, &gcvalues);
 	XSetForeground(xw.dpy, dc.gc, dc.col[defaultbg].pixel);
 	XFillRectangle(xw.dpy, xw.buf, dc.gc, 0, 0, win.w, win.h);
 
@@ -1186,6 +1352,7 @@ xinit(int cols, int rows)
 	xw.xembed = XInternAtom(xw.dpy, "_XEMBED", False);
 	xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
 	xw.netwmname = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
+	xw.netwmiconname = XInternAtom(xw.dpy, "_NET_WM_ICON_NAME", False);
 	XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);
 
 	xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
@@ -1205,12 +1372,14 @@ xinit(int cols, int rows)
 	xsel.xtarget = XInternAtom(xw.dpy, "UTF8_STRING", 0);
 	if (xsel.xtarget == None)
 		xsel.xtarget = XA_STRING;
+
+	boxdraw_xinit(xw.dpy, xw.cmap, xw.draw, xw.vis);
 }
 
 int
 xmakeglyphfontspecs(XftGlyphFontSpec *specs, const Glyph *glyphs, int len, int x, int y)
 {
-	float winx = borderpx + x * win.cw, winy = borderpx + y * win.ch, xp, yp;
+	float winx = win.hborderpx + x * win.cw, winy = win.vborderpx + y * win.ch, xp, yp;
 	ushort mode, prevmode = USHRT_MAX;
 	Font *font = &dc.font;
 	int frcflags = FRC_NORMAL;
@@ -1223,7 +1392,7 @@ xmakeglyphfontspecs(XftGlyphFontSpec *specs, const Gly
 	FcCharSet *fccharset;
 	int i, f, numspecs = 0;
 
-	for (i = 0, xp = winx, yp = winy + font->ascent; i < len; ++i) {
+	for (i = 0, xp = winx, yp = winy + font->ascent + win.cyo; i < len; ++i) {
 		/* Fetch rune and mode for current glyph. */
 		rune = glyphs[i].u;
 		mode = glyphs[i].mode;
@@ -1248,11 +1417,16 @@ xmakeglyphfontspecs(XftGlyphFontSpec *specs, const Gly
 				font = &dc.bfont;
 				frcflags = FRC_BOLD;
 			}
-			yp = winy + font->ascent;
+			yp = winy + font->ascent + win.cyo;
 		}
 
-		/* Lookup character index with default font. */
-		glyphidx = XftCharIndex(xw.dpy, font->match, rune);
+		if (mode & ATTR_BOXDRAW) {
+			/* minor shoehorning: boxdraw uses only this ushort */
+			glyphidx = boxdrawindex(&glyphs[i]);
+		} else {
+			/* Lookup character index with default font. */
+			glyphidx = XftCharIndex(xw.dpy, font->match, rune);
+		}
 		if (glyphidx) {
 			specs[numspecs].font = font->match;
 			specs[numspecs].glyph = glyphidx;
@@ -1343,7 +1517,7 @@ void
 xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Glyph base, int len, int x, int y)
 {
 	int charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);
-	int winx = borderpx + x * win.cw, winy = borderpx + y * win.ch,
+	int winx = win.hborderpx + x * win.cw, winy = win.vborderpx + y * win.ch,
 	    width = charlen * win.cw;
 	Color *fg, *bg, *temp, revfg, revbg, truefg, truebg;
 	XRenderColor colfg, colbg;
@@ -1380,10 +1554,6 @@ xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Gly
 		bg = &dc.col[base.bg];
 	}
 
-	/* Change basic system colors [0-7] to bright system colors [8-15] */
-	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base.fg, 0, 7))
-		fg = &dc.col[base.fg + 8];
-
 	if (IS_SET(MODE_REVERSE)) {
 		if (fg == &dc.col[defaultfg]) {
 			fg = &dc.col[defaultbg];
@@ -1433,17 +1603,17 @@ xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Gly
 
 	/* Intelligent cleaning up of the borders. */
 	if (x == 0) {
-		xclear(0, (y == 0)? 0 : winy, borderpx,
+		xclear(0, (y == 0)? 0 : winy, win.vborderpx,
 			winy + win.ch +
-			((winy + win.ch >= borderpx + win.th)? win.h : 0));
+			((winy + win.ch >= win.vborderpx + win.th)? win.h : 0));
 	}
-	if (winx + width >= borderpx + win.tw) {
+	if (winx + width >= win.hborderpx + win.tw) {
 		xclear(winx + width, (y == 0)? 0 : winy, win.w,
-			((winy + win.ch >= borderpx + win.th)? win.h : (winy + win.ch)));
+			((winy + win.ch >= win.vborderpx + win.th)? win.h : (winy + win.ch)));
 	}
 	if (y == 0)
-		xclear(winx, 0, winx + width, borderpx);
-	if (winy + win.ch >= borderpx + win.th)
+		xclear(winx, 0, winx + width, win.hborderpx);
+	if (winy + win.ch >= win.vborderpx + win.th)
 		xclear(winx, winy + win.ch, winx + width, win.h);
 
 	/* Clean up the region we want to draw to. */
@@ -1456,17 +1626,21 @@ xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Gly
 	r.width = width;
 	XftDrawSetClipRectangles(xw.draw, winx, winy, &r, 1);
 
-	/* Render the glyphs. */
-	XftDrawGlyphFontSpec(xw.draw, fg, specs, len);
+	if (base.mode & ATTR_BOXDRAW) {
+		drawboxes(winx, winy, width / len, win.ch, fg, bg, specs, len);
+	} else {
+		/* Render the glyphs. */
+		XftDrawGlyphFontSpec(xw.draw, fg, specs, len);
+	}
 
 	/* Render underline and strikethrough. */
 	if (base.mode & ATTR_UNDERLINE) {
-		XftDrawRect(xw.draw, fg, winx, winy + dc.font.ascent + 1,
+		XftDrawRect(xw.draw, fg, winx, winy + win.cyo + dc.font.ascent + 1,
 				width, 1);
 	}
 
 	if (base.mode & ATTR_STRUCK) {
-		XftDrawRect(xw.draw, fg, winx, winy + 2 * dc.font.ascent / 3,
+		XftDrawRect(xw.draw, fg, winx, winy + win.cyo + 2 * dc.font.ascent / 3,
 				width, 1);
 	}
 
@@ -1500,7 +1674,7 @@ xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, G
 	/*
 	 * Select the right color for the right mode.
 	 */
-	g.mode &= ATTR_BOLD|ATTR_ITALIC|ATTR_UNDERLINE|ATTR_STRUCK|ATTR_WIDE;
+	g.mode &= ATTR_BOLD|ATTR_ITALIC|ATTR_UNDERLINE|ATTR_STRUCK|ATTR_WIDE|ATTR_BOXDRAW;
 
 	if (IS_SET(MODE_REVERSE)) {
 		g.mode |= ATTR_REVERSE;
@@ -1526,46 +1700,60 @@ xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, G
 	/* draw the new one */
 	if (IS_SET(MODE_FOCUSED)) {
 		switch (win.cursor) {
-		case 7: /* st extension */
-			g.u = 0x2603; /* snowman (U+2603) */
+		case 0: /* Blinking block */
+		case 1: /* Blinking block (default) */
+			if (IS_SET(MODE_BLINK))
+				break;
 			/* FALLTHROUGH */
-		case 0: /* Blinking Block */
-		case 1: /* Blinking Block (Default) */
-		case 2: /* Steady Block */
+		case 2: /* Steady block */
 			xdrawglyph(g, cx, cy);
 			break;
-		case 3: /* Blinking Underline */
-		case 4: /* Steady Underline */
+		case 3: /* Blinking underline */
+			if (IS_SET(MODE_BLINK))
+				break;
+			/* FALLTHROUGH */
+		case 4: /* Steady underline */
 			XftDrawRect(xw.draw, &drawcol,
-					borderpx + cx * win.cw,
-					borderpx + (cy + 1) * win.ch - \
+					win.hborderpx + cx * win.cw,
+					win.vborderpx + (cy + 1) * win.ch - \
 						cursorthickness,
 					win.cw, cursorthickness);
 			break;
 		case 5: /* Blinking bar */
+			if (IS_SET(MODE_BLINK))
+				break;
+			/* FALLTHROUGH */
 		case 6: /* Steady bar */
 			XftDrawRect(xw.draw, &drawcol,
-					borderpx + cx * win.cw,
-					borderpx + cy * win.ch,
+					win.hborderpx + cx * win.cw,
+					win.vborderpx + cy * win.ch,
 					cursorthickness, win.ch);
 			break;
+		case 7: /* Blinking st cursor */
+			if (IS_SET(MODE_BLINK))
+				break;
+			/* FALLTHROUGH */
+		case 8: /* Steady st cursor */
+			g.u = stcursor;
+			xdrawglyph(g, cx, cy);
+			break;
 		}
 	} else {
 		XftDrawRect(xw.draw, &drawcol,
-				borderpx + cx * win.cw,
-				borderpx + cy * win.ch,
+				win.hborderpx + cx * win.cw,
+				win.vborderpx + cy * win.ch,
 				win.cw - 1, 1);
 		XftDrawRect(xw.draw, &drawcol,
-				borderpx + cx * win.cw,
-				borderpx + cy * win.ch,
+				win.hborderpx + cx * win.cw,
+				win.vborderpx + cy * win.ch,
 				1, win.ch - 1);
 		XftDrawRect(xw.draw, &drawcol,
-				borderpx + (cx + 1) * win.cw - 1,
-				borderpx + cy * win.ch,
+				win.hborderpx + (cx + 1) * win.cw - 1,
+				win.vborderpx + cy * win.ch,
 				1, win.ch - 1);
 		XftDrawRect(xw.draw, &drawcol,
-				borderpx + cx * win.cw,
-				borderpx + (cy + 1) * win.ch - 1,
+				win.hborderpx + cx * win.cw,
+				win.vborderpx + (cy + 1) * win.ch - 1,
 				win.cw, 1);
 	}
 }
@@ -1580,6 +1768,19 @@ xsetenv(void)
 }
 
 void
+xseticontitle(char *p)
+{
+	XTextProperty prop;
+	DEFAULT(p, opt_title);
+
+	Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle,
+			&prop);
+	XSetWMIconName(xw.dpy, xw.win, &prop);
+	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmiconname);
+	XFree(prop.value);
+}
+
+void
 xsettitle(char *p)
 {
 	XTextProperty prop;
@@ -1690,9 +1891,12 @@ xsetmode(int set, unsigned int flags)
 int
 xsetcursor(int cursor)
 {
-	if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
+	if (!BETWEEN(cursor, 0, 8)) /* 7-8: st extensions */
 		return 1;
 	win.cursor = cursor;
+	cursorblinks = win.cursor == 0 || win.cursor == 1 ||
+	               win.cursor == 3 || win.cursor == 5 ||
+	               win.cursor == 7;
 	return 0;
 }
 
@@ -1936,6 +2140,10 @@ run(void)
 		if (FD_ISSET(ttyfd, &rfd) || xev) {
 			if (!drawing) {
 				trigger = now;
+				if (IS_SET(MODE_BLINK)) {
+					win.mode ^= MODE_BLINK;
+				}
+				lastblink = now;
 				drawing = 1;
 			}
 			timeout = (maxlatency - TIMEDIFF(now, trigger)) \
@@ -1946,7 +2154,7 @@ run(void)
 
 		/* idle detected or maxlatency exhausted -> draw */
 		timeout = -1;
-		if (blinktimeout && tattrset(ATTR_BLINK)) {
+		if (blinktimeout && (cursorblinks || tattrset(ATTR_BLINK))) {
 			timeout = blinktimeout - TIMEDIFF(now, lastblink);
 			if (timeout <= 0) {
 				if (-timeout > blinktimeout) /* start visible */
@@ -1967,12 +2175,12 @@ run(void)
 void
 usage(void)
 {
-	die("usage: %s [-aiv] [-c class] [-f font] [-g geometry]"
-	    " [-n name] [-o file]\n"
+	die("usage: %s [-aiv] [-c class] [-d path] [-f font]"
+	    " [-g geometry] [-n name] [-o file]\n"
 	    "          [-T title] [-t title] [-w windowid]"
 	    " [[-e] command [args ...]]\n"
-	    "       %s [-aiv] [-c class] [-f font] [-g geometry]"
-	    " [-n name] [-o file]\n"
+	    "       %s [-aiv] [-c class] [-d path] [-f font]"
+	    " [-g geometry] [-n name] [-o file]\n"
 	    "          [-T title] [-t title] [-w windowid] -l line"
 	    " [stty_args ...]\n", argv0, argv0);
 }
@@ -1982,12 +2190,15 @@ main(int argc, char *argv[])
 {
 	xw.l = xw.t = 0;
 	xw.isfixed = False;
-	xsetcursor(cursorshape);
+	xsetcursor(cursorstyle);
 
 	ARGBEGIN {
 	case 'a':
 		allowaltscreen = 0;
 		break;
+	case 'A':
+		opt_alpha = EARGF(usage());
+		break;
 	case 'c':
 		opt_class = EARGF(usage());
 		break;
@@ -2024,6 +2235,9 @@ main(int argc, char *argv[])
 	case 'v':
 		die("%s " VERSION "\n", argv0);
 		break;
+	case 'd':
+		opt_dir = EARGF(usage());
+		break;
 	default:
 		usage();
 	} ARGEND;
@@ -2043,6 +2257,7 @@ run:
 	xinit(cols, rows);
 	xsetenv();
 	selinit();
+	chdir(opt_dir);
 	run();
 
 	return 0;
