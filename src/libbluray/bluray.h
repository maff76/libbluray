/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2010-2017  Petri Hintukainen
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

/**
 * @file
 * \brief libbluray API
 */

#ifndef BLURAY_H_
#define BLURAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define TITLES_ALL              0    /**< all titles. */
#define TITLES_FILTER_DUP_TITLE 0x01 /**< remove duplicate titles. */
#define TITLES_FILTER_DUP_CLIP  0x02 /**< remove titles that have duplicate
                                          clips. */
#define TITLES_RELEVANT \
  (TITLES_FILTER_DUP_TITLE | TITLES_FILTER_DUP_CLIP) /**< remove duplicate
                                                          titles and clips */

/** This structure is opaque. It represents a libbluray instance. */
typedef struct bluray BLURAY;

/*
 * Disc info
 */

/* AACS error codes */
#define BD_AACS_CORRUPTED_DISC  -1  /**< Corrupt disc (missing/invalid files) */
#define BD_AACS_NO_CONFIG       -2  /**< AACS configuration file missing      */
#define BD_AACS_NO_PK           -3  /**< No valid processing key found        */
#define BD_AACS_NO_CERT         -4  /**< No valid certificate found           */
#define BD_AACS_CERT_REVOKED    -5  /**< All certificates have been revoked   */
#define BD_AACS_MMC_FAILED      -6  /**< MMC (disc drive interaction) failed  */

/** HDMV / BD-J title information */
typedef struct {
    const char *name;         /**< optional title name in preferred language */
    uint8_t     interactive;  /**< 1 if title is interactive (title length and playback position should not be shown in UI) */
    uint8_t     accessible;   /**< 1 if it is allowed to jump into this title */
    uint8_t     hidden;       /**< 1 if title number should not be shown during playback */

    uint8_t     bdj;          /**< 0 - HDMV title. 1 - BD-J title */
    uint32_t    id_ref;       /**< Movie Object number / bdjo file number */
} BLURAY_TITLE;

/** BluRay disc information */
typedef struct {
    uint8_t  bluray_detected;   /**< 1 if BluRay disc was detected */

    /* Disc ID */
    const char *disc_name;      /**< optional disc name in preferred language */
    const char *udf_volume_id;  /**< optional UDF volume identifier */
    uint8_t     disc_id[20];    /**< Disc ID */

    /** HDMV / BD-J titles */
    uint8_t  no_menu_support;            /**< 1 if this disc can't be played using on-disc menus */
    uint8_t  first_play_supported;       /**< 1 if First Play title is present on the disc and can be played */
    uint8_t  top_menu_supported;         /**< 1 if Top Menu title is present on the disc and can be played */

    uint32_t             num_titles;     /**< number of titles on the disc, not including "First Play" and "Top Menu" */
    const BLURAY_TITLE  *const *titles;  /**< index is title number 1 ... N */
    const BLURAY_TITLE  *first_play;     /**< titles[N+1].   NULL if not present on the disc. */
    const BLURAY_TITLE  *top_menu;       /**< titles[0]. NULL if not present on the disc. */

    uint32_t num_hdmv_titles;            /**< number of HDMV titles */
    uint32_t num_bdj_titles;             /**< number of BD-J titles */
    uint32_t num_unsupported_titles;     /**< number of unsupported titles */

    /** BD-J info (valid only if disc uses BD-J) */
    uint8_t  bdj_detected;     /**< 1 if disc uses BD-J */
    uint8_t  bdj_supported;    /**< (deprecated) */
    uint8_t  libjvm_detected;  /**< 1 if usable Java VM was found */
    uint8_t  bdj_handled;      /**< 1 if usable Java VM + libbluray.jar was found */

    char bdj_org_id[9];        /**< (BD-J) disc organization ID */
    char bdj_disc_id[33];      /**< (BD-J) disc ID */

    /* disc application info */
    uint8_t video_format;                     /**< \ref bd_video_format_e */
    uint8_t frame_rate;                       /**< \ref bd_video_rate_e */
    uint8_t content_exist_3D;                 /**< 1 if 3D content exists on the disc */
    uint8_t initial_output_mode_preference;   /**< 0 - 2D, 1 - 3D */
    uint8_t provider_data[32];                /**< Content provider data */

    /* AACS info  (valid only if disc uses AACS) */
    uint8_t  aacs_detected;     /**< 1 if disc is using AACS encoding */
    uint8_t  libaacs_detected;  /**< 1 if usable AACS decoding library was found */
    uint8_t  aacs_handled;      /**< 1 if disc is using supported AACS encoding */

    int      aacs_error_code;   /**< AACS error code (BD_AACS_*) */
    int      aacs_mkbv;         /**< AACS MKB version */

    /* BD+ info  (valid only if disc uses BD+) */
    uint8_t  bdplus_detected;     /**< 1 if disc is using BD+ encoding */
    uint8_t  libbdplus_detected;  /**< 1 if usable BD+ decoding library was found */
    uint8_t  bdplus_handled;      /**< 1 if disc is using supporred BD+ encoding */

    uint8_t  bdplus_gen;          /**< BD+ content code generation */
    uint32_t bdplus_date;         /**< BD+ content code relese date ((year<<16)|(month<<8)|day) */

    /* disc application info (libbluray > 1.2.0) */
    uint8_t initial_dynamic_range_type; /**< bd_dynamic_range_type_e */

} BLURAY_DISC_INFO;

