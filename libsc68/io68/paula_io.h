/**
 * @ingroup   io68_paula_devel
 * @file      io68/paula_io.h
 * @author    Benjamin Gerard
 * @date      1998/06/18
 * @brief     Paula IO plugin header.
 *
 * $Id$
 */

/* Copyright (C) 1998-2007 Benjamin Gerard */

#ifndef _IO68_PAULA_IO_H_
#define _IO68_PAULA_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "emu68/struct68.h"
#include "paulaemul.h"

/** @addtogroup  io68_paula_devel
 *  @{
 */

/** @name Paula (Amiga soundchip) IO plugin
 *  @{
 */

/** Initialize paula library. */
int paulaio_init(paula_parms_t * const parms);

/** Shutdown paula library. */
void paulaio_shutdown(void);

/** Create paula io instance.
 *
 *   @param   emu68  68000 emulator instance
 *   @param   parms  Paula parameters
 *
 *   @return  Created shifter instance
 *   @retval  0  
 */
io68_t * paulaio_create(emu68_t * const emu68, paula_parms_t * const parms);

/** Get/Set sampling rate.
 *
 * @param  io  Paula IO instance
 * @param  hz  0:read current sampling rate, >0:new requested sampling rate
 *
 * @return actual sampling rate
 *
 */ 
uint68_t paulaio_sampling_rate(io68_t * const io, uint68_t sampling_rate);

/** Get paula emulator instance. */
paula_t * paulaio_emulator(io68_t * const io);

/**@}*/

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _IO68_PAULA_IO_H_ */