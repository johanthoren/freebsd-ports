--- dwm.c.orig	2019-02-02 12:55:28 UTC
+++ dwm.c
@@ -49,20 +49,41 @@
 #define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
 #define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                                * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
-#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
+#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
 #define LENGTH(X)               (sizeof X / sizeof X[0])
 #define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
-#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
-#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
-#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
+#define WIDTH(X)                ((X)->w + 2 * (X)->bw + gappx)
+#define HEIGHT(X)               ((X)->h + 2 * (X)->bw + gappx)
+#define NUMTAGS					(LENGTH(tags) + LENGTH(scratchpads))
+#define TAGMASK     			((1 << NUMTAGS) - 1)
+#define SPTAG(i) 				((1 << LENGTH(tags)) << (i))
+#define SPTAGMASK   			(((1 << LENGTH(scratchpads))-1) << LENGTH(tags))
 #define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
 
+#define SYSTEM_TRAY_REQUEST_DOCK    0
+
+/* XEMBED messages */
+#define XEMBED_EMBEDDED_NOTIFY      0
+#define XEMBED_WINDOW_ACTIVATE      1
+#define XEMBED_FOCUS_IN             4
+#define XEMBED_MODALITY_ON         10
+
+#define XEMBED_MAPPED              (1 << 0)
+#define XEMBED_WINDOW_ACTIVATE      1
+#define XEMBED_WINDOW_DEACTIVATE    2
+
+#define VERSION_MAJOR               0
+#define VERSION_MINOR               0
+#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR
+
 /* enums */
 enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
 enum { SchemeNorm, SchemeSel }; /* color schemes */
 enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
+       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
        NetWMFullscreen, NetActiveWindow, NetWMWindowType,
        NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
+enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
 enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
 enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
        ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
