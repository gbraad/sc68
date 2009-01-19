/*
 *                  file68 - "sc68" file functions
 *	      Copyright (C) 2001-2009 Ben(jamin) Gerard
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
#include "file68.h"

#include "error68.h"
#include "alloc68.h"
#include "debugmsg68.h"
#include "string68.h"
#include "rsc68.h"

#include "istream68_def.h"
#include "istream68_file.h"
#include "istream68_fd.h"
#include "istream68_curl.h"
#include "istream68_mem.h"
#include "istream68_z.h"
#include "istream68_null.h"
#include "gzip68.h"
#include "ice68.h"
#include "url68.h"

#ifndef u64
# ifdef HAVE_STDINT_H
#  include <stdint.h>
#  define u64 uint_least64_t
# elif defined(_MSC_VER)
#  define u64 unsigned __int64
# elif defined(__GNUC__)
#  define u64 unsigned long long
# endif
#endif

#ifndef u64
# error "u64 must be defined as an integer of at least 64 bit"
#endif

#include <string.h>
#include <ctype.h>
#include <stdio.h>


/* Current identifier string used to save file */
#define SC68_SAVE_IDSTR file68_idstr

#define FOURCC(A,B,C,D) ((int)( ((A)<<24) | ((B)<<16) | ((C)<<8) | (D) ))
#define gzip_cc FOURCC('g','z','i','p')
#define ice_cc  FOURCC('i','c','e','!')
#define sndh_cc FOURCC('S','N','D','H')
#define sc68_cc FOURCC('S','C','6','8')

/* SC68 file identifier string
 */
const char file68_idstr[]    = SC68_IDSTR;
const char file68_idstr_v2[] = SC68_IDSTR_V2;

int file68_feature = debugmsg68_DEFAULT; 

/* Peek Little Endian Unaligned 32 bit value */
static int LPeek(const void *a)
{
  int r;
  unsigned char *c = (unsigned char *) a;
  r = c[0] + (c[1] << 8) + (c[2] << 16) + ((int)(signed char)c[3] << 24);
  return r;
}

static int LPeekBE(const void *a)
{
  int r;
  unsigned char *c = (unsigned char *) a;
  r = ((int)(signed char)c[0] << 24) + (c[1] << 16) + (c[2] << 8) + c[3];
  return r;
}

/* Poke Little Endian Unaligned 32 bit value */
static void LPoke(void *a, int r)
{
  unsigned char *c = (unsigned char *) a;
  c[0] = r;
  c[1] = r >> 8;
  c[2] = r >> 16;
  c[3] = r >> 24;
}

static int myatoi(const char *s, int i, int max, int * pv)
{
  int v = 0;
  for (; i<max; ++i) {
    int c = s[i] & 255;
    if (c>='0' && c<='9') {
      v = v * 10 + c - '0';
    } else {
      break;
    }
  }
  if (pv) *pv = v;
  return i;
}

#define ok_int(V)  strok68(V)
#define strnull(S) strnevernull68(S)

static int sndh_is_magic(const char *buffer, int max)
{
  const int start = 6;
  int i=0, v = 0;
  if (max >= start) {
    for (i=start, v = LPeekBE(buffer); i < max && v != sndh_cc;
	 v = ((v<<8)| (buffer[i++]&255)) & 0xFFFFFFFF)
      ;
  }
  i = (v == sndh_cc) ? i-4: 0;
  TRACE68(file68_feature,"sndh_is_magic := %d\n",i);
  return i;
}

/* Verify header , return # byte to alloc & read
 * or -1 if error
 * or -gzip_cc if may be gzipped
 * or -ice_cc if may be iced
 * or -sndh_cc if may be sndh
 * or 
 */
