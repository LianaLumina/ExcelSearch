/* Minimal config for libxls built with MSVC + win-iconv */
#ifndef LIBXLS_CONFIG_H
#define LIBXLS_CONFIG_H

/* Version string reported by xls_getVersion() */
#define PACKAGE_VERSION "1.6.2"

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Define to 1 if you have the <iconv.h> header file. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

#endif
