/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2010-2019  Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "bluray-version.h"
#include "bluray.h"
#include "bluray_internal.h"
#include "keys.h"
#include "register.h"
#include "util/array.h"
#include "util/event_queue.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"
#include "util/mutex.h"
#include "bdnav/bdid_parse.h"
#include "bdnav/navigation.h"
#include "bdnav/index_parse.h"
#include "bdnav/meta_parse.h"
#include "bdnav/meta_data.h"
#include "bdnav/sound_parse.h"
#include "bdnav/uo_mask.h"
#include "hdmv/hdmv_vm.h"
#include "hdmv/mobj_parse.h"
#include "decoders/graphics_controller.h"
#include "decoders/hdmv_pids.h"
#include "decoders/m2ts_filter.h"
#include "decoders/overlay.h"
#include "disc/disc.h"
#include "disc/enc_info.h"
#include "file/file.h"
#include "bdj/bdj.h"
#include "bdj/bdjo_parse.h"

#include <stdio.h> // SEEK_
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>


typedef enum {
    title_undef = 0,
    title_hdmv,
    title_bdj,
} BD_TITLE_TYPE;

typedef struct {
    /* current clip */
    const NAV_CLIP *clip;
    BD_FILE_H      *fp;
    uint64_t       clip_size;
    uint64_t       clip_block_pos;
    uint64_t       clip_pos;

    /* current aligned unit */
    uint16_t       int_buf_off;

    /* current stream UO mask (combined from playlist and current clip UO masks) */
    BD_UO_MASK     uo_mask;

    /* internally handled pids */
    uint16_t        ig_pid; /* pid of currently selected IG stream */
    uint16_t        pg_pid; /* pid of currently selected PG stream */

    /* */
    uint8_t         eof_hit;
    uint8_t         encrypted_block_cnt;
    uint8_t         seek_flag;  /* used to fine-tune first read after seek */

    M2TS_FILTER    *m2ts_filter;
} BD_STREAM;

typedef struct {
    const NAV_CLIP *clip;
    size_t    clip_size;
    uint8_t  *buf;
} BD_PRELOAD;

struct bluray {

    BD_MUTEX          mutex;  /* protect API function access to internal data */

    /* current disc */
    BD_DISC          *disc;
    BLURAY_DISC_INFO  disc_info;
    BLURAY_TITLE    **titles;  /* titles from disc index */
    META_ROOT        *meta;
    NAV_TITLE_LIST   *title_list;

    /* current playlist */
    NAV_TITLE      *title;
    uint32_t       title_idx;
    uint64_t       s_pos;

    /* streams */
    BD_STREAM      st0;       /* main path */
    BD_PRELOAD     st_ig;     /* preloaded IG stream sub path */
    BD_PRELOAD     st_textst; /* preloaded TextST sub path */

    /* buffer for bd_read(): current aligned unit of main stream (st0) */
    uint8_t        int_buf[6144];

    /* seamless angle change request */
    int            seamless_angle_change;
    uint32_t       angle_change_pkt;
    uint32_t       angle_change_time;
    unsigned       request_angle;

    /* mark tracking */
    uint64_t       next_mark_pos;
    int            next_mark;

    /* player state */
    BD_REGISTERS   *regs;            /* player registers */
    BD_EVENT_QUEUE *event_queue;     /* navigation mode event queue */
    BD_UO_MASK      uo_mask;         /* Current UO mask */
    BD_UO_MASK      title_uo_mask;   /* UO mask from current .bdjo file or Movie Object */
    BD_TITLE_TYPE   title_type;      /* type of current title (in navigation mode) */
    /* Pending action after playlist end
     * BD-J: delayed sending of BDJ_EVENT_END_OF_PLAYLIST
     *       1 - message pending. 3 - message sent.
     */
    uint8_t         end_of_playlist; /* 1 - reached. 3 - processed . */
    uint8_t         app_scr;         /* 1 if application provides presentation timetamps */

    /* HDMV */
    HDMV_VM        *hdmv_vm;
    uint8_t         hdmv_suspended;
    uint8_t         hdmv_num_invalid_pl;

    /* BD-J */
    BDJAVA         *bdjava;
    BDJ_CONFIG      bdj_config;
    uint8_t         bdj_wait_start;  /* BD-J has selected playlist (prefetch) but not yet started playback */

    /* HDMV graphics */
    GRAPHICS_CONTROLLER *graphics_controller;
    SOUND_DATA          *sound_effects;
    BD_UO_MASK           gc_uo_mask;      /* UO mask from current menu page */
    uint32_t             gc_status;
    uint8_t              decode_pg;

    /* TextST */
    uint32_t gc_wakeup_time;  /* stream timestamp of next subtitle */
    uint64_t gc_wakeup_pos;   /* stream position of gc_wakeup_time */

    /* ARGB overlay output */
    void                *argb_overlay_proc_handle;
    bd_argb_overlay_proc_f argb_overlay_proc;
    BD_ARGB_BUFFER      *argb_buffer;
    BD_MUTEX             argb_buffer_mutex;
};

/* Stream Packet Number = byte offset / 192. Avoid 64-bit division. */
#define SPN(pos) (((uint32_t)((pos) >> 6)) / 3)


/*
 * Library version
 */
void bd_get_version(int *major, int *minor, int *micro)
{
    *major = BLURAY_VERSION_MAJOR;
    *minor = BLURAY_VERSION_MINOR;
    *micro = BLURAY_VERSION_MICRO;
}

/*
 * Navigation mode event queue
 */

const char *bd_event_name(uint32_t event)
{
  switch ((bd_event_e)event) {
#define EVENT_ENTRY(e) case e : return & (#e [9])
        EVENT_ENTRY(BD_EVENT_NONE);
        EVENT_ENTRY(BD_EVENT_ERROR);
        EVENT_ENTRY(BD_EVENT_READ_ERROR);
        EVENT_ENTRY(BD_EVENT_ENCRYPTED);
        EVENT_ENTRY(BD_EVENT_ANGLE);
        EVENT_ENTRY(BD_EVENT_TITLE);
        EVENT_ENTRY(BD_EVENT_PLAYLIST);
        EVENT_ENTRY(BD_EVENT_PLAYITEM);
        EVENT_ENTRY(BD_EVENT_CHAPTER);
        EVENT_ENTRY(BD_EVENT_PLAYMARK);
        EVENT_ENTRY(BD_EVENT_END_OF_TITLE);
        EVENT_ENTRY(BD_EVENT_AUDIO_STREAM);
        EVENT_ENTRY(BD_EVENT_IG_STREAM);
        EVENT_ENTRY(BD_EVENT_PG_TEXTST_STREAM);

        if (stn->num_audio) {
            _update_stream_psr_by_lang(bd->regs,
                                       PSR_AUDIO_LANG, PSR_PRIMARY_AUDIO_ID, 0,
                                       stn->audio, stn->num_audio,
                                       &audio_lang, 0);
        }

        if (stn->num_pg) {
            _update_stream_psr_by_lang(bd->regs,
                                       PSR_PG_AND_SUB_LANG, PSR_PG_STREAM, 0x80000000,
                                       stn->pg, stn->num_pg,
                                       NULL, audio_lang);
        }
    }
}

static int _is_interactive_title(BLURAY *bd)
{
    if (bd->titles && bd->title_type != title_undef) {
        unsigned title = bd_psr_read(bd->regs, PSR_TITLE_NUMBER);
        if (title == 0xffff && bd->disc_info.first_play->interactive) {
            return 1;
        }
        if (title <= bd->disc_info.num_titles && bd->titles[title]) {
            return bd->titles[title]->interactive;
        }
    }
    return 0;
}

static void _update_chapter_psr(BLURAY *bd)
{
    if (!_is_interactive_title(bd) && bd->title->chap_list.count > 0) {
        uint32_t current_chapter = bd_get_current_chapter(bd);
        bd_psr_write(bd->regs, PSR_CHAPTER,  current_chapter + 1);
    }
}

/*
 * PG
 */

