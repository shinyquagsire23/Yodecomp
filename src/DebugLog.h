// DebugLog.h — debug-only logging to a file next to the exe. Enabled by -D YODA_DEBUG
// (CMake option YODA_DEBUG=ON). Compiles to NOTHING when YODA_DEBUG is undefined, so the
// byte-match anchor build is unaffected. Use: YDBG(("tag=%s pos=0x%lx\n", tag, pos));
#ifndef YODA_DEBUGLOG_H
#define YODA_DEBUGLOG_H

#ifdef YODA_DEBUG
#include <stdio.h>
#include <stdarg.h>
static void YodaDbgLog(const char *fmt, ...)
{
    FILE *f = fopen("yoda_debug.log", "a");
    if (f == NULL)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}
#define YDBG(args) YodaDbgLog args
#else
#define YDBG(args) ((void)0)
#endif

#endif
