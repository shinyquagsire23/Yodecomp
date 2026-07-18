# SDL's Java classes are called via JNI from the native library — keep them.
-keep class org.libsdl.app.** { *; }
-keep class org.yodecomp.app.** { *; }