static int read_header(istream68_t * const is)
{
  char id[256];
  const int l = sizeof(file68_idstr_v2);
  const int sndh_req = (l<=32) ? 32 : l;
  int l2;
  const char * missing_id = "Not SC68 file : Missing ID";

  /* Read ID v2 string */
  l2 = istream68_read(is, id, l);
  if (l2 != l) {
    return error68_add(0,"missing ID bytes (%d)", l2);
  }
  
  if (!memcmp(id, file68_idstr, l)) {
    /* ID V1: read missig bytes */
    const int n = sizeof(file68_idstr) - l;
    if (istream68_read(is, id, n) != n || memcmp(id, file68_idstr+l, n)) {
      return error68_add(0,missing_id);
    }
    TRACE68(file68_feature,"found file-id: [%s]\n",file68_idstr);
  } else if (memcmp(id, file68_idstr_v2, l)) {
    if (gzip68_is_magic(id)) {
      TRACE68(file68_feature,"found GZIP signature\n");
      return -gzip_cc;
    } else if (ice68_is_magic(id)) {
      TRACE68(file68_feature,"found ICE! signature\n");
      return -ice_cc;
    } else {
      TRACE68(file68_feature,"try SNDH\n");
      /* Need some more bytes for sndh */
      if (istream68_read(is, id+l, sndh_req-l) == sndh_req-l
	  && sndh_is_magic(id,sndh_req)) {
      /* Must be done after gzip or ice becoz id-string may appear in
       * compressed buffer too.
       */
	TRACE68(file68_feature,"found SNDH signature\n");
	return -sndh_cc;
      }
    }
    return error68_add(0,missing_id);
  } else {
    TRACE68(file68_feature,"found file-id: [%s]\n",file68_idstr_v2);
  }

  /* Check 1st chunk */
  if (istream68_read(is, id, 4) != 4
      || memcmp(id, CH68_CHUNK CH68_BASE, 4)) {
    return error68_add(0,"Not SC68 file : Missing BASE Chunk");
  }

  /* Get base chunk : file total size */
  if (istream68_read(is, id, 4) != 4
      || (l2 = LPeek(id), l2 <= 8)) {
    return error68_add(0,"Not SC68 file : Weird BASE Chunk size");
  }
  TRACE68(file68_feature,"sc68-header: [%d bytes]\n",l2-8);
  return l2 - 8;
}

static char noname[] = SC68_NOFILENAME;


/* FR  = SEC * HZ
 * FR  = MS*HZ/1000
 *
 * SEC = FR / HZ
 * MS  = FR*1000/HZ
 */

static unsigned int frames_to_ms(unsigned int frames, unsigned int hz)
{
  u64 ms;

  ms = frames;
  ms *= 1000u;
  ms /= hz;

  return (unsigned int) ms;
}

static unsigned int ms_to_frames(unsigned int ms, unsigned int hz)
{
  u64 fr;

  fr =  ms;
  fr *= hz;
  fr /= 1000u;

  return (unsigned int ) fr;
}


/* This function inits all pointers for this music files It setup non
 * initialized data to defaut value.
 */
static int valid(disk68_t * mb)
{
  music68_t *m;
  int i, previousdatasz = 0;
  void *previousdata = 0;
  char *author = noname;
  char *composer = 0;
  char *mname;
  char *converter = noname;
  char *ripper = noname;

  if (mb->nb_six <= 0) {
    return error68_add(0,"No music defined");
  }

  /* Ensure default music in valid range */
  if (mb->default_six < 0 || mb->default_six >= mb->nb_six) {
    mb->default_six = 0;
  }

  /* No name : set default */
  if (mb->name == 0) {
    mb->name = noname;
  }
  mname = mb->name;

  /* Disk total time : 00:00 */
  mb->time_ms = 0;

  /* Clear flags */
  mb->hwflags.all = 0;

  /* Init all music in this file */
  for (m = mb->mus, i = 0; m < mb->mus + mb->nb_six; m++, i++) {
    /* default load address */
    if (m->a0 == 0) {
      m->a0 = SC68_LOADADDR;
    }
    /* default replay frequency is 50Hz */
    if (m->frq == 0) {
      m->frq = 50;
    }

    /* Compute ms from frames prior to frames from ms. */ 
    if (m->frames) {
      m->time_ms = frames_to_ms(m->frames, m->frq);
    } else {
      m->frames = ms_to_frames(m->time_ms,m->frq);
    }

    /* Set start time in the disk. */
    m->start_ms = mb->time_ms;

    /* Advance disk total time. */
    mb->time_ms += m->time_ms;

    /* default mode is YM2149 (Atari ST) */
    if (m->hwflags.all == 0) {
      m->hwflags.bit.ym = 1;
    }
    mb->hwflags.all |= m->hwflags.all;

    /* default music name is file name */
    mname = m->name = (m->name == 0) ? mname : m->name;
    /* default author */
    author = m->author = (m->author == 0) ? author : m->author;
    /* default composer is author */
    composer = m->composer = (m->composer == 0) ?
	((composer == 0) ? m->author : composer) : m->composer;
    /* default converter is the empty string */
    converter = m->converter = (m->converter == 0) ?
        ((converter == 0) ? m->converter : converter) : m->converter;
    /* default copier is the empty string */
    ripper = m->ripper = (m->ripper == 0) ?
        ((ripper == 0) ? m->ripper : ripper) : m->ripper;
    /* use data from previuous music */
    if (m->data == 0) {
      m->data = (char *) previousdata;
      m->datasz = previousdatasz;
    }
    if (m->data == 0) {
      return error68_add(0,"music #%d as no data", i);
    }
    previousdata = m->data;
    previousdatasz = m->datasz;
  }

  return 0;
}