@@ -92,7 +113,7 @@ struct Client {
 	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
 	int bw, oldbw;
 	unsigned int tags;
-	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
+	int isfixed, iscentered, isfloating, isurgent, neverfocus, oldstate, isfullscreen, issticky;
 	Client *next;
 	Client *snext;
 	Monitor *mon;
@@ -107,6 +128,11 @@ typedef struct {
 } Key;
 
 typedef struct {
+	const char * sig;
+	void (*func)(const Arg *);
+} Signal;
+
+typedef struct {
 	const char *symbol;
 	void (*arrange)(Monitor *);
 } Layout;
@@ -137,10 +163,17 @@ typedef struct {
 	const char *instance;
 	const char *title;
 	unsigned int tags;
+	int iscentered;
 	int isfloating;
 	int monitor;
 } Rule;
 
+typedef struct Systray   Systray;
+struct Systray {
+	Window win;
+	Client *icons;
+};
+
 /* function declarations */
 static void applyrules(Client *c);
 static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
@@ -148,6 +181,7 @@ static void arrange(Monitor *m);
 static void arrangemon(Monitor *m);
 static void attach(Client *c);
 static void attachstack(Client *c);
+static int fake_signal(void);
 static void buttonpress(XEvent *e);
 static void checkotherwm(void);
 static void cleanup(void);
@@ -169,8 +203,10 @@ static void focus(Client *c);
 static void focusin(XEvent *e);
 static void focusmon(const Arg *arg);
 static void focusstack(const Arg *arg);
+static Atom getatomprop(Client *c, Atom prop);
 static int getrootptr(int *x, int *y);
 static long getstate(Window w);
+static unsigned int getsystraywidth();
 static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
 static void grabbuttons(Client *c, int focused);
 static void grabkeys(void);
@@ -188,13 +224,16 @@ static void pop(Client *);
 static void propertynotify(XEvent *e);
 static void quit(const Arg *arg);
 static Monitor *recttomon(int x, int y, int w, int h);
+static void removesystrayicon(Client *i);
 static void resize(Client *c, int x, int y, int w, int h, int interact);
+static void resizebarwin(Monitor *m);
 static void resizeclient(Client *c, int x, int y, int w, int h);
 static void resizemouse(const Arg *arg);
+static void resizerequest(XEvent *e);
 static void restack(Monitor *m);
 static void run(void);
 static void scan(void);
-static int sendevent(Client *c, Atom proto);
+static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
 static void sendmon(Client *c, Monitor *m);
 static void setclientstate(Client *c, long state);
 static void setfocus(Client *c);
@@ -203,14 +242,19 @@ static void setlayout(const Arg *arg);
 static void setmfact(const Arg *arg);
 static void setup(void);
 static void seturgent(Client *c, int urg);
+static void shiftview(const Arg *arg);
 static void showhide(Client *c);
 static void sigchld(int unused);
 static void spawn(const Arg *arg);
+static Monitor *systraytomon(Monitor *m);
 static void tag(const Arg *arg);
 static void tagmon(const Arg *arg);
 static void tile(Monitor *);
 static void togglebar(const Arg *arg);
 static void togglefloating(const Arg *arg);
+static void togglescratch(const Arg *arg);
+static void togglesticky(const Arg *arg);
+static void togglefullscr(const Arg *arg);
 static void toggletag(const Arg *arg);
 static void toggleview(const Arg *arg);
 static void unfocus(Client *c, int setfocus);
@@ -223,18 +267,28 @@ static int updategeom(void);
 static void updatenumlockmask(void);
 static void updatesizehints(Client *c);
 static void updatestatus(void);
+static void updatesystray(void);
+static void updatesystrayicongeom(Client *i, int w, int h);
+static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
 static void updatetitle(Client *c);
 static void updatewindowtype(Client *c);
 static void updatewmhints(Client *c);
 static void view(const Arg *arg);
 static Client *wintoclient(Window w);
 static Monitor *wintomon(Window w);
+static Client *wintosystrayicon(Window w);
 static int xerror(Display *dpy, XErrorEvent *ee);
 static int xerrordummy(Display *dpy, XErrorEvent *ee);
 static int xerrorstart(Display *dpy, XErrorEvent *ee);
 static void zoom(const Arg *arg);
+static void autostart_exec(void);
 
+static void keyrelease(XEvent *e);
+static void combotag(const Arg *arg);
+static void comboview(const Arg *arg);
+
 /* variables */
+static Systray *systray =  NULL;
 static const char broken[] = "broken";
 static char stext[256];
 static int screen;
@@ -245,6 +299,7 @@ static int (*xerrorxlib)(Display *, XErrorEvent *);
 static unsigned int numlockmask = 0;
 static void (*handler[LASTEvent]) (XEvent *) = {
 	[ButtonPress] = buttonpress,
+	[ButtonRelease] = keyrelease,
 	[ClientMessage] = clientmessage,
 	[ConfigureRequest] = configurerequest,
 	[ConfigureNotify] = configurenotify,
@@ -252,14 +307,16 @@ static void (*handler[LASTEvent]) (XEvent *) = {
 	[EnterNotify] = enternotify,
 	[Expose] = expose,
 	[FocusIn] = focusin,
+	[KeyRelease] = keyrelease,
 	[KeyPress] = keypress,
 	[MappingNotify] = mappingnotify,
 	[MapRequest] = maprequest,
 	[MotionNotify] = motionnotify,
 	[PropertyNotify] = propertynotify,
+	[ResizeRequest] = resizerequest,
 	[UnmapNotify] = unmapnotify
 };
-static Atom wmatom[WMLast], netatom[NetLast];
+static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
 static int running = 1;
 static Cur *cursor[CurLast];
 static Clr **scheme;
@@ -274,8 +331,72 @@ static Window root, wmcheckwin;
 /* compile-time check if all tags fit into an unsigned int bit array. */
 struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };
 
+/* dwm will keep pid's of processes from autostart array and kill them at quit */
+static pid_t *autostart_pids;
+static size_t autostart_len;
+
+/* execute command from autostart array */
+static void
+autostart_exec() {
+	const char *const *p;
+	size_t i = 0;
+
+	/* count entries */
+	for (p = autostart; *p; autostart_len++, p++)
+		while (*++p);
+
+	autostart_pids = malloc(autostart_len * sizeof(pid_t));
+	for (p = autostart; *p; i++, p++) {
+		if ((autostart_pids[i] = fork()) == 0) {
+			setsid();
+			execvp(*p, (char *const *)p);
+			fprintf(stderr, "dwm: execvp %s\n", *p);
+			perror(" failed");
+			_exit(EXIT_FAILURE);
+		}
+		/* skip arguments */
+		while (*++p);
+	}
+}
+
 /* function implementations */
+static int combo = 0;
+
 void
+keyrelease(XEvent *e) {
+	combo = 0;
+}
+
+void
+combotag(const Arg *arg) {
+	if(selmon->sel && arg->ui & TAGMASK) {
+		if (combo) {
+			selmon->sel->tags |= arg->ui & TAGMASK;
+		} else {
+			combo = 1;
+			selmon->sel->tags = arg->ui & TAGMASK;
+		}
+		focus(NULL);
+		arrange(selmon);
+	}
+}
+
+void
+comboview(const Arg *arg) {
+	unsigned newtags = arg->ui & TAGMASK;
+	if (combo) {
+		selmon->tagset[selmon->seltags] |= newtags;
+	} else {
+		selmon->seltags ^= 1;	/*toggle tagset*/
+		combo = 1;
+		if (newtags)
+			selmon->tagset[selmon->seltags] = newtags;
+	}
+	focus(NULL);
+	arrange(selmon);
+}
+
+void
 applyrules(Client *c)
 {
 	const char *class, *instance;
@@ -285,6 +406,7 @@ applyrules(Client *c)
 	XClassHint ch = { NULL, NULL };
 
 	/* rule matching */
+	c->iscentered = 0;
 	c->isfloating = 0;
 	c->tags = 0;
 	XGetClassHint(dpy, c->win, &ch);
@@ -297,8 +419,14 @@ applyrules(Client *c)
 		&& (!r->class || strstr(class, r->class))
 		&& (!r->instance || strstr(instance, r->instance)))
 		{
+			c->iscentered = r->iscentered;
 			c->isfloating = r->isfloating;
 			c->tags |= r->tags;
+			if ((r->tags & SPTAGMASK) && r->isfloating) {
+				c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
+				c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
+			}
+
 			for (m = mons; m && m->num != r->monitor; m = m->next);
 			if (m)
 				c->mon = m;
@@ -308,7 +436,7 @@ applyrules(Client *c)
 		XFree(ch.res_class);
 	if (ch.res_name)
 		XFree(ch.res_name);
-	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
+	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : (c->mon->tagset[c->mon->seltags] & ~SPTAGMASK);
 }
 
 int
@@ -416,7 +544,7 @@ attachstack(Client *c)
 void
 buttonpress(XEvent *e)
 {
-	unsigned int i, x, click;
+	unsigned int i, x, click, occ = 0;
 	Arg arg = {0};
 	Client *c;
 	Monitor *m;
@@ -431,15 +559,20 @@ buttonpress(XEvent *e)
 	}
 	if (ev->window == selmon->barwin) {
 		i = x = 0;
-		do
+		for (c = m->clients; c; c = c->next)
+			occ |= c->tags == 255 ? 0 : c->tags;
+		do {
+			/* do not reserve space for vacant tags */
+			if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
+				continue;
 			x += TEXTW(tags[i]);
-		while (ev->x >= x && ++i < LENGTH(tags));
+		} while (ev->x >= x && ++i < LENGTH(tags));
 		if (i < LENGTH(tags)) {
 			click = ClkTagBar;
 			arg.ui = 1 << i;
 		} else if (ev->x < x + blw)
 			click = ClkLtSymbol;
-		else if (ev->x > selmon->ww - TEXTW(stext))
+		else if (ev->x > selmon->ww - (int)TEXTW(stext) - getsystraywidth())
 			click = ClkStatusText;
 		else
 			click = ClkWinTitle;
@@ -482,6 +615,11 @@ cleanup(void)
 	XUngrabKey(dpy, AnyKey, AnyModifier, root);
 	while (mons)
 		cleanupmon(mons);
+	if (showsystray) {
+		XUnmapWindow(dpy, systray->win);
+		XDestroyWindow(dpy, systray->win);
+		free(systray);
+	}
 	for (i = 0; i < CurLast; i++)
 		drw_cur_free(drw, cursor[i]);
 	for (i = 0; i < LENGTH(colors); i++)
@@ -512,9 +650,57 @@ cleanupmon(Monitor *mon)
 void
 clientmessage(XEvent *e)
 {
+	XWindowAttributes wa;
+	XSetWindowAttributes swa;
 	XClientMessageEvent *cme = &e->xclient;
 	Client *c = wintoclient(cme->window);
 
+	if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
+		/* add systray icons */
+		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
+			if (!(c = (Client *)calloc(1, sizeof(Client))))
+				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
+			if (!(c->win = cme->data.l[2])) {
+				free(c);
+				return;
+			}
+			c->mon = selmon;
+			c->next = systray->icons;
+			systray->icons = c;
+			if (!XGetWindowAttributes(dpy, c->win, &wa)) {
+				/* use sane defaults */
+				wa.width = bh;
+				wa.height = bh;
+				wa.border_width = 0;
+			}
+			c->x = c->oldx = c->y = c->oldy = 0;
+			c->w = c->oldw = wa.width;
+			c->h = c->oldh = wa.height;
+			c->oldbw = wa.border_width;
+			c->bw = 0;
+			c->isfloating = True;
+			/* reuse tags field as mapped status */
+			c->tags = 1;
+			updatesizehints(c);
+			updatesystrayicongeom(c, wa.width, wa.height);
+			XAddToSaveSet(dpy, c->win);
+			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
+			XReparentWindow(dpy, c->win, systray->win, 0, 0);
+			/* use parents background color */
+			swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
+			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
+			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
+			/* FIXME not sure if I have to send these events, too */
+			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
+			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
+			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
+			XSync(dpy, False);
+			resizebarwin(selmon);
+			updatesystray();
+			setclientstate(c, NormalState);
+		}
+		return;
+	}
 	if (!c)
 		return;
 	if (cme->message_type == netatom[NetWMState]) {
@@ -567,7 +753,7 @@ configurenotify(XEvent *e)
 				for (c = m->clients; c; c = c->next)
 					if (c->isfullscreen)
 						resizeclient(c, m->mx, m->my, m->mw, m->mh);
-				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
+				resizebarwin(m);
 			}
 			focus(NULL);
 			arrange(NULL);
@@ -652,6 +838,12 @@ destroynotify(XEvent *e)
 
 	if ((c = wintoclient(ev->window)))
 		unmanage(c, 1);
+
+	else if ((c = wintosystrayicon(ev->window))) {
+		removesystrayicon(c);
+		resizebarwin(selmon);
+		updatesystray();
+	}
 }
 
 void
