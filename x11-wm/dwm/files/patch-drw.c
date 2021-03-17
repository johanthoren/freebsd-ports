--- drw.c.orig	2019-02-02 12:55:28 UTC
+++ drw.c
@@ -95,6 +95,7 @@ drw_free(Drw *drw)
 {
 	XFreePixmap(drw->dpy, drw->drawable);
 	XFreeGC(drw->dpy, drw->gc);
+	drw_fontset_free(drw->fonts);
 	free(drw);
 }
 
