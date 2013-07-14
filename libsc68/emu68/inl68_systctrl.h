/**
 * @ingroup   emu68_lib_inline
 * @file      emu68/inl68_systctrl.h
 * @author    Benjamin Gerard
 * @date      2009/05/18
 * @brief     68k system control operation inlines.
 */
/* Time-stamp: <2013-07-14 19:42:13 ben>  */

/* Copyright (C) 1998-2013 Benjamin Gerard */

#ifndef _INL68_SYSTCTRL_H_
#define _INL68_SYSTCTRL_H_

static inline
void inl_andtosr68(emu68_t * const emu68, int68_t v)
{
  emu68->reg.sr &= v;
}

static inline
void inl_orrtosr68(emu68_t * const emu68, int68_t v)
{
  emu68->reg.sr |= v;
}

static inline
void inl_eortosr68(emu68_t * const emu68, int68_t v)
{
  emu68->reg.sr ^= v;
}

/**
 * @TODO  Implement reset instruction which is supposed to reset
 *        external chips.
 */
static inline
void inl_reset68(emu68_t * const emu68)
{
  if ( emu68->reg.sr & SR_S ) {
    /* $$$ Not it is suppose to do; it's a trick */
    emu68->status = EMU68_HLT;
    exception68(emu68, HWRESET_VECTOR, -1);
  } else {
    exception68(emu68, PRIVV_VECTOR, -1);
  }
}

static inline
void inl_stop68(emu68_t * const emu68)
{
  const u16 stop_sr = (u16) get_nextw();
  if ( emu68->reg.sr & SR_S ) {
    /* Superuser mode */
    const int save_sr = emu68->reg.sr;

    /* set the stop value and trigger status change exception */
    emu68->reg.sr = stop_sr;
    emu68->status = EMU68_STP;
    exception68(emu68, HWSTOP_VECTOR, -1);

    /* 68k trace mode release the stop mode. */
    if ((save_sr & (SR_T0|SR_T1)) == SR_T) {
      emu68->reg.sr |= SR_T;            /* $$$ or save_sr */
      if (emu68->status == EMU68_STP)
        emu68->status = EMU68_NRM;      /* Trace disable stop mode */
      /* Trace exception will be handle later out of here. */
    }
  } else {
    /* User mode triggers a privilege violation exception */
    exception68(emu68, PRIVV_VECTOR, -1);
  }
}

static inline
void inl_trapv68(emu68_t * const emu68)
{
  if (REG68.sr & SR_V) {
    inl_exception68(emu68, TRAPV_VECTOR, -1);
  }
}

static inline
void inl_trap68(emu68_t * const emu68, const int n)
{
  inl_exception68(emu68,TRAP_VECTOR(n), -1);
}

static inline
void inl_chk68(emu68_t * const emu68, const int68_t a, const int68_t b)
{
  REG68.sr &= 0xFF00 | (SR_X|SR_N);
  REG68.sr |= !b << SR_Z_BIT;
  if ( b < 0 ) {
    REG68.sr |= SR_N;
    inl_exception68(emu68, CHK_VECTOR, -1);
  } else if ( b > a ) {
    REG68.sr &= ~SR_N;
    inl_exception68(emu68, CHK_VECTOR, -1);
  }
}

static inline
void inl_illegal68(emu68_t * const emu68)
{

  inl_exception68(emu68, ILLEGAL_VECTOR, -1);
}

#endif /* #ifndef _INL68_SYSTCTRL_H_ */