@@ -695,40 +887,44 @@ dirtomon(int dir)
 void
 drawbar(Monitor *m)
 {
-	int x, w, sw = 0;
+	int x, w, tw = 0, stw = 0;
 	int boxs = drw->fonts->h / 9;
 	int boxw = drw->fonts->h / 6 + 2;
 	unsigned int i, occ = 0, urg = 0;
 	Client *c;
 
+	if(showsystray && m == systraytomon(m))
+		stw = getsystraywidth();
+
 	/* draw status first so it can be overdrawn by tags later */
 	if (m == selmon) { /* status is only drawn on selected monitor */
 		drw_setscheme(drw, scheme[SchemeNorm]);
-		sw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
-		drw_text(drw, m->ww - sw, 0, sw, bh, 0, stext, 0);
+		tw = TEXTW(stext) - lrpad / 2 + 2; /* 2px right padding */
+		drw_text(drw, m->ww - tw - stw, 0, tw, bh, lrpad / 2 - 2, stext, 0);
 	}
 
+	resizebarwin(m);
 	for (c = m->clients; c; c = c->next) {
-		occ |= c->tags;
+		occ |= c->tags == 255 ? 0 : c->tags;
 		if (c->isurgent)
 			urg |= c->tags;
 	}
 	x = 0;
 	for (i = 0; i < LENGTH(tags); i++) {
+		/* do not draw vacant tags */
+		if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
+		continue;
+
 		w = TEXTW(tags[i]);
 		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
 		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
-		if (occ & 1 << i)
-			drw_rect(drw, x + boxs, boxs, boxw, boxw,
-				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
-				urg & 1 << i);
 		x += w;
 	}
 	w = blw = TEXTW(m->ltsymbol);
 	drw_setscheme(drw, scheme[SchemeNorm]);
 	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
 
-	if ((w = m->ww - sw - x) > bh) {
+	if ((w = m->ww - tw - stw - x) > bh) {
 		if (m->sel) {
 			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
 			drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
@@ -739,7 +935,7 @@ drawbar(Monitor *m)
 			drw_rect(drw, x, 0, w, bh, 1, 1);
 		}
 	}
-	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
+	drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
 }
 
 void
@@ -776,8 +972,11 @@ expose(XEvent *e)
 	Monitor *m;
 	XExposeEvent *ev = &e->xexpose;
 
-	if (ev->count == 0 && (m = wintomon(ev->window)))
+	if (ev->count == 0 && (m = wintomon(ev->window))) {
 		drawbar(m);
+		if (m == selmon)
+			updatesystray();
+	}
 }
 
 void
