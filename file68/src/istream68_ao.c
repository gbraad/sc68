/*
 *                      file68 - libao stream
 *            Copyright (C) 2001-2009 Ben(jamin) Gerard
 *           <benjihan -4t- users.sourceforge -d0t- net>
 *
 * This  program is  free  software: you  can  redistribute it  and/or
 * modify  it under the  terms of  the GNU  General Public  License as
 * published by the Free Software  Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 *
 * You should have  received a copy of the  GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "file68_api.h"
#include "istream68_ao.h"
#include "audio68.h"
#include "msg68.h"

#ifndef DEBUG_AO68_O
# define DEBUG_AO68_O 0
#endif
int ao68_feature = msg68_DEFAULT;


/* Define this if you want xiph libao support. */
#ifdef USE_AO

#include "istream68_def.h"
#include "alloc68.h"
#include "string68.h"

#include <ao/ao.h>
#include <string.h>

struct ao68_info_s {
  int                default_id;
  ao_device        * default_device;
  int                driver_id;
  ao_device        * device;
  ao_sample_format   format;
  ao_option        * options;
};

typedef struct ao68_info_s ao68_info_t;

/** istream ao structure. */
typedef struct {
  istream68_t istream;        /**< istream function.                */
  unsigned int count;         /**< current position.                */
  ao68_info_t ao;             /**< ao specific struct.              */
  int  no_overwrite;          /**< overwriting file output.         */
  char *outname;              /**< part of name[] for file driver.  */
  char defoutname[16];        /**< default outname for file driver. */
  char name[1];               /**< filename.                        */
} istream68_ao_t;

static volatile int init;
static unsigned int istream68_ao_defaut_rate = 44100;

/* $$$ AO is the only audio backend we have. No need for much
   complicated mechanism right now. */
unsigned int audio68_sampling_rate(const unsigned int rate)
{
  unsigned int f;
  if (!rate) {
    f = istream68_ao_defaut_rate;
  } else {
    f = rate;
    if (f < 8000u) {
      f = 8000u;
    } else if (f > 96000u) {
      f = 96000u;
    }
    istream68_ao_defaut_rate = f;
    msg68_info("audio68: sampling rate -- %uhz\n", f);
  }
  return f;
}

int istream68_ao_init(void)
{
  int err = -1;

  if (ao68_feature != msg68_DEFAULT) {
    ao68_feature =
      msg68_feature("audio","Xiph AO audio stream",DEBUG_AO68_O);
    if (ao68_feature == -1)  ao68_feature = msg68_DEFAULT;
  }

  if (init) {
    msg68_critical("ao68: init -- already done\n");
  } else {
    ao_initialize();
    err = 0;
    init = 1;
    TRACE68(ao68_feature,"ao68: initialized\n");
  }
  return err;
}

void istream68_ao_shutdown(void)
{
  if (init) {
    init = 0;
    ao_shutdown();
    TRACE68(ao68_feature,"ao68: shutdowned\n");
  }
  if (ao68_feature != msg68_DEFAULT) {
    msg68_feature_free(ao68_feature);
    ao68_feature = msg68_DEFAULT;
  }
}

static const char * isao_name(istream68_t * istream)
{
  istream68_ao_t * ism = (istream68_ao_t *)istream;

  return (!ism || !ism->name[0])
    ? 0
    : ism->name
    ;
}

static void dump_ao_info(const int id, const ao_info * info, const int full)
{
  if (info) {
    msg68_debug(
      "\n"
      "ao68: ao_driver #%02d\n"
      "ao68: -------------\n"
      "ao68:   type    : %s\n"
      "ao68:   name    : %s\n"
      "ao68:   short   : %s\n"
      "ao68:   author  : %s\n"
      "ao68:   comment : %s\n"
      "ao68:   prio    : %d\n",
      id, info->type==AO_TYPE_LIVE?"live":"file",
      strnevernull68(info->name),
      strnevernull68(info->short_name),
      strnevernull68(info->author),
      strnevernull68(info->comment),info->priority);
    if (full) {
      int i;
      msg68_debug("ao68:   options : %d\n", info->option_count);
      for (i=0; i<info->option_count; ++i) {
        msg68_debug("ao68:             - %s\n",info->options[i]);
      }
    }
  }
}

