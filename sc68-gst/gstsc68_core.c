/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 Benjamin Gerard <benjihan -4t- sourceforge>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include "gstsc68.h"
#include <stdlib.h>
#include <string.h>

static gboolean is_init = FALSE;

void gst_sc68_flush_error(Gstsc68 * filter)
{
  sc68_error_flush(filter ? filter->sc68 : 0);
}

void gst_sc68_report_error(Gstsc68 * filter)
{
  const char * err;
  sc68_t * sc68 = filter ? filter->sc68 : 0;

  while ( err = sc68_error_get(sc68), err ) {
    GST_ERROR_OBJECT(filter,"%s",err);
  }
}

gboolean gst_sc68_lib_is_init(void)
{
  return is_init;
}

gboolean gst_sc68_lib_init(void)
{
  GST_DEBUG("ENTER {");
  if (is_init) {
    GST_DEBUG("already init");
  } else if (sc68_init(0)) {
    GST_ERROR("sc68 library init failed.");
    gst_sc68_report_error(0);
  } else {
    is_init = TRUE;
  }
  GST_DEBUG("} LEAVE => [%s]", is_init?"OK":"ERR");
  return is_init;
}

void gst_sc68_lib_shutdown(void)
{
  GST_DEBUG("ENTER {");
  if (is_init) {
    is_init = FALSE;
    gst_sc68_report_error(0);
    sc68_shutdown();
  }
  GST_DEBUG("} LEAVE");
}

gboolean gst_sc68_create_engine(Gstsc68 * filter)
{
  gboolean res;
  sc68_create_t create;

  GST_DEBUG("ENTER {");
  if (filter->sc68)
    gst_sc68_shutdown_engine(filter);
  filter->code = SC68_ERROR;
  memset(&create,0,sizeof(create));
  create.name = "gst-sc68";
  if (filter->prop.rate > 0)
    create.sampling_rate = filter->prop.rate;
  filter->sc68 = sc68_create(&create);
  res = !!filter->sc68;
  gst_sc68_report_error(filter);
  GST_DEBUG("} LEAVE => [%s]", res?"OK":"ERR");
  return res;
}

void gst_sc68_shutdown_engine(Gstsc68 * filter)
{
  sc68_t * sc68 = filter->sc68;

  GST_DEBUG("ENTER(%p) {",sc68);
  if (sc68) {
    gst_sc68_report_error(filter);      /* flush sc68 instance errors */
    filter->sc68 = NULL;
    sc68_destroy(sc68);
  }
  filter->code = SC68_ERROR;
  GST_DEBUG("} LEAVE");
}

gboolean gst_sc68_load_mem(Gstsc68 * filter, void * data , int size)
{
  int track;

  filter->code = SC68_ERROR;
  filter->samples = 0;

  if (!filter->sc68 && !gst_sc68_create_engine(filter))
    return FALSE;

  if (sc68_load_mem(filter->sc68, data, size)) {
    GST_ERROR_OBJECT(filter, "failed to load sc68 file");
    gst_sc68_report_error(filter);
    return FALSE;
  }

  if (sc68_music_info(filter->sc68,&filter->dskinfo,0,0)) {
    GST_ERROR_OBJECT(filter, "failed to retrieve disk info");
    gst_sc68_report_error(filter);
    return FALSE;
  }

  GST_DEBUG("disk loaded: %s %s - %s",
            filter->dskinfo.time,
            filter->dskinfo.author,
            filter->dskinfo.title);

  track = filter->prop.track;
  if (track < -1 || track > filter->dskinfo.tracks) {
    GST_DEBUG("track #%02d out of range -> using default", track);
    track = -1;
  }
  filter->single_track = TRUE;
  if (!track) {
    filter->single_track = FALSE;
    track = 1;
  }
  if (sc68_play(filter->sc68, track, filter->prop.loop) < 0) {
    GST_ERROR_OBJECT(filter, "failed to play trax #%02d", track);
    gst_sc68_report_error(filter);
    return FALSE;
  }

  filter->position_ns = 0;
  filter->duration_ns = -1;
  filter->samples = 0;
  filter->buffer_frames = 1024;

  filter->code    = sc68_process(filter->sc68, 0, 0);
  if (filter->code == SC68_ERROR) {
    GST_ERROR_OBJECT(filter, "failed to init sc68 music");
    gst_sc68_report_error(filter);
    return FALSE;
  }

  filter->trkinfo.track = 0;         /* force reload info  */
  gst_sc68_onchangetrack(filter);

  return TRUE;
}

