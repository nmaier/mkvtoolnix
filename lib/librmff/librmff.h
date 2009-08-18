/*
  librmff.h

  Copyright (C) Moritz Bunkus - March 2004

  librmff is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1, or (at your option)
  any later version.

  librmff is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; see the file COPYING.  If not, write to
  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** \file librmff.h
  \brief The RealMedia file format library
  \author Moritz Bunkus <moritz@bunkus.org>

*/

/** \mainpage

  \section Introduction

  \a librmff is short for 'RealMedia file format access library'. It aims
  at providing the programmer an easy way to read and write RealMedia
  files. It does not contain any codecs for audio/video handling.

  You can find a description of the RealMedia file format at
  http://wiki.multimedia.cx/index.php?title=RealMedia

  \section License

  The library was written by Moritz Bunkus <moritz@bunkus.org>. It is
  licensed under the terms of the GNU Lesser General Public License (GNU
  LGPL) which can be found in the file COPYING.

  \section Usage

  Here are very short samples of how to use the library.

  \subsection reading_existing Reading an existing file

  Reading an existing file requires four steps: Opening the file, reading
  the headers, reading all the frames and closing the file.

  Example: \code
  rmff_file_t *file;
  rmff_frame_t *frame;

  file = rmff_open_file("sample_file.rm", RMFF_OPEN_MODE_READING);
  if (file == NULL) {
    // Handle the error.
    return;
  }
  if (rmff_read_headers(file) == RMFF_ERR_OK) {
    // Output some general information about the tracks in the file.
    // Now read all the frames.
    while ((frame = rmff_read_next_frame(file, NULL)) != NULL) {
      // Do something with the frame and release it afterwards.
      rmff_release_frame(frame);
    }
  }
  rmff_close_file(file);
  \endcode

  \subsection creating_new Creating a new file

  Creating a new file is a bit more complex. This library only provides a
  low level codec independant layer to the RealMedia file format.

  Creating a new file involves the following step in this particular order:
  -# open a new file with \c RMFF_OPEN_MODR_WRITING,
  -# add track entries with ::rmff_add_track,
  -# write the headers with ::rmff_write_headers,
  -# write all the frames with ::rmff_write_frame,
  -# optionally write the indexes with ::rmff_write_index,
  -# fix all the headers with ::rmff_fix_headers and
  -# close the file with ::rmff_close_file.

  Please note that the error handling is not shown in the following example:
  \code
  rmff_file_t *file;
  rmff_track_t *track;
  rmff_frame_t *frame;

  file = rmff_open_file("new_file.rm", RMFF_OPEN_MODE_WRITING);
  track = rmff_add_track(file, 1); // Also create an index for this track.
  // The track types etc are stored in the ::rmff_mdpr_t#type_specific_data
  // It usually contains a ::real_audio_v4_props_t, ::real_audio_v5_props_t
  // or ::real_video_props_t structures which have to be set by the
  // calling application.

  // After setting the structures:
  rmff_write_headers(file);
  while (!done) {
    // Generate frames.
    ...
    frame = rmff_allocate_frame(size, NULL);
    rmff_write_frame(track, frame);
    rmff_release_frame(frame);
  }
  rmff_write_index(file);
  rmff_fix_headers(file);
  rmff_close_file(file);
  \endcode

  \section memory_handling Memory handling

  Generally \a librmff allocates and frees memory itself. You should
  \b never mess with pointers inside the structures directly but use
  the provided functions for manipulating it. There are two exceptions to
  this rule: the frame handling and the \c app_data pointers.

  The \c app_data members in ::rmff_file_t and ::rmff_track_t are never
  touched by \a librmff and can be used by the application to store
  its own data.

  The functions ::rmff_read_next_frame, ::rmff_release_frame and
  ::rmff_allocate_frame allow the application to provide its
  own buffers for storing the frame contents. \a librmff will not copy
  this memory further. It will read directly into the buffer or write directly
  from the buffer into the file.

  Example: \code
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

  \section packed_video_frames Packed video frames

  The RealVideo frames are not stored one-complete-frame in one packet
  (packet meaning one 'frame' used by ::rmff_write_frame and returned by
  ::rmff_read_next_frame). They are split up into several sub packets.
  However, the Real decoder libraries usually need a completely assembled
  frame that contains all sub packets along with the offsets to each sub
  packet in the complete frame.

  \a librmff can make the developper's life easier because it provides
  the function ::rmff_write_packed_video_frame that will split up such
  an assembled packet into all the sub packets and write those
  automatically, and two functions that assemble sub packets into such a
  packed frame (::rmff_assemble_packed_video_frame and
  ::rmff_get_packed_video_frame).

  \subsection packed_format Bitstream format

  There are three layouts for how the bitstream for such a sub packet looks
  like which depends on the first two bits.

  -# For <tt>AA = 00</tt> and <tt>AA = 10</tt>:\n
  <tt>AABBBBBB BCCCCCCC DEFFFFFF FFFFFFFF (GGGGGGGG GGGGGGGG)
  HIJJJJJJ JJJJJJJJ (KKKKKKKK KKKKKKKK) LLLLLLLL</tt>
  -# For <tt>AA = 01</tt>:\n
  <tt>AABBBBBB BCCCCCCC</tt>
  -# For <tt>AA = 11</tt>:\n
  <tt>AABBBBBB DEFFFFFF FFFFFFFF (GGGGGGGG GGGGGGGG)
  HIJJJJJJ JJJJJJJJ (KKKKKKKK KKKKKKKK) LLLLLLLL</tt>

  - \c A, two bits: Sub packet type indicator
    - \c 00 partial frame
    - \c 01 whole frame. If this is the case then only the bits <tt>A, B</tt>
    and \c C are present. In all the other cases the other bits are present
    as well.
    - \c 10 last partial frame
    - \c 11 multiple frames
  - \c B, seven bits: number of sub packets in the complete frame
  - \c C, seven bits: current sub packet's sequence number starting at 1
  - \c D, one bit: unknown
  - \c E, one bit: If set the \c G bits/bytes are not present.
  - \c F, 14 bits: The complete frame's length in bytes. If \c E is NOT set
  then the total length is <tt>(F << 16) | G</tt>, otherwise \c G is not present
  and the total length is just \c F.
  - \c H, one bit, unknown
  - \c I, one bit: If set the \c L bits/bytes are not present.
  - \c J, 14 bits: The current packet's offset in bytes. If \c J is NOT set
  then the offset is <tt>(J << 16) | K</tt>, otherwise \c K is not present
  and the offset is just \c J. Note that if \c AA is 10 then the offset is
  to be interpreted from the end of the of the complete frame. In this case
  it is equal to the current sub packet's length.
  - \c L, 8 bits: The current frame's sequence number / frame number % 255.
  Can be used for detecting frame boundaries if the sub packaging has failed
  or the stream is broken.
  - The rest is the video data.

  \subsection packed_example Packed frame assembly example

  Here's a short example how to use the packet assembly:
  \code
  rmff_frame_t *raw_frame, *packed_frame;
  rmff_track_t *track;
  int result;

  // Open the file, check the tracks.
  while ((raw_frame = rmff_read_next_frame(file, NULL)) != NULL) {
    track = rmff_find_track_with_id(file, raw_frame->id);
    if (track->type != RMFF_TRACK_TYPE_VIDEO) {
      // Handle audio etc.
    } else {
      if ((result = rmff_assemble_packed_video_frame(track, raw_frame)) < 0) {
        // Handle the error.
      } else {
        while ((packed_frame = rmff_get_packed_video_frame(track)) != NULL) {
          // Do something with the assembled frame and release it afterwards.
          rmff_release_frame(packed_frame);
        }
      }
    }
    rmff_release_frame(raw_frame);
  }
  \endcode
*/