static int _find_pg_stream(BLURAY *bd, uint16_t *pid, int *sub_path_idx, unsigned *sub_clip_idx, uint8_t *char_code)
{
    unsigned  main_clip_idx = bd->st0.clip ? bd->st0.clip->ref : 0;
    unsigned  pg_stream = bd_psr_read(bd->regs, PSR_PG_STREAM);
    const MPLS_STN *stn = &bd->title->pl->play_item[main_clip_idx].stn;

#if 0
    /* Enable decoder unconditionally (required for forced subtitles).
       Display flag is checked in graphics controller. */
    /* check PG display flag from PSR */
    if (!(pg_stream & 0x80000000)) {
      return 0;
    }
#endif

    pg_stream &= 0xfff;

    if (pg_stream > 0 && pg_stream <= stn->num_pg) {
        pg_stream--; /* stream number to table index */
        if (stn->pg[pg_stream].stream_type == 2) {
            *sub_path_idx = stn->pg[pg_stream].subpath_id;
            *sub_clip_idx = stn->pg[pg_stream].subclip_id;
        }
        *pid = stn->pg[pg_stream].pid;

        if (char_code && stn->pg[pg_stream].coding_type == BLURAY_STREAM_TYPE_SUB_TEXT) {
            *char_code = stn->pg[pg_stream].char_code;
    if (st->fp) {
        int64_t clip_size = file_size(st->fp);
        if (clip_size > 0) {

            if (file_seek(st->fp, st->clip_block_pos, SEEK_SET) < 0) {
                BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to seek clip %s!\n", st->clip->name);
                _close_m2ts(st);
                return 0;
            }

            st->clip_size   = clip_size;
            st->int_buf_off = 6144;

            if (st == &bd->st0) {
                const MPLS_PL *pl = st->clip->title->pl;
                const MPLS_STN *stn = &pl->play_item[st->clip->ref].stn;

                st->uo_mask = uo_mask_combine(pl->app_info.uo_mask,
                                              pl->play_item[st->clip->ref].uo_mask);
                _update_uo_mask(bd);

                st->m2ts_filter = m2ts_filter_init((int64_t)st->clip->in_time << 1,
                                                   (int64_t)st->clip->out_time << 1,
                                                   stn->num_video, stn->num_audio,
                                                   stn->num_ig, stn->num_pg);

                _update_clip_psrs(bd, st->clip);

                _init_pg_stream(bd);

                _init_textst_timer(bd);
            }

            return 1;
        }

        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Clip %s empty!\n", st->clip->name);
        _close_m2ts(st);
    }

    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open clip %s!\n", st->clip->name);

    return 0;
}

static int _validate_unit(BLURAY *bd, BD_STREAM *st, uint8_t *buf)
{
    /* Check TP_extra_header Copy_permission_indicator. If != 0, unit may be encrypted. */
    /* Check first sync byte. It should never be encrypted. */
    if (BD_UNLIKELY(buf[0] & 0xc0 || buf[4] != 0x47)) {

        /* Check first sync bytes. If not OK, drop unit. */
        if (buf[4] != 0x47 || buf [4 + 192] != 0x47 || buf[4 + 2*192] != 0x47 || buf[4 + 3*192] != 0x47) {

            /* Some streams have Copy_permission_indicator incorrectly set. */
            /* Check first TS sync byte. If unit is encrypted, first 16 bytes are plain, rest not. */
            /* not 100% accurate (can be random data too). But the unit is broken anyway ... */
            if (buf[4] == 0x47) {

                /* most likely encrypted stream. Check couple of blocks before erroring out. */
                st->encrypted_block_cnt++;

                if (st->encrypted_block_cnt > 10) {
                    /* error out */
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "TP header copy permission indicator != 0. Stream seems to be encrypted.\n");
                    _queue_event(bd, BD_EVENT_ENCRYPTED, BD_ERROR_AACS);
                    return -1;
                }
            }

            /* broken block, ignore it */
            _queue_event(bd, BD_EVENT_READ_ERROR, 1);
            return 0;
        }
    }

    st->eof_hit = 0;
    st->encrypted_block_cnt = 0;
    return 1;
}

static int _skip_unit(BLURAY *bd, BD_STREAM *st)
{
    const size_t len = 6144;

    /* skip broken unit */
    st->clip_block_pos += len;
    st->clip_pos += len;

    _queue_event(bd, BD_EVENT_READ_ERROR, 0);

    /* seek to next unit start */
    if (file_seek(st->fp, st->clip_block_pos, SEEK_SET) < 0) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to seek clip %s!\n", st->clip->name);
        return -1;
    }

    return 0;
}

static int _read_block(BLURAY *bd, BD_STREAM *st, uint8_t *buf)
{
    const size_t len = 6144;

    if (st->fp) {
        BD_DEBUG(DBG_STREAM, "Reading unit at %" PRIu64 "...\n", st->clip_block_pos);

        if (len + st->clip_block_pos <= st->clip_size) {
            size_t read_len;

            if ((read_len = file_read(st->fp, buf, len))) {
                int error;

                if (read_len != len) {
                    BD_DEBUG(DBG_STREAM | DBG_CRIT, "Read %d bytes at %" PRIu64 " ; requested %d !\n", (int)read_len, st->clip_block_pos, (int)len);
                    return _skip_unit(bd, st);
                }
                st->clip_block_pos += len;

                if ((error = _validate_unit(bd, st, buf)) <= 0) {
                    /* skip broken unit */
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Skipping broken unit at %" PRId64 "\n", st->clip_block_pos - len);
                    st->clip_pos += len;
                    return error;
                }

                if (st->m2ts_filter) {
                    int result = m2ts_filter(st->m2ts_filter, buf);
                    if (result < 0) {
                        m2ts_filter_close(&st->m2ts_filter);
                        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "m2ts filter error\n");
                    }
                }

                BD_DEBUG(DBG_STREAM, "Read unit OK!\n");

#ifdef BLURAY_READ_ERROR_TEST
                /* simulate broken blocks */
                if (random() % 1000)
#else
    p->buf = tmp;

    /* read clip to buffer */

    uint8_t *buf = p->buf;
    uint8_t *end = p->buf + p->clip_size;

    for (; buf < end; buf += 6144) {
        if (_read_block(bd, &st, buf) <= 0) {
            BD_DEBUG(DBG_BLURAY|DBG_CRIT, "_preload_m2ts(): error loading %s at %" PRIu64 "\n",
                  st.clip->name, (uint64_t)(buf - p->buf));
            _close_m2ts(&st);
            _close_preload(p);
            return 0;
        }
    }

    /* */

    BD_DEBUG(DBG_BLURAY, "_preload_m2ts(): loaded %" PRIu64 " bytes from %s\n",
          st.clip_size, st.clip->name);

    _close_m2ts(&st);

    return 1;
}

static int64_t _seek_stream(BLURAY *bd, BD_STREAM *st,
                            const NAV_CLIP *clip, uint32_t clip_pkt)
{
    if (!clip)
        return -1;

    if (!st->fp || !st->clip || clip->ref != st->clip->ref) {
        // The position is in a new clip
        st->clip = clip;
        if (!_open_m2ts(bd, st)) {
            return -1;
        }
    }

    if (st->m2ts_filter) {
        m2ts_filter_seek(st->m2ts_filter, 0, (int64_t)st->clip->in_time << 1);
    }

    st->clip_pos = (uint64_t)clip_pkt * 192;
    st->clip_block_pos = (st->clip_pos / 6144) * 6144;

    if (file_seek(st->fp, st->clip_block_pos, SEEK_SET) < 0) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to seek clip %s!\n", st->clip->name);
    }

    st->int_buf_off = 6144;
    st->seek_flag = 1;

    return st->clip_pos;
}

/*
 * Graphics controller interface
 */

static int _run_gc(BLURAY *bd, gc_ctrl_e msg, uint32_t param)
{
    int result = -1;

    if (!bd) {
        return -1;
    }

        bd->disc_info.titles = (const BLURAY_TITLE * const *)titles;
        bd->disc_info.num_titles = index->num_titles;

        /* count titles and fill title info */

        for (ii = 0; ii < index->num_titles; ii++) {
            if (index->titles[ii].object_type == indx_object_type_hdmv) {
                bd->disc_info.num_hdmv_titles++;
                titles[ii + 1]->interactive = (index->titles[ii].hdmv.playback_type == indx_hdmv_playback_type_interactive);
                titles[ii + 1]->id_ref = index->titles[ii].hdmv.id_ref;
            }
            if (index->titles[ii].object_type == indx_object_type_bdj) {
                bd->disc_info.num_bdj_titles++;
                bd->disc_info.bdj_detected = 1;
                titles[ii + 1]->bdj = 1;
                titles[ii + 1]->interactive = (index->titles[ii].bdj.playback_type == indx_bdj_playback_type_interactive);
                titles[ii + 1]->id_ref = atoi(index->titles[ii].bdj.name);
            }

            titles[ii + 1]->accessible =  !(index->titles[ii].access_type & INDX_ACCESS_PROHIBITED_MASK);
            titles[ii + 1]->hidden     = !!(index->titles[ii].access_type & INDX_ACCESS_HIDDEN_MASK);
        }

        pi = &index->first_play;
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.bdj_detected = 1;
            titles[index->num_titles + 1]->bdj = 1;
            titles[index->num_titles + 1]->interactive = (pi->bdj.playback_type == indx_bdj_playback_type_interactive);
            titles[index->num_titles + 1]->id_ref = atoi(pi->bdj.name);
        }
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            titles[index->num_titles + 1]->interactive = (pi->hdmv.playback_type == indx_hdmv_playback_type_interactive);
            titles[index->num_titles + 1]->id_ref = pi->hdmv.id_ref;
        }

        pi = &index->top_menu;
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.bdj_detected = 1;
            titles[0]->bdj = 1;
            titles[0]->interactive = (pi->bdj.playback_type == indx_bdj_playback_type_interactive);
            titles[0]->id_ref = atoi(pi->bdj.name);
        }
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            titles[0]->interactive = (pi->hdmv.playback_type == indx_hdmv_playback_type_interactive);
            titles[0]->id_ref = pi->hdmv.id_ref;
        }

        /* mark supported titles */

        _check_bdj(bd);

        if (bd->disc_info.bdj_detected && !bd->disc_info.bdj_handled) {
            bd->disc_info.num_unsupported_titles = bd->disc_info.num_bdj_titles;
        }

        pi = &index->first_play;
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            bd->disc_info.first_play_supported = 1;
        }
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.first_play_supported = bd->disc_info.bdj_handled;
        }

        pi = &index->top_menu;
        if (pi->object_type == indx_object_type_hdmv && pi->hdmv.id_ref != 0xffff) {
            bd->disc_info.top_menu_supported = 1;
        }
        if (pi->object_type == indx_object_type_bdj) {
            bd->disc_info.top_menu_supported = bd->disc_info.bdj_handled;
        }
    }

    bd_mutex_unlock(&bd->mutex);

    return ((uint64_t)out_time) * 2;
}