int file68_is_our_url(const char * url, const char * exts, int * is_remote)
{
  const char * url_end, *u;
  char protocol[16], *p;
  int has_protocol, remote, is_our;

/*   CONTEXT68_CHECK(context); */
  debugmsg68(-1,"file68_is_our_url([url:[%s],...) {\n",url);

  is_our = remote = 0;
  if (!url || !*url) {
    goto exit;
  }

  /* Default supported extensions */
  if (!exts) {
    exts = ".sc68\0.sndh\0.snd\0";
  }

  url_end = url + strlen(url);
  has_protocol = !url68_get_protocol(protocol, sizeof(protocol), url);

  if (has_protocol) {
    int i;
    is_our = !strcmp68(protocol,"SC68");
    if (!is_our && !strcmp68(protocol,"RSC68") && url+14<url_end) {
      is_our = strncmp(url+8, "music/", 6);
    }

    if (is_our)  {
      /* $$$ Not really sure; may be remote or not. The only way to
	 know is to check for the corresponding local file.
      */
      remote = 0;
      goto exit;
    }
  }

  /* Check remote for other protocol */
  remote = !url68_local_protocol(protocol);

  /* Check extension ... */
  p = protocol+sizeof(protocol);
  *--p = 0;
  for (u=url_end; u > url && p > protocol; ) {
    int c = *--u & 255;
    if (c == '/') {
      break;
    }
    *--p = c;
    if (c == '.') {
      if (!strcmp68(p,".GZ")) {
	p = protocol+sizeof(protocol)-1;
      } else {
	break;
      }
    }
  }
  
  while (*exts) {
    is_our = !strcmp68(p,exts);
    if (is_our) {
      break;
    }
    exts += strlen(exts)+1;
  }

 exit:
  if (is_remote) *is_remote = remote;
  debugmsg68(-1,"} file68_is_our_url => [%s]\n",ok_int(!is_our));
  return is_our;
}

/* Check if file is probable SC68 file
 * return 0:SC68-file
 */
int file68_verify(istream68_t * is)
{
  int res;
  const char * fname = strnull(istream68_filename(is));

  debugmsg68(-1,"file68_verify([is:[%s],...) {\n",fname);

  if (!is) {
    res = error68_add(0,"file68_verify(): null pointer");
    goto error;
  }

  res = read_header(is);

  /* Verify tells it is a gzip file, so we may give it a try. */
  if (res < 0) {
    void * buffer = 0;
    int len;
    
    switch (res) {
    case -gzip_cc:
      if (istream68_seek_to(is,0) == 0) {
	istream68_t * zis;
	zis = istream68_z_create(is,ISTREAM68_OPEN_READ,
				 istream68_z_default_option);
	res = -1;
	if (!istream68_open(zis)) {
	  res = file68_verify(zis);
	}
	istream68_destroy(zis);
      }
      break;
    case -ice_cc:
      if (istream68_seek_to(is,0) == 0) {
	buffer = ice68_load(is, &len);
      }
      break;
    case -sndh_cc:
      res = 0;
      break;
    }
    if (buffer) {
      res = -res;
      res = file68_verify_mem(buffer, len);
      free68(buffer);
    }
  }

 error:
  debugmsg68(-1,"} file68_verify => [%s]\n",ok_int(res));
  return -(res < 0);
}

static istream68_t * url_or_file_create(const char * url, int mode,
					rsc68_info_t * info)
{
  char protocol[16], tmp[512];
  const int max = sizeof(tmp)-1;
  istream68_t *isf = 0;
  int has_protocol;
  char * newname = 0;

/*   CONTEXT68_CHECK(context); */
  debugmsg68(-1,"url_or_file_create([url:[%s],mode:%d,info:%p) {\n",
	     strnull(url),mode,info);

  if (info) {
    info->type = rsc68_last;
  }

  has_protocol = !url68_get_protocol(protocol, sizeof(protocol), url);

  if (has_protocol) {

    /* convert sc68:// -> rsc68:// */
    if (!strcmp68(protocol, "SC68")) {
      url += 4+3;
      strcpy(tmp, "rsc68://music/");
      strncpy(tmp+14, url, max-14);
      tmp[max] = 0;
      url = tmp;
      strcpy(protocol,"rsc68");
      debugmsg68(-1,"url is now [%s]\n",url);
    }

    if (!strcmp68(protocol, "RSC68")) {
      return rsc68_open_url(url, mode, info);
    }
  }

  isf = url68_stream_create(url, mode);

  if (istream68_open(isf)) {
    istream68_destroy(isf);
    isf = 0;
  }

  free68(newname);
  debugmsg68(-1,"} url_or_file_create() => [%s,%s]\n",
	     ok_int(!isf),
	     strnull(istream68_filename(isf)));
  return isf;
}

