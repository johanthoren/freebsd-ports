--- st.c.orig	2020-06-19 09:29:45 UTC
+++ st.c
@@ -35,6 +35,7 @@
 #define ESC_ARG_SIZ   16
 #define STR_BUF_SIZ   ESC_BUF_SIZ
 #define STR_ARG_SIZ   ESC_ARG_SIZ
+#define HISTSIZE      2000
 
 /* macros */
 #define IS_SET(flag)		((term.mode & (flag)) != 0)
@@ -42,6 +43,10 @@
 #define ISCONTROLC1(c)		(BETWEEN(c, 0x80, 0x9f))
 #define ISCONTROL(c)		(ISCONTROLC0(c) || ISCONTROLC1(c))
 #define ISDELIM(u)		(u && wcschr(worddelimiters, u))
+#define TLINE(y)		((y) < term.scr ? term.hist[((y) + term.histi - \
+				term.scr + HISTSIZE + 1) % HISTSIZE] : \
+				term.line[(y) - term.scr])
+#define TLINE_HIST(y)           ((y) <= HISTSIZE-term.row+2 ? term.hist[(y)] : term.line[(y-HISTSIZE+term.row-3)])
 
 enum term_mode {
 	MODE_WRAP        = 1 << 0,
@@ -115,6 +120,9 @@ typedef struct {
 	int col;      /* nb col */
 	Line *line;   /* screen */
 	Line *alt;    /* alternate screen */
+	Line hist[HISTSIZE]; /* history buffer */
+	int histi;    /* history index */
+	int scr;      /* scroll back */
 	int *dirty;   /* dirtyness of lines */
 	TCursor c;    /* cursor */
 	int ocx;      /* old cursor col */
@@ -153,8 +161,10 @@ typedef struct {
 } STREscape;
 
 static void execsh(char *, char **);
+static char *getcwd_by_pid(pid_t pid);
 static void stty(char **);
 static void sigchld(int);
+static void sigusr1(int);
 static void ttywriteraw(const char *, size_t);
 
 static void csidump(void);
@@ -184,8 +194,8 @@ static void tnewline(int);
 static void tputtab(int);
 static void tputc(Rune);
 static void treset(void);
-static void tscrollup(int, int);
-static void tscrolldown(int, int);
+static void tscrollup(int, int, int);
+static void tscrolldown(int, int, int);
 static void tsetattr(int *, int);
 static void tsetchar(Rune, Glyph *, int, int);
 static void tsetdirt(int, int);
@@ -200,6 +210,8 @@ static void tdefutf8(char);
 static int32_t tdefcolor(int *, int *, int);
 static void tdeftran(char);
 static void tstrsequence(uchar);
+static void tsetcolor(int, int, int, uint32_t, uint32_t);
+static char * findlastany(char *, const char**, size_t);
 
 static void drawregion(int, int, int, int);
 
@@ -414,15 +426,29 @@ tlinelen(int y)
 {
 	int i = term.col;
 
-	if (term.line[y][i - 1].mode & ATTR_WRAP)
+	if (TLINE(y)[i - 1].mode & ATTR_WRAP)
 		return i;
 
-	while (i > 0 && term.line[y][i - 1].u == ' ')
+	while (i > 0 && TLINE(y)[i - 1].u == ' ')
 		--i;
 
 	return i;
 }
 
+int
+tlinehistlen(int y)
+{
+	int i = term.col;
+
+	if (TLINE_HIST(y)[i - 1].mode & ATTR_WRAP)
+		return i;
+
+	while (i > 0 && TLINE_HIST(y)[i - 1].u == ' ')
+		--i;
+
+	return i;
+}
+
 void
 selstart(int col, int row, int snap)
 {
@@ -526,7 +552,7 @@ selsnap(int *x, int *y, int direction)
 		 * Snap around if the word wraps around at the end or
 		 * beginning of a line.
 		 */
-		prevgp = &term.line[*y][*x];
+		prevgp = &TLINE(*y)[*x];
 		prevdelim = ISDELIM(prevgp->u);
 		for (;;) {
 			newx = *x + direction;
@@ -541,14 +567,14 @@ selsnap(int *x, int *y, int direction)
 					yt = *y, xt = *x;
 				else
 					yt = newy, xt = newx;
-				if (!(term.line[yt][xt].mode & ATTR_WRAP))
+				if (!(TLINE(yt)[xt].mode & ATTR_WRAP))
 					break;
 			}
 
 			if (newx >= tlinelen(newy))
 				break;
 
-			gp = &term.line[newy][newx];
+			gp = &TLINE(newy)[newx];
 			delim = ISDELIM(gp->u);
 			if (!(gp->mode & ATTR_WDUMMY) && (delim != prevdelim
 					|| (delim && gp->u != prevgp->u)))
@@ -569,14 +595,14 @@ selsnap(int *x, int *y, int direction)
 		*x = (direction < 0) ? 0 : term.col - 1;
 		if (direction < 0) {
 			for (; *y > 0; *y += direction) {
-				if (!(term.line[*y-1][term.col-1].mode
+				if (!(TLINE(*y-1)[term.col-1].mode
 						& ATTR_WRAP)) {
 					break;
 				}
 			}
 		} else if (direction > 0) {
 			for (; *y < term.row-1; *y += direction) {
-				if (!(term.line[*y][term.col-1].mode
+				if (!(TLINE(*y)[term.col-1].mode
 						& ATTR_WRAP)) {
 					break;
 				}
@@ -607,13 +633,13 @@ getsel(void)
 		}
 
 		if (sel.type == SEL_RECTANGULAR) {
-			gp = &term.line[y][sel.nb.x];
+			gp = &TLINE(y)[sel.nb.x];
 			lastx = sel.ne.x;
 		} else {
-			gp = &term.line[y][sel.nb.y == y ? sel.nb.x : 0];
+			gp = &TLINE(y)[sel.nb.y == y ? sel.nb.x : 0];
 			lastx = (sel.ne.y == y) ? sel.ne.x : term.col-1;
 		}
-		last = &term.line[y][MIN(lastx, linelen-1)];
+		last = &TLINE(y)[MIN(lastx, linelen-1)];
 		while (last >= gp && last->u == ' ')
 			--last;
 
@@ -715,6 +741,13 @@ execsh(char *cmd, char **args)
 }
 
 void
+sigusr1(int unused)
+{
+  static Arg a = {.v = externalpipe_sigusr1};
+  externalpipe(&a);
+}
+
+void
 sigchld(int a)
 {
 	int stat;
@@ -760,6 +793,12 @@ stty(char **args)
 int
 ttynew(char *line, char *cmd, char *out, char **args)
 {
+	static struct sigaction sa;
+	sa.sa_handler = sigusr1;
+	sigemptyset(&sa.sa_mask);
+	sa.sa_flags = SA_RESTART;
+	sigaction(SIGUSR1, &sa, NULL);
+
 	int m, s;
 
 	if (out) {
@@ -848,7 +887,10 @@ void
 ttywrite(const char *s, size_t n, int may_echo)
 {
 	const char *next;
+	Arg arg = (Arg) { .i = term.scr };
 
+	kscrolldown(&arg);
+
 	if (may_echo && IS_SET(MODE_ECHO))
 		twrite(s, n, 1);
 
@@ -1047,6 +1089,11 @@ tnew(int col, int row)
 	treset();
 }
 
+int tisaltscr(void)
+{
+	return IS_SET(MODE_ALTSCREEN);
+}
+
 void
 tswapscreen(void)
 {
@@ -1059,13 +1106,73 @@ tswapscreen(void)
 }
 
 void
-tscrolldown(int orig, int n)
+kscrolldown(const Arg* a)
 {
+	int n = a->i;
+
+	if (n < 0)
+		n = term.row + n;
+
+	if (n > term.scr)
+		n = term.scr;
+
+	if (term.scr > 0) {
+		term.scr -= n;
+		selscroll(0, -n);
+		tfulldirt();
+	}
+}
+
+void
+kscrollup(const Arg* a)
+{
+	int n = a->i;
+
+	if (n < 0)
+		n = term.row + n;
+
+	if (term.scr <= HISTSIZE-n) {
+		term.scr += n;
+		selscroll(0, n);
+		tfulldirt();
+	}
+}
+
+void
+newterm(const Arg* a)
+{
+	switch (fork()) {
+	case -1:
+		die("fork failed: %s\n", strerror(errno));
+		break;
+	case 0:
+		chdir(getcwd_by_pid(pid));
+		execlp("st", "./st", NULL);
+		break;
+	}
+}
+
+static char *getcwd_by_pid(pid_t pid) {
+	char buf[32];
+	snprintf(buf, sizeof buf, "/proc/%d/cwd", pid);
+	return realpath(buf, NULL);
+}
+
+void
+tscrolldown(int orig, int n, int copyhist)
+{
 	int i;
 	Line temp;
 
 	LIMIT(n, 0, term.bot-orig+1);
 
+	if (copyhist) {
+		term.histi = (term.histi - 1 + HISTSIZE) % HISTSIZE;
+		temp = term.hist[term.histi];
+		term.hist[term.histi] = term.line[term.bot];
+		term.line[term.bot] = temp;
+	}
+
 	tsetdirt(orig, term.bot-n);
 	tclearregion(0, term.bot-n+1, term.col-1, term.bot);
 
@@ -1075,17 +1182,28 @@ tscrolldown(int orig, int n)
 		term.line[i-n] = temp;
 	}
 
-	selscroll(orig, n);
+	if (term.scr == 0)
+		selscroll(orig, n);
 }
 
 void
-tscrollup(int orig, int n)
+tscrollup(int orig, int n, int copyhist)
 {
 	int i;
 	Line temp;
 
 	LIMIT(n, 0, term.bot-orig+1);
 
+	if (copyhist) {
+		term.histi = (term.histi + 1) % HISTSIZE;
+		temp = term.hist[term.histi];
+		term.hist[term.histi] = term.line[orig];
+		term.line[orig] = temp;
+	}
+
+	if (term.scr > 0 && term.scr < HISTSIZE)
+		term.scr = MIN(term.scr + n, HISTSIZE-1);
+
 	tclearregion(0, orig, term.col-1, orig+n-1);
 	tsetdirt(orig+n, term.bot);
 
@@ -1095,7 +1213,8 @@ tscrollup(int orig, int n)
 		term.line[i+n] = temp;
 	}
 
-	selscroll(orig, -n);
+	if (term.scr == 0)
+		selscroll(orig, -n);
 }
 
 void
@@ -1124,7 +1243,7 @@ tnewline(int first_col)
 	int y = term.c.y;
 
 	if (y == term.bot) {
-		tscrollup(term.top, 1);
+		tscrollup(term.top, 1, 1);
 	} else {
 		y++;
 	}
@@ -1219,6 +1338,9 @@ tsetchar(Rune u, Glyph *attr, int x, int y)
 	term.dirty[y] = 1;
 	term.line[y][x] = *attr;
 	term.line[y][x].u = u;
+
+	if (isboxdraw(u))
+		term.line[y][x].mode |= ATTR_BOXDRAW;
 }
 
 void
@@ -1289,14 +1411,14 @@ void
 tinsertblankline(int n)
 {
 	if (BETWEEN(term.c.y, term.top, term.bot))
-		tscrolldown(term.c.y, n);
+		tscrolldown(term.c.y, n, 0);
 }
 
 void
 tdeleteline(int n)
 {
 	if (BETWEEN(term.c.y, term.top, term.bot))
-		tscrollup(term.c.y, n);
+		tscrollup(term.c.y, n, 0);
 }
 
 int32_t
@@ -1733,11 +1855,11 @@ csihandle(void)
 		break;
 	case 'S': /* SU -- Scroll <n> line up */
 		DEFAULT(csiescseq.arg[0], 1);
-		tscrollup(term.top, csiescseq.arg[0]);
+		tscrollup(term.top, csiescseq.arg[0], 0);
 		break;
 	case 'T': /* SD -- Scroll <n> line down */
 		DEFAULT(csiescseq.arg[0], 1);
-		tscrolldown(term.top, csiescseq.arg[0]);
+		tscrolldown(term.top, csiescseq.arg[0], 0);
 		break;
 	case 'L': /* IL -- Insert <n> blank lines */
 		DEFAULT(csiescseq.arg[0], 1);
@@ -1853,7 +1975,15 @@ strhandle(void)
 	case ']': /* OSC -- Operating System Command */
 		switch (par) {
 		case 0:
+			if (narg > 1) {
+				xsettitle(strescseq.args[1]);
+				xseticontitle(strescseq.args[1]);
+			}
+			return;
 		case 1:
+			if (narg > 1)
+				xseticontitle(strescseq.args[1]);
+			return;
 		case 2:
 			if (narg > 1)
 				xsettitle(strescseq.args[1]);
@@ -1882,10 +2012,8 @@ strhandle(void)
 				fprintf(stderr, "erresc: invalid color j=%d, p=%s\n",
 				        j, p ? p : "(null)");
 			} else {
-				/*
-				 * TODO if defaultbg color is changed, borders
-				 * are dirty
-				 */
+				if (j == defaultbg)
+					xclearwin();
 				redraw();
 			}
 			return;
@@ -1927,6 +2055,61 @@ strparse(void)
 }
 
 void
+externalpipe(const Arg *arg)
+{
+	int to[2];
+	char buf[UTF_SIZ];
+	void (*oldsigpipe)(int);
+	Glyph *bp, *end;
+	int lastpos, n, newline;
+
+	if (pipe(to) == -1)
+		return;
+
+	switch (fork()) {
+	case -1:
+		close(to[0]);
+		close(to[1]);
+		return;
+	case 0:
+		dup2(to[0], STDIN_FILENO);
+		close(to[0]);
+		close(to[1]);
+		execvp(((char **)arg->v)[0], (char **)arg->v);
+		fprintf(stderr, "st: execvp %s\n", ((char **)arg->v)[0]);
+		perror("failed");
+		exit(0);
+	}
+
+	close(to[0]);
+	/* ignore sigpipe for now, in case child exists early */
+	oldsigpipe = signal(SIGPIPE, SIG_IGN);
+	newline = 0;
+	for (n = 0; n <= HISTSIZE + 2; n++) {
+		bp = TLINE_HIST(n);
+		lastpos = MIN(tlinehistlen(n) + 1, term.col) - 1;
+		if (lastpos < 0)
+			break;
+        if (lastpos == 0)
+            continue;
+		end = &bp[lastpos + 1];
+		for (; bp < end; ++bp)
+			if (xwrite(to[1], buf, utf8encode(bp->u, buf)) < 0)
+				break;
+		if ((newline = TLINE_HIST(n)[lastpos].mode & ATTR_WRAP))
+			continue;
+		if (xwrite(to[1], "\n", 1) < 0)
+			break;
+		newline = 0;
+	}
+	if (newline)
+		(void)xwrite(to[1], "\n", 1);
+	close(to[1]);
+	/* restore */
+	signal(SIGPIPE, oldsigpipe);
+}
+
+void
 strdump(void)
 {
 	size_t i;
@@ -2241,7 +2424,7 @@ eschandle(uchar ascii)
 		return 0;
 	case 'D': /* IND -- Linefeed */
 		if (term.c.y == term.bot) {
-			tscrollup(term.top, 1);
+			tscrollup(term.top, 1, 1);
 		} else {
 			tmoveto(term.c.x, term.c.y+1);
 		}
@@ -2254,7 +2437,7 @@ eschandle(uchar ascii)
 		break;
 	case 'M': /* RI -- Reverse index */
 		if (term.c.y == term.top) {
-			tscrolldown(term.top, 1);
+			tscrolldown(term.top, 1, 1);
 		} else {
 			tmoveto(term.c.x, term.c.y-1);
 		}
@@ -2464,7 +2647,7 @@ twrite(const char *buf, int buflen, int show_ctrl)
 void
 tresize(int col, int row)
 {
-	int i;
+	int i, j;
 	int minrow = MIN(row, term.row);
 	int mincol = MIN(col, term.col);
 	int *bp;
@@ -2501,6 +2684,14 @@ tresize(int col, int row)
 	term.dirty = xrealloc(term.dirty, row * sizeof(*term.dirty));
 	term.tabs = xrealloc(term.tabs, col * sizeof(*term.tabs));
 
+	for (i = 0; i < HISTSIZE; i++) {
+		term.hist[i] = xrealloc(term.hist[i], col * sizeof(Glyph));
+		for (j = mincol; j < col; j++) {
+			term.hist[i][j] = term.c.attr;
+			term.hist[i][j].u = ' ';
+		}
+	}
+
 	/* resize each row to new width, zero-pad if needed */
 	for (i = 0; i < minrow; i++) {
 		term.line[i] = xrealloc(term.line[i], col * sizeof(Glyph));
@@ -2559,7 +2750,7 @@ drawregion(int x1, int y1, int x2, int y2)
 			continue;
 
 		term.dirty[y] = 0;
-		xdrawline(term.line[y], x1, y, x2);
+		xdrawline(TLINE(y), x1, y, x2);
 	}
 }
 
@@ -2580,8 +2771,9 @@ draw(void)
 		cx--;
 
 	drawregion(0, 0, term.col, term.row);
-	xdrawcursor(cx, term.c.y, term.line[term.c.y][cx],
-			term.ocx, term.ocy, term.line[term.ocy][term.ocx]);
+	if (term.scr == 0)
+		xdrawcursor(cx, term.c.y, term.line[term.c.y][cx],
+				term.ocx, term.ocy, term.line[term.ocy][term.ocx]);
 	term.ocx = cx;
 	term.ocy = term.c.y;
 	xfinishdraw();
@@ -2594,4 +2786,126 @@ redraw(void)
 {
 	tfulldirt();
 	draw();
+}
+
+void
+tsetcolor( int row, int start, int end, uint32_t fg, uint32_t bg )
+{
+	int i = start;
+	for( ; i < end; ++i )
+	{
+		term.line[row][i].fg = fg;
+		term.line[row][i].bg = bg;
+	}
+}
+
+char *
+findlastany(char *str, const char** find, size_t len)
+{
+	char* found = NULL;
+	int i = 0;
+	for(found = str + strlen(str) - 1; found >= str; --found) {
+		for(i = 0; i < len; i++) {
+			if(strncmp(found, find[i], strlen(find[i])) == 0) {
+				return found;
+			}
+		}
+	}
+
+	return NULL;
+}
+
+/*
+** Select and copy the previous url on screen (do nothing if there's no url).
+**
+** FIXME: doesn't handle urls that span multiple lines; will need to add support
+**        for multiline "getsel()" first
+*/
+void
+copyurl(const Arg *arg) {
+	/* () and [] can appear in urls, but excluding them here will reduce false
+	 * positives when figuring out where a given url ends.
+	 */
+	static char URLCHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
+		"abcdefghijklmnopqrstuvwxyz"
+		"0123456789-._~:/?#@!$&'*+,;=%";
+
+	static const char* URLSTRINGS[] = {"http://", "https://"};
+
+	/* remove highlighting from previous selection if any */
+	if(sel.ob.x >= 0 && sel.oe.x >= 0)
+		tsetcolor(sel.nb.y, sel.ob.x, sel.oe.x + 1, defaultfg, defaultbg);
+
+	int i = 0,
+		row = 0, /* row of current URL */
+		col = 0, /* column of current URL start */
+		startrow = 0, /* row of last occurrence */
+		colend = 0, /* column of last occurrence */
+		passes = 0; /* how many rows have been scanned */
+
+	char *linestr = calloc(sizeof(char), term.col+1); /* assume ascii */
+	char *c = NULL,
+		 *match = NULL;
+
+	row = (sel.ob.x >= 0 && sel.nb.y > 0) ? sel.nb.y : term.bot;
+	LIMIT(row, term.top, term.bot);
+	startrow = row;
+
+	colend = (sel.ob.x >= 0 && sel.nb.y > 0) ? sel.nb.x : term.col;
+	LIMIT(colend, 0, term.col);
+
+	/*
+ 	** Scan from (term.bot,term.col) to (0,0) and find
+	** next occurrance of a URL
+	*/
+	while(passes !=term.bot + 2) {
+		/* Read in each column of every row until
+ 		** we hit previous occurrence of URL
+		*/
+		for (col = 0, i = 0; col < colend; ++col,++i) {
+			/* assume ascii */
+			if (term.line[row][col].u > 127)
+				continue;
+			linestr[i] = term.line[row][col].u;
+		}
+		linestr[term.col] = '\0';
+
+		if ((match = findlastany(linestr, URLSTRINGS,
+						sizeof(URLSTRINGS)/sizeof(URLSTRINGS[0]))))
+			break;
+
+		if (--row < term.top)
+			row = term.bot;
+
+		colend = term.col;
+		passes++;
+	};
+
+	if (match) {
+		/* must happen before trim */
+		selclear();
+		sel.ob.x = strlen(linestr) - strlen(match);
+
+		/* trim the rest of the line from the url match */
+		for (c = match; *c != '\0'; ++c)
+			if (!strchr(URLCHARS, *c)) {
+				*c = '\0';
+				break;
+			}
+
+		/* highlight selection by inverting terminal colors */
+		tsetcolor(row, sel.ob.x, sel.ob.x + strlen( match ), defaultbg, defaultfg);
+
+		/* select and copy */
+		sel.mode = 1;
+		sel.type = SEL_REGULAR;
+		sel.oe.x = sel.ob.x + strlen(match)-1;
+		sel.ob.y = sel.oe.y = row;
+		selnormalize();
+		tsetdirt(sel.nb.y, sel.ne.y);
+		xsetsel(getsel());
+		xclipcopy();
+	}
+
+	free(linestr);
 }