int64_t bd_seek_chapter(BLURAY *bd, unsigned chapter)
{
    uint32_t clip_pkt, out_pkt;
    const NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        chapter < bd->title->chap_list.count) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_chapter_search(bd->title, chapter, &clip_pkt, &out_pkt);

        _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_chapter(%u) failed\n", chapter);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

int64_t bd_chapter_pos(BLURAY *bd, unsigned chapter)
{
    uint32_t clip_pkt, out_pkt;
    int64_t ret = -1;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        chapter < bd->title->chap_list.count) {

        // Find the closest access unit to the requested position
        nav_chapter_search(bd->title, chapter, &clip_pkt, &out_pkt);
        ret = (int64_t)out_pkt * 192;
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

uint32_t bd_get_current_chapter(BLURAY *bd)
{
    uint32_t ret = 0;

    bd_mutex_lock(&bd->mutex);

    if (bd->title) {
        ret = nav_chapter_get_current(bd->title, SPN(bd->s_pos));
    }

    bd_mutex_unlock(&bd->mutex);

    return ret;
}

int64_t bd_seek_playitem(BLURAY *bd, unsigned clip_ref)
{
    uint32_t clip_pkt, out_pkt;
    const NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        clip_ref < bd->title->clip_list.count) {

      _change_angle(bd);

      clip     = &bd->title->clip_list.clip[clip_ref];
      clip_pkt = clip->start_pkt;
      out_pkt  = clip->title_pkt;

      _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_playitem(%u) failed\n", clip_ref);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

int64_t bd_seek_mark(BLURAY *bd, unsigned mark)
{
    uint32_t clip_pkt, out_pkt;
    const NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        mark < bd->title->mark_list.count) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_mark_search(bd->title, mark, &clip_pkt, &out_pkt);

        _seek_internal(bd, clip, out_pkt, clip_pkt);

    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_seek_mark(%u) failed\n", mark);
    }

    bd_mutex_unlock(&bd->mutex);

    return bd->s_pos;
}

int64_t bd_seek(BLURAY *bd, uint64_t pos)
{
    uint32_t pkt, clip_pkt, out_pkt, out_time;
    const NAV_CLIP *clip;

    bd_mutex_lock(&bd->mutex);

    if (bd->title &&
        pos < (uint64_t)bd->title->packets * 192) {

        pkt = SPN(pos);

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_packet_search(bd->title, pkt, &clip_pkt, &out_pkt, &out_time);

        _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

                }

                int r = _read_block(bd, st, bd->int_buf);
                if (r > 0) {

                    if (st->ig_pid > 0) {
                        if (gc_decode_ts(bd->graphics_controller, st->ig_pid, bd->int_buf, 1, -1) > 0) {
                            /* initialize menus */
                            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
                        }
                    }
                    if (st->pg_pid > 0) {
                        if (gc_decode_ts(bd->graphics_controller, st->pg_pid, bd->int_buf, 1, -1) > 0) {
                            /* render subtitles */
                            gc_run(bd->graphics_controller, GC_CTRL_PG_UPDATE, 0, NULL);
                        }
                    }
                    if (bd->st_textst.clip) {
                        _update_textst_timer(bd);
                    }

                    st->int_buf_off = st->clip_pos % 6144;

                } else if (r == 0) {
                    /* recoverable error (EOF, broken block) */
                    return out_len;
                } else {
                    /* fatal error */
                    return -1;
                }

                /* finetune seek point (avoid skipping PAT/PMT/PCR) */
                if (BD_UNLIKELY(st->seek_flag)) {
                    st->seek_flag = 0;

                    /* rewind if previous packets contain PAT/PMT/PCR */
                    while (st->int_buf_off >= 192 && TS_PID(bd->int_buf + st->int_buf_off - 192) <= HDMV_PID_PCR) {
                        st->clip_pos -= 192;
                        st->int_buf_off -= 192;
                        bd->s_pos -= 192;
                    }
                }

            }
            if (size > (unsigned int)6144 - st->int_buf_off) {
                size = 6144 - st->int_buf_off;
            }

            /* cut read at clip end packet */
            uint32_t new_clip_pkt = SPN(st->clip_pos + size);
            if (new_clip_pkt > st->clip->end_pkt) {
                BD_DEBUG(DBG_STREAM, "cut %d bytes at end of block\n", (new_clip_pkt - st->clip->end_pkt) * 192);
                size -= (new_clip_pkt - st->clip->end_pkt) * 192;
            }

            /* copy chunk */
            memcpy(buf, bd->int_buf + st->int_buf_off, size);
            buf += size;
            len -= size;
            out_len += size;
            st->clip_pos += size;
            st->int_buf_off += size;
            bd->s_pos += size;
        }

        BD_DEBUG(DBG_STREAM, "%d bytes read OK!\n", out_len);
        return out_len;
}

    _close_preload(&bd->st_textst);

    if (bd->title->sub_path_count <= 0) {
        return 0;
    }

    return _preload_ig_subpath(bd) | _preload_textst_subpath(bd);
}

static int _init_ig_stream(BLURAY *bd)
{
    int      ig_subpath = -1;
    unsigned ig_subclip = 0;
    uint16_t ig_pid     = 0;

    bd->st0.ig_pid = 0;

    if (!bd->title || !bd->graphics_controller) {
        return 0;
    }

    _find_ig_stream(bd, &ig_pid, &ig_subpath, &ig_subclip);

    /* decode already preloaded IG sub-path */
    if (bd->st_ig.clip) {
        gc_decode_ts(bd->graphics_controller, ig_pid, bd->st_ig.buf, SPN(bd->st_ig.clip_size) / 32, -1);
        return 1;
    }

    /* store PID of main path embedded IG stream */
    if (ig_subpath < 0) {
        bd->st0.ig_pid = ig_pid;
        return 1;
    }

    return 0;
}

/*
 * select title / angle
 */

