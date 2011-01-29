#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

int aio_perror(const char * f, ...)
{
    int r1, r2, e = errno;
    va_list ap;
    va_start(ap, f);
    if ((r1 = vfprintf(stderr, f, ap)) < 0)
    {
        va_end(ap);
        return r1;
    }
    if ((r2 = fprintf(stderr, ": %s\n", strerror(e))) < 0)
    {
        va_end(ap);
        return r2;
    }
    fflush(stderr);
    va_end(ap);
    return r1 + r2;
}

#ifdef DEBUG
int aio_pdebug(const char * f, ...)
{
    int r;
    va_list ap;
    va_start(ap, f);
    r = vprintf(f, ap);
    fflush(stdout);
    va_end(ap);
    return r;
}
#endif

