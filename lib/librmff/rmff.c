/*
  rmff.c

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

#include "common/os.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "librmff.h"

#if defined(COMP_MSC)
#define inline __inline
#endif

typedef struct rmff_video_segment_t {
  uint32_t size;
  uint32_t offset;
  unsigned char *data;
} rmff_video_segment_t;

typedef struct rmff_bitrate_t {
  uint32_t *timecodes;
  uint32_t *frame_sizes;
  uint32_t num_entries;
} rmff_bitrate_t;

typedef struct rmff_file_internal_t {
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t highest_timecode;
  uint32_t num_packets;
  uint32_t data_offset;
  uint32_t next_data_offset;
  uint32_t data_contents_size;
  uint32_t index_offset;
  int num_index_chunks;
  uint32_t total_bytes;
  rmff_bitrate_t bitrate;
} rmff_file_internal_t;

typedef struct rmff_track_internal_t {
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t highest_timecode;
  uint32_t num_packets;
  uint32_t num_packed_frames;
  int index_this;
  uint32_t total_bytes;
  rmff_bitrate_t bitrate;

  /* Used for video packet assembly. */
  uint32_t c_timecode;
  int f_merged;
  int c_keyframe;
  rmff_video_segment_t *segments;
  int num_segments;
  rmff_frame_t **assembled_frames;
  int num_assembled_frames;
} rmff_track_internal_t;

int rmff_last_error = RMFF_ERR_OK;
const char *rmff_last_error_msg = NULL;
static char error_msg_buffer[1000];
static const char *rmff_std_error_messages[] = {
  "No error",
  "File is not a RealMedia file",
  "Inconsistent data found in file",
  "End of file reached",
  "Input/output error",
  "Invalid parameters"
};

const char *
rmff_get_error_str(int code) {
  code *= -1;
  if ((code >= 0) && (code <= 5))
    return rmff_std_error_messages[code];
  else
    return "Unknown error";
}

static void
set_error(int error_number,
          const char *error_msg) {
  rmff_last_error = error_number;
  if (error_msg == NULL)
    rmff_last_error_msg = rmff_get_error_str(error_number);
  else
    rmff_last_error_msg = error_msg;
}

#define clear_error() set_error(RMFF_ERR_OK, NULL)

uint16_t
rmff_get_uint16_be(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);

  return ret;
}

uint32_t
rmff_get_uint32_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);

  return ret;
}