static int isao_open(istream68_t * istream)
{
  istream68_ao_t * is = (istream68_ao_t *)istream;
  int err = -1;
  ao_info * info;
  char * url;

  TRACE68(ao68_feature,"ao68: open -- '%s'\n", istream68_filename(istream));
  if (!is || is->ao.device) {
    goto error;
  }

  url = is->name;
  do {
    char * s = strchr(url,'/'), *s2;
    if (s) *s = '\0';
    s2 = strchr(url,'=');
    if (s2) {
      char * key = url;
      char * val = s2+1;
      *s2 = 0;

      TRACE68(ao68_feature,"ao68: open -- key[%s]='%s'\n", key, val);
      if (!strcmp68(key,"output")) {
        if (s) *s = '/';
        s = 0;
        TRACE68(ao68_feature,"ao68: open -- *FILENAME* '%s'\n", val);
        is->outname = val;
      } else if (!strcmp68(key,"driver")) {
        int id = !strcmp68(val,"default")
          ? ao_default_driver_id()
          : ao_driver_id(val);
        info = ao_driver_info(id);
        if (info) {
          is->ao.driver_id = id;
          TRACE68(ao68_feature,"ao68: open -- *DRIVER* #%d (%s) %s\n",
                  id, info->short_name, info->name);
        }
      } else if (!strcmp68(key,"rate")) {
        int frq = 0;
        while (*val>='0' && *val<='9') frq = frq*10 + *val++ - '0';
        if (frq > 0) {
          TRACE68(ao68_feature,"ao68: open -- *SAMPLING-RATE* %d\n",frq);
          is->ao.format.rate = frq;
        }
      } else if (!strcmp68(key,"format")) {
        while(*val) {
          switch (*val++) {

            /* ENDIANESS */
          case 'n': /* native (same as cpu) */
            TRACE68(ao68_feature,"ao68: open -- *NATIVE-ENDIAN*\n");
            is->ao.format.byte_format = AO_FMT_NATIVE;
            break;
          case 'l': /* little */
            TRACE68(ao68_feature,"ao68: open -- *LITTLE-ENDIAN*\n");
            is->ao.format.byte_format = AO_FMT_LITTLE;
            break;
          case 'b': /* big    */
            TRACE68(ao68_feature,"ao68: open -- *BIG-ENDIAN*\n");
            is->ao.format.byte_format = AO_FMT_BIG;
            break;

            /* SIGNE */
          case '+': /* unsigned */
            TRACE68(ao68_feature,"ao68: open -- *UNSIGNED*\n");
            break;
          case '-': /*   signed */
            TRACE68(ao68_feature,"ao68: open -- *SIGNED*\n");
            break;

            /* CHANNELS */
          case '1': /* mono, 1 channel */
            TRACE68(ao68_feature,"ao68: open -- *MONO*\n");
            is->ao.format.channels = 1;
            break;
          case '2': /* stereo, 2 channels */
            TRACE68(ao68_feature,"ao68: open -- *STEREO*\n");
            is->ao.format.channels = 2;
            break;

            /* FORMAT */
          case 'W': /* 16 bit */
            TRACE68(ao68_feature,"ao68: open -- *16-BIT*\n");
            is->ao.format.bits = 16;
            break;
          case 'B': /*  8 bit */
            TRACE68(ao68_feature,"ao68: open -- 8-BIT\n");
            is->ao.format.bits = 8;
            break;
          case 'F': /* float  */
            TRACE68(ao68_feature,"ao68: open -- ignoring float request\n");
            break;
          } /* switch */
        } /* while */
      } else {
        /* Unknown options -> append to driver */
        int res = ao_append_option(&is->ao.options, key, val);
        TRACE68(ao68_feature,"ao68: open -- append option [%s]='%s' => [%s]\n",
                key, val, strok68(!res));
      }
      *s2 = '=';
    } else {
      /* Options w/out '=' */
      if (!strcmp68(url,"drivers")) {
        /* List all drivers in debug */
        int id,n;
        ao_info ** infolist = ao_driver_info_list(&n);
        if (infolist) {
          for (id=0; id<n; ++id) {
            dump_ao_info(id,infolist[id],1);
          }
        }
      }
    }

    if (s) {
      *s = '/';
      url = s+1;
    } else {
      url = 0;
    }
  } while (url);

  info = ao_driver_info(is->ao.driver_id);
  if (!info) {
    goto error;
  }

  dump_ao_info(is->ao.driver_id,info,1);

  if (!is->outname) {
/*     char * ext = ao_file_extension(is->ao.driver_id); */
    /* $$$ ao_file_extension() is missing in action !!!
     *
     */
    char * ext = ".out";
    if (!strcmp68(info->short_name,"wav")) {
      ext = ".wav";
    } else if (!strcmp68(info->short_name,"au")) {
      ext = ".au";
    } else if (!strcmp68(info->short_name,"iff")) {
      ext = ".iff";
    } else if (!strcmp68(info->short_name,"mp3")) {
      ext = ".mp3";
    } else if (!strcmp68(info->short_name,"ogg")) {
      ext = ".ogg";
    } else if (!strcmp68(info->short_name,"raw")) {
      ext = ".raw";
    }

    strcpy(is->defoutname,"sc68");
    strcat68(is->defoutname,ext,sizeof(is->defoutname)-1);
    is->outname = is->defoutname;
    TRACE68(ao68_feature,
            "ao68: open -- default *FILENAME* '%s'\n", is->outname);
  }

  is->ao.device =
    (info->type==AO_TYPE_LIVE)
    ? ao_open_live(is->ao.driver_id, &is->ao.format, is->ao.options)
    : ao_open_file(is->ao.driver_id, is->outname, !is->no_overwrite,
                   &is->ao.format, is->ao.options);

  if (!is->ao.device) {
    goto error;
  }

  if (info->type == AO_TYPE_LIVE) {
    msg68_info("ao68: using *%s* live driver\n", info->short_name);
  } else {
    msg68_info("ao68: using *%s* driver to write '%s'\n",
               info->short_name, is->outname);
  }

  is->count = 0;
  err = 0;
  error:
  TRACE68(ao68_feature, "ao68: open -- [%s]\n", strok68(err));
  return err;
}