@@ -862,10 +1061,17 @@ getatomprop(Client *c, Atom prop)
 	unsigned long dl;
 	unsigned char *p = NULL;
 	Atom da, atom = None;
+	/* FIXME getatomprop should return the number of items and a pointer to
+	 * the stored data instead of this workaround */
+	Atom req = XA_ATOM;
+	if (prop == xatom[XembedInfo])
+		req = xatom[XembedInfo];
 
-	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
+	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
 		&da, &di, &dl, &dl, &p) == Success && p) {
 		atom = *(Atom *)p;
+		if (da == xatom[XembedInfo] && dl == 2)
+			atom = ((Atom *)p)[1];
 		XFree(p);
 	}
 	return atom;
@@ -899,6 +1105,16 @@ getstate(Window w)
 	return result;
 }
 
+unsigned int
+getsystraywidth()
+{
+	unsigned int w = 0;
+	Client *i;
+	if(showsystray)
+		for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
+	return w ? w + systrayspacing : 1;
+}
+
 int
 gettextprop(Window w, Atom atom, char *text, unsigned int size)
 {
@@ -998,12 +1214,55 @@ keypress(XEvent *e)
 			keys[i].func(&(keys[i].arg));
 }
 
+int
+fake_signal(void)
+{
+	char fsignal[256];
+	char indicator[9] = "fsignal:";
+	char str_sig[50];
+	char param[16];
+	int i, len_str_sig, n, paramn;
+	size_t len_fsignal, len_indicator = strlen(indicator);
+	Arg arg;
+
+	// Get root name property
+	if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
+		len_fsignal = strlen(fsignal);
+
+		// Check if this is indeed a fake signal
+		if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
+			paramn = sscanf(fsignal+len_indicator, "%s%n%s%n", str_sig, &len_str_sig, param, &n);
+
+			if (paramn == 1) arg = (Arg) {0};
+			else if (paramn > 2) return 1;
+			else if (strncmp(param, "i", n - len_str_sig) == 0)
+				sscanf(fsignal + len_indicator + n, "%i", &(arg.i));
+			else if (strncmp(param, "ui", n - len_str_sig) == 0)
+				sscanf(fsignal + len_indicator + n, "%u", &(arg.ui));
+			else if (strncmp(param, "f", n - len_str_sig) == 0)
+				sscanf(fsignal + len_indicator + n, "%f", &(arg.f));
+			else return 1;
+
+			// Check if a signal was found, and if so handle it
+			for (i = 0; i < LENGTH(signals); i++)
+				if (strncmp(str_sig, signals[i].sig, len_str_sig) == 0 && signals[i].func)
+					signals[i].func(&(arg));
+
+			// A fake signal was sent
+			return 1;
+		}
+	}
+
+	// No fake signal was sent, so proceed with update
+	return 0;
+}
+
 void
 killclient(const Arg *arg)
 {
 	if (!selmon->sel)
 		return;
-	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
+	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
 		XGrabServer(dpy);
 		XSetErrorHandler(xerrordummy);
 		XSetCloseDownMode(dpy, DestroyAll);
@@ -1056,6 +1315,10 @@ manage(Window w, XWindowAttributes *wa)
 	updatewindowtype(c);
 	updatesizehints(c);
 	updatewmhints(c);
+	if (c->iscentered) {
+		c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
+		c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
+	}
 	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
 	grabbuttons(c, 0);
 	if (!c->isfloating)
@@ -1091,6 +1354,12 @@ maprequest(XEvent *e)
 {
 	static XWindowAttributes wa;
 	XMapRequestEvent *ev = &e->xmaprequest;
+	Client *i;
+	if ((i = wintosystrayicon(ev->window))) {
+		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
+		resizebarwin(selmon);
+		updatesystray();
+	}
 
 	if (!XGetWindowAttributes(dpy, ev->window, &wa))
 		return;
@@ -1215,8 +1484,20 @@ propertynotify(XEvent *e)
 	Window trans;
 	XPropertyEvent *ev = &e->xproperty;
 
-	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
-		updatestatus();
+	if ((c = wintosystrayicon(ev->window))) {
+		if (ev->atom == XA_WM_NORMAL_HINTS) {
+			updatesizehints(c);
+			updatesystrayicongeom(c, c->w, c->h);
+		}
+		else
+			updatesystrayiconstate(c, ev);
+		resizebarwin(selmon);
+		updatesystray();
+	}
+	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
+		if (!fake_signal())
+			updatestatus();
+	}
 	else if (ev->state == PropertyDelete)
 		return; /* ignore */
 	else if ((c = wintoclient(ev->window))) {
@@ -1248,6 +1529,16 @@ propertynotify(XEvent *e)
 void
 quit(const Arg *arg)
 {
+	size_t i;
+
+	/* kill child processes */
+	for (i = 0; i < autostart_len; i++) {
+		if (0 < autostart_pids[i]) {
+			kill(autostart_pids[i], SIGTERM);
+			waitpid(autostart_pids[i], NULL, 0);
+		}
+	}
+
 	running = 0;
 }
 
@@ -1266,6 +1557,19 @@ recttomon(int x, int y, int w, int h)
 }
 
 void
+removesystrayicon(Client *i)
+{
+	Client **ii;
+
+	if (!showsystray || !i)
+		return;
+	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
+	if (ii)
+		*ii = i->next;
+	free(i);
+}
+
+void
 resize(Client *c, int x, int y, int w, int h, int interact)
 {
 	if (applysizehints(c, &x, &y, &w, &h, interact))
@@ -1273,15 +1577,47 @@ resize(Client *c, int x, int y, int w, int h, int inte
 }
 
 void
+resizebarwin(Monitor *m) {
+	unsigned int w = m->ww;
+	if (showsystray && m == systraytomon(m))
+		w -= getsystraywidth();
+	XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
+}
+
+void
 resizeclient(Client *c, int x, int y, int w, int h)
 {
 	XWindowChanges wc;
+	unsigned int n;
+	unsigned int gapoffset;
+	unsigned int gapincr;
+	Client *nbc;
 
-	c->oldx = c->x; c->x = wc.x = x;
-	c->oldy = c->y; c->y = wc.y = y;
-	c->oldw = c->w; c->w = wc.width = w;
-	c->oldh = c->h; c->h = wc.height = h;
 	wc.border_width = c->bw;
+
+	/* Get number of clients for the selected monitor */
+	for (n = 0, nbc = nexttiled(selmon->clients); nbc; nbc = nexttiled(nbc->next), n++);
+
+	/* Do nothing if layout is floating */
+	if (c->isfloating || selmon->lt[selmon->sellt]->arrange == NULL) {
+		gapincr = gapoffset = 0;
+	} else {
+		/* Remove border and gap if layout is monocle or only one client */
+		if (selmon->lt[selmon->sellt]->arrange == monocle || n == 1) {
+			gapoffset = 0;
+			gapincr = -2 * borderpx;
+			wc.border_width = 0;
+		} else {
+			gapoffset = gappx;
+			gapincr = 2 * gappx;
+		}
+	}
+
+	c->oldx = c->x; c->x = wc.x = x + gapoffset;
+	c->oldy = c->y; c->y = wc.y = y + gapoffset;
+	c->oldw = c->w; c->w = wc.width = w - gapincr;
+	c->oldh = c->h; c->h = wc.height = h - gapincr;
+
 	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
 	configure(c);
 	XSync(dpy, False);
@@ -1345,6 +1681,19 @@ resizemouse(const Arg *arg)
 }
 
 void
+resizerequest(XEvent *e)
+{
+	XResizeRequestEvent *ev = &e->xresizerequest;
+	Client *i;
+
+	if ((i = wintosystrayicon(ev->window))) {
+		updatesystrayicongeom(i, ev->width, ev->height);
+		resizebarwin(selmon);
+		updatesystray();
+	}
+}
+
+void
 restack(Monitor *m)
 {
 	Client *c;
@@ -1433,26 +1782,36 @@ setclientstate(Client *c, long state)
 }
 
 int
-sendevent(Client *c, Atom proto)
+sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
 {
 	int n;
-	Atom *protocols;
+	Atom *protocols, mt;
 	int exists = 0;
 	XEvent ev;
 
-	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
-		while (!exists && n--)
-			exists = protocols[n] == proto;
-		XFree(protocols);
+	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
+		mt = wmatom[WMProtocols];
+		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
+			while (!exists && n--)
+				exists = protocols[n] == proto;
+			XFree(protocols);
+		}
 	}
+	else {
+		exists = True;
+		mt = proto;
+	}
 	if (exists) {
 		ev.type = ClientMessage;
-		ev.xclient.window = c->win;
-		ev.xclient.message_type = wmatom[WMProtocols];
+		ev.xclient.window = w;
+		ev.xclient.message_type = mt;
 		ev.xclient.format = 32;
-		ev.xclient.data.l[0] = proto;
-		ev.xclient.data.l[1] = CurrentTime;
-		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
+		ev.xclient.data.l[0] = d0;
+		ev.xclient.data.l[1] = d1;
+		ev.xclient.data.l[2] = d2;
+		ev.xclient.data.l[3] = d3;
+		ev.xclient.data.l[4] = d4;
+		XSendEvent(dpy, w, False, mask, &ev);
 	}
 	return exists;
 }