static void _close_playlist(BLURAY *bd)
{
    if (bd->graphics_controller) {
        gc_run(bd->graphics_controller, GC_CTRL_RESET, 0, NULL);
    }

    /* stopping playback in middle of playlist ? */
    if (bd->title && bd->st0.clip) {
        if (bd->st0.clip->ref < bd->title->clip_list.count - 1) {
            /* not last clip of playlist */
            BD_DEBUG(DBG_BLURAY, "close playlist (not last clip)\n");
            _queue_event(bd, BD_EVENT_PLAYLIST_STOP, 0);
        } else {
            /* last clip of playlist */
            int clip_pkt = SPN(bd->st0.clip_pos);
            int skip = bd->st0.clip->end_pkt - clip_pkt;
            BD_DEBUG(DBG_BLURAY, "close playlist (last clip), packets skipped %d\n", skip);
            if (skip > 100) {
                _queue_event(bd, BD_EVENT_PLAYLIST_STOP, 0);
            }
        }
    }

    _close_m2ts(&bd->st0);
    _close_preload(&bd->st_ig);
    _close_preload(&bd->st_textst);

    nav_title_close(&bd->title);

    bd->st0.clip = NULL;

    /* reset UO mask */
    memset(&bd->st0.uo_mask, 0, sizeof(BD_UO_MASK));
    memset(&bd->gc_uo_mask,  0, sizeof(BD_UO_MASK));
    _update_uo_mask(bd);
}

static int _add_known_playlist(BD_DISC *p, const char *mpls_id)
{
    char *old_mpls_ids;
    char *new_mpls_ids = NULL;
    int result = -1;

    old_mpls_ids = disc_property_get(p, DISC_PROPERTY_PLAYLISTS);
    if (!old_mpls_ids) {
        return disc_property_put(p, DISC_PROPERTY_PLAYLISTS, mpls_id);
    }

    /* no duplicates */
    if (str_strcasestr(old_mpls_ids, mpls_id)) {
        goto out;
    }

    new_mpls_ids = str_printf("%s,%s", old_mpls_ids, mpls_id);
    if (new_mpls_ids) {
        result = disc_property_put(p, DISC_PROPERTY_PLAYLISTS, new_mpls_ids);
    }

 out:
    X_FREE(old_mpls_ids);
    X_FREE(new_mpls_ids);
    return result;
}

static int _open_playlist(BLURAY *bd, const char *f_name, unsigned angle)
{
    if (!bd->title_list && bd->title_type == title_undef) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "open_playlist(%s): bd_play() or bd_get_titles() not called\n", f_name);
        disc_event(bd->disc, DISC_EVENT_START, bd->disc_info.num_titles);
    }

    _close_playlist(bd);

    bd->title = nav_title_open(bd->disc, f_name, angle);
    if (bd->title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s!\n", f_name);
        return 0;
    }

    bd->seamless_angle_change = 0;
    bd->s_pos = 0;
    bd->end_of_playlist = 0;
    bd->st0.ig_pid = 0;

    // Get the initial clip of the playlist
    bd->st0.clip = nav_next_clip(bd->title, NULL);

    _update_playlist_psrs(bd);

    if (_open_m2ts(bd, &bd->st0)) {
        BD_DEBUG(DBG_BLURAY, "Title %s selected\n", f_name);

        _find_next_playmark(bd);

        _preload_subpaths(bd);

        bd->st0.seek_flag = 1;

/* BD-J callback */
int bd_play_playlist_at(BLURAY *bd, int playlist, int playitem, int playmark, int64_t time)
{
    int result;

    /* select + seek should be atomic (= player can't read data between select and seek to start position) */
    bd_mutex_lock(&bd->mutex);
    result = _play_playlist_at(bd, playlist, playitem, playmark, time);
    bd_mutex_unlock(&bd->mutex);

    return result;
}

// Select a title for playback
// The title index is an index into the list
// established by bd_get_titles()
int bd_select_title(BLURAY *bd, uint32_t title_idx)
{
    const char *f_name;
    int result;

    // Open the playlist
    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_CRIT | DBG_BLURAY, "Title list not yet read!\n");
        return 0;
    }
    if (bd->title_list->count <= title_idx) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Invalid title index %d!\n", title_idx);
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    bd->title_idx = title_idx;
    f_name = bd->title_list->title_info[title_idx].name;

    result = _open_playlist(bd, f_name, 0);

    bd_mutex_unlock(&bd->mutex);

    return result;
}

uint32_t bd_get_current_title(BLURAY *bd)
{
    return bd->title_idx;
}

static int _bd_select_angle(BLURAY *bd, unsigned angle)
{
    unsigned orig_angle;

    if (bd->title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't select angle: title not yet selected!\n");
        return 0;
    }

    orig_angle = bd->title->angle;

    nav_set_angle(bd->title, angle);

    if (orig_angle == bd->title->angle) {
        return 1;
    }

    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);

    if (!_open_m2ts(bd, &bd->st0)) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Error selecting angle %d !\n", angle);
        return 0;
    }
    title_info->mark_count = title->mark_list.count;
    if (title_info->mark_count) {
        title_info->marks = calloc(title_info->mark_count, sizeof(BLURAY_TITLE_MARK));
        if (!title_info->marks) {
            goto error;
        }
        for (ii = 0; ii < title_info->mark_count; ii++) {
            title_info->marks[ii].idx = ii;
            title_info->marks[ii].type = title->mark_list.mark[ii].mark_type;
            title_info->marks[ii].start = (uint64_t)title->mark_list.mark[ii].title_time * 2;
            title_info->marks[ii].duration = (uint64_t)title->mark_list.mark[ii].duration * 2;
            title_info->marks[ii].offset = (uint64_t)title->mark_list.mark[ii].title_pkt * 192L;
            title_info->marks[ii].clip_ref = title->mark_list.mark[ii].clip_ref;
        }
    }
    title_info->clip_count = title->clip_list.count;
    if (title_info->clip_count) {
        title_info->clips = calloc(title_info->clip_count, sizeof(BLURAY_CLIP_INFO));
        if (!title_info->clips) {
            goto error;
        }
        for (ii = 0; ii < title_info->clip_count; ii++) {
            BLURAY_CLIP_INFO *ci = &title_info->clips[ii];
            const MPLS_PI *pi = &title->pl->play_item[ii];
            const NAV_CLIP *nc = &title->clip_list.clip[ii];

            memcpy(ci->clip_id, pi->clip->clip_id, sizeof(ci->clip_id));
            ci->pkt_count = nc->end_pkt - nc->start_pkt;
            ci->start_time = (uint64_t)nc->title_time * 2;
            ci->in_time = (uint64_t)pi->in_time * 2;
            ci->out_time = (uint64_t)pi->out_time * 2;
            ci->still_mode = pi->still_mode;
            ci->still_time = pi->still_time;
            ci->video_stream_count = pi->stn.num_video;
            ci->audio_stream_count = pi->stn.num_audio;
            ci->pg_stream_count = pi->stn.num_pg + pi->stn.num_pip_pg;
            ci->ig_stream_count = pi->stn.num_ig;
            ci->sec_video_stream_count = pi->stn.num_secondary_video;
            ci->sec_audio_stream_count = pi->stn.num_secondary_audio;
            if (!_copy_streams(nc, &ci->video_streams, pi->stn.video, ci->video_stream_count) ||
                !_copy_streams(nc, &ci->audio_streams, pi->stn.audio, ci->audio_stream_count) ||
                !_copy_streams(nc, &ci->pg_streams, pi->stn.pg, ci->pg_stream_count) ||
                !_copy_streams(nc, &ci->ig_streams, pi->stn.ig, ci->ig_stream_count) ||
                !_copy_streams(nc, &ci->sec_video_streams, pi->stn.secondary_video, ci->sec_video_stream_count) ||
                !_copy_streams(nc, &ci->sec_audio_streams, pi->stn.secondary_audio, ci->sec_audio_stream_count)) {

                goto error;
            }
        }
    }

    title_info->mvc_base_view_r_flag = title->pl->app_info.mvc_base_view_r_flag;

    return title_info;

 error:
    BD_DEBUG(DBG_CRIT, "Out of memory\n");
    bd_free_title_info(title_info);
    return NULL;
}