/*
 * Playlist info
 */

/** Stream video coding type */
typedef enum {
    BLURAY_STREAM_TYPE_VIDEO_MPEG1              = 0x01,
    BLURAY_STREAM_TYPE_VIDEO_MPEG2              = 0x02,
    BLURAY_STREAM_TYPE_AUDIO_MPEG1              = 0x03,
    BLURAY_STREAM_TYPE_AUDIO_MPEG2              = 0x04,
    BLURAY_STREAM_TYPE_AUDIO_LPCM               = 0x80,
typedef struct bd_mark {
    uint32_t    idx;       /**< Mark index (number - 1) */
    int         type;      /**< \ref bd_mark_type_e */
    uint64_t    start;     /**< mark media time, 90kHz, ("playlist time") */
    uint64_t    duration;  /**< time to next mark */
    uint64_t    offset;    /**< mark distance from title start, bytes */
    unsigned    clip_ref;  /**< Clip reference (index to playlist clips list) */
} BLURAY_TITLE_MARK;

/** Playlist information */
typedef struct bd_title_info {
    uint32_t             idx;            /**< Playlist index number (filled only with bd_get_title_info()) */
    uint32_t             playlist;       /**< Playlist ID (mpls file name) */
    uint64_t             duration;       /**< Playlist duration, 90 kHz */
    uint32_t             clip_count;     /**< Number of clips */
    uint8_t              angle_count;    /**< Number of angles */
    uint32_t             chapter_count;  /**< Number of chapters */
    uint32_t             mark_count;     /**< Number of playmarks */
    BLURAY_CLIP_INFO     *clips;         /**< Clip information */
    BLURAY_TITLE_CHAPTER *chapters;      /**< Chapter information */
    BLURAY_TITLE_MARK    *marks;         /**< Playmark information */

    uint8_t              mvc_base_view_r_flag;  /**< MVC base view (0 - left, 1 - right) */
} BLURAY_TITLE_INFO;

/** Sound effect data */
typedef struct bd_sound_effect {
    uint8_t         num_channels; /**< 1 - mono, 2 - stereo */
    uint32_t        num_frames;   /**< Number of audio frames */
    const int16_t  *samples;      /**< 48000 Hz, 16 bit LPCM. Interleaved if stereo */
} BLURAY_SOUND_EFFECT;


/**
 *  Get libbluray version
 *
 *  Get the version of libbluray (runtime)
 *
 *  See also bluray-version.h
 *
 * @param major where to store major version
 * @param minor where to store minor version
 * @param micro where to store micro version
 */
void bd_get_version(int *major, int *minor, int *micro);

/*
 * Disc functions
 */

struct bd_dir_s;
struct bd_file_s;
struct meta_dl;

/**
 *  Open BluRay disc
 *
 *  Shortcut for bd_open_disc(bd_init(), device_path, keyfile_path)
 *
 * @param device_path   path to mounted Blu-ray disc, device or image file
 * @param keyfile_path  path to KEYDB.cfg (may be NULL)
 * @return allocated BLURAY object, NULL if error
 */
BLURAY *bd_open(const char *device_path, const char *keyfile_path);

/**
 *  Initialize BLURAY object
 *
 *  Resulting object can be passed to following bd_open_??? functions.
 *
 * @return allocated BLURAY object, NULL if error
 */
BLURAY *bd_init(void);

/**
 *  Open BluRay disc
 *
 * @param bd  BLURAY object
 * @param device_path   path to mounted Blu-ray disc, device or image file
 * @param keyfile_path  path to KEYDB.cfg (may be NULL)
 * @return 1 on success, 0 if error
 */
int bd_open_disc(BLURAY *bd, const char *device_path, const char *keyfile_path);

/**
 *  Open BluRay disc
 *
 * @param bd  BLURAY object
 * @param read_blocks_handle  opaque handle for read_blocks
 * @param read_blocks  function used to read disc blocks
 * @return 1 on success, 0 if error
 */
int bd_open_stream(BLURAY *bd,
                   void *read_blocks_handle,
                   int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks));

