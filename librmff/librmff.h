/*
 *  librmff.h
 *
 *  Copyright (C) Moritz Bunkus - March 2004
 *      
 *  librmff is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *   
 *  librmff is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

/** \file librmff.h
 * \brief The RealMedia file format library
 * \author Moritz Bunkus <moritz@bunkus.org>
 */

#ifndef __RMFF_H
#define __RMFF_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <inttypes.h>
#include <stdio.h>

#include "mb_file_io.h"

/** \brief Contains the PROP file header.
 * This header is mandatory for a RealMedia file. It contains statistical
 * and global data. Each member is in big endian.
 */
typedef struct {
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t num_packets;
  uint32_t duration;
  uint32_t preroll;
  uint32_t index_offset;
  uint32_t data_offset;
  uint16_t num_streams;
  uint16_t flags;
} rmff_prop_t;

/** \brief Comments about the file in question.
 * This structure contains the parsed values of the CONT header. These
 * strings must not be modified by the application. The function
 * rmff_set_cont_header(const_char*,const_char*,const_char*,const_char*)
 * must be used instead.
 */
typedef struct {
  char *title;
  char *author;
  char *copyright;
  char *comment;
} rmff_cont_t;

typedef struct {
  uint16_t id;
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t start_time;
  uint32_t preroll;
  uint32_t duration;
  char *name;
  char *mime_type;
  uint32_t type_specific_size;
  unsigned char *type_specific_data;
} rmff_mdpr_t;

typedef struct __attribute__((__packed__)) real_video_props_t {
  uint32_t size;
  uint32_t fourcc1;
  uint32_t fourcc2;
  uint16_t width;
  uint16_t height;
  uint16_t bpp;
  uint32_t unknown1;
  uint32_t fps;
  uint32_t type1;
  uint32_t type2;
} real_video_props_t;

typedef struct __attribute__((__packed__)) real_audio_v4_props_t {
  uint32_t fourcc1;             /* '.', 'r', 'a', 0xfd */
  uint16_t version1;            /* 4 or 5 */
  uint16_t unknown1;            /* 00 00 */
  uint32_t fourcc2;             /* .ra4 or .ra5 */
  uint32_t unknown2;            /* ??? */
  uint16_t version2;            /* 4 or 5 */
  uint32_t header_size;         /* == 0x4e */
  uint16_t flavor;              /* codec flavor id */
  uint32_t coded_frame_size;    /* coded frame size */
  uint32_t unknown3;            /* big number */
  uint32_t unknown4;            /* bigger number */
  uint32_t unknown5;            /* yet another number */
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            /* 00 00 */
  uint16_t sample_rate;
  uint16_t unknown8;            /* 0 */
  uint16_t sample_size;
  uint16_t channels;
} real_audio_v4_props_t;

typedef struct __attribute__((__packed__)) real_audio_v5_props_t {
  uint32_t fourcc1;             /* '.', 'r', 'a', 0xfd */
  uint16_t version1;            /* 4 or 5 */
  uint16_t unknown1;            /* 00 00 */
  uint32_t fourcc2;             /* .ra4 or .ra5 */
  uint32_t unknown2;            /* ??? */
  uint16_t version2;            /* 4 or 5 */
  uint32_t header_size;         /* == 0x4e */
  uint16_t flavor;              /* codec flavor id */
  uint32_t coded_frame_size;    /* coded frame size */
  uint32_t unknown3;            /* big number */
  uint32_t unknown4;            /* bigger number */
  uint32_t unknown5;            /* yet another number */
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            /* 00 00 */
  uint8_t unknown7[6];          /* 0, srate, 0 */
  uint16_t sample_rate;
  uint16_t unknown8;            /* 0 */
  uint16_t sample_size;
  uint16_t channels;
  uint32_t genr;                /* "genr" */
  uint32_t fourcc3;             /* fourcc */
} real_audio_v5_props_t;

typedef struct rmff_frame_t {
  void *data;
  uint32_t size;
  int allocated_by_rmff;

  uint16_t id;
  uint32_t timecode;
  uint8_t reserved;
  uint8_t flags;
} rmff_frame_t;

/** \brief Unknown track type. */
#define RMFF_TRACK_TYPE_UNKNOWN                        0
/** \brief The track contains audio data. */
#define RMFF_TRACK_TYPE_AUDIO                          1
/** \brief The track contains video data. */
#define RMFF_TRACK_TYPE_VIDEO                          2

struct rmff_file_t;

typedef struct {
  uint32_t id;
  int type;
  rmff_mdpr_t mdpr_header;
  int is_big_endian;

  struct rmff_file_t *file;
} rmff_track_t;

typedef struct {
  mb_file_io_t *io;
  void *handle;
  char *name;
  int64_t size;

  int headers_read;

  rmff_prop_t prop_header;
  int prop_header_found;
  rmff_cont_t cont_header;
  int cont_header_found;
  int64_t first_data_header_offset;
  int64_t next_data_header_offset;
  uint32_t num_packets_in_chunk;
  uint32_t num_packets_read;
  int is_big_endian;

  rmff_track_t *tracks;
  int num_tracks;
} rmff_file_t;

