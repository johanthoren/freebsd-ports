--- binding.gyp.orig	2020-11-13 05:47:17 UTC
+++ binding.gyp
@@ -14,29 +14,6 @@
         "lib/to_string.cc",
         "lib/accessors.cc",
         "lib/util.cc",
-        "vendor/re2/bitstate.cc",
-        "vendor/re2/compile.cc",
-        "vendor/re2/dfa.cc",
-        "vendor/re2/filtered_re2.cc",
-        "vendor/re2/mimics_pcre.cc",
-        "vendor/re2/nfa.cc",
-        "vendor/re2/onepass.cc",
-        "vendor/re2/parse.cc",
-        "vendor/re2/perl_groups.cc",
-        "vendor/re2/prefilter.cc",
-        "vendor/re2/prefilter_tree.cc",
-        "vendor/re2/prog.cc",
-        "vendor/re2/re2.cc",
-        "vendor/re2/regexp.cc",
-        "vendor/re2/set.cc",
-        "vendor/re2/simplify.cc",
-        "vendor/re2/stringpiece.cc",
-        "vendor/re2/tostring.cc",
-        "vendor/re2/unicode_casefold.cc",
-        "vendor/re2/unicode_groups.cc",
-        "vendor/util/pcre.cc",
-        "vendor/util/rune.cc",
-        "vendor/util/strutil.cc"
       ],
       "cflags": [
         "-std=c++11",
@@ -45,8 +22,7 @@
         "-Wno-sign-compare",
         "-Wno-unused-parameter",
         "-Wno-missing-field-initializers",
-        "-Wno-cast-function-type",
-        "-O3",
+        "-Wno-bad-function-cast",
         "-g"
       ],
       "defines": [
@@ -54,7 +30,8 @@
         "NOMINMAX"
       ],
       "include_dirs": [
-        "<!(node -e \"require('nan')\")",
+        "%%PREFIX%%/include",
+        "%%NODEPATH%%/nan",
         "vendor"
       ],
       "xcode_settings": {
@@ -73,7 +50,7 @@
         ]
       },
       "conditions": [
-        ["OS==\"linux\"", {
+        ["OS==\"freebsd\"", {
           "cflags": [
             "-pthread"
           ],
