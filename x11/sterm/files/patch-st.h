--- st.h.orig	2020-06-19 09:29:45 UTC
+++ st.h
@@ -33,6 +33,7 @@ enum glyph_attribute {
 	ATTR_WRAP       = 1 << 8,
 	ATTR_WIDE       = 1 << 9,
 	ATTR_WDUMMY     = 1 << 10,
+	ATTR_BOXDRAW    = 1 << 11,
 	ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
 };
 
@@ -81,12 +82,19 @@ void die(const char *, ...);
 void redraw(void);
 void draw(void);
 
+void externalpipe(const Arg *);
+void kscrolldown(const Arg *);
+void kscrollup(const Arg *);
+void newterm(const Arg *);
+void opencopied(const Arg *);
 void printscreen(const Arg *);
 void printsel(const Arg *);
 void sendbreak(const Arg *);
 void toggleprinter(const Arg *);
+void copyurl(const Arg *);
 
 int tattrset(int);
+int tisaltscr(void);
 void tnew(int, int);
 void tresize(int, int);
 void tsetdirtattr(int);
@@ -111,7 +119,16 @@ void *xmalloc(size_t);
 void *xrealloc(void *, size_t);
 char *xstrdup(char *);
 
+int isboxdraw(Rune);
+ushort boxdrawindex(const Glyph *);
+#ifdef XFT_VERSION
+/* only exposed to x.c, otherwise we'll need Xft.h for the types */
+void boxdraw_xinit(Display *, Colormap, XftDraw *, Visual *);
+void drawboxes(int, int, int, int, XftColor *, XftColor *, const XftGlyphFontSpec *, int);
+#endif
+
 /* config.h globals */
+extern char *externalpipe_sigusr1[];
 extern char *utmp;
 extern char *scroll;
 extern char *stty_args;
@@ -123,3 +140,5 @@ extern char *termname;
 extern unsigned int tabspaces;
 extern unsigned int defaultfg;
 extern unsigned int defaultbg;
+extern float alpha;
+extern const int boxdraw, boxdraw_bold, boxdraw_braille;