int file68_verify_url(const char * url)
{
  int res;
  istream68_t * is;

/*   CONTEXT68_CHECK(context); */
  debugmsg68(-1,"file68_verify_url([url:[%s]) {\n",url?url:"<NUL>");

  is = url_or_file_create(url,1,0);
  res = file68_verify(is);
  istream68_destroy(is);

  debugmsg68(-1,"} file68_verify_url() => [%s]\n",!res?"success":"error");
  return -(res < 0);
}

int file68_verify_mem(const void * buffer, int len)
{
  int res;
  istream68_t * is;

  is = istream68_mem_create((void *)buffer,len,1);
  res = istream68_open(is) ? -1 : file68_verify(is);
  istream68_destroy(is);

  return res;
}

/* Check if file is probable SC68 file
 * return 0:SC68-file
 */
int file68_diskname(istream68_t * is, char *dest, int max)
{
  return -1;
}

#if 0
{
  FILE *f;
  chunk68_t chunk;
  int total, len;
  
  dest[0] = dest[max-1] = 0;
  
  if (f = SC68_fopen(name, "rb"), f == 0)
    return -1;
    
  total = len = read_header(f);
  while (len >= 0) {
  
    /* Check chunk */
    if (SC68_fread(&chunk, sizeof(chunk), f)) {
      len = SC68error_add("Can't read next chunk");
      break;
    }
    total -= sizeof(chunk);
    
    if (memcmp(chunk.id, CH68_CHUNK, 2)) {
      len =  SC68error_add("Invalid chunk");
      break;
    }
    len = LPeek(chunk.size);
    if (len < 0) {
      break;
    }
    
    if (memcmp(chunk.id+2, CH68_FNAME, 2)) {
      total -= len;
      if (SC68_fseek(f, len) < 0) {
        len = SC68error_add("Can't reach next chunk");
      }
    } else if (!memcmp(chunk.id+2, CH68_EOF, 2)) {
      total = -1;
    } else {
      if (len >= max) len = max-1;
      if (SC68_fread(dest, len, f) < 0) {
        len =  SC68error_add("Can't read disk filename");
      }
      break;
    }
    
    if (total <= 0) {
      len = SC68error_add( CH68_CHUNK CH68_FNAME " chunk not found");
    }
  }
  
  SC68_fclose(f);
  
  return len < 0 ? -1 : 0;
}
#endif

disk68_t * file68_load_url(const char * fname)
{
  disk68_t * d;
  istream68_t * is;
  rsc68_info_t info;

  debugmsg68(-1,"file68_load_url(url:[%s]) {\n", strnull(fname));

  is = url_or_file_create(fname, 1, &info);
  d = file68_load(is);
  istream68_destroy(is);
  
  if (d && info.type == rsc68_music) {
    int i;

    debugmsg68(-1,
	       "On the fly path: #%d/%d/%d\n",
	       info.data.music.track,
	       info.data.music.loop,
	       info.data.music.time);

    if (info.data.music.track > 0 && info.data.music.track <= d->nb_six) {
      d->mus[0] = d->mus[info.data.music.track-1];
      d->mus[0].start_ms = 0;
      d->mus[0].track = info.data.music.track;
      d->default_six = 0;
      d->nb_six = 1;
      d->time_ms = d->mus[0].time_ms;
      d->hwflags.all = d->mus[0].hwflags.all;
    }
    if (info.data.music.loop != -1) {
      for (i=0; i<d->nb_six; ++i) {
	d->mus[i].loop = info.data.music.loop;
      }
    }
    if (info.data.music.time != -1) {
      unsigned int ms = info.data.music.time * 1000u;
      d->time_ms = 0;
      for (i=0; i<d->nb_six; ++i) {
	d->mus[i].frames   = ms_to_frames(ms, d->mus[i].frq);
	d->mus[i].time_ms  = ms;
	d->mus[i].start_ms = d->time_ms;
	d->time_ms += ms;
      }
    }
  }

  debugmsg68(-1,"} file68_load_url => [%s]\n", ok_int(!d));

  return d;
}

disk68_t * file68_load_mem(const void * buffer, int len)
{
  disk68_t * d;
  istream68_t * is;

  is = istream68_mem_create((void *)buffer,len,1);
  d = istream68_open(is) ? 0 : file68_load(is);
  istream68_destroy(is);

  return d;
}