/**
 *  Open BluRay disc
 *
 * @param bd  BLURAY object
 * @param handle  opaque handle for open_dir and open_file
 * @param open_dir  function used to open a directory
 * @param open_file  function used to open a file
 * @return 1 on success, 0 if error
 */
int bd_open_files(BLURAY *bd,
                  void *handle,
                  struct bd_dir_s *(*open_dir)(void *handle, const char *rel_path),
                  struct bd_file_s *(*open_file)(void *handle, const char *rel_path));

/**
 *  Close BluRay disc
 *
 * @param bd  BLURAY object
 */
void bd_close(BLURAY *bd);

/**
 *
 *  Get information about current BluRay disc
 *
 * @param bd  BLURAY object
 * @return pointer to BLURAY_DISC_INFO object, NULL on error
 */
const BLURAY_DISC_INFO *bd_get_disc_info(BLURAY *bd);

/**
 *
 *  Get meta information about current BluRay disc.
 *
 *  Meta information is optional in BluRay discs.
 *  If information is provided in multiple languages, currently
 *  selected language (BLURAY_PLAYER_SETTING_MENU_LANG) is used.
 *
 *  Referenced thumbnail images should be read with bd_get_meta_file().
 *
 * @param bd  BLURAY object
 * @return META_DL (disclib) object, NULL on error
 */
const struct meta_dl *bd_get_meta(BLURAY *bd);
 *  Number of titles can be found from BLURAY_DISC_INFO.
 *
 * @param bd  BLURAY object
 * @param title title number from disc index
 * @return 1 on success, 0 if error
 */
int  bd_play_title(BLURAY *bd, unsigned title);

/**
 *
 *  Open BluRay disc Top Menu.
 *
 *  Current pts is needed for resuming playback when menu is closed.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 * @return 1 on success, 0 if error
 */
int  bd_menu_call(BLURAY *bd, int64_t pts);

/**
 *
 *  Read from currently playing title.
 *
 *  When playing disc in navigation mode this function must be used instead of bd_read().
 *
 * @param bd  BLURAY object
 * @param buf buffer to read data into
 * @param len size of data to be read
 * @param event next BD_EVENT from event queue (BD_EVENT_NONE if no events)
 * @return size of data read, -1 if error, 0 if event needs to be handled first, 0 if end of title was reached
 */
int  bd_read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event);

/**
 *
 *  Continue reading after still mode clip
 *
 * @param bd  BLURAY object
 * @return 0 on error
 */
int bd_read_skip_still(BLURAY *bd);

/**
 *
 *  Get information about a playlist
 *
 * @param bd  BLURAY object
 * @param playlist playlist number
 * @param angle angle number (chapter offsets and clip size depend on selected angle)
 * @return allocated BLURAY_TITLE_INFO object, NULL on error
 */
BLURAY_TITLE_INFO* bd_get_playlist_info(BLURAY *bd, uint32_t playlist, unsigned angle);

/**
 *
 *  Get sound effect
 *
 * @param bd  BLURAY object
 * @param sound_id  sound effect id (0...N)
 * @param effect     sound effect data
 * @return <0 when no effects, 0 when id out of range, 1 on success
 */
int bd_get_sound_effect(BLURAY *bd, unsigned sound_id, struct bd_sound_effect *effect);


/*
 * User interaction
 */

/**
 *
 *  Update current pts.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 */
void bd_set_scr(BLURAY *bd, int64_t pts);

/**
 *
 *  Set current playback rate.
 *
 *  Notify BD-J media player when user changes playback rate
 *  (ex. pauses playback).
 *  Changing rate may fail if corresponding UO is masked or
 *  playlist is not playing.
 *
 * @param bd  BLURAY object
 * @param rate current playback rate * 90000 (0 = paused, 90000 = normal)
 * @return <0 on error, 0 on success
 */
int bd_set_rate(BLURAY *bd, uint32_t rate);

#define BLURAY_RATE_PAUSED  0      /**< Set playback rate to PAUSED  */
#define BLURAY_RATE_NORMAL  90000  /**< Set playback rate to NORMAL  */

