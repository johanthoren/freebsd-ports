--- dmenu.c.orig	2020-09-02 16:31:23 UTC
+++ dmenu.c
@@ -1,6 +1,7 @@
 /* See LICENSE file for copyright and license details. */
 #include <ctype.h>
 #include <locale.h>
+#include <math.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
@@ -26,18 +27,21 @@
 #define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)
 
 /* enums */
-enum { SchemeNorm, SchemeSel, SchemeOut, SchemeLast }; /* color schemes */
+enum { SchemeNorm, SchemeSel, SchemeNormHighlight, SchemeSelHighlight,
+       SchemeOut, SchemeLast }; /* color schemes */
 
+
 struct item {
 	char *text;
 	struct item *left, *right;
 	int out;
+	double distance;
 };
 
 static char text[BUFSIZ] = "";
 static char *embed;
 static int bh, mw, mh;
-static int inputw = 0, promptw;
+static int inputw = 0, promptw, passwd = 0;
 static int lrpad; /* sum of left and right padding */
 static size_t cursor;
 static struct item *items = NULL;
@@ -53,10 +57,15 @@ static XIC xic;
 static Drw *drw;
 static Clr *scheme[SchemeLast];
 
+static char *histfile;
+static char *histbuf, *histptr;
+static size_t histsz;
+
 #include "config.h"
 
-static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
-static char *(*fstrstr)(const char *, const char *) = strstr;
+static char * cistrstr(const char *s, const char *sub);
+static int (*fstrncmp)(const char *, const char *, size_t) = strncasecmp;
+static char *(*fstrstr)(const char *, const char *) = cistrstr;
 
 static void
 appenditem(struct item *item, struct item **list, struct item **last)
@@ -113,9 +122,49 @@ cistrstr(const char *s, const char *sub)
 	return NULL;
 }
 
+static void
+drawhighlights(struct item *item, int x, int y, int maxw)
+{
+	int i, indent;
+	char *highlight;
+	char c;
+
+	if (!(strlen(item->text) && strlen(text)))
+		return;
+
+	drw_setscheme(drw, scheme[item == sel
+	                   ? SchemeSelHighlight
+	                   : SchemeNormHighlight]);
+	for (i = 0, highlight = item->text; *highlight && text[i];) {
+if (!fstrncmp(&(*highlight), &text[i], 1)) {
+			/* get indentation */
+			c = *highlight;
+			*highlight = '\0';
+			indent = TEXTW(item->text);
+			*highlight = c;
+
+			/* highlight character */
+			c = highlight[1];
+			highlight[1] = '\0';
+			drw_text(
+				drw,
+				x + indent - (lrpad / 2),
+				y,
+				MIN(maxw - indent, TEXTW(highlight) - lrpad),
+				bh, 0, highlight, 0
+			);
+			highlight[1] = c;
+			i++;
+		}
+		highlight++;
+	}
+}
+
+
 static int
 drawitem(struct item *item, int x, int y, int w)
 {
+	int r;
 	if (item == sel)
 		drw_setscheme(drw, scheme[SchemeSel]);
 	else if (item->out)
@@ -123,7 +172,9 @@ drawitem(struct item *item, int x, int y, int w)
 	else
 		drw_setscheme(drw, scheme[SchemeNorm]);
 
-	return drw_text(drw, x, y, w, bh, lrpad / 2, item->text, 0);
+	r = drw_text(drw, x, y, w, bh, lrpad / 2, item->text, 0);
+	drawhighlights(item, x, y, w);
+	return r;
 }
 
 static void
@@ -132,6 +183,7 @@ drawmenu(void)
 	unsigned int curpos;
 	struct item *item;
 	int x = 0, y = 0, w;
+ char *censort;
 
 	drw_setscheme(drw, scheme[SchemeNorm]);
 	drw_rect(drw, 0, 0, mw, mh, 1, 1);
@@ -143,7 +195,12 @@ drawmenu(void)
 	/* draw input field */
 	w = (lines > 0 || !matches) ? mw - x : inputw;
 	drw_setscheme(drw, scheme[SchemeNorm]);