gboolean gst_sc68_load_buf(Gstsc68 * filter, GstBuffer * buffer)
{
  return
    gst_sc68_load_mem(filter, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
}

gboolean gst_sc68_caps(Gstsc68 * filter)
{
  int sampling;

  if (filter->bufcaps) {
    gst_caps_unref(filter->bufcaps);
    filter->bufcaps = 0;
  }

  if (filter->bufcaps && !gst_caps_is_fixed(filter->bufcaps)) {
    GST_ERROR_OBJECT(filter, "caps is not fixed: %s", gst_caps_to_string(filter->bufcaps));
    gst_caps_unref(filter->bufcaps);
    filter->bufcaps = 0;
  }

  if (filter->sc68)
    sampling = sc68_sampling_rate(filter->sc68, SC68_SPR_QUERY);
  else
    sampling = filter->prop.rate;
  if (sampling <= 0)
    sampling = sc68_sampling_rate(0, SC68_SPR_QUERY);
  filter->prop.rate = sampling;
  filter->bufcaps =
    gst_caps_new_simple("audio/x-raw-int",
                        "signed",     G_TYPE_BOOLEAN, TRUE,
                        "width",      G_TYPE_INT,     16,
                        "depth",      G_TYPE_INT,     16,
                        "endianness", G_TYPE_INT,     G_BYTE_ORDER,
                        "rate",       G_TYPE_INT,     sampling,
                        "channels",   G_TYPE_INT,     2,
                        NULL);
  if (!filter->bufcaps) {
    GST_ERROR_OBJECT(filter, "failed to create caps (sampling was %dhz)",sampling);
  } else {
    GST_DEBUG_OBJECT(filter, "new caps: %s ", gst_caps_to_string(filter->bufcaps));
  }

  return !!filter->bufcaps;
}

gboolean gst_sc68_onchangetrack(Gstsc68 * filter)
{
  GstTagList * tags = NULL;
  int track, pos_ms;

  track = sc68_play(filter->sc68, -1, 0);
  if (track == -1) {
    GST_ERROR_OBJECT(filter, "failed to retrieve current trax number");
    gst_sc68_report_error(filter);
    return FALSE;
  }
  if (track > 0 && track != filter->trkinfo.track) {
    if (sc68_music_info(filter->sc68, &filter->trkinfo, -1, 0)) {
      GST_ERROR_OBJECT(filter, "failed to retrieve current trax info");
      gst_sc68_report_error(filter);
      return FALSE;
    }
    pos_ms = sc68_seek(filter->sc68, -1, 0);
    if (pos_ms == -1) {
      GST_ERROR_OBJECT(filter, "failed to retrieve position");
      gst_sc68_report_error(filter);
      return FALSE;
    }
    filter->position_ns = gst_sc68_mstons(pos_ms-filter->trkinfo.start_ms);
    filter->duration_ns = gst_sc68_mstons(filter->trkinfo.time_ms);

    filter->buffer_frames =
      (filter->prop.rate + filter->trkinfo.rate - 1) / filter->trkinfo.rate;

    GST_DEBUG("trax loaded: %s %s - %s replay %s (%dhz) -> %d spf, [%lld .. %lld], ",
              filter->trkinfo.time,
              filter->trkinfo.author,
              filter->trkinfo.title,
              filter->trkinfo.replay,
              filter->trkinfo.rate,
              filter->buffer_frames,
              filter->position_ns,
              filter->duration_ns);

    gst_pad_push_event(filter->srcpad,
                       gst_event_new_new_segment(
                         FALSE, // gboolean update,
                         1.0,
                         GST_FORMAT_TIME,
                         0,
                         filter->duration_ns,
                         filter->position_ns
                         ));

    tags =
      gst_tag_list_new_full(GST_TAG_ALBUM,        filter->dskinfo.title,
                            GST_TAG_ALBUM_ARTIST, filter->dskinfo.author,
                            GST_TAG_TITLE,        filter->trkinfo.title,
                            GST_TAG_ARTIST,       filter->trkinfo.author,
                            GST_TAG_GENRE,        filter->trkinfo.hw.amiga ? "Amiga Chiptune" : "Atari-ST Chiptune",
                            GST_TAG_DURATION,     filter->duration_ns,
                            GST_TAG_TRACK_NUMBER, filter->trkinfo.track,
                            GST_TAG_TRACK_COUNT,  filter->trkinfo.tracks,
                            GST_TAG_HOMEPAGE,     "http://sc68.atari.org/",
                            GST_TAG_COMPOSER,     filter->trkinfo.composer,
                            GST_TAG_AUDIO_CODEC,  "sc68",
                            NULL);
    if (!tags) {
      GST_ERROR_OBJECT(filter,"could not create tag list");
      return FALSE;
    }
    gst_element_found_tags(&filter->element, tags);
    /* gst_tag_list_free(tags); */
  }

  return TRUE;
}

guint64 gst_sc68_mstons(guint ms)
{
  return ms * G_GINT64_CONSTANT(1000000);
}

guint gst_sc68_nstoms(guint64 ns)
{
  return ns / G_GINT64_CONSTANT(1000000);
}