static int isao_close(istream68_t * istream)
{
  istream68_ao_t * is = (istream68_ao_t *)istream;
  int err = -1;

  if (!is || !is->ao.device) {
    goto error;
  }
  ao_close(is->ao.device);
  is->ao.device = 0;
  istream68_ao_shutdown();

  err = 0;
  error:
  TRACE68(ao68_feature, "ao68: close -- [%s]\n",strok68(err));
  return err;
}

static int isao_read(istream68_t * istream, void * data, int n)
{
  return -1;
}

static int isao_write(istream68_t * istream, const void * data, int n)
{
  istream68_ao_t * isao = (istream68_ao_t *)istream;

  if (!isao || !isao->ao.device) {
    return -1;
  }
  if (!n) {
    return 0;
  }
  if (n<0) {
    return 0;
  }
  if (ao_play(isao->ao.device, (void *)data, n)) {
    isao->count += n;
    return n;
  } else {
    return -1;
  }
}

static int isao_length(istream68_t * istream)
{
  istream68_ao_t * isao = (istream68_ao_t *)istream;

  return (isao) ? isao->count : -1;
}

static int isao_tell(istream68_t * istream)
{
  istream68_ao_t * isao = (istream68_ao_t *)istream;
  return (!isao || !isao->ao.device)
    ? -1
    : isao->count;
}

static int isao_seek(istream68_t * istream, int offset)
{
  return -1;
}

static void isao_destroy(istream68_t * istream)
{
  TRACE68(ao68_feature, "ao68: destroy '%s'\n", istream68_filename(istream));
  istream68_ao_shutdown();
  free68(istream);
}

static const istream68_t istream68_ao = {
  isao_name,
  isao_open, isao_close,
  isao_read, isao_write,
  isao_length, isao_tell, isao_seek, isao_seek,
  isao_destroy
};

/* ao stream URL is composed by /key=value pairs.
 *
 * Specific keys are:
 *
 *  - driver=[null|alsa|oss|esd|au|raw|wav....]
 *  - rate=hz
 *  - format=[endianess][sign][channels][format]
 *    with (first value is the default)
 *        - endian   := [n|l|b]
 *        - sign     := [-|+]
 *        - channels := [1|2]
 *        - format   := [W|B|F]
 *
 * Other keys will be used as ao driver options.
 *
 */
istream68_t * istream68_ao_create(const char * fname, int mode)
{
  istream68_ao_t *isf=0;
  int len;
  ao68_info_t ao;

  TRACE68(ao68_feature,"ao68: create '%s' mode:%d\n",
          strnevernull68(fname),mode);

  if (!init) {
    msg68_critical("ao68: create -- ao has not been initialized properly\n");
    goto error;
  }

  if (!fname || !fname[0]) {
    msg68_critical("ao68: create -- invalid parameter\n");
    goto error;
  }

  if (mode != ISTREAM68_OPEN_WRITE) {
    msg68_error("ao68: create -- invalid mode\n");
    goto error;
  }

  /* Don't need 0, because 1 byte already allocated in the
   * istream68_ao_t::fname.
   */
  len = strlen(fname);
  isf = calloc68(sizeof(istream68_ao_t) + len);
  if (!isf) {
    goto error;
  }

  /* -- Setup for default driver -- */
  ao.default_id         = ao_default_driver_id();
  ao.driver_id          = ao.default_id;
  ao.device             = 0;
  ao.default_device     = 0;
  ao.format.bits        = 16;
  ao.format.channels    = 2;
  ao.format.rate        = istream68_ao_defaut_rate;
  ao.format.byte_format = AO_FMT_NATIVE;
  ao.options            = 0;

  /* Copy istream functions. */
  memcpy(&isf->istream, &istream68_ao, sizeof(istream68_ao));
  /*   isf->mode = mode & (ISTREAM68_OPEN_READ|ISTREAM68_OPEN_WRITE); */
  isf->ao   = ao;
  strcpy(isf->name, fname);

  error:
  fname = istream68_filename(&isf->istream);
  TRACE68(ao68_feature,"ao68: create => [%s,'%s']\n",
          strok68(!isf),strnevernull68(fname));
  return isf ? &isf->istream : 0;
}

#else /* #ifdef USE_AO */

/* istream ao must not be include in this package. Anyway the creation
 * still exist but it always returns error.
 */

#include "istream68_ao.h"
#include "istream68_def.h"

istream68_t * istream68_ao_create(const char * fname, int mode)
{
  msg68_error("ao68: not supported\n");
  return 0;
}

int istream68_ao_init(void)
{
  return 0;
}

void istream68_ao_shutdown(void)
{
}

unsigned int audio68_sampling_rate(const unsigned int rate)
{
  return rate;
}

#endif /* #ifdef USE_AO */