static BLURAY_TITLE_INFO *_get_title_info(BLURAY *bd, uint32_t title_idx, uint32_t playlist, const char *mpls_name,
                                          unsigned angle)
{
    NAV_TITLE *title;
    BLURAY_TITLE_INFO *title_info;

    /* current title ? => no need to load mpls file */
    bd_mutex_lock(&bd->mutex);
    if (bd->title && bd->title->angle == angle && !strcmp(bd->title->name, mpls_name)) {
        title_info = _fill_title_info(bd->title, title_idx, playlist);
        bd_mutex_unlock(&bd->mutex);
        return title_info;
    }
    bd_mutex_unlock(&bd->mutex);

    title = nav_title_open(bd->disc, mpls_name, angle);
    if (title == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s!\n", mpls_name);
        return NULL;
    }

    title_info = _fill_title_info(title, title_idx, playlist);

    nav_title_close(&title);
    return title_info;
}

BLURAY_TITLE_INFO* bd_get_title_info(BLURAY *bd, uint32_t title_idx, unsigned angle)
{
    if (bd->title_list == NULL) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Title list not yet read!\n");
        return NULL;
    }
    if (bd->title_list->count <= title_idx) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Invalid title index %d!\n", title_idx);
        return NULL;
    }

    return _get_title_info(bd,
                           title_idx, bd->title_list->title_info[title_idx].mpls_id,
                           bd->title_list->title_info[title_idx].name,
                           angle);
}

BLURAY_TITLE_INFO* bd_get_playlist_info(BLURAY *bd, uint32_t playlist, unsigned angle)
{
    char *f_name;
    BLURAY_TITLE_INFO *title_info;

    f_name = str_printf("%05d.mpls", playlist);
    if (!f_name) {
        return NULL;
    }

    title_info = _get_title_info(bd, 0, playlist, f_name, angle);

    X_FREE(f_name);

    return title_info;
}

void bd_free_title_info(BLURAY_TITLE_INFO *title_info)
{
    unsigned int ii;

    if (title_info) {
        X_FREE(title_info->chapters);
        X_FREE(title_info->marks);
        if (title_info->clips) {
            for (ii = 0; ii < title_info->clip_count; ii++) {
                X_FREE(title_info->clips[ii].video_streams);
                X_FREE(title_info->clips[ii].audio_streams);
                X_FREE(title_info->clips[ii].pg_streams);
                X_FREE(title_info->clips[ii].ig_streams);
                X_FREE(title_info->clips[ii].sec_video_streams);
                X_FREE(title_info->clips[ii].sec_audio_streams);
            }
            X_FREE(title_info->clips);
        }
        X_FREE(title_info);
    }
}

/*
 * player settings
 */

int bd_set_player_setting(BLURAY *bd, uint32_t idx, uint32_t value)
{
    static const struct { uint32_t idx; uint32_t  psr; } map[] = {
        { BLURAY_PLAYER_SETTING_PARENTAL,       PSR_PARENTAL },
        { BLURAY_PLAYER_SETTING_AUDIO_CAP,      PSR_AUDIO_CAP },
        { BLURAY_PLAYER_SETTING_AUDIO_LANG,     PSR_AUDIO_LANG },
        { BLURAY_PLAYER_SETTING_PG_LANG,        PSR_PG_AND_SUB_LANG },
        { BLURAY_PLAYER_SETTING_MENU_LANG,      PSR_MENU_LANG },
        { BLURAY_PLAYER_SETTING_COUNTRY_CODE,   PSR_COUNTRY },
        { BLURAY_PLAYER_SETTING_REGION_CODE,    PSR_REGION },
        { BLURAY_PLAYER_SETTING_OUTPUT_PREFER,  PSR_OUTPUT_PREFER },
        { BLURAY_PLAYER_SETTING_DISPLAY_CAP,    PSR_DISPLAY_CAP },
        { BLURAY_PLAYER_SETTING_3D_CAP,         PSR_3D_CAP },
        { BLURAY_PLAYER_SETTING_UHD_CAP,         PSR_UHD_CAP },
        { BLURAY_PLAYER_SETTING_UHD_DISPLAY_CAP, PSR_UHD_DISPLAY_CAP },
        { BLURAY_PLAYER_SETTING_HDR_PREFERENCE,  PSR_UHD_HDR_PREFER },
        { BLURAY_PLAYER_SETTING_SDR_CONV_PREFER, PSR_UHD_SDR_CONV_PREFER },
        { BLURAY_PLAYER_SETTING_VIDEO_CAP,      PSR_VIDEO_CAP },
        { BLURAY_PLAYER_SETTING_TEXT_CAP,       PSR_TEXT_CAP },
        { BLURAY_PLAYER_SETTING_PLAYER_PROFILE, PSR_PROFILE_VERSION },
    };

    unsigned i;
    int result;

    if (idx == BLURAY_PLAYER_SETTING_DECODE_PG) {
        bd_mutex_lock(&bd->mutex);

        bd->decode_pg = !!value;
        result = !bd_psr_write_bits(bd->regs, PSR_PG_STREAM,
                                    (!!value) << 31,
                                    0x80000000);

        bd_mutex_unlock(&bd->mutex);
        return result;
    }

    if (idx == BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE) {
        if (bd->title_type != title_undef) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't disable persistent storage during playback\n");
            return 0;
        }
        bd->bdj_config.no_persistent_storage = !value;
        return 1;
    }

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (idx == map[i].idx) {
            bd_mutex_lock(&bd->mutex);
            result = !bd_psr_setting_write(bd->regs, map[i].psr, value);
            bd_mutex_unlock(&bd->mutex);
            return result;
        }
    }

    return 0;
}

int bd_set_player_setting_str(BLURAY *bd, uint32_t idx, const char *s)
{
    switch (idx) {
        return 0;
    }

    /* first play object ? */
    if (bd->disc_info.first_play_supported) {
        t = bd->disc_info.first_play;
        if (t && t->bdj && t->id_ref == title_num) {
            return _start_bdj(bd, BLURAY_TITLE_FIRST_PLAY);
        }
    }

    /* valid BD-J title from disc index ? */
    if (bd->disc_info.titles) {
        for (ii = 0; ii <= bd->disc_info.num_titles; ii++) {
            t = bd->disc_info.titles[ii];
            if (t && t->bdj && t->id_ref == title_num) {
                return _start_bdj(bd, ii);
            }
        }
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No %s.bdjo in disc index\n", start_object);
    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "No disc index\n");
    }

    return 0;
 }

void bd_stop_bdj(BLURAY *bd)
{
    bd_mutex_lock(&bd->mutex);
    _close_bdj(bd);
    bd_mutex_unlock(&bd->mutex);
}

/*
 * Navigation mode interface
 */

static void _set_scr(BLURAY *bd, int64_t pts)
{
    if (pts >= 0) {
        uint32_t tick = (uint32_t)(((uint64_t)pts) >> 1);
        _update_time_psr(bd, tick);

    } else if (!bd->app_scr) {
        _update_time_psr_from_stream(bd);
    }
}

static void _process_psr_restore_event(BLURAY *bd, const BD_PSR_EVENT *ev)
{
    /* PSR restore events are handled internally.
     * Restore stored playback position.
     */

    BD_DEBUG(DBG_BLURAY, "PSR restore: psr%u = %u\n", ev->psr_idx, ev->new_val);

    switch (ev->psr_idx) {
        case PSR_ANGLE_NUMBER:
            /* can't set angle before playlist is opened */
            return;
        case PSR_TITLE_NUMBER:
            /* pass to the application */
            _queue_event(bd, BD_EVENT_TITLE, ev->new_val);
            return;
        case PSR_CHAPTER:
            /* will be selected automatically */
            return;
        case PSR_PLAYLIST:
            bd_select_playlist(bd, ev->new_val);
            nav_set_angle(bd->title, bd_psr_read(bd->regs, PSR_ANGLE_NUMBER) - 1);
            return;
        case PSR_PLAYITEM:
            bd_seek_playitem(bd, ev->new_val);
            return;
        case PSR_TIME:
            _clip_seek_time(bd, ev->new_val);
            _init_ig_stream(bd);
            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
            return;

        case PSR_SELECTED_BUTTON_ID:
        case PSR_MENU_PAGE_ID:
            /* handled by graphics controller */
            return;

        default:
            /* others: ignore */
            return;
    }
}

/*
 * notification events to APP
 */