uint32_t
rmff_get_uint32_le(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

void
rmff_put_uint16_be(void *buf,
                   uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[1] = value & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
rmff_put_uint32_be(void *buf,
                   uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[3] = value & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
rmff_put_uint32_le(void *buf,
                   uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
}

#define read_uint8() file_read_uint8(io, fh)
#define read_uint16_be() file_read_uint16_be(io, fh)
#define read_uint32_be() file_read_uint32_be(io, fh)
#define read_uint16_be_to(addr) rmff_put_uint16_be(addr, read_uint16_be())
#define read_uint32_be_to(addr) rmff_put_uint32_be(addr, read_uint32_be())
#define write_uint8(v) file_write_uint8(io, fh, v)
#define write_uint16_be(v) file_write_uint16_be(io, fh, v)
#define write_uint32_be(v) file_write_uint32_be(io, fh, v)
#define write_uint16_be_from(addr) write_uint16_be(rmff_get_uint16_be(addr))
#define write_uint32_be_from(addr) write_uint32_be(rmff_get_uint32_be(addr))
#define get_fourcc(b) rmff_get_uint32_be(b)

static uint8_t
file_read_uint8(mb_file_io_t *io,
                void *fh) {
  unsigned char tmp;

  if (io->read(fh, &tmp, 1) != 1) {
    set_error(RMFF_ERR_IO, NULL);
    return 0;
  }
  return tmp;
}

static uint16_t
file_read_uint16_be(mb_file_io_t *io,
                    void *fh) {
  unsigned char tmp[2];

  if (io->read(fh, tmp, 2) != 2) {
    set_error(RMFF_ERR_IO, NULL);
    return 0;
  }
  return rmff_get_uint16_be(tmp);
}

static uint32_t
file_read_uint32_be(mb_file_io_t *io,
                    void *fh) {
  unsigned char tmp[4];

  if (io->read(fh, tmp, 4) != 4) {
    set_error(RMFF_ERR_IO, NULL);
    return 0;
  }
  return rmff_get_uint32_be(tmp);
}

static int
file_write_uint8(mb_file_io_t *io,
                 void *fh,
                 uint8_t value) {
  return io->write(fh, &value, 1);
}

static int
file_write_uint16_be(mb_file_io_t *io,
                     void *fh,
                     uint16_t value) {
  unsigned char tmp[2];

  rmff_put_uint16_be(tmp, value);
  return io->write(fh, &tmp, 2);
}

static int
file_write_uint32_be(mb_file_io_t *io,
                     void *fh,
                     uint32_t value) {
  unsigned char tmp[4];

  rmff_put_uint32_be(tmp, value);
  return io->write(fh, &tmp, 4);
}

void
die(const char *fmt,
    ...) {
  va_list ap;

  fprintf(stderr, "'die' called: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

#define safestrdup(s) _safestrdup(s, __FILE__, __LINE__)
#define safememdup(s, b) _safememdup(s, b, __FILE__, __LINE__)
#define safemalloc(s) _safemalloc(s, __FILE__, __LINE__)
#define safecalloc(s) _safecalloc(s, __FILE__, __LINE__)
#define saferealloc(s, b) _saferealloc(s, b, __FILE__, __LINE__)
#define safefree(s) { if ((s) != NULL) free(s); }

static char *
_safestrdup(const char *s,
            const char *file,
            int line) {
  char *copy;

  if (s == NULL)
    return NULL;

  copy = strdup(s);
  if (copy == NULL)
    die("safestrdup() called from file %s, line %d: strdup() "
        "returned NULL for '%s'.", file, line, s);

  return copy;
}

static void *
_safememdup(const void *s,
            size_t size,
            const char *file,
            int line) {
  void *copy;

  if (s == NULL)
    return NULL;

  copy = malloc(size);
  if (copy == NULL)
    die("safememdup() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);
  memcpy(copy, s, size);

  return copy;
}

static void *
_safemalloc(size_t size,
            const char *file,
            int line) {
  void *mem;

  mem = malloc(size);
  if (mem == NULL)
    die("safemalloc() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

static void *_safecalloc(size_t size,
                         const char *file,
                         int line) {
  void *mem;

  mem = calloc(size, 1);
  if (mem == NULL)
    die("safemalloc() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

static void *
_saferealloc(void *mem,
             size_t size,
             const char *file,
             int line) {
  mem = realloc(mem, size);
  if (mem == NULL)
    die("saferealloc() called from file %s, line %d: realloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

static rmff_file_t *
open_file_for_reading(const char *path,
                      mb_file_io_t *io) {
  rmff_file_t *file;
  void *file_h;
  char signature[5];

  file_h = io->open(path, MB_OPEN_MODE_READING);
  if (file_h == NULL)
    return NULL;

  signature[4] = 0;
  if ((io->read(file_h, signature, 4) != 4) ||
      strcmp(signature, ".RMF")) {
    io->close(file_h);
    set_error(RMFF_ERR_NOT_RMFF, NULL);
    return NULL;
  }

  file = (rmff_file_t *)safecalloc(sizeof(rmff_file_t));
  file->handle = file_h;
  file->name = safestrdup(path);
  file->io = io;
  io->seek(file_h, 0, SEEK_END);
  file->size = io->tell(file_h);
  io->seek(file_h, 4, SEEK_SET);
  file->open_mode = RMFF_OPEN_MODE_READING;
  file->internal = safecalloc(sizeof(rmff_file_internal_t));

  clear_error();
  return file;
}

static rmff_file_t *
open_file_for_writing(const char *path,
                      mb_file_io_t *io) {
  rmff_file_t *file;
  void *file_h;
  const char *signature = ".RMF";

  file_h = io->open(path, MB_OPEN_MODE_WRITING);
  if (file_h == NULL)
    return NULL;

  if (io->write(file_h, signature, 4) != 4) {
    io->close(file_h);
    set_error(RMFF_ERR_IO, NULL);
    return NULL;
  }

  file = (rmff_file_t *)safecalloc(sizeof(rmff_file_t));
  file->handle = file_h;
  file->name = safestrdup(path);
  file->io = io;
  file->size = -1;
  file->open_mode = RMFF_OPEN_MODE_WRITING;
  file->internal = safecalloc(sizeof(rmff_file_internal_t));

  /* save allowed & perfect play */
  rmff_put_uint16_be(&file->prop_header.flags,
                     RMFF_FILE_FLAG_SAVE_ENABLED |
                     RMFF_FILE_FLAG_DOWNLOAD_ENABLED);

  clear_error();
  return file;
}

rmff_file_t *
rmff_open_file(const char *path,
               int mode) {
  return rmff_open_file_with_io(path, mode, &std_mb_file_io);
}

rmff_file_t *
rmff_open_file_with_io(const char *path,
                       int mode,
                       mb_file_io_t *io) {
  if ((path == NULL) || (io == NULL) ||
      ((mode != RMFF_OPEN_MODE_READING) && (mode != RMFF_OPEN_MODE_WRITING))) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return NULL;
  }

  if (mode == RMFF_OPEN_MODE_READING)
    return open_file_for_reading(path, io);
  else
    return open_file_for_writing(path, io);
}

void
rmff_free_track_data(rmff_track_t *track) {
  rmff_track_internal_t *tint;
  int i;

  if (track == NULL)
    return;
  safefree(track->index);
  safefree(track->mdpr_header.name);
  safefree(track->mdpr_header.mime_type);
  safefree(track->mdpr_header.type_specific_data);
  tint = (rmff_track_internal_t *)track->internal;
  for (i = 0; i < tint->num_segments; i++)
    safefree(tint->segments[i].data);
  safefree(tint->segments);
  for (i = 0; i < tint->num_assembled_frames; i++)
    rmff_release_frame(tint->assembled_frames[i]);
  safefree(tint->assembled_frames);
  safefree(track->internal);
}

void
rmff_close_file(rmff_file_t *file) {
  int i;

  if (file == NULL)
    return;

  safefree(file->name);
  for (i = 0; i < file->num_tracks; i++) {
    rmff_free_track_data(file->tracks[i]);
    safefree(file->tracks[i]);
  }
  safefree(file->tracks);
  safefree(file->cont_header.title);
  safefree(file->cont_header.author);
  safefree(file->cont_header.copyright);
  safefree(file->cont_header.comment);
  safefree(file->internal);
  file->io->close(file->handle);
  safefree(file);
}

#define skip(num) \
{ \
  if (io->seek(fh, num, SEEK_CUR) != 0) {\
    set_error(RMFF_ERR_IO, NULL); \
    return -1; \
  } \
}

static void
add_to_index(rmff_track_t *track,
             int64_t pos,
             uint32_t timecode,
             uint32_t packet_number) {
  if ((track->index != NULL) &&
      (track->index[track->num_index_entries - 1].timecode == timecode))
    return;
  track->index = (rmff_index_entry_t *)
    saferealloc(track->index, (track->num_index_entries + 1) *
                sizeof(rmff_index_entry_t));
  track->index[track->num_index_entries].pos = pos;
  track->index[track->num_index_entries].timecode = timecode;
  track->index[track->num_index_entries].packet_number = packet_number;
  track->num_index_entries++;
}

static void
read_index(rmff_file_t *file,
           int64_t pos) {
  void *fh;
  mb_file_io_t *io;
  rmff_track_t *track;
  uint32_t object_id, num_entries;
  uint32_t next_header_offset, i, timecode, offset, packet_number;
  uint16_t id;

  fh = file->handle;
  io = file->io;

  rmff_last_error = RMFF_ERR_OK;
  io->seek(fh, pos, SEEK_SET);
  object_id = read_uint32_be();
  read_uint32_be();             /* object_size */
  read_uint16_be();             /* object_version (2) */
  if (rmff_last_error != RMFF_ERR_OK)
    return;

  if (object_id != rmffFOURCC('I', 'N', 'D', 'X'))
    return;

  num_entries = read_uint32_be();
  id = read_uint16_be();
  track = rmff_find_track_with_id(file, id);
  if (track == NULL)
    return;
  next_header_offset = read_uint32_be();
  track->num_index_entries = 0;
  safefree(track->index);
  track->index = NULL;

  for (i = 0; i < num_entries; i++) {
    if (read_uint16_be() != 0)  /* version */
      return;
    timecode = read_uint32_be();
    offset = read_uint32_be();
    packet_number = read_uint32_be();
    add_to_index(track, timecode, offset, packet_number);
  }

  if (next_header_offset > 0)
    read_index(file, next_header_offset);
}

int
rmff_read_headers(rmff_file_t *file) {
  mb_file_io_t *io;
  void *fh;
  uint32_t object_id, object_size, size;
  uint16_t object_version;
  rmff_rmf_t *rmf;
  rmff_prop_t *prop;
  rmff_cont_t *cont;
  rmff_mdpr_t *mdpr;
  rmff_track_t *track;
  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;
  rmff_file_internal_t *fint;
  int prop_header_found;
  int64_t old_pos;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }
  if (file->headers_read)
    return 0;

  io = file->io;
  fh = file->handle;
  fint = (rmff_file_internal_t *)file->internal;
  if (io->seek(fh, 0, SEEK_SET)) {
    set_error(RMFF_ERR_IO, NULL);
    return RMFF_ERR_IO;
  }

  rmf = &file->rmf_header;

  rmff_last_error = RMFF_ERR_OK;
  read_uint32_be_to(&rmf->obj.id);
  read_uint32_be_to(&rmf->obj.size);
  read_uint16_be_to(&rmf->obj.version);
  read_uint32_be_to(&rmf->format_version);
  read_uint32_be_to(&rmf->num_headers);
  if (rmff_last_error != RMFF_ERR_OK)
    return rmff_last_error;

  prop = &file->prop_header;
  prop_header_found = 0;
  cont = &file->cont_header;
  while (1) {
    rmff_last_error = RMFF_ERR_OK;
    object_id = read_uint32_be();
    object_size = read_uint32_be();
    object_version = read_uint16_be();
    if (rmff_last_error != RMFF_ERR_OK)
      break;

    if (object_id == rmffFOURCC('P', 'R', 'O', 'P')) {
      rmff_put_uint32_be(&prop->obj.id, object_id);
      rmff_put_uint32_be(&prop->obj.size, object_size);
      rmff_put_uint16_be(&prop->obj.version, object_version);
      read_uint32_be_to(&prop->max_bit_rate);
      read_uint32_be_to(&prop->avg_bit_rate);
      read_uint32_be_to(&prop->max_packet_size);
      read_uint32_be_to(&prop->avg_packet_size);
      read_uint32_be_to(&prop->num_packets);
      read_uint32_be_to(&prop->duration);
      read_uint32_be_to(&prop->preroll);
      read_uint32_be_to(&prop->index_offset);
      read_uint32_be_to(&prop->data_offset);
      read_uint16_be_to(&prop->num_streams);
      read_uint16_be_to(&prop->flags);
      prop_header_found = 1;

    } else if (object_id == rmffFOURCC('C', 'O', 'N', 'T')) {
      if (file->cont_header_present) {
        safefree(cont->title);
        safefree(cont->author);
        safefree(cont->copyright);
        safefree(cont->comment);
      }
      memset(cont, 0, sizeof(rmff_cont_t));
      rmff_put_uint32_be(&cont->obj.id, object_id);
      rmff_put_uint32_be(&cont->obj.size, object_size);
      rmff_put_uint16_be(&cont->obj.version, object_version);

      size = read_uint16_be(); /* title_len */
      if (size > 0) {
        cont->title = (char *)safecalloc(size + 1);
        io->read(fh, cont->title, size);
      }
      size = read_uint16_be(); /* author_len */
      if (size > 0) {
        cont->author = (char *)safecalloc(size + 1);
        io->read(fh, cont->author, size);
      }
      size = read_uint16_be(); /* copyright_len */
      if (size > 0) {
        cont->copyright = (char *)safecalloc(size + 1);
        io->read(fh, cont->copyright, size);
      }
      size = read_uint16_be(); /* comment_len */
      if (size > 0) {
        cont->comment = (char *)safecalloc(size + 1);
        io->read(fh, cont->comment, size);
      }
      file->cont_header_present = 1;

    } else if (object_id == rmffFOURCC('M', 'D', 'P', 'R')) {
      track = (rmff_track_t *)safecalloc(sizeof(rmff_track_t));
      track->file = (struct rmff_file_t *)file;
      mdpr = &track->mdpr_header;
      rmff_put_uint32_be(&mdpr->obj.id, object_id);
      rmff_put_uint32_be(&mdpr->obj.size, object_size);
      rmff_put_uint16_be(&mdpr->obj.version, object_version);
      read_uint16_be_to(&mdpr->id);
      track->id = rmff_get_uint16_be(&mdpr->id);
      read_uint32_be_to(&mdpr->max_bit_rate);
      read_uint32_be_to(&mdpr->avg_bit_rate);
      read_uint32_be_to(&mdpr->max_packet_size);
      read_uint32_be_to(&mdpr->avg_packet_size);
      read_uint32_be_to(&mdpr->start_time);
      read_uint32_be_to(&mdpr->preroll);
      read_uint32_be_to(&mdpr->duration);
      size = read_uint8(); /* stream_name_len */
      if (size > 0) {
        mdpr->name = (char *)safemalloc(size + 1);
        io->read(fh, mdpr->name, size);
        mdpr->name[size] = 0;
      }
      size = read_uint8(); /* mime_type_len */
      if (size > 0) {
        mdpr->mime_type = (char *)safemalloc(size + 1);
        io->read(fh, mdpr->mime_type, size);
        mdpr->mime_type[size] = 0;
      }
      size = read_uint32_be();  /* type_specific_size */
      rmff_put_uint32_be(&mdpr->type_specific_size, size);
      if (size > 0) {
        mdpr->type_specific_data = (unsigned char *)safemalloc(size);
        io->read(fh, mdpr->type_specific_data, size);
      }

      rvp = (real_video_props_t *)mdpr->type_specific_data;
      ra4p = (real_audio_v4_props_t *)mdpr->type_specific_data;
      if ((size >= sizeof(real_video_props_t)) &&
          (get_fourcc(&rvp->fourcc1) == rmffFOURCC('V', 'I', 'D', 'O')))
        track->type = RMFF_TRACK_TYPE_VIDEO;
      else if ((size >= sizeof(real_audio_v4_props_t)) &&
               (get_fourcc(&ra4p->fourcc1) ==
                rmffFOURCC('.', 'r', 'a', 0xfd))) {
        track->type = RMFF_TRACK_TYPE_AUDIO;
        if ((rmff_get_uint16_be(&ra4p->version1) == 5) &&
            (size < sizeof(real_audio_v5_props_t))) {
          set_error(RMFF_ERR_DATA, "RealAudio v5 data indicated but "
                    "data too small");
          return RMFF_ERR_DATA;
        }
      }

      track->internal = safecalloc(sizeof(rmff_track_internal_t));
      file->tracks =
        (rmff_track_t **)saferealloc(file->tracks, (file->num_tracks + 1) *
                                     sizeof(rmff_track_t *));
      file->tracks[file->num_tracks] = track;
      file->num_tracks++;

    } else if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
      fint->data_offset = io->tell(fh) - (4 + 4 + 2);
      file->num_packets_in_chunk = read_uint32_be();
      fint->next_data_offset = read_uint32_be();
      break;

    } else {
      /* Unknown header type */
      set_error(RMFF_ERR_DATA, NULL);
      return RMFF_ERR_DATA;
    }
  }

  if (prop_header_found && (fint->data_offset > 0)) {
    if (rmff_get_uint32_be(&file->prop_header.index_offset) > 0) {
      old_pos = io->tell(fh);
      read_index(file, rmff_get_uint32_be(&file->prop_header.index_offset));
      io->seek(fh, old_pos, SEEK_SET);
    }
    file->headers_read = 1;
    return 0;
  }
  set_error(RMFF_ERR_DATA, NULL);
  return RMFF_ERR_DATA;
}

int
rmff_get_next_frame_size(rmff_file_t *file) {
  uint16_t object_version, length;
  uint32_t object_id;
  mb_file_io_t *io;
  void *fh;
  int result;
  int64_t old_pos;

  if ((file == NULL) || (!file->headers_read) || (file->io == NULL) ||
      (file->handle == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }
  io = file->io;
  fh = file->handle;
  old_pos = io->tell(fh);
  if ((file->size - old_pos) < 12) {
    set_error(RMFF_ERR_EOF, NULL);
    return RMFF_ERR_EOF;
  }

  object_version = read_uint16_be();
  length = read_uint16_be();
  object_id = ((uint32_t)object_version) << 16 | length;
  if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
    skip(4 + 4);                /* packets_in_chunk, next_data_header */
    result = rmff_get_next_frame_size(file);
    io->seek(fh, old_pos, SEEK_SET);
    return result;
  }
  io->seek(fh, old_pos, SEEK_SET);
  if (object_id == rmffFOURCC('I', 'N', 'D', 'X')) {
    set_error(RMFF_ERR_EOF, NULL);
    return RMFF_ERR_EOF;
  }
  return length - 12;
}

rmff_frame_t *
rmff_read_next_frame(rmff_file_t *file,
                     void *buffer) {
  rmff_frame_t *frame;
  rmff_file_internal_t *fint;
  uint16_t object_version, length;
  uint32_t object_id;
  mb_file_io_t *io;
  void *fh;

  if ((file == NULL) || (!file->headers_read) || (file->io == NULL) ||
      (file->handle == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return NULL;
  }
  io = file->io;
  fh = file->handle;
  fint = (rmff_file_internal_t *)file->internal;
  if ((file->size - io->tell(fh)) < 12) {
    set_error(RMFF_ERR_EOF, NULL);
    return NULL;
  }

  object_version = read_uint16_be();
  length = read_uint16_be();
  object_id = ((uint32_t)object_version) << 16 | length;
  if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
    file->num_packets_in_chunk = read_uint32_be();
    fint->next_data_offset = read_uint32_be();
    file->num_packets_read = 0;
    return rmff_read_next_frame(file, buffer);
  }
  if ((file->num_packets_read >= file->num_packets_in_chunk) ||
      (object_id == rmffFOURCC('I', 'N', 'D', 'X'))) {
    set_error(RMFF_ERR_EOF, NULL);
    return NULL;
  }

  frame = (rmff_frame_t *)safecalloc(sizeof(rmff_frame_t));
  if (buffer == NULL) {
    buffer = safemalloc(length);
    frame->allocated_by_rmff = 1;
  }
  frame->data = (unsigned char *)buffer;
  frame->size = length - 12;
  frame->id = read_uint16_be();
  frame->timecode = read_uint32_be();
  frame->reserved = read_uint8();
  frame->flags = read_uint8();
  if (io->read(fh, frame->data, frame->size) != frame->size) {
    rmff_release_frame(frame);
    set_error(RMFF_ERR_EOF, NULL);
    return NULL;
  }
  file->num_packets_read++;

  return frame;
}

rmff_frame_t *
rmff_allocate_frame(uint32_t size,
                    void *buffer) {
  rmff_frame_t *frame;

  if (size == 0) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return NULL;
  }
  frame = (rmff_frame_t *)safecalloc(sizeof(rmff_frame_t));
  if (buffer == NULL) {
    buffer = safemalloc(size);
    frame->allocated_by_rmff = 1;
  }
  frame->size = size;
  frame->data = (unsigned char *)buffer;

  return frame;
}

void
rmff_release_frame(rmff_frame_t *frame) {
  if (frame == NULL)
    return;
  if (frame->allocated_by_rmff)
    safefree(frame->data);
  safefree(frame);
}

void
rmff_set_cont_header(rmff_file_t *file,
                     const char *title,
                     const char *author,
                     const char *copyright,
                     const char *comment) {
  if (file == NULL)
    return;

  safefree(file->cont_header.title);
  safefree(file->cont_header.author);
  safefree(file->cont_header.copyright);
  safefree(file->cont_header.comment);
  file->cont_header.title = safestrdup(title);
  file->cont_header.author = safestrdup(author);
  file->cont_header.copyright = safestrdup(copyright);
  file->cont_header.comment = safestrdup(comment);
}

void
rmff_set_track_data(rmff_track_t *track,
                    const char *name,
                    const char *mime_type) {
  if (track == NULL)
    return;
  if (name != track->mdpr_header.name) {
    safefree(track->mdpr_header.name);
    track->mdpr_header.name = safestrdup(name);
  }
  if (mime_type != track->mdpr_header.mime_type) {
    safefree(track->mdpr_header.mime_type);
    track->mdpr_header.mime_type = safestrdup(mime_type);
  }
}

void
rmff_set_type_specific_data(rmff_track_t *track,
                            const unsigned char *data,
                            uint32_t size) {
  if (track == NULL)
    return;
  if (data != track->mdpr_header.type_specific_data) {
    safefree(track->mdpr_header.type_specific_data);
    track->mdpr_header.type_specific_data =
      (unsigned char *)safememdup(data, size);
    rmff_put_uint32_be(&track->mdpr_header.type_specific_size, size);
  }
}

rmff_track_t *
rmff_add_track(rmff_file_t *file,
               int create_index) {
  rmff_track_t *track;
  rmff_track_internal_t *tint;
  int i, id, found;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return NULL;
  }

  id = 0;
  do {
    found = 0;
    for (i = 0; i < file->num_tracks; i++)
      if (file->tracks[i]->id == id) {
        found = 1;
        id++;
        break;
      }
  } while (found);

  track = (rmff_track_t *)safecalloc(sizeof(rmff_track_t));
  track->id = id;
  track->file = file;
  tint = (rmff_track_internal_t *)safecalloc(sizeof(rmff_track_internal_t));
  tint->index_this = create_index;
  track->internal = tint;

  file->tracks =
    (rmff_track_t **)saferealloc(file->tracks, (file->num_tracks + 1) *
                                 sizeof(rmff_track_t *));
  file->tracks[file->num_tracks] = track;
  file->num_tracks++;

  return track;
}

void
rmff_set_std_audio_v4_values(real_audio_v4_props_t *props) {
  if (props == NULL)
    return;

  memset(props, 0, sizeof(real_audio_v4_props_t));
  rmff_put_uint32_be(&props->fourcc1, rmffFOURCC('.', 'r', 'a', 0xfd));
  rmff_put_uint16_be(&props->version1, 4);
  rmff_put_uint32_be(&props->fourcc2, rmffFOURCC('.', 'r', 'a', '4'));
  rmff_put_uint32_be(&props->stream_length, 0x10000000);
  rmff_put_uint16_be(&props->version2, 4);
  rmff_put_uint16_be(&props->header_size, 0x4e);

}

void
rmff_set_std_audio_v5_values(real_audio_v5_props_t *props) {
}

void
rmff_set_std_video_values(real_video_props_t *props) {
}

static int
write_prop_header(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  int bw;

  io = file->io;
  fh = file->handle;

  /* Write the PROP header. */
  bw = write_uint32_be(rmffFOURCC('P', 'R', 'O', 'P'));
  bw += write_uint32_be(0x32);  /* object_size */
  bw += write_uint16_be(0);     /* object_version */

  bw += write_uint32_be_from(&file->prop_header.max_bit_rate);
  bw += write_uint32_be_from(&file->prop_header.avg_bit_rate);
  bw += write_uint32_be_from(&file->prop_header.max_packet_size);
  bw += write_uint32_be_from(&file->prop_header.avg_packet_size);
  bw += write_uint32_be_from(&file->prop_header.num_packets);
  bw += write_uint32_be_from(&file->prop_header.duration);
  bw += write_uint32_be_from(&file->prop_header.preroll);
  bw += write_uint32_be_from(&file->prop_header.index_offset);
  bw += write_uint32_be_from(&file->prop_header.data_offset);
  bw += write_uint16_be_from(&file->prop_header.num_streams);
  bw += write_uint16_be_from(&file->prop_header.flags);

  return bw;
}

static int
write_cont_header(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  int bw, wanted_len, title_len, author_len, copyright_len, comment_len;

  io = file->io;
  fh = file->handle;

  if (file->cont_header.title == NULL)
    title_len = 0;
  else
    title_len = strlen(file->cont_header.title);
  if (file->cont_header.author == NULL)
    author_len = 0;
  else
    author_len = strlen(file->cont_header.author);
  if (file->cont_header.copyright == NULL)
    copyright_len = 0;
  else
    copyright_len = strlen(file->cont_header.copyright);
  if (file->cont_header.comment == NULL)
    comment_len = 0;
  else
    comment_len = strlen(file->cont_header.comment);

  wanted_len = 4 + 4 + 2 + 4 * 2 + title_len + author_len + copyright_len +
    comment_len;

  /* Write the CONT header. */
  bw = write_uint32_be(rmffFOURCC('C', 'O', 'N', 'T'));
  bw += write_uint32_be(wanted_len); /* object_size */
  bw += write_uint16_be(0);     /* object_version */
  bw += write_uint16_be(title_len);
  if (file->cont_header.title != NULL)
    bw += io->write(fh, file->cont_header.title, title_len);
  bw += write_uint16_be(author_len);
  if (file->cont_header.author != NULL)
    bw += io->write(fh, file->cont_header.author, author_len);
  bw += write_uint16_be(copyright_len);
  if (file->cont_header.copyright != NULL)
    bw += io->write(fh, file->cont_header.copyright, copyright_len);
  bw += write_uint16_be(comment_len);
  if (file->cont_header.comment != NULL)
    bw += io->write(fh, file->cont_header.comment, comment_len);

  if (bw == wanted_len)
    return 0;
  set_error(RMFF_ERR_IO, "Could not write the CONT header");
  return RMFF_ERR_IO;
}

static int
write_mdpr_header(rmff_track_t *track) {
  void *fh;
  mb_file_io_t *io;
  int bw, wanted_len, name_len, mime_type_len;

  io = track->file->io;
  fh = track->file->handle;

  rmff_put_uint16_be(&track->mdpr_header.id, track->id);
  if (track->mdpr_header.name == NULL)
    name_len = 0;
  else
    name_len = strlen(track->mdpr_header.name);
  if (track->mdpr_header.mime_type == NULL)
    mime_type_len = 0;
  else
    mime_type_len = strlen(track->mdpr_header.mime_type);

  wanted_len = 4 + 4 + 2 + 2 + 7 * 4 + 1 + name_len + 1 + mime_type_len +
    4 + rmff_get_uint32_be(&track->mdpr_header.type_specific_size);

  /* Write the MDPR header. */
  bw = write_uint32_be(rmffFOURCC('M', 'D', 'P', 'R'));
  bw += write_uint32_be(wanted_len); /* object_size */
  bw += write_uint16_be(0);     /* object_version */

  bw += write_uint16_be_from(&track->mdpr_header.id);
  bw += write_uint32_be_from(&track->mdpr_header.max_bit_rate);
  bw += write_uint32_be_from(&track->mdpr_header.avg_bit_rate);
  bw += write_uint32_be_from(&track->mdpr_header.max_packet_size);
  bw += write_uint32_be_from(&track->mdpr_header.avg_packet_size);
  bw += write_uint32_be_from(&track->mdpr_header.start_time);
  bw += write_uint32_be_from(&track->mdpr_header.preroll);
  bw += write_uint32_be_from(&track->mdpr_header.duration);
  bw += write_uint8(name_len);
  if (track->mdpr_header.name != NULL)
    bw += io->write(fh, track->mdpr_header.name, name_len);
  bw += write_uint8(mime_type_len);
  if (track->mdpr_header.mime_type != NULL)
    bw += io->write(fh, track->mdpr_header.mime_type, mime_type_len);
  bw += write_uint32_be_from(&track->mdpr_header.type_specific_size);
  if (track->mdpr_header.type_specific_data != NULL)
    bw +=
      io->write(fh, track->mdpr_header.type_specific_data,
                rmff_get_uint32_be(&track->mdpr_header.type_specific_size));

  if (wanted_len != bw) {
    set_error(RMFF_ERR_IO, "Could not write the MDPR header");
    return RMFF_ERR_IO;
  }
  clear_error();
  return RMFF_ERR_OK;
}

static int
write_data_header(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  rmff_file_internal_t *fint;
  int bw;

  io = file->io;
  fh = file->handle;
  fint = (rmff_file_internal_t *)file->internal;

  bw = write_uint32_be(rmffFOURCC('D', 'A', 'T', 'A'));
  bw += write_uint32_be(fint->data_contents_size + 4 + 4 + 2 + 4 + 4);
  bw += write_uint16_be(0);     /* object_version */
  bw += write_uint32_be(fint->num_packets); /* num_packets_in_chunk */
  bw += write_uint32_be(0);     /* next_data_header_offset */

  if (bw != 18) {
    set_error(RMFF_ERR_IO, "Could not write the DATA header");
    return RMFF_ERR_IO;
  }
  clear_error();
  return RMFF_ERR_OK;
}

int
rmff_write_headers(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  int i, bw, num_headers;
  rmff_file_internal_t *fint;
  const char *signature = ".RMF";

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }

  io = file->io;
  fh = file->handle;
  io->seek(fh, 0, SEEK_SET);
  fint = (rmff_file_internal_t *)file->internal;

  num_headers = 1 +             /* PROP */
    1 +                         /* DATA */
    file->num_tracks +          /* MDPR */
    fint->num_index_chunks;     /* INDX */
  if (file->cont_header_present)
    num_headers++;

  /* Write the file header. */
  bw = io->write(fh, signature, 4);
  bw += write_uint32_be(0x12);  /* header_size */
  bw += write_uint16_be(0);     /* object_version */
  bw += write_uint32_be(0);     /* file_version */
  bw += write_uint32_be(num_headers);

  if (bw != 18) {
    set_error(RMFF_ERR_IO, "Could not write the file header");
    return RMFF_ERR_IO;
  }

  bw = write_prop_header(file);
  if (bw != 0x32) {
    set_error(RMFF_ERR_IO, "Could not write the PROP header");
    return RMFF_ERR_IO;
  }

  if (file->cont_header_present) {
    bw = write_cont_header(file);
    if (bw != RMFF_ERR_OK)
      return bw;
  }

  for (i = 0; i < file->num_tracks; i++) {
    bw = write_mdpr_header(file->tracks[i]);
    if (bw < RMFF_ERR_OK)
      return bw;
  }

  fint->data_offset = io->tell(fh);

  bw = write_data_header(file);
  if (bw < RMFF_ERR_OK)
    return bw;

  clear_error();
  return RMFF_ERR_OK;
}

int
rmff_fix_headers(rmff_file_t *file) {
  rmff_prop_t *prop;
  rmff_mdpr_t *mdpr;
  rmff_track_t *track;
  rmff_file_internal_t *fint;
  rmff_track_internal_t *tint;
  int i;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }

  fint = (rmff_file_internal_t *)file->internal;

  if (fint->highest_timecode > 0)
    fint->avg_bit_rate = (int64_t)fint->total_bytes * 8 * 1000 /
      fint->highest_timecode;

  prop = &file->prop_header;
  rmff_put_uint32_be(&prop->max_bit_rate, fint->max_bit_rate);
  rmff_put_uint32_be(&prop->avg_bit_rate, fint->avg_bit_rate);
  rmff_put_uint32_be(&prop->max_packet_size, fint->max_packet_size);
  rmff_put_uint32_be(&prop->avg_packet_size, fint->avg_packet_size);
  rmff_put_uint32_be(&prop->num_packets, fint->num_packets);
  rmff_put_uint32_be(&prop->duration, fint->highest_timecode);
  rmff_put_uint32_be(&prop->index_offset, fint->index_offset);
  rmff_put_uint32_be(&prop->data_offset, fint->data_offset);
  rmff_put_uint16_be(&prop->num_streams, file->num_tracks);

  for (i = 0; i < file->num_tracks; i++) {
    track = file->tracks[i];
    mdpr = &track->mdpr_header;
    tint = (rmff_track_internal_t *)track->internal;

    if (tint->highest_timecode > 0)
      tint->avg_bit_rate = (int64_t)tint->total_bytes * 8 * 1000 /
        tint->highest_timecode;

    rmff_put_uint16_be(&mdpr->id, track->id);
    rmff_put_uint32_be(&mdpr->max_bit_rate, tint->max_bit_rate);
    rmff_put_uint32_be(&mdpr->avg_bit_rate, tint->avg_bit_rate);
    rmff_put_uint32_be(&mdpr->max_packet_size, tint->max_packet_size);
    rmff_put_uint32_be(&mdpr->avg_packet_size, tint->avg_packet_size);
    rmff_put_uint32_be(&mdpr->duration, tint->highest_timecode);
  }

  return rmff_write_headers(file);
}

void
rmff_copy_track_headers(rmff_track_t *dst,
                        rmff_track_t *src) {
  if ((dst == NULL) || (src == NULL))
    return;

  safefree(dst->mdpr_header.name);
  safefree(dst->mdpr_header.mime_type);
  safefree(dst->mdpr_header.type_specific_data);
  memcpy(&dst->mdpr_header, &src->mdpr_header, sizeof(rmff_mdpr_t));
  dst->mdpr_header.name = safestrdup(src->mdpr_header.name);
  dst->mdpr_header.mime_type = safestrdup(src->mdpr_header.mime_type);
  dst->mdpr_header.type_specific_data = (unsigned char *)
    safememdup(src->mdpr_header.type_specific_data,
               rmff_get_uint32_be(&src->mdpr_header.type_specific_size));
  dst->type = src->type;
}

static uint32_t
rmff_update_max_bitrate(rmff_bitrate_t *bitrate,
                        uint32_t timecode,
                        uint32_t frame_size) {
  uint32_t i, max_bit_rate, total_size;

  bitrate->timecodes = (uint32_t *)saferealloc(bitrate->timecodes,
                                               (bitrate->num_entries + 1) *
                                               sizeof(uint32_t));
  bitrate->timecodes[bitrate->num_entries] = timecode;
  bitrate->frame_sizes = (uint32_t *)saferealloc(bitrate->frame_sizes,
                                                 (bitrate->num_entries + 1) *
                                                 sizeof(uint32_t));
  bitrate->frame_sizes[bitrate->num_entries] = frame_size;
  bitrate->num_entries++;

  if ((bitrate->timecodes[bitrate->num_entries - 1] -
       bitrate->timecodes[0]) < 1000)
    return 0;

  total_size = 0;
  for (i = 0; i < bitrate->num_entries; i++)
    total_size += bitrate->frame_sizes[i];
  max_bit_rate = (uint32_t)
    ((int64_t)total_size * 8 * 1000 /
     (bitrate->timecodes[bitrate->num_entries - 1] -
      bitrate->timecodes[0]));
  i = 0;
  while ((i < bitrate->num_entries) &&
         ((bitrate->timecodes[bitrate->num_entries - 1] -
           bitrate->timecodes[i]) >= 1000))
    i++;
  memmove(&bitrate->timecodes[0], &bitrate->timecodes[i],
          (bitrate->num_entries - i) * sizeof(uint32_t));
  bitrate->timecodes = (uint32_t *)saferealloc(bitrate->timecodes,
                                               (bitrate->num_entries - i) *
                                               sizeof(uint32_t));
  memmove(&bitrate->frame_sizes[0], &bitrate->frame_sizes[i],
          (bitrate->num_entries - i) * sizeof(uint32_t));
  bitrate->frame_sizes = (uint32_t *)saferealloc(bitrate->frame_sizes,
                                                 (bitrate->num_entries - i) *
                                                 sizeof(uint32_t));
  bitrate->num_entries -= i;

  return max_bit_rate;
}

int
rmff_write_frame(rmff_track_t *track,
                 rmff_frame_t *frame) {
  void *fh;
  mb_file_io_t *io;
  rmff_file_internal_t *fint;
  rmff_track_internal_t *tint;
  int bw, wanted_len;
  uint32_t bit_rate;
  int64_t pos;

  if ((track == NULL) || (frame == NULL) || (frame->data == NULL) ||
      (track->file->open_mode != RMFF_OPEN_MODE_WRITING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }

  io = track->file->io;
  fh = track->file->handle;
  fint = (rmff_file_internal_t *)track->file->internal;
  tint = (rmff_track_internal_t *)track->internal;

  pos = io->tell(fh);
  if (tint->index_this && ((frame->flags & 2) == 2))
    add_to_index(track, pos, frame->timecode, fint->num_packets);
  wanted_len = 2 + 2 + 2 + 4 + 1 + 1 + frame->size;

  bw = write_uint16_be(0);      /* object_version */
  bw += write_uint16_be(wanted_len);
  bw += write_uint16_be(track->id);
  bw += write_uint32_be(frame->timecode);
  bw += write_uint8(0);         /* reserved */
  bw += write_uint8(frame->flags);
  bw += io->write(fh, frame->data, frame->size);

  if (bw != wanted_len) {
    set_error(RMFF_ERR_IO, "Could not write the frame");
    return RMFF_ERR_IO;
  }

  if (frame->size > fint->max_packet_size)
    fint->max_packet_size = frame->size;
  fint->avg_packet_size = (fint->avg_packet_size * fint->num_packets +
                           frame->size) / (fint->num_packets + 1);
  fint->num_packets++;
  fint->total_bytes += frame->size;
  if (frame->timecode > fint->highest_timecode)
    fint->highest_timecode = frame->timecode;
  fint->data_contents_size += wanted_len;

  if (frame->size > tint->max_packet_size)
    tint->max_packet_size = frame->size;
  tint->avg_packet_size = (tint->avg_packet_size * tint->num_packets +
                           frame->size) / (tint->num_packets + 1);
  tint->num_packets++;
  tint->total_bytes += frame->size;
  if (frame->timecode > tint->highest_timecode)
    tint->highest_timecode = frame->timecode;

  bit_rate = rmff_update_max_bitrate(&fint->bitrate, frame->timecode,
                                     frame->size);
  if (bit_rate > fint->max_bit_rate)
    fint->max_bit_rate = bit_rate;
  bit_rate = rmff_update_max_bitrate(&tint->bitrate, frame->timecode,
                                     frame->size);
  if (bit_rate > tint->max_bit_rate)
    tint->max_bit_rate = bit_rate;

  clear_error();
  return RMFF_ERR_OK;
}

rmff_track_t *
rmff_find_track_with_id(rmff_file_t *file,
                        uint16_t id) {
  int i;

  clear_error();
  if (file == 0) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return NULL;
  }
  for (i = 0; i < file->num_tracks; i++)
    if (file->tracks[i]->id == id)
      return file->tracks[i];
  return NULL;
}

int
rmff_write_index(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  rmff_file_internal_t *fint;
  rmff_track_t *track;
  int i, j, bw, wanted_len;
  int64_t pos;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }

  fh = file->handle;
  io = file->io;
  fint = (rmff_file_internal_t *)file->internal;

  fint->num_index_chunks = 0;
  for (i = 0; i < file->num_tracks; i++)
    if (file->tracks[i]->num_index_entries > 0)
      fint->num_index_chunks++;

  if (fint->num_index_chunks == 0) {
    clear_error();
    return RMFF_ERR_OK;
  }

  io->seek(fh, 0, SEEK_END);

  for (i = 0; i < file->num_tracks; i++) {
    track = file->tracks[i];
    if (track->num_index_entries > 0) {
      pos = io->tell(fh);
      if (fint->index_offset == 0)
        fint->index_offset = pos;
      wanted_len = 4 + 4 + 2 +  /* normal chunk header */
        4 +                     /* num_entries */
        2 +                     /* track_id */
        4 +                     /* next_header_pos */
        (2 +                    /* version */
         4 +                    /* timecode */
         4 +                    /* offset */
         4) *                   /* packet_number */
        track->num_index_entries;
      bw = write_uint32_be(rmffFOURCC('I', 'N', 'D', 'X'));
      bw += write_uint32_be(wanted_len);
      bw += write_uint16_be(0);     /* object_version */
      bw += write_uint32_be(track->num_index_entries);
      bw += write_uint16_be(track->id);
      if ((i + 1) < fint->num_index_chunks)
        bw += write_uint32_be(pos + wanted_len); /* next_indx_offset */
      else
        bw += write_uint32_be(0); /* no next_indx_chunk */
      for (j = 0; j < track->num_index_entries; j++) {
        bw += write_uint16_be(0); /* version */
        bw += write_uint32_be(track->index[j].timecode);
        bw += write_uint32_be(track->index[j].pos);
        bw += write_uint32_be(track->index[j].packet_number);
      }
      if (bw != wanted_len) {
        set_error(RMFF_ERR_IO, "Could not write the INDX chunk");
        return RMFF_ERR_IO;
      }
    }
  }

  clear_error();
  return RMFF_ERR_OK;
}

int
rmff_write_packed_video_frame(rmff_track_t *track,
                              rmff_frame_t *frame) {
  unsigned char *src_ptr, *ptr, *dst;
  int num_subpackets, i, offset, total_len;
  uint32_t *offsets, *lengths;
  rmff_frame_t *spframe;
  rmff_track_internal_t *tint;

  if ((track == NULL) || (frame == NULL) ||
      (track->file == NULL) ||
      (track->file->open_mode != RMFF_OPEN_MODE_WRITING)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }

  tint = (rmff_track_internal_t *)track->internal;
  src_ptr = frame->data;
  num_subpackets = *src_ptr + 1;
  src_ptr++;
  if (frame->size < (num_subpackets * 8 + 1)) {
    set_error(RMFF_ERR_DATA, "RealVideo unpacking failed: frame size "
              "too small. Could not extract sub packet offsets.");
    return RMFF_ERR_DATA;
  }

  offsets = (uint32_t *)safemalloc(num_subpackets * sizeof(uint32_t));
  for (i = 0; i < num_subpackets; i++) {
    src_ptr += 4;
    offsets[i] = rmff_get_uint32_le(src_ptr);
    src_ptr += 4;
  }
  if ((offsets[num_subpackets - 1] + (src_ptr - frame->data)) >= frame->size) {
    safefree(offsets);
    set_error(RMFF_ERR_DATA, "RealVideo unpacking failed: frame size "
              "too small. The sub packet offsets indicate a size "
              "larger than the actual size.");
    return RMFF_ERR_DATA;
  }
  total_len = frame->size - (src_ptr - frame->data);
  lengths = (uint32_t *)safemalloc(num_subpackets * sizeof(uint32_t));
  for (i = 0; i < (num_subpackets - 1); i++)
    lengths[i] = offsets[i + 1] - offsets[i];
  lengths[num_subpackets - 1] = total_len - offsets[num_subpackets - 1];

  dst = (unsigned char *)safemalloc(frame->size * 2);
  for (i = 0; i < num_subpackets; i++) {
    ptr = dst;
    if (num_subpackets == 1) {
      /* BROKEN! */
      *ptr = 0xc0;              /* complete frame */
      ptr++;

    } else {
      *ptr = (num_subpackets >> 1) & 0x7f; /* number of subpackets */
      if (i == (num_subpackets - 1)) /* last fragment? */
        *ptr |= 0x80;
      ptr++;

      *ptr = i + 1;             /* fragment number */
      *ptr |= ((num_subpackets & 0x01) << 7); /* number of subpackets */
      ptr++;
    }

    /* total packet length: */
    if (total_len > 0x3fff) {
      rmff_put_uint16_be(ptr, ((total_len & 0x3fff0000) >> 16));
      ptr += 2;
      rmff_put_uint16_be(ptr, total_len & 0x0000ffff);
    } else
      rmff_put_uint16_be(ptr, 0x4000 | total_len);
    ptr += 2;

    /* fragment offset from beginning/end: */
    if (num_subpackets == 1)
      offset = frame->timecode;
    else if (i < (num_subpackets - 1))
      offset = offsets[i];
    else
      /* If it's the last packet then the 'offset' is the fragment's length. */
      offset = lengths[i];

    if (offset > 0x3fff) {
      rmff_put_uint16_be(ptr, ((offset & 0x3fff0000) >> 16));
      ptr += 2;
      rmff_put_uint16_be(ptr, offset & 0x0000ffff);
    } else
      rmff_put_uint16_be(ptr, 0x4000 | offset);
    ptr += 2;

    /* sequence number = frame number & 0xff */
    *ptr = tint->num_packed_frames & 0xff;
    ptr++;

    memcpy(ptr, src_ptr, lengths[i]);
    src_ptr += lengths[i];
    ptr += lengths[i];

    spframe = rmff_allocate_frame(ptr - dst, dst);
    if (spframe == NULL) {
      safefree(offsets);
      safefree(lengths);
      safefree(dst);
      set_error(RMFF_ERR_IO, "Memory allocation error: Could not get a "
                "rmff_frame_t");
      return RMFF_ERR_IO;
    }
    spframe->timecode = frame->timecode;
    spframe->flags = frame->flags;
    if (rmff_write_frame(track, spframe) != RMFF_ERR_OK) {
      safefree(offsets);
      safefree(lengths);
      safefree(dst);
      return rmff_last_error;
    }
    rmff_release_frame(spframe);
  }
  safefree(offsets);
  safefree(lengths);
  safefree(dst);

  tint->num_packed_frames++;

  clear_error();
  return RMFF_ERR_OK;
}

static inline uint16_t
data_get_uint16_be(unsigned char **data,
                   int *len) {
  (*data) += 2;
  (*len) -= 2;
  return rmff_get_uint16_be((*data) - 2);
}

static inline unsigned char
data_get_uint8(unsigned char **data,
               int *len) {
  (*data)++;
  (*len)--;
  return *((*data) - 1);
}

static int
deliver_segments(rmff_track_t *track,
                 uint32_t timecode) {
  uint32_t len, total;
  int i;
  unsigned char *buffer, *ptr;
  rmff_video_segment_t *segment;
  rmff_track_internal_t *tint;
  rmff_frame_t *frame;

  tint = (rmff_track_internal_t *)track->internal;
  if (tint->num_segments == 0)
    return tint->num_assembled_frames;

  len = 0;
  total = 0;

  for (i = 0; i < tint->num_segments; i++) {
    segment = &tint->segments[i];
    if (len < (segment->offset + segment->size))
      len = segment->offset + segment->size;
    total += segment->size;
  }

  if (len != total) {
    sprintf(error_msg_buffer, "Packet assembly failed. "
            "Expected packet length was %d but found only %d sub packets "
            "containing %d bytes.", len, tint->num_segments, total);
    set_error(RMFF_ERR_DATA, error_msg_buffer);
    return RMFF_ERR_DATA;
  }

  len += 1 + 2 * 4 * (tint->f_merged ? 1: tint->num_segments);
  buffer = (unsigned char *)safemalloc(len);
  ptr = buffer;

  *ptr = tint->f_merged ? 0 : tint->num_segments - 1;
  ptr++;

  if (tint->f_merged) {
    rmff_put_uint32_le(ptr, 1);
    ptr += 4;
    rmff_put_uint32_le(ptr, 0);
    ptr += 4;
  } else {
    for (i = 0; i < tint->num_segments; i++) {
      rmff_put_uint32_le(ptr, 1);
      ptr += 4;
      rmff_put_uint32_le(ptr, tint->segments[i].offset);
      ptr += 4;
    }
  }

  for (i = 0; i < tint->num_segments; i++) {
    segment = &tint->segments[i];
    memcpy(ptr, segment->data, segment->size);
    ptr += segment->size;
  }

  frame = rmff_allocate_frame(len, buffer);
  frame->timecode = timecode;
  frame->flags = tint->c_keyframe ? RMFF_FRAME_FLAG_KEYFRAME : 0;
  tint->assembled_frames = (rmff_frame_t **)
    saferealloc(tint->assembled_frames, (tint->num_assembled_frames + 1) *
                sizeof(rmff_frame_t *));
  tint->assembled_frames[tint->num_assembled_frames] = frame;
  tint->num_assembled_frames++;

  for (i = 0; i < tint->num_segments; i++)
    safefree(tint->segments[i].data);
  safefree(tint->segments);
  tint->segments = NULL;
  tint->num_segments = 0;

  return tint->num_assembled_frames;
}

int
rmff_assemble_packed_video_frame(rmff_track_t *track,
                                 rmff_frame_t *frame) {
  uint32_t vpkg_header, vpkg_length, vpkg_offset;
  uint32_t len, this_timecode;
  int data_len, result;
  unsigned char *data;
  rmff_track_internal_t *tint;
  rmff_video_segment_t *segment;

  if ((track == NULL) || (frame == NULL) || (frame->data == NULL) ||
      (frame->size == 0)) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return RMFF_ERR_PARAMETERS;
  }

  tint = (rmff_track_internal_t *)track->internal;
  if (tint->num_segments == 0) {
    tint->c_keyframe = (frame->flags & RMFF_FRAME_FLAG_KEYFRAME) ==
      RMFF_FRAME_FLAG_KEYFRAME ? 1 : 0;
    tint->f_merged = 0;
  }

  if (frame->timecode != tint->c_timecode)
    deliver_segments(track, tint->c_timecode);

  data = frame->data;
  data_len = frame->size;

  while (data_len > 2) {
    vpkg_length = 0;
    vpkg_offset = 0;
    this_timecode = frame->timecode;

    // bit 7: 1=last block in block chain
    // bit 6: 1=short header (only one block?)
    vpkg_header = data_get_uint8(&data, &data_len);

    if ((vpkg_header & 0xc0) == 0x40) {
      // seems to be a very short header
      // 2 bytes, purpose of the second byte yet unknown
      data_get_uint8(&data, &data_len);
      vpkg_length = data_len;

    } else {
      if ((vpkg_header & 0x40) == 0) {
        if (data_len < 1) {
          set_error(RMFF_ERR_DATA, "Assembly failed: not enough frame "
                    "data available");
          return RMFF_ERR_DATA;
        }
        // sub-seqnum (bits 0-6: number of fragment. bit 7: ???)
        data_get_uint8(&data, &data_len);
      }

      // size of the complete packet
      // bit 14 is always one (same applies to the offset)
      if (data_len < 2) {
        set_error(RMFF_ERR_DATA, "Assembly failed: not enough frame "
                  "data available");
        return RMFF_ERR_DATA;
      }
      vpkg_length = data_get_uint16_be(&data, &data_len);

      if ((vpkg_length & 0x8000) == 0x8000)
        tint->f_merged = 1;

      if ((vpkg_length & 0x4000) == 0) {
        if (data_len < 2) {
          set_error(RMFF_ERR_DATA, "Assembly failed: not enough frame "
                    "data available");
          return RMFF_ERR_DATA;
        }
        vpkg_length <<= 16;
        vpkg_length |= data_get_uint16_be(&data, &data_len);
        vpkg_length &= 0x3fffffff;

      } else
        vpkg_length &= 0x3fff;

      // offset of the following data inside the complete packet
      // Note: if (hdr&0xC0)==0x80 then offset is relative to the
      // _end_ of the packet, so it's equal to fragment size!!!
      if (data_len < 2) {
        set_error(RMFF_ERR_DATA, "Assembly failed: not enough frame "
                  "data available");
        return RMFF_ERR_DATA;
      }
      vpkg_offset = data_get_uint16_be(&data, &data_len);

      if ((vpkg_offset & 0x4000) == 0) {
        if (data_len < 2) {
          set_error(RMFF_ERR_DATA, "Assembly failed: not enough frame "
                    "data available");
          return RMFF_ERR_DATA;
        }
        vpkg_offset <<= 16;
        vpkg_offset |= data_get_uint16_be(&data, &data_len);
        vpkg_offset &= 0x3fffffff;

      } else
        vpkg_offset &= 0x3fff;

      if (data_len < 1) {
        set_error(RMFF_ERR_DATA, "Assembly failed: not enough frame "
                  "data available");
        return RMFF_ERR_DATA;
      }
      data_get_uint8(&data, &data_len);

      if ((vpkg_header & 0xc0) == 0xc0) {
        this_timecode = vpkg_offset;
        vpkg_offset = 0;
      } else if ((vpkg_header & 0xc0) == 0x80)
        vpkg_offset = vpkg_length - vpkg_offset;
    }

    if (data_len < (int)(vpkg_length - vpkg_offset))
      len = data_len;
    else
      len = (int)(vpkg_length - vpkg_offset);

    tint->segments = (rmff_video_segment_t *)
      saferealloc(tint->segments, (tint->num_segments + 1) *
                  sizeof(rmff_video_segment_t));
    segment = &tint->segments[tint->num_segments];
    tint->num_segments++;

    segment->offset = vpkg_offset;
    segment->data = (unsigned char *)safemalloc(len);
    segment->size = len;
    memcpy(segment->data, data, len);
    data += len;
    data_len -= len;

    tint->c_timecode = this_timecode;

    if (((vpkg_header & 0x80) == 0x80) ||
        ((vpkg_offset + len) >= vpkg_length)) {
      result = deliver_segments(track, this_timecode);
      tint->c_keyframe = 0;
      if (result < 0)
        return result;
    }
  }

  return tint->num_assembled_frames;
}

rmff_frame_t *
rmff_get_packed_video_frame(rmff_track_t *track) {
  rmff_track_internal_t *tint;
  rmff_frame_t *frame;

  if (track == NULL) {
    set_error(RMFF_ERR_PARAMETERS, NULL);
    return NULL;
  }

  tint = (rmff_track_internal_t *)track->internal;
  if (tint->num_assembled_frames == 0) {
    clear_error();
    return NULL;
  }

  frame = tint->assembled_frames[0];
  tint->num_assembled_frames--;
  if (tint->num_assembled_frames == 0) {
    safefree(tint->assembled_frames);
    tint->assembled_frames = NULL;
  } else {
    memmove(&tint->assembled_frames[0], &tint->assembled_frames[1],
            tint->num_assembled_frames * sizeof(rmff_frame_t *));
    tint->assembled_frames = (rmff_frame_t **)
      saferealloc(tint->assembled_frames, tint->num_assembled_frames *
                  sizeof(rmff_frame_t *));
  }

  clear_error();
  return frame;
}