-	drw_text(drw, x, 0, w, bh, lrpad / 2, text, 0);
+	if (passwd) {
+	        censort = ecalloc(1, sizeof(text));
+		memset(censort, '.', strlen(text));
+		drw_text(drw, x, 0, w, bh, lrpad / 2, censort, 0);
+		free(censort);
+	} else drw_text(drw, x, 0, w, bh, lrpad / 2, text, 0);
 
 	curpos = TEXTW(text) - TEXTW(&text[cursor]);
 	if ((curpos += lrpad / 2 - 1) < w) {
@@ -210,9 +267,94 @@ grabkeyboard(void)
 	die("cannot grab keyboard");
 }
 
+int
+compare_distance(const void *a, const void *b)
+{
+	struct item *da = *(struct item **) a;
+	struct item *db = *(struct item **) b;
+
+	if (!db)
+		return 1;
+	if (!da)
+		return -1;
+
+	return da->distance == db->distance ? 0 : da->distance < db->distance ? -1 : 1;
+}
+
+void
+fuzzymatch(void)
+{
+	/* bang - we have so much memory */
+	struct item *it;
+	struct item **fuzzymatches = NULL;
+	char c;
+	int number_of_matches = 0, i, pidx, sidx, eidx;
+	int text_len = strlen(text), itext_len;
+
+	matches = matchend = NULL;
+
+	/* walk through all items */
+	for (it = items; it && it->text; it++) {
+		if (text_len) {
+			itext_len = strlen(it->text);
+			pidx = 0; /* pointer */
+			sidx = eidx = -1; /* start of match, end of match */
+			/* walk through item text */
+			for (i = 0; i < itext_len && (c = it->text[i]); i++) {
+				/* fuzzy match pattern */
+				if (!fstrncmp(&text[pidx], &c, 1)) {
+					if(sidx == -1)
+						sidx = i;
+					pidx++;
+					if (pidx == text_len) {
+						eidx = i;
+						break;
+					}
+				}
+			}
+			/* build list of matches */
+			if (eidx != -1) {
+				/* compute distance */
+				/* add penalty if match starts late (log(sidx+2))
+				 * add penalty for long a match without many matching characters */
+				it->distance = log(sidx + 2) + (double)(eidx - sidx - text_len);
+				/* fprintf(stderr, "distance %s %f\n", it->text, it->distance); */
+				appenditem(it, &matches, &matchend);
+				number_of_matches++;
+			}
+		} else {
+			appenditem(it, &matches, &matchend);
+		}
+	}
+
+	if (number_of_matches) {
+		/* initialize array with matches */
+		if (!(fuzzymatches = realloc(fuzzymatches, number_of_matches * sizeof(struct item*))))
+			die("cannot realloc %u bytes:", number_of_matches * sizeof(struct item*));
+		for (i = 0, it = matches; it && i < number_of_matches; i++, it = it->right) {
+			fuzzymatches[i] = it;
+		}
+		/* sort matches according to distance */
+		qsort(fuzzymatches, number_of_matches, sizeof(struct item*), compare_distance);
+		/* rebuild list of matches */
+		matches = matchend = NULL;
+		for (i = 0, it = fuzzymatches[i];  i < number_of_matches && it && \
+				it->text; i++, it = fuzzymatches[i]) {
+			appenditem(it, &matches, &matchend);
+		}
+		free(fuzzymatches);
+	}
+	curr = sel = matches;
+	calcoffsets();
+}
+
 static void
 match(void)
 {
+	if (fuzzy) {
+		fuzzymatch();
+		return;
+	}
 	static char **tokv = NULL;
 	static int tokn = 0;
 
@@ -305,6 +447,105 @@ movewordedge(int dir)
 }
 
 static void
