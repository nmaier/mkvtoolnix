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

/** \mainpage
 *
 * \section Introduction
 *
 * \a librmff is short for 'RealMedia file format access library'. It aims
 * at providing the programmer an easy way to read and write RealMedia
 * files. It does not contain any codecs for audio/video handling.
 *
 * \section License
 *
 * The library was written by Moritz Bunkus <moritz@bunkus.org>. It is
 * licensed under the terms of the GNU Lesser General Public License (GNU
 * LGPL) which can be found in the file COPYING.
 *
 * \section Usage
 *
 * Here are very short samples of how to use the library.
 *
 * \subsection reading_existing Reading an existing file
 *
 * Reading an existing file requires four steps: Opening the file, reading
 * the headers, reading all the frames and closing the file.
 *
 * \code
  rmff_file_t *file;
  rmff_frame_t *frame;

  file = rmff_open_file("sample_file.rm", RMFF_OPEN_MODE_READING);
  if (file == NULL) {
    // Output some general information about the tracks in the file.
    // Now read all the frames.
    while ((frame = rmff_read_next_frame(file, NULL)) != NULL) {
      // Do something with the frame and release it afterwards.
      rmff_release_frame(frame);
    }
    rmff_close_file(file);
  }
    \endcode
 *
 * \section memory_handling Memory handling
 *
 * Generally \a librmff allocates and frees memory itself. You should
 * \b never mess with pointers inside the structures directly but use
 * the provided functions for manipulating it. There's one exception to
 * this rule: the frame handling.
 * 
 * The functions rmff_read_next_frame(rmff_file_t*,void*),
 * rmff_release_frame(rmff_frame_t*) and
 * rmff_allocate_frame(uint32_t,void*) allow the application to provide its
 * own buffers for storing the frame contents. \a librmff will not copy
 * this memory further. It will read directly into the buffer or write directly
 * from the buffer into the file.
 *
 * Example: \code
  rmff_frame_t *frame;
  unsigned char *buffer,
  int size;

  while (1) {
    // Get the next frame's size.
    size = rmff_get_next_frame_size(file);
    // Have we reached the end of the file?
    if (size == -1)
      break;
    // Allocate enough space for this frame and let librmff read directly
    // into it.
    buffer = (unsigned char *)malloc(size);
    frame = rmff_read_next_frame(file, buffer);
    // Now do something with the buffer. Afterwards release the frame.
    rmff_release_frame(frame);
    // The buffer is still allocated. So free it now.
    free(buffer);
  }
  \endcode
 */

#ifndef __RMFF_H
#define __RMFF_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <inttypes.h>
#include <stdio.h>

#include "mb_file_io.h"

/** \brief The global PROP file header.
 *
 * This header is mandatory for a RealMedia file. It contains statistical
 * and global data. The values are stored in big endian byte order.
 * The application should use the functions
 * rmff_get_uint16_be(const void*), rmff_get_uint32_be(const void*),
 * rmff_put_uint16_be(void*,uint16_t) and
 * rmff_put_uint32_be(void*,uint32_t) for accessing the members.
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
 *
 * This structure contains the parsed values of the CONT header. These
 * strings must not be modified by the application. The function
 * rmff_set_cont_header(rmff_file_t*,const char*,const char*,const char*,const char*)
 * must be used instead.
 */
typedef struct {
  char *title;
  char *author;
  char *copyright;
  char *comment;
} rmff_cont_t;

/** \brief The MDPR track headers.
 *
 * Each track in a RealMedia file contains the MDPR header. The values
 * are stored in big endian byte order.
 * The application should use the functions
 * rmff_get_uint16_be(const void*), rmff_get_uint32_be(const void*),
 * rmff_put_uint16_be(void*,uint16_t) and
 * rmff_put_uint32_be(void*,uint32_t) for accessing the members.
 */
