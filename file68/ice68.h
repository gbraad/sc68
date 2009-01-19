/**
 * @ingroup  file68_lib
 * @file     file68/ice68.h
 * @author   Benjamin Gerard
 * @date     2003-09-06
 * @brief    ICE loader header.
 *
 */

/* $Id$ */

/* Copyright (C) 1998-2009 Benjamin Gerard */

#ifndef _FILE68_ICE68_H_
#define _FILE68_ICE68_H_

#include "istream68.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup  file68_ice68  ICE loader support.
 *  @ingroup   file68_lib
 *
 *    Provides functions for loading ICE stream.
 *
 *  @{
 */

/** Get ICE! depacker version.
 *
 *  @retval   1  ICE! is supported but unknown version
 *  @retval   0  ICE! is not supported
 *  @return  ICE! depacker version
 *
 *  @see unice68_ice_version()
 */
int ice68_version(void);

/** Test ice file header magic header.
 *
 *  @param  buffer  Buffer containing at least 12 bytes from ice header.
 *
 *  @retval  1  buffer seems to be ice packed..
 *  @retval  0  buffer is not ice packed.
 */
int ice68_is_magic(const void * buffer);

/** Load an iced stream.
 *
 *    The ice68_load() function loads and depack an ice packed file from a
 *    stream and returns a allocate buffer with unpacked data.
 *
 * @param  is     Stream to load (must be opened in read mode).
 * @param  ulen   Pointer to save uncompressed size.
 *
 * @return Pointer to the uncompressed data buffer.
 * @retval 0 Error
 */
void * ice68_load(istream68_t * is, int * ulen);

/** Load an iced file.
 *
 * @param  fname    File to load.
 * @param  ulen     Pointer to save uncompressed size.
 *
 * @return Pointer to the uncompressed data buffer.
 * @retval 0 Error
 *
 * @see ice68_load()
 */
void * ice68_load_file(const char * fname, int * ulen);

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _FILE68_ICE68_H_ */