+loadhistory(void)
+{
+	FILE *fp = NULL;
+	size_t sz;
+
+	if (!histfile)
+		return;
+	if (!(fp = fopen(histfile, "r")))
+		return;
+	fseek(fp, 0, SEEK_END);
+	sz = ftell(fp);
+	fseek(fp, 0, SEEK_SET);
+	if (sz) {
+		histsz = sz + 1 + BUFSIZ;
+		if (!(histbuf = malloc(histsz))) {
+			fprintf(stderr, "warning: cannot malloc %lu "\
+				"bytes", histsz);
+		} else {
+			histptr = histbuf + fread(histbuf, 1, sz, fp);
+			if (histptr <= histbuf) { /* fread error */
+				free(histbuf);
+				histbuf = NULL;
+				return;
+			}
+			if (histptr[-1] != '\n')
+				*histptr++ = '\n';
+			histptr[BUFSIZ - 1] = '\0';
+			*histptr = '\0';
+			histsz = histptr - histbuf + BUFSIZ;
+		}
+	}
+	fclose(fp);
+}
+
+static void
+navhistory(int dir)
+{
+	char *p;
+	size_t len = 0, textlen;
+
+	if (!histbuf)
+		return;
+	if (dir > 0) {
+		if (histptr == histbuf + histsz - BUFSIZ)
+			return;
+		while (*histptr && *histptr++ != '\n');
+		for (p = histptr; *p && *p++ != '\n'; len++);
+	} else {
+		if (histptr == histbuf)
+			return;
+		if (histptr == histbuf + histsz - BUFSIZ) {
+			textlen = strlen(text);
+			textlen = MIN(textlen, BUFSIZ - 1);
+			strncpy(histptr, text, textlen);
+			histptr[textlen] = '\0';
+		}
+		for (histptr--; histptr != histbuf && histptr[-1] != '\n';
+		     histptr--, len++);
+	}
+	len = MIN(len, BUFSIZ - 1);
+	strncpy(text, histptr, len);
+	text[len] = '\0';
+	cursor = len;
+	match();
+} 
+static void
+savehistory(char *str)
+{
+	unsigned int n, len = 0;
+	size_t slen;
+	char *p;
+	FILE *fp;
+
+	if (!histfile || !maxhist)
+		return;
+	if (!(slen = strlen(str)))
+		return;
+	if (histbuf && maxhist > 1) {
+		p = histbuf + histsz - BUFSIZ - 1; /* skip the last newline */
+		if (histnodup) {
+			for (; p != histbuf && p[-1] != '\n'; p--, len++);
+			n++;
+			if (slen == len && !strncmp(p, str, len)) {
+				return;
+			}
+		}
+		for (; p != histbuf; p--, len++)
+			if (p[-1] == '\n' && ++n + 1 > maxhist)
+				break;
+		fp = fopen(histfile, "w");
+		fwrite(p, 1, len + 1, fp);	/* plus the last newline */
+	} else {
+		fp = fopen(histfile, "w");
+	}
+	fwrite(str, 1, strlen(str), fp);
+	fclose(fp);
+}
+
+static void
 keypress(XKeyEvent *ev)
 {
 	char buf[32];
@@ -388,6 +629,8 @@ keypress(XKeyEvent *ev)
 		case XK_j: ksym = XK_Next;  break;
 		case XK_k: ksym = XK_Prior; break;
 		case XK_l: ksym = XK_Down;  break;
+		case XK_p: navhistory(-1); buf[0]=0; break;
+		case XK_n: navhistory(1); buf[0]=0; break;
 		default:
 			return;
 		}
@@ -466,6 +709,8 @@ insert:
 	case XK_KP_Enter:
 		puts((sel && !(ev->state & ShiftMask)) ? sel->text : text);
 		if (!(ev->state & ControlMask)) {
+			savehistory((sel && !(ev->state & ShiftMask))
+				    ? sel->text : text);
 			cleanup();
 			exit(0);
 		}
@@ -525,6 +770,11 @@ readstdin(void)
 	size_t i, imax = 0, size = 0;
 	unsigned int tmpmax = 0;
 
