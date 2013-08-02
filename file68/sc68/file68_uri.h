/**
 * @ingroup  file68_lib
 * @file     sc68/file68_uri.h
 * @author   Benjamin Gerard
 * @date     2003-10-28
 * @brief    URI manipulation header.
 */
/* Time-stamp: <2013-08-02 23:04:12 ben> */

/* Copyright (C) 1998-2013 Benjamin Gerard */

#ifndef _FILE68_URI_H_
#define _FILE68_URI_H_

#ifndef FILE68_API
# include "file68_api.h"
#endif
#include "file68_vfs.h"
#include "file68_vfs_def.h"

#include <stdarg.h>

/**
 * @defgroup  file68_uri  URI manipulation
 * @ingroup   file68_lib
 *
 *    Provides functions for sc68 URI manipulation and access.
 *
 * @{
 */

/**
 * Enum for the scheme68_t::ismine() return value.
 */
enum {
  SCHEME68_READ   = VFS68_OPEN_READ,   /**< can be open for read.   */
  SCHEME68_WRITE  = VFS68_OPEN_WRITE,  /**< can be open for write.  */
  SCHEME68_ISMINE = SCHEME68_WRITE<<1, /**< URI might be mime.      */
  SCHEME68_REMOTE = SCHEME68_ISMINE<<1 /**< access might be remote. */
};

typedef struct scheme68_s scheme68_t;

struct scheme68_s {
  scheme68_t * next;                 /**< next scheme in the list.  */
  char       * name;                 /**< register name (curl ...). */

  /** is mine callback. */
  int       (*ismine)(const char *);

  /** VFS stream create callback. */
  vfs68_t * (*create)(const char *, int, int, va_list);
};

FILE68_API
/**
 * Register a scheme handler.
 *
 * @param  scheme  scheme info,
 *
 * @return error code
 * @retval 0     success
 * @retval -1    failure
 */
int uri68_register(scheme68_t * scheme);

FILE68_API
/**
 * Test if a scheme is local.
 *
 * @param  uri  uri to test
 *
 * @retval  1  scheme is local
 * @retcal  0  scheme may be remote
 */
int uri68_is_local(const char * uri);

FILE68_API
/**
 * Create a stream for an URI.
 *
 * @param  uri      URI or path
 * @param  mode     open mode (1:read, 2:write).
 * @param  argc     number of supplemental arguments
 *
 * @return stream
 * @retval 0 error
 */
vfs68_t * uri68_create_vfs(const char * uri, int mode, int argc, ...);

FILE68_API
/**
 * Retrieve scheme from an URI.
 *
 * @param  scheme   Buffer to store scheme or 0
 * @param  max      Size of scheme buffer
 * @param  uri      URI
 *
 * @return scheme length
 * @retval -1  on error
 * @retval  0  no scheme
 */
int uri68_get_scheme(char * scheme, int max, const char *uri);

/**
 * @}
 */

#endif