/**
 *
 *  Pass user input to graphics controller or BD-J.
 *  Keys are defined in libbluray/keys.h.
 *
 *  Two user input models are supported:
 *    - Single event when a key is typed once.
 *    - Separate events when key is pressed and released.
 *      VD_VK_KEY_PRESSED, BD_VK_TYPED and BD_VK_KEY_RELEASED are or'd with the key.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 * @param key input key (@see keys.h)
 * @return <0 on error, 0 on success, >0 if selection/activation changed
 */
int bd_user_input(BLURAY *bd, int64_t pts, uint32_t key);

/**
 *
 *  Select menu button at location (x,y).
 *
 *  This function has no effect with BD-J menus.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 * @param x mouse pointer x-position
 * @param y mouse pointer y-position
 * @return <0 on error, 0 when mouse is outside of buttons, 1 when mouse is inside button
 */
int bd_mouse_select(BLURAY *bd, int64_t pts, uint16_t x, uint16_t y);


/*
 * Testing and debugging
 *
 * Note: parsing functions can't be used with UDF images.
 */

/* access to internal information */

struct clpi_cl;
struct mpls_pl;
struct bdjo_data;
struct mobj_objects;

/**
 *
 *  Get copy of clip information for requested playitem.
 *
 * @param bd  BLURAY objects
 * @param clip_ref  requested playitem number
 * @return pointer to allocated CLPI_CL object on success, NULL on error
 */
struct clpi_cl *bd_get_clpi(BLURAY *bd, unsigned clip_ref);

/** Testing/debugging: Parse clip information (CLPI) file */
struct clpi_cl *bd_read_clpi(const char *clpi_file);

/**
 *
 *  Free CLPI_CL object
 *
 * @param cl  CLPI_CL objects
 */
void bd_free_clpi(struct clpi_cl *cl);


/** Testing/debugging: Parse playlist (MPLS) file */
struct mpls_pl *bd_read_mpls(const char *mpls_file);
/** Testing/debugging: Free parsed playlist */
void bd_free_mpls(struct mpls_pl *);

/** Testing/debugging: Parse movie objects (MOBJ) file */
struct mobj_objects *bd_read_mobj(const char *mobj_file);
/** Testing/debugging: Free parsed movie objects */
void bd_free_mobj(struct mobj_objects *);

/** Testing/debugging: Parse BD-J object file (BDJO) */
struct bdjo_data *bd_read_bdjo(const char *bdjo_file);
/** Testing/debugging: Free parsed BDJO object */
void bd_free_bdjo(struct bdjo_data *);

/* BD-J testing */

/** Testing/debugging: start BD-J from the specified BD-J object (should be a 5 character string) */
int  bd_start_bdj(BLURAY *bd, const char* start_object);
/** Testing/debugging: shutdown BD-J and clean up resources */
void bd_stop_bdj(BLURAY *bd);

/**
 *
 *  Read a file from BluRay Virtual File System.
 *
 *  Allocate large enough memory block and read file contents.
 *  Caller must free the memory block with free().
 *
 * @param bd  BLURAY object
 * @param path  path to the file (relative to disc root)
 * @param data  where to store pointer to allocated data
 * @param size  where to store file size
 * @return 1 on success, 0 on error
 */
int bd_read_file(BLURAY *bd, const char *path, void **data, int64_t *size);

/**
 *
 *  Open a directory from BluRay Virtual File System.
 *
 *  Caller must close with dir->close().
 *
 * @param bd  BLURAY object
 * @param dir  target directory (relative to disc root)
 * @return BD_DIR_H *, NULL if failed
 */
struct bd_dir_s *bd_open_dir(BLURAY *bd, const char *dir);

/**
 *
 *  Open a file from BluRay Virtual File System.
 *
 *  encrypted streams are decrypted, and because of how
 *  decryption works, it can only seek to (N*6144) bytes,
 *  and read 6144 bytes at a time.
 *  DO NOT mix any play functionalities with these functions.
 *  It might cause broken stream. In general, accessing
 *  mutiple file on disk at the same time is a bad idea.
 *
 *  Caller must close with file->close().
 *
 * @param bd  BLURAY object
 * @param path  path to the file (relative to disc root)
 * @return BD_FILE_H *, NULL if failed
 */
struct bd_file_s *bd_open_file_dec(BLURAY *bd, const char *path);


#ifdef __cplusplus
}
#endif

#endif /* BLURAY_H_ */
