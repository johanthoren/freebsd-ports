--- win.h.orig	2020-06-19 09:29:45 UTC
+++ win.h
@@ -30,6 +30,7 @@ void xdrawline(Line, int, int, int);
 void xfinishdraw(void);
 void xloadcols(void);
 int xsetcolorname(int, const char *);
+void xseticontitle(char *);
 void xsettitle(char *);
 int xsetcursor(int);
 void xsetmode(int, unsigned int);
@@ -37,3 +38,4 @@ void xsetpointermotion(int);
 void xsetsel(char *);
 int xstartdraw(void);
 void xximspot(int, int);
+void xclearwin(void);