@@ -1466,7 +1825,7 @@ setfocus(Client *c)
 			XA_WINDOW, 32, PropModeReplace,
 			(unsigned char *) &(c->win), 1);
 	}
-	sendevent(c, wmatom[WMTakeFocus]);
+	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
 }
 
 void
@@ -1520,7 +1879,7 @@ setmfact(const Arg *arg)
 	if (!arg || !selmon->lt[selmon->sellt]->arrange)
 		return;
 	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
-	if (f < 0.1 || f > 0.9)
+	if (f < 0.05 || f > 0.95)
 		return;
 	selmon->mfact = f;
 	arrange(selmon);
@@ -1555,6 +1914,10 @@ setup(void)
 	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
 	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
 	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
+	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
+	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
+	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
+	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
 	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
 	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
 	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
@@ -1562,6 +1925,9 @@ setup(void)
 	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
 	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
 	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
+	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
+	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
+	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
 	/* init cursors */
 	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
 	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
@@ -1570,6 +1936,8 @@ setup(void)
 	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
 	for (i = 0; i < LENGTH(colors); i++)
 		scheme[i] = drw_scm_create(drw, colors[i], 3);
+	/* init system tray */
+	updatesystray();
 	/* init bars */
 	updatebars();
 	updatestatus();
@@ -1610,12 +1978,36 @@ seturgent(Client *c, int urg)
 	XFree(wmh);
 }
 