#ifndef __RMFF_H
#define __RMFF_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "common/os.h"

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif
#include <stdio.h>

#include "mb_file_io.h"

#define RMFF_VERSION_MAJOR 0
#define RMFF_VERSION_MINOR 6
#define RMFF_VERSION_MICRO 2
#define RMFF_VERSION (RMFF_VERSION_MAJOR * 10000 + RMFF_VERSION_MINOR * 100 + \
                      RMFF_VERSION_MICRO)

/** \brief The stream may be saved to disc. Can be set in the
    \link ::rmff_prop_t PROP header\endlink. */
#define RMFF_FILE_FLAG_SAVE_ENABLED     0x0001
/** \brief Allows the client to use extra buffering. Can be set in the
    \link ::rmff_prop_t PROP header\endlink. */
#define RMFF_FILE_FLAG_PERFECT_PLAY     0x0002
/** \brief The stream is being generated by a live broadcast. Can be set in the
    \link ::rmff_prop_t PROP header\endlink. */
#define RMFF_FILE_FLAG_LIVE_BROADCAST   0x0004
/** \brief The stream may be downloaded. Can be set in the
    \link ::rmff_prop_t PROP header\endlink. */
#define RMFF_FILE_FLAG_DOWNLOAD_ENABLED 0x0008