+  if(passwd){
+    inputw = lines = 0;
+    return;
+  }
+
 	/* read each line from stdin and add it to the item list */
 	for (i = 0; fgets(buf, sizeof buf, stdin); i++) {
 		if (i + 1 >= size / sizeof *items)
@@ -551,8 +801,19 @@ static void
 run(void)
 {
 	XEvent ev;
+	int i;
 
 	while (!XNextEvent(dpy, &ev)) {
+		if (preselected) {
+			for (i = 0; i < preselected; i++) {
+				if (sel && sel->right && (sel = sel->right) == next) {
+					curr = next;
+					calcoffsets();
+				}
+			}
+			drawmenu();
+			preselected = 0;
+		}
 		if (XFilterEvent(&ev, win))
 			continue;
 		switch(ev.type) {
@@ -689,8 +950,9 @@ setup(void)
 static void
 usage(void)
 {
-	fputs("usage: dmenu [-bfiv] [-l lines] [-p prompt] [-fn font] [-m monitor]\n"
-	      "             [-nb color] [-nf color] [-sb color] [-sf color] [-w windowid]\n", stderr);
+	fputs("usage: dmenu [-b] [-f] [-H histfile] [-i] [-l lines] [-p prompt] [-P] [-fn font] [-m monitor]\n"
+	      "             [-nb color] [-nf color] [-sb color] [-sf color]\n"
+	      "             [-nhb color] [-nhf color] [-shb color] [-shf color] [-w windowid] [-n number]\n", stderr);
 	exit(1);
 }
 
@@ -709,12 +971,18 @@ main(int argc, char *argv[])
 			topbar = 0;
 		else if (!strcmp(argv[i], "-f"))   /* grabs keyboard before reading stdin */
 			fast = 1;
-		else if (!strcmp(argv[i], "-i")) { /* case-insensitive item matching */
-			fstrncmp = strncasecmp;
-			fstrstr = cistrstr;
-		} else if (i + 1 == argc)
+		else if (!strcmp(argv[i], "-F"))   /* grabs keyboard before reading stdin */
+			fuzzy = 0;
+		else if (!strcmp(argv[i], "-s")) { /* case-sensitive item matching */
+			fstrncmp = strncmp;
+			fstrstr = strstr;
+		} else if (!strcmp(argv[i], "-P"))   /* is the input a password */
+		        passwd = 1;
+		else if (i + 1 == argc)
 			usage();
 		/* these options take one argument */
+		else if (!strcmp(argv[i], "-H"))
+			histfile = argv[++i];
 		else if (!strcmp(argv[i], "-l"))   /* number of lines in vertical list */
 			lines = atoi(argv[++i]);
 		else if (!strcmp(argv[i], "-m"))
@@ -731,8 +999,18 @@ main(int argc, char *argv[])
 			colors[SchemeSel][ColBg] = argv[++i];
 		else if (!strcmp(argv[i], "-sf"))  /* selected foreground color */
 			colors[SchemeSel][ColFg] = argv[++i];
+		else if (!strcmp(argv[i], "-nhb")) /* normal hi background color */
+			colors[SchemeNormHighlight][ColBg] = argv[++i];
+		else if (!strcmp(argv[i], "-nhf")) /* normal hi foreground color */
+			colors[SchemeNormHighlight][ColFg] = argv[++i];
+		else if (!strcmp(argv[i], "-shb")) /* selected hi background color */
+			colors[SchemeSelHighlight][ColBg] = argv[++i];
+		else if (!strcmp(argv[i], "-shf")) /* selected hi foreground color */
+			colors[SchemeSelHighlight][ColFg] = argv[++i];
 		else if (!strcmp(argv[i], "-w"))   /* embedding window id */
 			embed = argv[++i];
+		else if (!strcmp(argv[i], "-n"))   /* preselected item */
+			preselected = atoi(argv[++i]);
 		else
 			usage();
 
@@ -751,6 +1029,7 @@ main(int argc, char *argv[])
 	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
 		die("no fonts could be loaded.");
 	lrpad = drw->fonts->h;
+	loadhistory();
 
 #ifdef __OpenBSD__
 	if (pledge("stdio rpath", NULL) == -1)