+/** Function to shift the current view to the left/right
+ **
+ ** @param: "arg->i" stores the number of tags to shift right (positive value)
+ **          or left (negative value)
+ **/
 void
+shiftview(const Arg *arg) {
+	Arg shifted;
+
+	if(arg->i > 0) // right circular shift
+		shifted.ui = (selmon->tagset[selmon->seltags] << arg->i)
+		   | (selmon->tagset[selmon->seltags] >> (LENGTH(tags) - arg->i));
+
+	else // left circular shift
+		shifted.ui = selmon->tagset[selmon->seltags] >> (- arg->i)
+		   | selmon->tagset[selmon->seltags] << (LENGTH(tags) + arg->i);
+
+	view(&shifted);
+}
+
+void
 showhide(Client *c)
 {
 	if (!c)
 		return;
 	if (ISVISIBLE(c)) {
+		if ((c->tags & SPTAGMASK) && c->isfloating) {
+			c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
+			c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
+		}
 		/* show clients top down */
 		XMoveWindow(dpy, c->win, c->x, c->y);
 		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
@@ -1631,9 +2023,25 @@ showhide(Client *c)
 void
 sigchld(int unused)
 {
+	pid_t pid;
+
 	if (signal(SIGCHLD, sigchld) == SIG_ERR)
 		die("can't install SIGCHLD handler:");
-	while (0 < waitpid(-1, NULL, WNOHANG));
+	while (0 < (pid = waitpid(-1, NULL, WNOHANG))) {
+		pid_t *p, *lim;
+
+		if (!(p = autostart_pids))
+			continue;
+		lim = &p[autostart_len];
+
+		for (; p < lim; p++) {
+			if (*p == pid) {
+				*p = -1;
+				break;
+			}
+		}
+
+	}
 }
 
 void
@@ -1688,11 +2096,13 @@ tile(Monitor *m)
 		if (i < m->nmaster) {
 			h = (m->wh - my) / (MIN(n, m->nmaster) - i);
 			resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
-			my += HEIGHT(c);
+			if (my + HEIGHT(c) < m->wh)
+				my += HEIGHT(c);
 		} else {
 			h = (m->wh - ty) / (n - i);
 			resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
-			ty += HEIGHT(c);
+			if (ty + HEIGHT(c) < m->wh)
+				ty += HEIGHT(c);
 		}
 }
 
@@ -1701,7 +2111,18 @@ togglebar(const Arg *arg)
 {
 	selmon->showbar = !selmon->showbar;
 	updatebarpos(selmon);
-	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
+	resizebarwin(selmon);
+	if (showsystray) {
+		XWindowChanges wc;
+		if (!selmon->showbar)
+			wc.y = -bh;
+		else if (selmon->showbar) {
+			wc.y = 0;
+			if (!selmon->topbar)
+				wc.y = selmon->mh - bh;
+		}
+		XConfigureWindow(dpy, systray->win, CWY, &wc);
+	}
 	arrange(selmon);
 }
 
@@ -1720,6 +2141,48 @@ togglefloating(const Arg *arg)
 }
 
 void