/** \brief No error has occured. */
#define RMFF_ERR_OK                                    0
/** \brief The file is not a valid RealMedia file. */
#define RMFF_ERR_NOT_RMFF                             -1
/** \brief The structures/data read from the file were invalid. */
#define RMFF_ERR_DATA                                 -2
/** \brief The end of the file has been reached. */
#define RMFF_ERR_EOF                                  -3
/** \brief An error occured during file I/O. */
#define RMFF_ERR_IO                                   -4
/** \brief The parameters were invalid. */
#define RMFF_ERR_PARAMETERS                           -5
/** \brief An error has occured for which \c errno should be consulted. */
#define RMFF_ERR_CHECK_ERRNO                          -6

#define rmffFOURCC(a, b, c, d) (uint32_t)((((unsigned char)a) << 24) + \
                               (((unsigned char)b) << 16) + \
                               (((unsigned char)c) << 8) + \
                               ((unsigned char)d))

/** \brief Opens a RealMedia file for reading or writing.
 * Can be used to open an existing file for reading or for creating a new
 * file. The file headers will neither be read nor written automatically.
 * This function uses the standard file I/O functions provided by the
 * current operating system.
 *
 * \param path the name of the file that should be opened
 * \param mode either ::RMFF_OPEN_MODE_READING or ::RMFF_OPEN_MODE_WRITING
 * \returns a pointer to \c rmff_file_t structure or \c NULL if an error
 *   occured. In the latter case ::rmff_last_error will be set.
 * \see rmff_open_file_with_io
 */
rmff_file_t *rmff_open_file(const char *path, int mode);

/** \brief Opens a RealMedia file for reading or writing.
 * Can be used to open an existing file for reading or for creating a new
 * file. The file headers will neither be read nor written automatically.
 * This function uses I/O functions provided by the \c io parameter.
 *
 * \param path the name of the file that should be opened
 * \param mode either ::RMFF_OPEN_MODE_READING or ::RMFF_OPEN_MODE_WRITING
 * \param io a set of I/O functions
 * \returns a pointer to a rmff_file_t structure or \c NULL if an error
 *   occured. In the latter case ::rmff_last_error will be set.
 * \see rmff_open_file
 */
rmff_file_t *rmff_open_file_with_io(const char *path, int mode,
                                    mb_file_io_t *io);

/** \brief Close the file and release all resources.
 * Closes the file and releases all resources associated with it, including
 * the \c file structure. If the file was open for writing then
 * rmff_write_headers() should be called prior to closing the file as
 * \c rmff_close_file does not fix the headers itself.
 *
 * \param file The file to close.
 */
void rmff_close_file(rmff_file_t *file);

/** \brief Reads the file and track headers.
 * This function should be called after directly after opening it for reading.
 * It will try to read the file and track headers and position the file pointer
 * right before the first data packet.
 *
 * \param file The file to read the headers from.
 * \returns ::RMFF_ERR_OK on success and one of the other \c RMFF_ERR_*
 *   constants on error.
 */
int rmff_read_headers(rmff_file_t *file);

/** \brief Retrieves the size of the next frame.
 * \param file The file to read from.
 * \returns the size of the following frame or one of the \c RMFF_ERR_*
 *   constants on error.
 */
int rmff_get_next_frame_size(rmff_file_t *file);

/** \brief Reads the next frame from the file.
 * The frame must be released by rmff_release_frame(rmff_frame_t*).
 * \param file The file to read from.
 * \param buffer A buffer to read the frame into. This parameter may be
 *   \c NULL in which case the buffer will be allocated by the library.
 *   If the application provides the buffer it must be large enough.
 *   The function rmff_get_next_frame_size() can be used in this case.
 * \returns a pointer to a ::rmff_frame_t structure containing the frame
 *   and its metadata on success or \c NULL if the call failed. This frame
 *   must be freed with rmff_release_frame(rmff_frame_t*).
 */
rmff_frame_t *rmff_read_next_frame(rmff_file_t *file, void *buffer);

/** \brief Frees all resources associated with a frame.
 * If the frame buffer was allocated by the library it will be freed as well.
 * \param frame The frame to free.
 */
void rmff_release_frame(rmff_frame_t *frame);



/** \brief The error code of the last function call.
 * Contains the last error code for a function that failed. If a function
 * succeeded it usually does not reset \c rmff_last_error to \c RMFF_ERR_OK.
 * This variable can contain one of the \c RMFF_ERR_* constants.
 */
extern int rmff_last_error;

/** \brief The error message of the last function call.
 * Contains the last error message for a function that failed. If a function
 * succeeded it usually does not reset \c rmff_last_error_msg to \c NULL.
 */
extern const char *rmff_last_error_msg;

/** \brief Map an error code to an error message.
 * Returns the error message that corresponds to the error code.
 * \param code the error code which must be one of the \c RMFF_ERR_* macros.
 * \returns the error message or "Unknown error" for unknown error codes,
 *   but never \c NULL.
 */
const char *rmff_get_error_str(int code);

#if defined(__cplusplus)
}
#endif

#endif /* __RMFF_H */