static int sndh_info(disk68_t * mb, int len)
{
  int frq = 0, time = 0 , musicmon = 0;
  int i;
  int unknowns = 0;
  const int unknowns_max = 8;
  int fail = 0;
  char * b = mb->data;
  char empty_tag[4] = { 0, 0, 0, 0 };

/*   debugmsg68("SNDH_INFO\n"); */

  /* Default */
  mb->mus[0].data = b;
  mb->mus[0].datasz = len;
  mb->nb_six = 0; /* Make validate failed */
  mb->mus[0].replay = "sndh_ice";

  i = sndh_is_magic(mb->data, len);
  if (!i) {
    debugmsg68(-1,"NO MAGIC ! What the heck\n");
    /* should not happen since we already have tested it. */
    return -1;
  }

  i += 4; /* Skip sndh_cc */
  len -= 4;

/*   debugmsg68("SNDH FIRST TAG at %d\n", i); */

  /* $$$ Hacky:
     Some music have 0 after values. I don't know what are
     sndh rules. May be 0 must be skipped or may be tag must be word
     aligned.
     Anyway the current parser allows a given number of successive
     unknown tags. May be this number should be increase in order to prevent
     some "large" unknown tag to break the parser.
  */

  while (i < len) {
    char * s;
    int unknown;

    s = 0;
    unknown = 0;
    if (!memcmp(b+i,"COMM",4)) {
      /* Composer */
      s = mb->mus[0].author = b+i+4;
    } else if (!memcmp(b+i,"TITL",4)) { /* title    */
      /* Title */
      s = mb->name = b+i+4;
    } else if (!memcmp(b+i,"RIPP",4)) {
      /* Ripper    */
      s = mb->mus[0].ripper = b+i+4;
    } else if (!memcmp(b+i,"CONV",4)) {
      /* Converter */
      s = mb->mus[0].converter = b+i+4;
    } else if (!memcmp(b+i,"MuMo",4)) {
      /* Music Mon ???  */
      debugmsg68(-1,"FOUND MuMo (don't know what to do ith that)\n");
      musicmon = 1;
      i += 4;
    } else if (!memcmp(b+i,"TIME",4)) {
      /* Time in second */
      time = (((unsigned char)*(b + i + 4)) << 8) |
             ((unsigned char)*(b + i + 5));
      i += 6;
    } else if (!memcmp(b+i,"##",2)) {
      /* +'xx' number of track  */
      i = myatoi(b, i+2, len, &mb->nb_six);
      while( i < len && *(b + i) == 0 ) {
        i++;
      }
/*     } else if (!memcmp(b+i,"TC",2)) { */
/*       /\* +string frq hz' Timer C frq *\/ */
/*       i = myatoi(b, i+2, len, &frq); */
    } else if (!memcmp(b+i,"!V",2)) {
      /* +string VBL frq */
      if (!frq) {
	i = myatoi(b, i+2, len, &frq);
      }
    } else if (!memcmp(b+i,"**",2)) {
      /* FX +string 2 char ??? */
      i += 4;
    }
    else if (!memcmp(b+i, "YEAR", 4)) {
      /* year */
      s = b+i+4;
    }
    else if( *(b+i) == 'T' && (*(b+i+1) == 'A' ||
                               *(b+i+1) == 'B' ||
                               *(b+i+1) == 'C' ||
                               *(b+i+1) == 'D') ) {
      i = myatoi(b, i+2, len, &frq);
      while( i < len && *(b + i) == 0 ) {
        i++;
      }
    }
    else if( memcmp( b + i, empty_tag, 4 ) == 0 ||
             memcmp( b + i, "HDNS", 4 ) == 0 ) {
      i = len;
    } else { 
      /* skip until next 0 byte, as long as it's inside the tag area */

      i += 4;
      while( *(b + i) != 0 && i < len ) {
        i++;
      }

      unknown = 1;
/*
      if( i >= len ) {
        fail = 1;
      }
*/
    }

    if (unknown) {
      ++unknowns;
         /* Unkwown tag, finish here. */
      debugmsg68(-1,"UNKNOWN TAG #%02d [%c%c%c%c] at %d\n",unknowns,
		 b[i],b[i+1],b[i+2],b[i+3], i);
      ++i;
      if (fail || unknowns >= unknowns_max) {
	i = len;
      }
      unknown = 0;
    } else {
      unknowns = 0; /* Reset successive unkwown. */
      if (s) {
	int j,k,c;
	for (j=k=0; c=(s[j]&255), c; ++j) {
	  if (c <= 32) s[j] = 32;
	  else k=j+1;
	}
	s[k] = 0; /* Strip triling space */
	i += j + 5;

        /* skip the trailing null chars */
        while( i < len && *(b + i) == 0 ) {
          i++;
        }
      }
    }

  }
  if (mb->nb_six > SC68_MAX_TRACK) {
    mb->nb_six = SC68_MAX_TRACK;
  }
  time *= 1000;
  for (i=0; i<mb->nb_six; ++i) {
    mb->mus[i].d0 = i+1;
    mb->mus[i].loop = 1;
    mb->mus[i].frq = frq;
    mb->mus[i].time_ms = time;
  }
  return 0;
}

/* Load , allocate memory and valid struct for SC68 music
 */