static void _process_psr_write_event(BLURAY *bd, const BD_PSR_EVENT *ev)
{
    if (ev->ev_type == BD_PSR_WRITE) {
        BD_DEBUG(DBG_BLURAY, "PSR write: psr%u = %u\n", ev->psr_idx, ev->new_val);
    }

    switch (ev->psr_idx) {

        /* current playback position */

        case PSR_ANGLE_NUMBER:
            _bdj_event  (bd, BDJ_EVENT_ANGLE,   ev->new_val);
            _queue_event(bd, BD_EVENT_ANGLE,    ev->new_val);
            break;
        case PSR_TITLE_NUMBER:
            _queue_event(bd, BD_EVENT_TITLE,    ev->new_val);
            break;
        case PSR_PLAYLIST:
            _bdj_event  (bd, BDJ_EVENT_PLAYLIST,ev->new_val);
            _queue_event(bd, BD_EVENT_PLAYLIST, ev->new_val);
            break;
        case PSR_PLAYITEM:
            _bdj_event  (bd, BDJ_EVENT_PLAYITEM,ev->new_val);
            _queue_event(bd, BD_EVENT_PLAYITEM, ev->new_val);
            break;
        case PSR_TIME:
            _bdj_event  (bd, BDJ_EVENT_PTS,     ev->new_val);
            break;

        case 102:
            _bdj_event  (bd, BDJ_EVENT_PSR102,  ev->new_val);
            break;
        case 103:
            disc_event(bd->disc, DISC_EVENT_APPLICATION, ev->new_val);
            break;

        default:;
    }
}

static void _process_psr_change_event(BLURAY *bd, const BD_PSR_EVENT *ev)
{
    BD_DEBUG(DBG_BLURAY, "PSR change: psr%u = %u\n", ev->psr_idx, ev->new_val);

    _process_psr_write_event(bd, ev);

    switch (ev->psr_idx) {

        /* current playback position */

        case PSR_TITLE_NUMBER:
            disc_event(bd->disc, DISC_EVENT_TITLE, ev->new_val);
            break;

        case PSR_CHAPTER:
            _bdj_event  (bd, BDJ_EVENT_CHAPTER, ev->new_val);
            if (ev->new_val != 0xffff) {
                _queue_event(bd, BD_EVENT_CHAPTER,  ev->new_val);
            }
            break;

        /* stream selection */

        case PSR_IG_STREAM_ID:
            _queue_event(bd, BD_EVENT_IG_STREAM, ev->new_val);
            break;

        case PSR_PRIMARY_AUDIO_ID:
            _bdj_event(bd, BDJ_EVENT_AUDIO_STREAM, ev->new_val);
            _queue_event(bd, BD_EVENT_AUDIO_STREAM, ev->new_val);
            break;

        case PSR_PG_STREAM:
            _bdj_event(bd, BDJ_EVENT_SUBTITLE, ev->new_val);
            if ((ev->new_val & 0x80000fff) != (ev->old_val & 0x80000fff)) {
                _queue_event(bd, BD_EVENT_PG_TEXTST,        !!(ev->new_val & 0x80000000));
                _queue_event(bd, BD_EVENT_PG_TEXTST_STREAM,    ev->new_val & 0xfff);
            }

            bd_mutex_lock(&bd->mutex);
            if (bd->st0.clip) {
                _init_pg_stream(bd);
                if (bd->st_textst.clip) {
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Changing TextST stream\n");
                    _preload_textst_subpath(bd);
                }
            }
            bd_mutex_unlock(&bd->mutex);

            break;

        case PSR_SECONDARY_AUDIO_VIDEO:
            /* secondary video */
            if ((ev->new_val & 0x8f00ff00) != (ev->old_val & 0x8f00ff00)) {
                _queue_event(bd, BD_EVENT_SECONDARY_VIDEO, !!(ev->new_val & 0x80000000));
                _queue_event(bd, BD_EVENT_SECONDARY_VIDEO_SIZE, (ev->new_val >> 24) & 0xf);
                _queue_event(bd, BD_EVENT_SECONDARY_VIDEO_STREAM, (ev->new_val & 0xff00) >> 8);
            }
            /* secondary audio */
            if ((ev->new_val & 0x400000ff) != (ev->old_val & 0x400000ff)) {
                _queue_event(bd, BD_EVENT_SECONDARY_AUDIO, !!(ev->new_val & 0x40000000));
                _queue_event(bd, BD_EVENT_SECONDARY_AUDIO_STREAM, ev->new_val & 0xff);
            }
            _bdj_event(bd, BDJ_EVENT_SECONDARY_STREAM, ev->new_val);
            break;

        /* 3D status */
        case PSR_3D_STATUS:
            _queue_event(bd, BD_EVENT_STEREOSCOPIC_STATUS, ev->new_val & 1);
            break;

        default:;
    }
}

static void _process_psr_event(void *handle, const BD_PSR_EVENT *ev)
{
    BLURAY *bd = (BLURAY*)handle;

    switch(ev->ev_type) {
        case BD_PSR_WRITE:
            _process_psr_write_event(bd, ev);
            break;
        case BD_PSR_CHANGE:
            _process_psr_change_event(bd, ev);
            break;
        case BD_PSR_RESTORE:
            _process_psr_restore_event(bd, ev);
            break;

        case BD_PSR_SAVE:
            BD_DEBUG(DBG_BLURAY, "PSR save event\n");
            break;
        default:
            BD_DEBUG(DBG_BLURAY, "PSR event %d: psr%u = %u\n", ev->ev_type, ev->psr_idx, ev->new_val);
            break;
    }
}

static void _queue_initial_psr_events(BLURAY *bd)
{
    const uint32_t psrs[] = {
        PSR_ANGLE_NUMBER,
        PSR_TITLE_NUMBER,
        PSR_IG_STREAM_ID,
        PSR_PRIMARY_AUDIO_ID,
        PSR_PG_STREAM,
        PSR_SECONDARY_AUDIO_VIDEO,
    };
    unsigned ii;
    BD_PSR_EVENT ev;

    ev.ev_type = BD_PSR_CHANGE;
    ev.old_val = 0;

    for (ii = 0; ii < sizeof(psrs) / sizeof(psrs[0]); ii++) {
        ev.psr_idx = psrs[ii];
        ev.new_val = bd_psr_read(bd->regs, psrs[ii]);

        _process_psr_change_event(bd, &ev);
    }
}

static int _play_bdj(BLURAY *bd, unsigned title)
{
    int result;

    bd->title_type = title_bdj;

    result = _start_bdj(bd, title);
    if (result <= 0) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Can't play BD-J title %d\n", title);
        bd->title_type = title_undef;
        _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_BDJ);
    }

    return result;
}

static int _play_hdmv(BLURAY *bd, unsigned id_ref)
{
    int result = 1;

    _stop_bdj(bd);
    if (title <= bd->disc_info.num_titles) {

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, title); /* 5.2.3.3 */
        if (bd->disc_info.titles[title]->bdj) {
            return _play_bdj(bd, title);
        } else {
            return _play_hdmv(bd, bd->disc_info.titles[title]->id_ref);
        }
    } else {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "_play_title(#%d): Title not found\n", title);
    }

    return 0;
}

/* BD-J callback */
int bd_play_title_internal(BLURAY *bd, unsigned title)
{
    /* used by BD-J. Like bd_play_title() but bypasses UO mask checks. */
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _play_title(bd, title);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

int bd_play(BLURAY *bd)
{
    int result;

    bd_mutex_lock(&bd->mutex);

    /* reset player state */

    bd->title_type = title_undef;

    if (bd->hdmv_vm) {
        hdmv_vm_free(&bd->hdmv_vm);
    }

    if (!bd->event_queue) {
        bd->event_queue = event_queue_new(sizeof(BD_EVENT));

        bd_psr_lock(bd->regs);
        bd_psr_register_cb(bd->regs, _process_psr_event, bd);
        _queue_initial_psr_events(bd);
        bd_psr_unlock(bd->regs);
    }

    disc_event(bd->disc, DISC_EVENT_START, 0);

    /* start playback from FIRST PLAY title */

    result = _play_title(bd, BLURAY_TITLE_FIRST_PLAY);

    bd_mutex_unlock(&bd->mutex);

    return result;
}

static int _try_play_title(BLURAY *bd, unsigned title)
{
    if (bd->title_type == title_undef && title != BLURAY_TITLE_FIRST_PLAY) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_play_title(): bd_play() not called\n");
        return 0;
    }

    if (bd->uo_mask.title_search) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "title search masked\n");
        _bdj_event(bd, BDJ_EVENT_UO_MASKED, UO_MASK_TITLE_SEARCH_INDEX);
        return 0;
    }

    return _play_title(bd, title);
}