typedef struct {
  /** \brief The track number. It is unique regarding the file. */
  uint16_t id;
  /** \brief The maximum bitrate in bit/second.
   *
   * When creating a file this value will
   * be updated automatically by the library. */
  uint32_t max_bit_rate;
  /** \brief The average bitrate in bit/second.
   *
   * When creating a file this value will
   * be updated automatically by the library. */
  uint32_t avg_bit_rate;
  /** \brief The maximum packet size in bytes.
   *
   * When creating a file this value will
   * be updated automatically by the library. */
  uint32_t max_packet_size;
  /** \brief The average packet size in bytes.
   *
   * When creating a file this value will
   * be updated automatically by the library. */
  uint32_t avg_packet_size;
  uint32_t start_time;
  uint32_t preroll;
  uint32_t duration;
  /** \brief The track's name.
   *
   * Use the rmff_set_track_data(rmff_track_t*,const char*,const char*)
   * function for setting it. */
  char *name;
  /** \brief The track's MIME type.
   *
   * Use the rmff_set_track_data(rmff_track_t*,const char*,const char*)
   * function for setting it. */
  char *mime_type;
  /** \brief The size of the track specific data in bytes. */
  uint32_t type_specific_size;
  /** \brief Track type specific data.
   *
   * Use the
   * rmff_set_track_specific_data(rmff_track_t*,const unsigned char*,uint32_t)
   * function for setting it. It usually contains a ::real_video_props_t or
   * ::real_audio_v4_props_t structure. */
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
  unsigned char *data;
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

typedef struct rmff_track_t {
  uint32_t id;
  int type;
  rmff_mdpr_t mdpr_header;
  int is_big_endian;

  struct rmff_file_t *file;
} rmff_track_t;

typedef struct rmff_file_t {
  mb_file_io_t *io;
  void *handle;
  char *name;
  int open_mode;
  int64_t size;

  int headers_read;

  rmff_prop_t prop_header;
  rmff_cont_t cont_header;
  int cont_header_present;
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

/** \brief Convert the four bytes into a 'FOURCC' uint32. */
#define rmffFOURCC(a, b, c, d) \
  (uint32_t)((((unsigned char)a) << 24) + \
  (((unsigned char)b) << 16) + \
  (((unsigned char)c) << 8) + \
  ((unsigned char)d))

#define RMFF_OPEN_MODE_READING                         0
#define RMFF_OPEN_MODE_WRITING                         1

/** \brief Opens a RealMedia file for reading or writing.
 *
 * Can be used to open an existing file for reading or for creating a new
 * file. The file headers will neither be read nor written automatically.
 * This function uses the standard file I/O functions provided by the
 * current operating system.
 *
 * \param path the name of the file that should be opened
 * \param mode either ::RMFF_OPEN_MODE_READING or ::RMFF_OPEN_MODE_WRITING
 * \returns a pointer to ::rmff_file_t structure or \c NULL if an error
 *   occured. In the latter case ::rmff_last_error will be set.
 * \see rmff_open_file_with_io
 */
rmff_file_t *rmff_open_file(const char *path, int mode);

/** \brief Opens a RealMedia file for reading or writing.
 *
 * Can be used to open an existing file for reading or for creating a new
 * file. The file headers will neither be read nor written automatically.
 * This function uses I/O functions provided by the \a io parameter.
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
 *
 * Closes the file and releases all resources associated with it, including
 * the ::rmff_file_t structure. If the file was open for writing then
 * rmff_write_headers() should be called prior to closing the file as
 * \c rmff_close_file does not fix the headers itself.
 *
 * \param file The file to close.
 */
void rmff_close_file(rmff_file_t *file);

/** \brief Reads the file and track headers.
 *
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
 *
 * \param file The file to read from.
 * \returns the size of the following frame or one of the \c RMFF_ERR_*
 *   constants on error.
 */
int rmff_get_next_frame_size(rmff_file_t *file);

/** \brief Reads the next frame from the file.
 *
 * The frame must be released by rmff_release_frame(rmff_frame_t*).
 *
 * \param file The file to read from.
 * \param buffer A buffer to read the frame into. This parameter may be
 *   \c NULL in which case the buffer will be allocated by the library.
 *   If the application provides the buffer it must be large enough.
 *   The function rmff_get_next_frame_size(rmff_file_t*) can be used in this
 *   case.
 * \returns a pointer to a ::rmff_frame_t structure containing the frame
 *   and its metadata on success or \c NULL if the call failed. This frame
 *   must be freed with rmff_release_frame(rmff_frame_t*).
 */
rmff_frame_t *rmff_read_next_frame(rmff_file_t *file, void *buffer);

/** \brief Allocates a frame and possibly a buffer for its contents.
 *
 * \param size The size of this frame.
 * \param buffer A buffer that holds the frame. This parameter may be
 *   \c NULL in which case the buffer will be allocated by the library.
 *   If the application provides the buffer it must be large enough.
 * \returns a pointer to an empty ::rmff_frame_t structure which can be filled
 *   with the frame contents and its metadata. This frame
 *   must be freed with rmff_release_frame(rmff_frame_t*).
 */
rmff_frame_t *rmff_allocate_frame(uint32_t size, void *buffer);

/** \brief Frees all resources associated with a frame.
*
 * If the frame buffer was allocated by the library it will be freed as well.
 *
 * \param frame The frame to free.
 */
void rmff_release_frame(rmff_frame_t *frame);

/** \brief Sets the contents of the \link ::rmff_cont_t CONT file header
 * \endlink.
 *
 * Frees the old contents if any and allocates copies of the given
 * strings. If the CONT header should be written to the file
 * in rmff_write_headers(rmff_file_t*) then the \a cont_header_present
 * member must be set to 1.
 *
 * \param file The file whose CONT header should be set.
 * \param title The file's title, e.g. "Muriel's Wedding"
 * \param author The file's author, e.g. "P.J. Hogan"
 * \param copyright The copyright assigned to the file.
 * \param comment A free-style comment.
 */
void rmff_set_cont_header(rmff_file_t *file, const char *title,
                          const char *author, const char *copyright,
                          const char *comment);

/** \brief Sets the strings in the \link ::rmff_mdpr_t MDPR track header
 * \endlink structure.
 *
 * Frees the old contents and allocates copies of the given strings.
 *
 * \param track The track whose MDPR header should be set.
 * \param name The track's name.
 * \param mime_type The MIME type. A video track should have the MIME type
 *   \c video/x-pn-realvideo, and an audio track should have the MIMT type
 *   \c audio/x-pn-realaudio.
 */
void rmff_set_track_data(rmff_track_t *track, const char *name,
                         const char *mime_type);

/** \brief Sets the \a track_specific_data member of the \link ::rmff_mdpr_t
 * MDPR header\endlink structure.
 *
 * The \a track_specific_data usually contains a
 * ::real_video_props_t structure or a ::real_audio_props_t structure.
 * The existing data, if any, will be freed, and a copy of the memory
 * \a data points to will be made.
 *
 * \param track The track whose MDPR header should be set.
 * \param data A pointer to the track specific data.
 * \param size The track specific data's size in bytes.
 */
void rmff_set_track_specific_data(rmff_track_t *track,
                                  const unsigned char *data, uint32_t size);

/** \brief The error code of the last function call.
 * Contains the last error code for a function that failed. If a function
 * succeeded it usually does not reset \a rmff_last_error to \c RMFF_ERR_OK.
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

/** \brief Reads a 16bit uint from an address.
 * The uint is converted from big endian byte order to the machine's byte
 * order.
 *
 * \param buf The address to read from.
 * \returns The 16bit uint converted to the machine's byte order.
 */
uint16_t rmff_get_uint16_be(const void *buf);

/** \brief Reads a 32bit uint from an address.
 * The uint is converted from big endian byte order to the machine's byte
 * order.
 *
 * \param buf The address to read from.
 * \returns The 32bit uint converted to the machine's byte order.
 */
uint32_t rmff_get_uint32_be(const void *buf);

/** \brief Write a 16bit uint at an address.
 * The value is converted from the machine's byte order to big endian byte
 * order.
 *
 * \param buf The address to write to.
 * \param value The value to write.
 */
void rmff_put_uint16_be(void *buf, uint16_t value);

/** \brief Write a 32bit uint at an address.
 * The value is converted from the machine's byte order to big endian byte
 * order.
 *
 * \param buf The address to write to.
 * \param value The value to write.
 */
void rmff_put_uint32_be(void *buf, uint32_t value);

#if defined(__cplusplus)
}
#endif

#endif /* __RMFF_H */