disk68_t * file68_load(istream68_t * is)
{
  disk68_t *mb = 0;
  int len, room;
  int chk_size;
  int opened = 0;
  music68_t *cursix;
  char *b;
  const char *fname = istream68_filename(is);
  const char *errorstr = "no more info";

/*   if (context&&is) { */
/*     context=is->context; */
/*   } */

  if (!fname) {
    fname = "<nil>";
  }

  /* Open stream. */
/*   if (istream68_open(is) == -1) { */
/*     errorstr = "open failure"; */
/*     goto error; */
/*   } */
/*   opened = 1; */

  /* Read header and get data length. */
  if (len = read_header(is), len < 0) {
    /* Verify tells it is a gzip or unice file, so we may give it a try.
     */
    if (1) {
      void * buffer = 0;
      int l;
      switch (len) {
      case -gzip_cc:
	/* gzipped */
/* 	debugmsg68("GZIP LOAD [%s]!\n", fname); */
	if (istream68_seek_to(is,0) == 0) {
	  istream68_t * zis;
	  zis=istream68_z_create(is,ISTREAM68_OPEN_READ,istream68_z_default_option);
	  if (!istream68_open(zis)) {
	    mb = file68_load(zis);
	  }
	  istream68_destroy(zis);
	  if (mb) {
	    goto already_valid;
	  }
	}
	break;

      case -ice_cc:
/* 	debugmsg68("ICE LOAD [%s]!\n", fname); */
	if (istream68_seek_to(is,0) == 0) {
	  buffer = ice68_load(is, &l);
/* 	  debugmsg68("-> %p %d\n", buffer, l); */
	}
	break;
      case -sndh_cc:
/* 	debugmsg68("SNDH dans la place\n"); */
	if (istream68_seek_to(is,0) != 0) {
	  break;
	}
	len = istream68_length(is);
	if (len <= 32) {
	  break;
	}
	mb = alloc68(sizeof(*mb) + len);
	if (!mb) {
	  break;
	}
	memset(mb,0,sizeof(*mb));
	if (istream68_read(is, mb->data, len) != len) {
	  break;
	}
	if (sndh_info(mb, len)) {
	  break;
	}
	goto validate;
      }

      if (buffer) {
	mb = file68_load_mem(buffer, l);
	free68(buffer);
	if (mb) {
	  return mb;
	}
      }
    }
    errorstr = "read header";
    goto error;
  }

  room = len + sizeof(disk68_t) - sizeof(mb->data);
  mb = alloc68(room);
  if (!mb) {
    errorstr = "memory allocation";
    goto error;
  }
  memset(mb, 0, room);

  if (istream68_read(is, mb->data, len) != len) {
    errorstr = "read data";
    goto error;
  }

  for (b = mb->data, cursix = 0; len >= 8; b += chk_size, len -= chk_size) {
    char chk[8];

    if (b[0] != 'S' || b[1] != 'C') {
      break;
    }

    chk[0] = b[2];
    chk[1] = b[3];
    chk[2] = 0;
    chk_size = LPeek(b + 4);
    b += 8;
    len -= 8;

    if (!strcmp(chk, CH68_BASE)) {
      /* nothing to do. */
    }
    /* Music general info */
    else if (!strcmp(chk, CH68_DEFAULT)) {
      mb->default_six = LPeek(b);
    } else if (!strcmp(chk, CH68_FNAME)) {
      mb->name = b;
    }
    /* start music session */
    else if (!strcmp(chk, CH68_MUSIC)) {
      /* More than 256 musix !!! : Prematured end */
      if (mb->nb_six >= 256) {
	len = 0;
	break;
      }
      cursix = mb->mus + mb->nb_six;
      cursix->loop = 1; /* default loop */
      mb->nb_six++;
    }
    /* Music name */
    else if (!strcmp(chk, CH68_MNAME)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->name = b;
    }
    /* Author name */
    else if (!strcmp(chk, CH68_ANAME)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->author = b;
    }
    /* Composer name */
    else if (!strcmp(chk, CH68_CNAME)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->composer = b;
    }
    /* External replay */
    else if (!strcmp(chk, CH68_REPLAY)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->replay = b;
    }
    /* 68000 D0 init value */
    else if (!strcmp(chk, CH68_D0)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->d0 = LPeek(b);
    }
    /* 68000 memory load address */
    else if (!strcmp(chk, CH68_AT)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->a0 = LPeek(b);
    }
    /* Playing time */
    else if (!strcmp(chk, CH68_TIME)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->time_ms = LPeek(b) * 1000u;
    }

    /* Playing time */
    else if (!strcmp(chk, CH68_FRAME)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->frames = LPeek(b);
    }

    /* Replay frequency */
    else if (!strcmp(chk, CH68_FRQ)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->frq = LPeek(b);
    }

    /* Loop */
    else if (!strcmp(chk, CH68_LOOP)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->loop = LPeek(b);
    }

    /* replay flags */
    else if (!strcmp(chk, CH68_TYP)) {
      int f;
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      f = LPeek(b);
      cursix->hwflags.all = 0;
      cursix->hwflags.bit.ym = !!(f & SC68_YM);
      cursix->hwflags.bit.ste = !!(f & SC68_STE);
      cursix->hwflags.bit.amiga = !!(f & SC68_AMIGA);
      cursix->hwflags.bit.stechoice = !!(f & SC68_STECHOICE);
    }
    /* music data */
    else if (!strcmp(chk, CH68_MDATA)) {
      if (cursix == 0) {
	errorstr = chk;
	goto error;
      }
      cursix->data = b;
      cursix->datasz = chk_size;
    }
    /* EOF */
    else if (!strcmp(chk, CH68_EOF)) {
      len = 0;
      break;
    }
  }

  /* Check it */
  if (len) {
    errorstr = "prematured end of file";
    goto error;
  }

 validate:
  if (valid(mb)) {
    errorstr = "validation test";
    goto error;
  }

 already_valid:
  if (opened) {
    istream68_close(is);
  }
  return mb;

error:
  if (opened) {
    istream68_close(is);
  }
  free68(mb);
  error68_add(0,"file68_load(%s) : Failed (%s)", fname, errorstr);
  return 0;
}