+togglescratch(const Arg *arg)
+{
+	Client *c;
+	unsigned int found = 0;
+	unsigned int scratchtag = SPTAG(arg->ui);
+	Arg sparg = {.v = scratchpads[arg->ui].cmd};
+
+	for (c = selmon->clients; c && !(found = c->tags & scratchtag); c = c->next);
+	if (found) {
+		unsigned int newtagset = selmon->tagset[selmon->seltags] ^ scratchtag;
+		if (newtagset) {
+			selmon->tagset[selmon->seltags] = newtagset;
+			focus(NULL);
+			arrange(selmon);
+		}
+		if (ISVISIBLE(c)) {
+			focus(c);
+			restack(selmon);
+		}
+	} else {
+		selmon->tagset[selmon->seltags] |= scratchtag;
+		spawn(&sparg);
+	}
+}
+
+void
+togglesticky(const Arg *arg)
+{
+	if (!selmon->sel)
+		return;
+	selmon->sel->issticky = !selmon->sel->issticky;
+	arrange(selmon);
+}
+
+void
+togglefullscr(const Arg *arg)
+{
+  if(selmon->sel)
+    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
+}
+
+void
 toggletag(const Arg *arg)
 {
 	unsigned int newtags;
@@ -1796,11 +2259,18 @@ unmapnotify(XEvent *e)
 		else
 			unmanage(c, 0);
 	}