int bd_play_title(BLURAY *bd, unsigned title)
{
    int ret;

    if (title == BLURAY_TITLE_TOP_MENU) {
        /* menu call uses different UO mask */
        return bd_menu_call(bd, -1);
    }

    bd_mutex_lock(&bd->mutex);
    ret = _try_play_title(bd, title);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

static int _try_menu_call(BLURAY *bd, int64_t pts)
{
    _set_scr(bd, pts);

    if (bd->title_type == title_undef) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_menu_call(): bd_play() not called\n");
        return 0;
    }

    if (bd->uo_mask.menu_call) {
        BD_DEBUG(DBG_BLURAY | DBG_CRIT, "menu call masked\n");
        _bdj_event(bd, BDJ_EVENT_UO_MASKED, UO_MASK_MENU_CALL_INDEX);
        return 0;
    }

    if (bd->title_type == title_hdmv) {
        if (hdmv_vm_suspend_pl(bd->hdmv_vm) < 0) {
            BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_menu_call(): error storing playback location\n");
        }
    }

    return _play_title(bd, BLURAY_TITLE_TOP_MENU);
}

int bd_menu_call(BLURAY *bd, int64_t pts)
{
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _try_menu_call(bd, pts);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

static void _process_hdmv_vm_event(BLURAY *bd, HDMV_EVENT *hev)
{
    BD_DEBUG(DBG_BLURAY, "HDMV event: %s(%d): %d\n", hdmv_event_str(hev->event), hev->event, hev->param);

    switch (hev->event) {
        case HDMV_EVENT_TITLE:
            _close_playlist(bd);
            _play_title(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_PL:
            if (!bd_select_playlist(bd, hev->param)) {
                /* Missing playlist ?
                 * Seen on some discs while checking UHD capability.
                 * It seems only error message playlist is present, on success
                 * non-existing playlist is selected ...
                 */
                bd->hdmv_num_invalid_pl++;
                if (bd->hdmv_num_invalid_pl < 10) {
                    hdmv_vm_resume(bd->hdmv_vm);
                    bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
                    BD_DEBUG(DBG_BLURAY | DBG_CRIT, "Ignoring non-existing playlist %05d.mpls in HDMV mode\n", hev->param);
                    break;
                }
            } else {
                bd->hdmv_num_invalid_pl = 0;
            }

            /* initialize menus */
            _init_ig_stream(bd);
            _run_gc(bd, GC_CTRL_INIT_MENU, 0);
            break;

        case HDMV_EVENT_PLAY_PI:
            bd_seek_playitem(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_PM:
            bd_seek_mark(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_STOP:
            // stop current playlist
            _close_playlist(bd);

            bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
            break;

        case HDMV_EVENT_STILL:
            _queue_event(bd, BD_EVENT_STILL, hev->param);
            break;

        case HDMV_EVENT_ENABLE_BUTTON:
            _run_gc(bd, GC_CTRL_ENABLE_BUTTON, hev->param);
            break;

        case HDMV_EVENT_DISABLE_BUTTON:
            _run_gc(bd, GC_CTRL_DISABLE_BUTTON, hev->param);
            break;

        case HDMV_EVENT_SET_BUTTON_PAGE:
            _run_gc(bd, GC_CTRL_SET_BUTTON_PAGE, hev->param);
            break;

        case HDMV_EVENT_POPUP_OFF:
            _run_gc(bd, GC_CTRL_POPUP, 0);
            break;

        case HDMV_EVENT_IG_END:
            _run_gc(bd, GC_CTRL_IG_END, 0);
            break;

        case HDMV_EVENT_END:
        case HDMV_EVENT_NONE:
      //default:
            break;
    }
}

static int _run_hdmv(BLURAY *bd)
{
    HDMV_EVENT hdmv_ev;

    /* run VM */
    if (hdmv_vm_run(bd->hdmv_vm, &hdmv_ev) < 0) {
        _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_HDMV);
        bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
        return -1;
    }

    /* process all events */
    do {
        _process_hdmv_vm_event(bd, &hdmv_ev);

    } while (!hdmv_vm_get_event(bd->hdmv_vm, &hdmv_ev));

    /* update VM state */
    bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);

    /* update UO mask */
    _update_hdmv_uo_mask(bd);

    return 0;
}

static int _read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event)
{
    if (_get_event(bd, event)) {
        return 0;
    }

    /* run HDMV VM ? */
    if (bd->title_type == title_hdmv) {

        int loops = 0;
        while (!bd->hdmv_suspended) {

            if (_run_hdmv(bd) < 0) {
                BD_DEBUG(DBG_BLURAY|DBG_CRIT, "bd_read_ext(): HDMV VM error\n");
                bd->title_type = title_undef;
                return -1;
            }
            if (loops++ > 100) {
                /* Detect infinite loops.
                 * Broken disc may cause infinite loop between graphics controller and HDMV VM.
                 * This happens ex. with "Butterfly on a Wheel":
                 * Triggering unmasked "Menu Call" UO in language selection menu skips
                 * menu system initialization code, resulting in infinite loop in root menu.
                 */
                BD_DEBUG(DBG_BLURAY | DBG_CRIT, "bd_read_ext(): detected possible HDMV mode live lock (%d loops)\n", loops);
                _queue_event(bd, BD_EVENT_ERROR, BD_ERROR_HDMV);
            }
            if (_get_event(bd, event)) {
                return 0;
            }
        }

        if (bd->gc_status & GC_STATUS_ANIMATE) {
            _run_gc(bd, GC_CTRL_NOP, 0);
        }
    }

    if (len < 1) {
        /* just polled events ? */
        return 0;
    }

    if (bd->title_type == title_bdj) {
        if (bd->end_of_playlist == 1) {
            _bdj_event(bd, BDJ_EVENT_END_OF_PLAYLIST, bd_psr_read(bd->regs, PSR_PLAYLIST));
            bd->end_of_playlist |= 2;
        }

        if (!bd->title) {
            /* BD-J title running but no playlist playing */
            _queue_event(bd, BD_EVENT_IDLE, 0);
            return 0;
        }

        if (bd->bdj_wait_start) {
            /* BD-J playlist prefethed but not yet playing */
            _queue_event(bd, BD_EVENT_IDLE, 1);
            return 0;
        }
    }

    int bytes = _bd_read_locked(bd, buf, len);

    if (bytes == 0) {

        // if no next clip (=end of title), resume HDMV VM
        if (!bd->st0.clip && bd->title_type == title_hdmv) {
            hdmv_vm_resume(bd->hdmv_vm);
            bd->hdmv_suspended = !hdmv_vm_running(bd->hdmv_vm);
            BD_DEBUG(DBG_BLURAY, "bd_read_ext(): reached end of playlist. hdmv_suspended=%d\n", bd->hdmv_suspended);
        }
    }

    _get_event(bd, event);

    return bytes;
}

int bd_read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event)
{
    int ret;
    bd_mutex_lock(&bd->mutex);
    ret = _read_ext(bd, buf, len, event);
    bd_mutex_unlock(&bd->mutex);
    return ret;
}

int bd_get_event(BLURAY *bd, BD_EVENT *event)
{
    if (!bd->event_queue) {
        bd->event_queue = event_queue_new(sizeof(BD_EVENT));

        bd_psr_register_cb(bd->regs, _process_psr_event, bd);
        _queue_initial_psr_events(bd);
    }

    if (event) {
        return _get_event(bd, event);
    }

    return 0;
}

/*
 * user interaction
 */

void bd_set_scr(BLURAY *bd, int64_t pts)
{
    bd_mutex_lock(&bd->mutex);
    bd->app_scr = 1;
    _set_scr(bd, pts);
    bd_mutex_unlock(&bd->mutex);
}

static int _set_rate(BLURAY *bd, uint32_t rate)
{
    if (!bd->title) {
        return -1;
    }

    if (bd->title_type == title_bdj) {
        return _bdj_event(bd, BDJ_EVENT_RATE, rate);
    }

    return 0;
}

