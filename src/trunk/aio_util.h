#ifndef AIO_UTIL_H
#define AIO_UTIL_H
/**
 * Defines a function for pretty and formatted printing of error messages.
 * Includes the stringified version of the global errno variable.
 */
int aio_perror(const char * f, ...);

/*
 * Defines a function for pretty and formatted printing of debug messages,
 * if the compile-time constant DEBUG is included. You can do so by adding
 * the -DDEBUG switch. If the DEBUG constant is not defined, debug messages
 * will simply be ignored.
 */
#ifdef DEBUG
int aio_pdebug(const char * f, ...);
#else
#define aio_pdebug(f, ...)
#endif

#endif // AIO_UTIL_H