#ifndef _FILE68_NO_SAVE_FUNCTION_

/* save CHUNK and data */
/* $$$ NEW: Add auto 16-bit alignement. */
static int save_chunk(istream68_t * os,
		      const char * chunk, const void * data, int size)
{
  static char zero[4] = {0,0,0,0};
  chunk68_t chk;
  int align;

  memcpy(chk.id, CH68_CHUNK, 2);
  memcpy(chk.id + 2, chunk, 2);
  align = size & 1;
  LPoke(chk.size, size + align);
  if (istream68_write(os, &chk, (int)sizeof(chunk68_t)) != sizeof(chunk68_t)) {
    goto error;
  }
  /* Special case data is 0 should happen only for SC68 total size
   * chunk.
   */
  if (size && data) {
    if (istream68_write(os, data, size) != size) {
      goto error;
    }
    if (align && istream68_write(os, zero, align) != align) {
      goto error;
    }
  }
  return 0;

 error:
  return -1;
}

/* save CHUNK and string (only if non-0 & lenght>0) */
static int save_string(istream68_t * os,
		       const char * chunk, const char * str)
{
  int len;

  if (!str || !(len = strlen(str))) {
    return 0;
  }
  return save_chunk(os, chunk, str, len + 1);
}

/* save CHUNK & string str ( only if oldstr!=str & lenght>0 ) */
static int save_differstr(istream68_t * os,
			  const char *chunk, char *str, char *oldstr)
{
  int len;

  if (oldstr == str
      || !str
      || (oldstr && !strcmp(oldstr, str))) {
    return 0;
  }
  len = strlen(str);
  return !len ? 0 :save_chunk(os, chunk, str, len + 1);
}

/* save CHUNK and 4 bytes Big Endian integer */
static int save_number(istream68_t * os, const char * chunk, int n)
{
  char number[4];

  LPoke(number, n);
  return save_chunk(os, chunk, number, 4);
}

/* save CHUNK and number (only if n!=0) */
static int save_nonzero(istream68_t * os, const char * chunk, int n)
{
  return !n ? 0 : save_number(os, chunk, n);
}

int file68_save_url(const char * fname, const disk68_t * mb,
		    int gzip)
{
  int feature = debugmsg68_feature_current(file68_feature);
  istream68_t * os;
  int err;

  os = url_or_file_create(fname, 2, 0);
  err = file68_save(os, mb, gzip);
  istream68_destroy(os);

  debugmsg68_feature_current(feature);
  return err;
}

int file68_save_mem(const char * buffer, int len, const disk68_t * mb,
		    int gzip)
{
  int feature =  debugmsg68_feature_current(file68_feature);
  istream68_t * os;
  int err;


  os = istream68_mem_create((char *)buffer, len, 2);
  err = file68_save(os, mb, gzip);
  istream68_destroy(os);

  debugmsg68_feature_current(feature);
  return err;
}

static const char * save_sc68(istream68_t * os, const disk68_t * mb, int len);