int bd_set_rate(BLURAY *bd, uint32_t rate)
{
    int result;

    bd_mutex_lock(&bd->mutex);
    result = _set_rate(bd, rate);
    bd_mutex_unlock(&bd->mutex);

    return result;
}

int bd_mouse_select(BLURAY *bd, int64_t pts, uint16_t x, uint16_t y)
{
    uint32_t param = (x << 16) | y;
    int result = -1;

    bd_mutex_lock(&bd->mutex);

    _set_scr(bd, pts);

    if (bd->title_type == title_hdmv) {
        result = _run_gc(bd, GC_CTRL_MOUSE_MOVE, param);
    } else if (bd->title_type == title_bdj) {
        result = _bdj_event(bd, BDJ_EVENT_MOUSE, param);
    }

    bd_mutex_unlock(&bd->mutex);

    return result;
}

#define BD_VK_FLAGS_MASK (BD_VK_KEY_PRESSED | BD_VK_KEY_TYPED | BD_VK_KEY_RELEASED)
#define BD_VK_KEY(k)     ((k) & ~(BD_VK_FLAGS_MASK))
#define BD_VK_FLAGS(k)   ((k) & BD_VK_FLAGS_MASK)
/* HDMV: key is triggered when pressed down */
#define BD_KEY_TYPED(k)  (!((k) & (BD_VK_KEY_TYPED | BD_VK_KEY_RELEASED)))

int bd_user_input(BLURAY *bd, int64_t pts, uint32_t key)
{
    int result = -1;

    if (BD_VK_KEY(key) == BD_VK_ROOT_MENU) {
        if (BD_KEY_TYPED(key)) {
            return bd_menu_call(bd, pts);
        }
        return 0;
    }

    bd_mutex_lock(&bd->mutex);

    _set_scr(bd, pts);

    if (bd->title_type == title_hdmv) {
        if (BD_KEY_TYPED(key)) {
            result = _run_gc(bd, GC_CTRL_VK_KEY, BD_VK_KEY(key));
        } else {
            result = 0;
        }

    } else if (bd->title_type == title_bdj) {
        if (!BD_VK_FLAGS(key)) {
            /* No flags --> single key press event */
            key |= BD_VK_KEY_PRESSED | BD_VK_KEY_TYPED | BD_VK_KEY_RELEASED;
        }
        result = _bdj_event(bd, BDJ_EVENT_VK_KEY, key);
    }

    bd_mutex_unlock(&bd->mutex);

    return result;
}

void bd_register_overlay_proc(BLURAY *bd, void *handle, bd_overlay_proc_f func)
{
    if (!bd) {
        return;
    }

    bd_mutex_lock(&bd->mutex);

    gc_free(&bd->graphics_controller);

    if (func) {
        bd->graphics_controller = gc_init(bd->regs, handle, func);
    }

    bd_mutex_unlock(&bd->mutex);
}

void bd_register_argb_overlay_proc(BLURAY *bd, void *handle, bd_argb_overlay_proc_f func, BD_ARGB_BUFFER *buf)
{
    if (!bd) {
        return;
    }

    bd_mutex_lock(&bd->argb_buffer_mutex);

    bd->argb_overlay_proc        = func;
    bd->argb_overlay_proc_handle = handle;
    bd->argb_buffer              = buf;

    bd_mutex_unlock(&bd->argb_buffer_mutex);
}

int bd_get_sound_effect(BLURAY *bd, unsigned sound_id, BLURAY_SOUND_EFFECT *effect)
{
    if (!bd || !effect) {
        return -1;
    }

    if (!bd->sound_effects) {

        bd->sound_effects = sound_get(bd->disc);
        if (!bd->sound_effects) {
            return -1;
        }
    }

    if (sound_id < bd->sound_effects->num_sounds) {
        SOUND_OBJECT *o = &bd->sound_effects->sounds[sound_id];

        effect->num_channels = o->num_channels;
        effect->num_frames   = o->num_frames;
        effect->samples      = (const int16_t *)o->samples;

        return 1;
    }

    return 0;
}

/*
 * Direct file access
 */

static int _bd_read_file(BLURAY *bd, const char *dir, const char *file, void **data, int64_t *size)
{
    if (!bd || !bd->disc || !file || !data || !size) {
        BD_DEBUG(DBG_CRIT, "Invalid arguments for bd_read_file()\n");
        return 0;
    }

    *data = NULL;
    *size = (int64_t)disc_read_file(bd->disc, dir, file, (uint8_t**)data);
    if (!*data || *size < 0) {
        BD_DEBUG(DBG_BLURAY, "bd_read_file() failed\n");
        X_FREE(*data);
        return 0;
    }

    BD_DEBUG(DBG_BLURAY, "bd_read_file(): read %" PRId64 " bytes from %s" DIR_SEP "%s\n",
             *size, dir ? dir : "", file);
    return 1;
}

int bd_read_file(BLURAY *bd, const char *path, void **data, int64_t *size)
{
    return _bd_read_file(bd, NULL, path, data, size);
}

struct bd_dir_s *bd_open_dir(BLURAY *bd, const char *dir)
{
    if (!bd || dir == NULL) {
        return NULL;
    }
    return disc_open_dir(bd->disc, dir);
}

struct bd_file_s *bd_open_file_dec(BLURAY *bd, const char *path)
{
    if (!bd || path == NULL) {
        return NULL;
    }
    return disc_open_path_dec(bd->disc, path);
}

/*
 * Metadata
 */

const struct meta_dl *bd_get_meta(BLURAY *bd)
{
    const struct meta_dl *meta = NULL;

    if (!bd) {
        return NULL;
    }

    if (!bd->meta) {
        bd->meta = meta_parse(bd->disc);
    }

    uint32_t psr_menu_lang = bd_psr_read(bd->regs, PSR_MENU_LANG);

    if (psr_menu_lang != 0 && psr_menu_lang != 0xffffff) {
        const char language_code[] = {(psr_menu_lang >> 16) & 0xff, (psr_menu_lang >> 8) & 0xff, psr_menu_lang & 0xff, 0 };
        meta = meta_get(bd->meta, language_code);
    } else {
        meta = meta_get(bd->meta, NULL);
    }

    /* assign title names to disc_info */
    if (meta && bd->titles) {
        unsigned ii;
        for (ii = 0; ii < meta->toc_count; ii++) {
            if (meta->toc_entries[ii].title_number > 0 && meta->toc_entries[ii].title_number <= bd->disc_info.num_titles) {
                bd->titles[meta->toc_entries[ii].title_number]->name = meta->toc_entries[ii].title_name;
            }
        }
        bd->disc_info.disc_name = meta->di_name;
    }

    return meta;
}

int bd_get_meta_file(BLURAY *bd, const char *name, void **data, int64_t *size)
{
    return _bd_read_file(bd, DIR_SEP "BDMV" DIR_SEP "META" DIR_SEP "DL", name, data, size);
}

/*
 * Database access
 */

#include "bdnav/clpi_parse.h"
#include "bdnav/mpls_parse.h"

struct clpi_cl *bd_get_clpi(BLURAY *bd, unsigned clip_ref)
{
    if (bd->title && clip_ref < bd->title->clip_list.count) {
        const NAV_CLIP *clip = &bd->title->clip_list.clip[clip_ref];
        return clpi_copy(clip->cl);
    }
    return NULL;
}

struct clpi_cl *bd_read_clpi(const char *path)
{
    return clpi_parse(path);
}

void bd_free_clpi(struct clpi_cl *cl)
{
    clpi_free(&cl);
}

struct mpls_pl *bd_read_mpls(const char *mpls_file)
{
    return mpls_parse(mpls_file);
}

void bd_free_mpls(struct mpls_pl *pl)
{
    mpls_free(&pl);
}

struct mobj_objects *bd_read_mobj(const char *mobj_file)
{
    return mobj_parse(mobj_file);
}

void bd_free_mobj(struct mobj_objects *obj)
{
    mobj_free(&obj);
}

struct bdjo_data *bd_read_bdjo(const char *bdjo_file)
{
    return bdjo_parse(bdjo_file);
}

void bd_free_bdjo(struct bdjo_data *obj)
{
    bdjo_free(&obj);
}