/** \brief The common object handler. It precedes all the other headers.
 */
typedef struct rmff_object_t {
  /** \brief The object's id, e.g. \c 'PROP'. */
  uint32_t  id;
  /** \brief The size of this header including this object header. */
  uint32_t size;
  /** \brief The version, usually 0. */
  uint16_t version;
} rmff_object_t;

/** \brief The main file header. It is located at the very beginning of
    the file. */
typedef struct rmff_rmf_t {
  rmff_object_t obj;
  uint32_t format_version;
  uint32_t num_headers;
} rmff_rmf_t;

/** \brief The global PROP file header.

  This header is mandatory for a RealMedia file. It contains statistical
  and global data. The values are stored in big endian byte order.
  The application should use the functions ::rmff_get_uint16_be,
  ::rmff_get_uint32_be, ::rmff_put_uint16_be and
  ::rmff_put_uint32_be for accessing the members.
*/
typedef struct rmff_prop_t {
  rmff_object_t obj;
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

  This structure contains the parsed values of the CONT header. These
  strings must not be modified by the application. The function
  ::rmff_set_cont_header must be used instead.
*/
typedef struct rmff_cont_t {
  rmff_object_t obj;
  char *title;
  char *author;
  char *copyright;
  char *comment;
} rmff_cont_t;

/** \brief The MDPR track headers.

  Each track in a RealMedia file contains the MDPR header. The values
  are stored in big endian byte order.
  The application should use the functions ::rmff_get_uint16_be,
  ::rmff_get_uint32_be, ::rmff_put_uint16_be and
  ::rmff_put_uint32_be for accessing the members.
*/
typedef struct rmff_mdpr_t {
  rmff_object_t obj;
  /** \brief The track number. It is unique regarding the file. */
  uint16_t id;
  /** \brief The maximum bitrate in bits/second.

    When creating a file this value will
    be updated automatically by the library. */
  uint32_t max_bit_rate;
  /** \brief The average bitrate in bits/second.
    When creating a file this value will
    be updated automatically by the library. */
  uint32_t avg_bit_rate;
  /** \brief The maximum packet size in bytes.

    When creating a file this value will
    be updated automatically by the library. */
  uint32_t max_packet_size;
  /** \brief The average packet size in bytes.

    When creating a file this value will
    be updated automatically by the library. */
  uint32_t avg_packet_size;
  uint32_t start_time;
  uint32_t preroll;
  uint32_t duration;
  /** \brief The track's name.

    Use the ::rmff_set_track_data function for setting it. */
  char *name;
  /** \brief The track's MIME type.

    Use the ::rmff_set_track_data function for setting it. */
  char *mime_type;
  /** \brief The size of the track specific data in bytes. */
  uint32_t type_specific_size;
  /** \brief Track type specific data.

    Use the ::rmff_set_type_specific_data function for setting it. It usually
    contains a ::real_video_props_t or ::real_audio_v4_props_t structure. */
  unsigned char *type_specific_data;
} rmff_mdpr_t;

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
typedef struct PACKED_STRUCTURE real_video_props_t {
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

typedef struct PACKED_STRUCTURE real_audio_v4_props_t {
  uint32_t fourcc1;             /* '.', 'r', 'a', 0xfd */
  uint16_t version1;            /* 4 or 5 */
  uint16_t unknown1;            /* 00 00 */
  uint32_t fourcc2;             /* .ra4 or .ra5 */
  uint32_t stream_length;       /* ??? */
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

typedef struct PACKED_STRUCTURE real_audio_v5_props_t {
  uint32_t fourcc1;             /* '.', 'r', 'a', 0xfd */
  uint16_t version1;            /* 4 or 5 */
  uint16_t unknown1;            /* 00 00 */
  uint32_t fourcc2;             /* .ra4 or .ra5 */
  uint32_t stream_length;       /* ??? */
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
#if defined(COMP_MSC)
#pragma pack(pop)
#endif

typedef struct rmff_index_entry_t {
  uint32_t pos;
  uint32_t timecode;
  uint32_t packet_number;
} rmff_index_entry_t;

/** \brief The packet is being delivered reliably. Can be set in the
    ::rmff_frame_t#flags member. */
#define RMFF_FRAME_FLAG_RELIABLE 0x01
/** \brief The frame is a key frame. Can be set in the ::rmff_frame_t#flags
    member. */
#define RMFF_FRAME_FLAG_KEYFRAME 0x02

/** A frame or packet of media data.

  A new frame can be obtained with ::rmff_allocate_frame or read from a file
  with ::rmff_read_next_frame. In both cases the application can either
  have \a librmff allocate a buffer for the frame contents or provide the
  buffer itself. In the latter case ::rmff_release_frame will not free the
  buffer either.

  \note The fields in this structure are stored in the machine's byte order
  which is little endian on x86 systems.
*/
typedef struct rmff_frame_t {
  /** \brief The frame/packet contents. */
  unsigned char *data;
  /** \brief The number of bytes in this frame. */
  uint32_t size;
  /** \brief Set by \a librmff if \a librmff has allocated the buffer and
      should free it in ::rmff_release_frame. */
  int allocated_by_rmff;

  /** \brief The track ID this frame belongs to. This will be overwritten
      in ::rmff_write_frame. */
  uint16_t id;
  /** \brief The presentation timestamp in ms. */
  uint32_t timecode;
  uint8_t reserved;
  /** \brief Flags, e.g. bit 1 = key frame */
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

  rmff_index_entry_t *index;
  int num_index_entries;

  struct rmff_file_t *file;

  void *app_data;
  void *internal;
} rmff_track_t;

typedef struct rmff_file_t {
  mb_file_io_t *io;
  void *handle;
  char *name;
  int open_mode;
  int64_t size;

  int headers_read;

  rmff_rmf_t rmf_header;
  rmff_prop_t prop_header;
  rmff_cont_t cont_header;
  int cont_header_present;
  uint32_t num_packets_in_chunk;
  uint32_t num_packets_read;

  rmff_track_t **tracks;
  int num_tracks;

  void *app_data;
  void *internal;
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

/** \brief Open the file for reading. */
#define RMFF_OPEN_MODE_READING                         0
/** \brief Create a new file. */
#define RMFF_OPEN_MODE_WRITING                         1

/** \brief Opens a RealMedia file for reading or writing.

  Can be used to open an existing file for reading or for creating a new
  file. The file headers will neither be read nor written automatically.
  This function uses the standard file I/O functions provided by the
  current operating system.

  \param path the name of the file that should be opened
  \param mode either ::RMFF_OPEN_MODE_READING or ::RMFF_OPEN_MODE_WRITING

  \returns a pointer to ::rmff_file_t structure or \c NULL if an error
  occured. In the latter case ::rmff_last_error will be set.

  \see rmff_open_file_with_io
*/
rmff_file_t *rmff_open_file(const char *path, int mode);

/** \brief Opens a RealMedia file for reading or writing.

  Can be used to open an existing file for reading or for creating a new
  file. The file headers will neither be read nor written automatically.
  This function uses I/O functions provided by the \a io parameter.

  \param path the name of the file that should be opened
  \param mode either ::RMFF_OPEN_MODE_READING or ::RMFF_OPEN_MODE_WRITING
  \param io a set of I/O functions

  \returns a pointer to a rmff_file_t structure or \c NULL if an error
  occured. In the latter case ::rmff_last_error will be set.

  \see rmff_open_file
*/
rmff_file_t *rmff_open_file_with_io(const char *path, int mode,
                                    mb_file_io_t *io);

/** \brief Close the file and release all resources.

  Closes the file and releases all resources associated with it, including
  the ::rmff_file_t structure. If the file was open for writing then
  ::rmff_fix_headers should be called prior to closing the file as
  \c rmff_close_file does not fix the headers itself.

  \param file The file to close.
*/
void rmff_close_file(rmff_file_t *file);

/** \brief Reads the file and track headers.

  This function should be called after directly after opening it for reading.
  It will try to read the file and track headers and position the file pointer
  right before the first data packet.

  \param file The file to read the headers from.

  \returns ::RMFF_ERR_OK on success and one of the other \c RMFF_ERR_*
  constants on error.
*/
int rmff_read_headers(rmff_file_t *file);

/** \brief Retrieves the size of the next frame.

  \param file The file to read from.

  \returns the size of the following frame or one of the \c RMFF_ERR_*
  constants on error.
*/
int rmff_get_next_frame_size(rmff_file_t *file);

/** \brief Reads the next frame from the file.

  The frame must be released by rmff_release_frame(rmff_frame_t*).

  \param file The file to read from.
  \param buffer A buffer to read the frame into. This parameter may be
  \c NULL in which case the buffer will be allocated by the library.
  If the application provides the buffer it must be large enough.
  The function ::rmff_get_next_frame_size can be used in this
  case.

  \returns a pointer to a ::rmff_frame_t structure containing the frame
  and its metadata on success or \c NULL if the call failed. This frame
  must be freed with ::rmff_release_frame.
*/
rmff_frame_t *rmff_read_next_frame(rmff_file_t *file, void *buffer);

/** \brief Allocates a frame and possibly a buffer for its contents.

  \param size The size of this frame.
  \param buffer A buffer that holds the frame. This parameter may be
  \c NULL in which case the buffer will be allocated by the library.
  If the application provides the buffer it must be large enough.

  \returns a pointer to an empty ::rmff_frame_t structure which can be filled
  with the frame contents and its metadata. This frame
  must be freed with ::rmff_release_frame.
*/
rmff_frame_t *rmff_allocate_frame(uint32_t size, void *buffer);

/** \brief Frees all resources associated with a frame.

  If the frame buffer was allocated by the library it will be freed as well.

  \param frame The frame to free.
*/
void rmff_release_frame(rmff_frame_t *frame);

/** \brief Creates a new track for a file.

  The track will be added to the list of tracks in this file.
  \a librmff will allocate a track ID that is unique in this file.

  \param file The file the track will be added to.
  \param create_index Indicates if \a librmff should create an index
  for this track that'll be written to the file when ::rmff_write_index is
  invoked.

  \returns A pointer to a newly allocated ::rmff_track_t structure or
  \c NULL in case of an error.
*/
rmff_track_t *rmff_add_track(rmff_file_t *file, int create_index);

/** \brief Set some default values for RealAudio streams.

  Some of the fields in the \a props structure will be set to pre-defined
  values, especially the \c uknown fields. Should be used after
  ::rmff_add_track.

  \note This function has not been implemented yet.

  \param props A pointer to the structure whose values should be set.

  \see rmff_set_std_audio_v5_values(real_audio_v5_props_t*),
  rmff_set_std_video_values(real_video_props_t*)
*/
void rmff_set_std_audio_v4_values(real_audio_v4_props_t *props);

/** \brief Set some default values for RealAudio streams.

  Some of the fields in the \a props structure will be set to pre-defined
  values, especially the \c uknown fields. Should be used after
  ::rmff_add_track.

  \note This function has not been implemented yet.

  \param props A pointer to the structure whose values should be set.

  \see rmff_set_std_audio_v4_values(real_audio_v5_props_t*),
  rmff_set_std_video_values(real_video_props_t*)
*/
void rmff_set_std_audio_v5_values(real_audio_v5_props_t *props);

/** \brief Set some default values for RealVideo streams.

  Some of the fields in the \a props structure will be set to pre-defined
  values, especially the \c uknown fields. Should be used after
  ::rmff_add_track.

  \note This function has not been implemented yet.

  \param props A pointer to the structure whose values should be set.

  \see rmff_set_std_audio_v4_values(real_audio_v4_props_t*),
  rmff_set_std_audio_v5_values(real_audio_v5_props_t*)
*/
void rmff_set_std_video_values(real_video_props_t *props);

/** \brief Sets the contents of the \link ::rmff_cont_t CONT file header
  \endlink.

  Frees the old contents if any and allocates copies of the given
  strings. If the CONT header should be written to the file
  in ""rmff_write_headers then the \a cont_header_present
  member must be set to 1.

  \param file The file whose CONT header should be set.
  \param title The file's title, e.g. "Muriel's Wedding"
  \param author The file's author, e.g. "P.J. Hogan"
  \param copyright The copyright assigned to the file.
  \param comment A free-style comment.
*/
void rmff_set_cont_header(rmff_file_t *file, const char *title,
                          const char *author, const char *copyright,
                          const char *comment);

/** \brief Sets the strings in the \link ::rmff_mdpr_t MDPR track header
  \endlink structure.

  Frees the old contents and allocates copies of the given strings.

  \param track The track whose MDPR header should be set.
  \param name The track's name.
  \param mime_type The MIME type. A video track should have the MIME type
  \c video/x-pn-realvideo, and an audio track should have the MIMT type
  \c audio/x-pn-realaudio.
*/
void rmff_set_track_data(rmff_track_t *track, const char *name,
                         const char *mime_type);

/** \brief Sets the \a track_specific_data member of the \link ::rmff_mdpr_t
  MDPR header\endlink structure.

  The \a track_specific_data usually contains a
  ::real_video_props_t structure or a ::real_audio_props_t structure.
  The existing data, if any, will be freed, and a copy of the memory
  \a data points to will be made.

  \param track The track whose MDPR header should be set.
  \param data A pointer to the track specific data.
  \param size The track specific data's size in bytes.
*/
void rmff_set_type_specific_data(rmff_track_t *track,
                                 const unsigned char *data, uint32_t size);

/** \brief Writes the file and track headers.

  This function should be called after adding all tracks and before the
  first call to ::rmff_write_frame. Before closing the file the application
  should call ::rmff_fix_headers in order to write the corrected header
  values.

  \param file The file whose headers should be written.

  \returns \c RMFF_ERR_OK on success or one of the other \c RMFF_ERR_*
  constants on error.
*/
int rmff_write_headers(rmff_file_t *file);

/** \brief Fixes some values in the headers and re-writes them.

  \a librmff automatically calculates some header values, e.g. the
  \c max_packet_size fields and the offsets in the file headers. This function
  updates these values and writes them to the file. It should be called
  before ::rmff_close_file.

  \param file The file whose headers should be fixed and written.

  \returns \c RMFF_ERR_OK on success or one of the other \c RMFF_ERR_*
  constants on error.
*/
int rmff_fix_headers(rmff_file_t *file);

/** \brief Writes the frame contents to the file.

  Should be called after ::rmff_write_headers. The track ID stored in the
  \a track parameter will overwrite the track ID in the \a frame structure.

  \param track The track this frame belongs to.
  \param frame The frame data to write.

  \returns \c RMFF_ERR_OK on success or one of the other \c RMFF_ERR_*
  constants on error.
*/
int rmff_write_frame(rmff_track_t *track, rmff_frame_t *frame);

/** \brief Unpacks a packed video frame into sub packets and writes each.

  Please see the section about packed video \ref packed_video_frames frames
  for a description of this function.

  This function takes such a complete frame, splits it up into their sub
  packets and writes each sub packet out into the file this track belongs to.

  \param track The track this frame belongs to.
  \param frame The assembled/packed video frame that is to be split up.

  \returns \c RMFF_ERR_OK on success or one of the other \c RMFF_ERR_*
  constants on error.
*/
int rmff_write_packed_video_frame(rmff_track_t *track, rmff_frame_t *frame);

/** \brief Parses a sub packet and attempts to assemble a packed video frame.

  Please see the section about packed video \ref packed_video_frames frames
  for a description of this function.

  This function takes a sub packet as they are stored in a RealMedia file
  and attempts to assemble the complete packet. More than one call to
  this function with consecutive frames read from a file are likely
  necessary before a complete frame can be assembled. The function
  ::rmff_get_packed_video_frame can be used to retrieve the assembled
  frames.

  These two functions, ::rmff_assemble_packed_video_frame and
  ::rmff_get_packed_video_frame, are the counterparts to
  ::rmff_write_packed_video_frame.

  \param track The track this frame belongs to.
  \param frame The sub packet that should be assembled.

  \returns one of the other \c RMFF_ERR_* constants on error and the number
  of available assembled frames on success.
*/
int rmff_assemble_packed_video_frame(rmff_track_t *track, rmff_frame_t *frame);

/** \brief Retrieve an assembled video frame.

  Please see the section about packed video \ref packed_video_frames frames
  for a description of this function.

  This function returns one assembled video frame after some calls to
  ::rmff_assemble_packed_video_frame.

  These two functions, ::rmff_assemble_packed_video_frame and
  ::rmff_get_packed_video_frame, are the counterparts to
  ::rmff_write_packed_video_frame.

  \param track The track an assembled frame should be read from.

  \returns an assembled frame on success or \c NULL if there's none
  available.
*/
rmff_frame_t *rmff_get_packed_video_frame(rmff_track_t *track);

/** \brief Creates an index at the end of the file.

  The application can request that an index is created For each track
  with the ::rmff_add_track call. For each of these tracks an index
  is created in which each key frame is listed. Consecutive key frames with
  the same timecode only get one index entry.

  This function should be called after all calls to ::rmff_write_frame
  but before ::rmff_fix_headers.

  \param file The file for which the index should be written.

  \returns \c RMFF_ERR_OK on success or one of the other \c RMFF_ERR_*
  constants on error.
*/
int rmff_write_index(rmff_file_t *file);

/** \brief Duplicated the track headers.

  Takes a ::rmff_track_t structure and duplicates its members into
  another one. The destination must have been created first with
  ::rmff_add_track. Everything but the track ID will be copied.

  \param dst The destination to write the copies to.
  \param src The structure that should be duplicated.
*/
void rmff_copy_track_headers(rmff_track_t *dst, rmff_track_t *src);

/** \brief Finds the track the track ID belongs to.

  \param file The file whose tracks are to be searched.
  \param id The track ID.

  \returns A pointer to a ::rmff_track_t structure or \c NULL on error.
*/
rmff_track_t *rmff_find_track_with_id(rmff_file_t *file, uint16_t id);

/** \brief The error code of the last function call.

 Contains the last error code for a function that failed. If a function
 succeeded it usually does not reset \a rmff_last_error to \c RMFF_ERR_OK.
 This variable can contain one of the \c RMFF_ERR_* constants.
*/
extern int rmff_last_error;

/** \brief The error message of the last function call.

  Contains the last error message for a function that failed. If a function
  succeeded it usually does not reset \c rmff_last_error_msg to \c NULL.
*/
extern const char *rmff_last_error_msg;

/** \brief Map an error code to an error message.

  Returns the error message that corresponds to the error code.

  \param code the error code which must be one of the \c RMFF_ERR_* macros.

  \returns the error message or "Unknown error" for unknown error codes,
  but never \c NULL.
*/
const char *rmff_get_error_str(int code);

/** \brief Reads a 16bit uint from an address.

  The uint is converted from big endian byte order to the machine's byte
  order.

  \param buf The address to read from.

  \returns The 16bit uint converted to the machine's byte order.
*/
uint16_t rmff_get_uint16_be(const void *buf);

/** \brief Reads a 32bit uint from an address.

  The uint is converted from big endian byte order to the machine's byte
  order.

  \param buf The address to read from.

  \returns The 32bit uint converted to the machine's byte order.
*/
uint32_t rmff_get_uint32_be(const void *buf);

/** \brief Reads a 32bit uint from an address.

  The uint is converted from little endian byte order to the machine's byte
  order.

  \param buf The address to read from.

  \returns The 32bit uint converted to the machine's byte order.
*/
uint32_t rmff_get_uint32_le(const void *buf);

/** \brief Write a 16bit uint at an address.

  The value is converted from the machine's byte order to big endian byte
  order.

  \param buf The address to write to.
  \param value The value to write.
*/
void rmff_put_uint16_be(void *buf, uint16_t value);

/** \brief Write a 32bit uint at an address.

  The value is converted from the machine's byte order to big endian byte
  order.

  \param buf The address to write to.
  \param value The value to write.
*/
void rmff_put_uint32_be(void *buf, uint32_t value);

/** \brief Write a 32bit uint at an address.

  The value is converted from the machine's byte order to little endian byte
  order.

  \param buf The address to write to.
  \param value The value to write.
*/
void rmff_put_uint32_le(void *buf, uint32_t value);

#if defined(ARCH_LITTLEENDIAN)
# define rmff_get_uint32_me(b) rmff_get_uint32_le(b)
# define rmff_put_uint32_me(b) rmff_put_uint32_le(b)
#elif defined(ARCH_BIGENDIAN)
# define rmff_get_uint32_me(b) rmff_get_uint32_be(b)
# define rmff_put_uint32_me(b) rmff_put_uint32_be(b)
#else
# error Neither ARCH_LITTLEENDIAN nor ARCH_BIGENDIAN has been defined.
#endif

#if defined(__cplusplus)
}
#endif

#endif /* __RMFF_H */