/* Save disk into file. */
int file68_save(istream68_t * os, const disk68_t * mb, int gzip)
{
  int len;
  const char * fname  = 0;
  const char * errstr = 0;
  istream68_t * null_os = 0;
  istream68_t * org_os  = 0;

  if (!os) {

  }

  /* Get filename (for error message) */
  fname = istream68_filename(os);

  /* Create a null stream to calculate total size.
     Needed by gzip stream that can't seek back. */
  null_os = istream68_null_create(fname);
  if (istream68_open(null_os)) {
    errstr = "open error";
  } else {
    errstr = save_sc68(null_os, mb, 0);
  }
  if (errstr) {
    goto error;
  }
  len = istream68_length(null_os) - sizeof(SC68_SAVE_IDSTR);
  if (len <= 0) {
    errstr = "weird stream length error";
    goto error;
  }

  /* Wrap to gzip stream */
  if (gzip) {
    istream68_z_option_t gzopt;

    org_os = os;
    gzopt = istream68_z_default_option;
    gzopt.level = gzip;
    gzopt.name  = 0;
    os = istream68_z_create(org_os, 2, gzopt);
    if (istream68_open(os)) {
      errstr = "open error";
      goto error;
    }
  }

  errstr = save_sc68(os, mb, len);

 error:
  if (org_os) {
    /* Was gzipped: clean-up */
    istream68_destroy(os);
  }
  istream68_destroy(null_os);

  return errstr
    ? error68_add("file68_save(%s) : %s",fname,errstr)
    : 0;
}

static const char * save_sc68(istream68_t * os, const disk68_t * mb, int len)
{
  const char * errstr = 0;
  int opened = 0;

  const music68_t * mus;
  char * mname, * aname, * cname, * data;

  if (!os) {
    errstr = "null stream error";
    goto error;
  }

  /* Check number of music */
  if (mb->nb_six <= 0 || mb->nb_six > SC68_MAX_TRACK) {
    errstr = "tracks error";
    goto error;
  }

  /* SC68 file header string */
  if (istream68_write(os, SC68_SAVE_IDSTR, sizeof(SC68_SAVE_IDSTR))
      != sizeof(SC68_SAVE_IDSTR)) {
    errstr = "header write error";
    goto error;
  }

  /* SC68 disk-info chunks */
  if (save_chunk(os, CH68_BASE, 0, len)
      || save_string(os, CH68_FNAME, mb->name)
      || save_nonzero(os, CH68_DEFAULT, mb->default_six)
      ) {
    errstr = "chunk write error";
    goto error;
  }

  /* Reset previous value for various string */
  mname = mb->name;
  aname = cname = data = 0;
  for (mus = mb->mus; mus < mb->mus + mb->nb_six; mus++) {
    int flags;

    flags = 0
      | (mus->hwflags.bit.ym ? SC68_YM : 0)
      | (mus->hwflags.bit.ste ? SC68_STE : 0)
      | (mus->hwflags.bit.amiga ? SC68_AMIGA : 0)
      | (mus->hwflags.bit.stechoice ? SC68_STE : 0);

    /* Save track-name, author, composer, replay */
    if (save_chunk(os, CH68_MUSIC, 0, 0) == -1
	|| save_differstr(os, CH68_MNAME, mus->name, mname)
	|| save_differstr(os, CH68_ANAME, mus->author, aname)
	|| save_differstr(os, CH68_CNAME, mus->composer, cname)
	|| save_string(os, CH68_REPLAY, mus->replay)
	) {
      errstr = "chunk write error";
      goto error;
    }
    if (mus->name) {
      mname = mus->name;
    }
    if (mus->author) {
      aname = mus->author;
    }
    if (mus->composer) {
      cname = mus->composer;
    }

    /* Save play parms */
    if (save_nonzero(os, CH68_D0, mus->d0)
	|| save_nonzero(os, CH68_AT, (mus->a0 == SC68_LOADADDR) ? 0 : mus->a0)
	|| save_nonzero(os, CH68_FRAME, mus->frames)
	|| save_nonzero(os, CH68_TIME, (mus->time_ms+999u) / 1000u)
	|| save_nonzero(os, CH68_FRQ, (mus->frq == 50) ? 0 : mus->frq)
	|| save_nonzero(os, CH68_LOOP, mus->loop)
	|| save_number(os, CH68_TYP, flags)
	) {
      errstr = "chunk write error";
      goto error;
    }

    /* Save music data */
    if (mus->data && mus->data != data) {
      if (save_chunk(os, CH68_MDATA, mus->data, mus->datasz)) {
	errstr = "chunk write error";
	goto error;
      }
      data = mus->data;
    }
  }

  /* SC68 last chunk */
  if (save_chunk(os, CH68_EOF, 0, 0)) {
    errstr = "chunk write error";
    goto error;
  }

 error:
  if (opened) {
    istream68_close(os);
  }
  return errstr;
}

#endif /* #ifndef _FILE68_NO_SAVE_FUNCTION_ */

int file68_version(void)
{
  return PACKAGE_VERNUM;
}

const char * file68_versionstr()
{
  return PACKAGE_STRING;
}