+	else if ((c = wintosystrayicon(ev->window))) {
+		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
+		 * _not_ destroy them. We map those windows back */
+		XMapRaised(dpy, c->win);
+		updatesystray();
+	}
 }
 
 void
 updatebars(void)
 {
+	unsigned int w;
 	Monitor *m;
 	XSetWindowAttributes wa = {
 		.override_redirect = True,
@@ -1811,10 +2281,15 @@ updatebars(void)
 	for (m = mons; m; m = m->next) {
 		if (m->barwin)
 			continue;
-		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
+		w = m->ww;
+		if (showsystray && m == systraytomon(m))
+			w -= getsystraywidth();
+		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
 				CopyFromParent, DefaultVisual(dpy, screen),
 				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
 		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
+		if (showsystray && m == systraytomon(m))
+			XMapRaised(dpy, systray->win);
 		XMapRaised(dpy, m->barwin);
 		XSetClassHint(dpy, m->barwin, &ch);
 	}
@@ -1990,9 +2465,124 @@ updatestatus(void)
 	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
 		strcpy(stext, "dwm-"VERSION);
 	drawbar(selmon);
+	updatesystray();
 }
 
 void
+updatesystrayicongeom(Client *i, int w, int h)
+{
+	if (i) {
+		i->h = bh;
+		if (w == h)
+			i->w = bh;
+		else if (h == bh)
+			i->w = w;
+		else
+			i->w = (int) ((float)bh * ((float)w / (float)h));
+		applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
+		/* force icons into the systray dimensions if they don't want to */
+		if (i->h > bh) {
+			if (i->w == i->h)
+				i->w = bh;
+			else
+				i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
+			i->h = bh;
+		}
+	}
+}
+
+void
+updatesystrayiconstate(Client *i, XPropertyEvent *ev)
+{
+	long flags;
+	int code = 0;
+
+	if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
+			!(flags = getatomprop(i, xatom[XembedInfo])))
+		return;
+
+	if (flags & XEMBED_MAPPED && !i->tags) {
+		i->tags = 1;
+		code = XEMBED_WINDOW_ACTIVATE;
+		XMapRaised(dpy, i->win);
+		setclientstate(i, NormalState);
+	}
+	else if (!(flags & XEMBED_MAPPED) && i->tags) {
+		i->tags = 0;
+		code = XEMBED_WINDOW_DEACTIVATE;
+		XUnmapWindow(dpy, i->win);
+		setclientstate(i, WithdrawnState);
+	}
+	else
+		return;
+	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
+			systray->win, XEMBED_EMBEDDED_VERSION);
+}
+
+void
+updatesystray(void)
+{
+	XSetWindowAttributes wa;
+	XWindowChanges wc;
+	Client *i;
+	Monitor *m = systraytomon(NULL);
+	unsigned int x = m->mx + m->mw;
+	unsigned int w = 1;
+
+	if (!showsystray)
+		return;
+	if (!systray) {
+		/* init systray */
+		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
+			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
+		systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
+		wa.event_mask        = ButtonPressMask | ExposureMask;
+		wa.override_redirect = True;
+		wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
+		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
+		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
+				PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
+		XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
+		XMapRaised(dpy, systray->win);
+		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
+		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
+			sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
+			XSync(dpy, False);
+		}
+		else {
+			fprintf(stderr, "dwm: unable to obtain system tray.\n");
+			free(systray);
+			systray = NULL;
+			return;
+		}
+	}
+	for (w = 0, i = systray->icons; i; i = i->next) {
+		/* make sure the background color stays the same */
+		wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
+		XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
+		XMapRaised(dpy, i->win);
+		w += systrayspacing;
+		i->x = w;
+		XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
+		w += i->w;
+		if (i->mon != m)
+			i->mon = m;
+	}
+	w = w ? w + systrayspacing : 1;
+	x -= w;
+	XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
+	wc.x = x; wc.y = m->by; wc.width = w; wc.height = bh;
+	wc.stack_mode = Above; wc.sibling = m->barwin;
+	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
+	XMapWindow(dpy, systray->win);
+	XMapSubwindows(dpy, systray->win);
+	/* redraw background */
+	XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
+	XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
+	XSync(dpy, False);
+}
+
+void
 updatetitle(Client *c)
 {
 	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
@@ -2009,8 +2599,10 @@ updatewindowtype(Client *c)
 
 	if (state == netatom[NetWMFullscreen])
 		setfullscreen(c, 1);
-	if (wtype == netatom[NetWMWindowTypeDialog])
+	if (wtype == netatom[NetWMWindowTypeDialog]) {
+		c->iscentered = 1;
 		c->isfloating = 1;
+	}
 }
 
 void
@@ -2057,6 +2649,16 @@ wintoclient(Window w)
 	return NULL;
 }
 
+Client *
+wintosystrayicon(Window w) {
+	Client *i = NULL;
+
+	if (!showsystray || !w)
+		return i;
+	for (i = systray->icons; i && i->win != w; i = i->next) ;
+	return i;
+}
+
 Monitor *
 wintomon(Window w)
 {
@@ -2110,6 +2712,22 @@ xerrorstart(Display *dpy, XErrorEvent *ee)
 	return -1;
 }
 
+Monitor *
+systraytomon(Monitor *m) {
+	Monitor *t;
+	int i, n;
+	if(!systraypinning) {
+		if(!m)
+			return selmon;
+		return m == selmon ? m : NULL;
+	}
+	for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
+	for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
+	if(systraypinningfailfirst && n < systraypinning)
+		return mons;
+	return t;
+}
+
 void
 zoom(const Arg *arg)
 {
@@ -2136,6 +2754,7 @@ main(int argc, char *argv[])
 	if (!(dpy = XOpenDisplay(NULL)))
 		die("dwm: cannot open display");
 	checkotherwm();
+	autostart_exec();
 	setup();
 #ifdef __OpenBSD__
 	if (pledge("stdio rpath proc exec", NULL) == -1)
