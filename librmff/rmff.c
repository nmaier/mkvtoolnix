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

  $Id$
 
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "librmff.h"

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
} rmff_file_internal_t;

typedef struct rmff_track_internal_t {
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t highest_timecode;
  uint32_t num_packets;
  int index_this;
  uint32_t last_timecode;
  uint32_t bytes_since_timecode_change;
  uint32_t total_bytes;
} rmff_track_internal_t;

int rmff_last_error = RMFF_ERR_OK;
const char *rmff_last_error_msg = NULL;
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

static int
set_error(int error_number,
          const char *error_msg,
          int return_value) {
  rmff_last_error = error_number;
  if (error_msg == NULL)
    rmff_last_error_msg = rmff_get_error_str(error_number);
  else
    rmff_last_error_msg = error_msg;

  return return_value;
}

#define clear_error() set_error(RMFF_ERR_OK, NULL, RMFF_ERR_OK)

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

  io->read(fh, &tmp, 1);
  return tmp;
}

static uint16_t
file_read_uint16_be(mb_file_io_t *io,
                    void *fh) {
  unsigned char tmp[2];

  io->read(fh, tmp, 2);
  return rmff_get_uint16_be(tmp);
}

static uint32_t
file_read_uint32_be(mb_file_io_t *io,
                    void *fh) {
  unsigned char tmp[4];

  io->read(fh, tmp, 4);
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
    return (rmff_file_t *)set_error(RMFF_ERR_NOT_RMFF, NULL, 0);
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
    return (rmff_file_t *)set_error(RMFF_ERR_IO, NULL, 0);
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
                     RMFF_FILE_FLAG_PERFECT_PLAY);

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
      ((mode != RMFF_OPEN_MODE_READING) && (mode != RMFF_OPEN_MODE_WRITING)))
    return (rmff_file_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);

  if (mode == RMFF_OPEN_MODE_READING)
    return open_file_for_reading(path, io);
  else
    return open_file_for_writing(path, io);
}

void
rmff_free_track_data(rmff_track_t *track) {
  if (track == NULL)
    return;
  safefree(track->mdpr_header.name);
  safefree(track->mdpr_header.mime_type);
  safefree(track->mdpr_header.type_specific_data);
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


#define skip(num) { if (io->seek(fh, num, SEEK_CUR) != 0) \
                      return set_error(RMFF_ERR_IO, NULL, -1); }

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
  uint32_t object_id, object_size, object_version, num_entries;
  uint32_t next_header_offset, i, timecode, offset, packet_number;
  uint16_t id;

  fh = file->handle;
  io = file->io;

  rmff_last_error = RMFF_ERR_OK;
  io->seek(fh, pos, SEEK_SET);
  object_id = read_uint32_be();
  object_size = read_uint32_be();
  object_version = read_uint16_be();
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
  rmff_prop_t *prop;
  rmff_cont_t *cont;
  rmff_mdpr_t *mdpr;
  rmff_track_t *track;
  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;
  rmff_file_internal_t *fint;
  int prop_header_found;
  int64_t old_pos;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);
  if (file->headers_read)
    return 0;

  io = file->io;
  fh = file->handle;
  fint = (rmff_file_internal_t *)file->internal;
  if (io->seek(fh, 4, SEEK_SET))
    return set_error(RMFF_ERR_IO, NULL, -1);

  skip(4);                      /* header_size */
  skip(2);                      /* object_version */
  skip(4);                      /* file_version */
  skip(4);                      /* num_headers */

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
      }
      size = read_uint8(); /* mime_type_len */
      if (size > 0) {
        mdpr->mime_type = (char *)safemalloc(size + 1);
        io->read(fh, mdpr->mime_type, size);
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
            (size < sizeof(real_audio_v5_props_t)))
          return set_error(RMFF_ERR_DATA, "RealAudio v5 data indicated but "
                           "data too small", RMFF_ERR_DATA);
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
      return set_error(RMFF_ERR_DATA, NULL, RMFF_ERR_DATA);
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
  return set_error(RMFF_ERR_DATA, NULL, RMFF_ERR_DATA);
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
      (file->handle == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);
  io = file->io;
  fh = file->handle;
  old_pos = io->tell(fh);
  if ((file->size - old_pos) < 12)
    return set_error(RMFF_ERR_EOF, NULL, RMFF_ERR_EOF);

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
  if (object_id == rmffFOURCC('I', 'N', 'D', 'X'))
    return set_error(RMFF_ERR_EOF, NULL, RMFF_ERR_EOF);
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
      (file->handle == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING))
    return (rmff_frame_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);
  io = file->io;
  fh = file->handle;
  fint = (rmff_file_internal_t *)file->internal;
  if ((file->size - io->tell(fh)) < 12)
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);

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
      (object_id == rmffFOURCC('I', 'N', 'D', 'X')))
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);

  frame = (rmff_frame_t *)safecalloc(sizeof(rmff_frame_t));
  if (buffer == NULL) {
    buffer = safemalloc(length);
    frame->allocated_by_rmff = 1;
  }
  frame->data = buffer;
  frame->size = length - 12;
  frame->id = read_uint16_be();
  frame->timecode = read_uint32_be();
  frame->reserved = read_uint8();
  frame->flags = read_uint8();
  if (io->read(fh, frame->data, frame->size) != frame->size) {
    rmff_release_frame(frame);
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);
  }
  file->num_packets_read++;

  return frame;
}

rmff_frame_t *
rmff_allocate_frame(uint32_t size,
                    void *buffer) {
  rmff_frame_t *frame;

  if (size == 0)
    return (rmff_frame_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);
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

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING))
    return (rmff_track_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);

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
  return set_error(RMFF_ERR_IO, "Could not write the CONT header",
                   RMFF_ERR_IO);
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

  if (wanted_len != bw)
    return set_error(RMFF_ERR_IO, "Could not write the MDPR header",
                     RMFF_ERR_IO);
  return clear_error();
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
  bw += write_uint32_be(fint->data_contents_size + 4 + 4 + 2 + 4 + 4 + 4);
  bw += write_uint16_be(0);     /* object_version */
  bw += write_uint32_be(fint->num_packets); /* num_packets_in_chunk */
  bw += write_uint32_be(0);     /* next_data_header_offset */

  if (bw != 18)
    return set_error(RMFF_ERR_IO, "Could not write the DATA header",
                     RMFF_ERR_IO);
  return clear_error();
}

int
rmff_write_headers(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  int i, bw, num_headers;
  rmff_file_internal_t *fint;
  const char *signature = ".RMF";

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);

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
  bw += write_uint16_be(1);     /* object_version */
  bw += write_uint32_be(0);     /* file_version */
  bw += write_uint32_be(num_headers);

  if (bw != 18)
    return set_error(RMFF_ERR_IO, "Could not write the file header",
                     RMFF_ERR_IO);

  bw = write_prop_header(file);
  if (bw != 0x32)
    return set_error(RMFF_ERR_IO, "Could not write the PROP header",
                     RMFF_ERR_IO);

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

  return clear_error();
}

int
rmff_fix_headers(rmff_file_t *file) {
  rmff_prop_t *prop;
  rmff_mdpr_t *mdpr;
  rmff_track_t *track;
  rmff_file_internal_t *fint;
  rmff_track_internal_t *tint;
  int i;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);

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
      (track->file->open_mode != RMFF_OPEN_MODE_WRITING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);

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

  if (bw != wanted_len)
    return set_error(RMFF_ERR_IO, "Could not write the frame", RMFF_ERR_IO);

  if (wanted_len > fint->max_packet_size)
    fint->max_packet_size = wanted_len;
  fint->avg_packet_size = (fint->avg_packet_size * fint->num_packets +
                           wanted_len) / (fint->num_packets + 1);
  fint->num_packets++;
  fint->total_bytes += frame->size;
  if (frame->timecode > fint->highest_timecode)
    fint->highest_timecode = frame->timecode;
  fint->data_contents_size += wanted_len;

  if (wanted_len > tint->max_packet_size)
    tint->max_packet_size = wanted_len;
  tint->avg_packet_size = (tint->avg_packet_size * tint->num_packets +
                           wanted_len) / (tint->num_packets + 1);
  tint->num_packets++;
  tint->total_bytes += frame->size;

  /* Update the max_bit_rates. Crude... */
  if (frame->timecode > tint->highest_timecode)
    tint->highest_timecode = frame->timecode;
  if (frame->timecode != tint->last_timecode) {
    if (tint->last_timecode != 0) {
      bit_rate = (int64_t)tint->bytes_since_timecode_change * 8 * 1000 /
        (frame->timecode - tint->last_timecode);
      if (bit_rate > tint->max_bit_rate)
        tint->max_bit_rate = bit_rate;
      if (bit_rate > fint->max_bit_rate)
        fint->max_bit_rate = bit_rate;
    }
    tint->last_timecode = frame->timecode;
    tint->bytes_since_timecode_change = frame->size;
  } else
    tint->bytes_since_timecode_change += frame->size;

  return clear_error();
}

rmff_track_t *
rmff_find_track_with_id(rmff_file_t *file,
                        uint16_t id) {
  int i;

  clear_error();
  if (file == 0)
    return (rmff_track_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);
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
  rmff_track_internal_t *tint;
  rmff_track_t *track;
  int i, j, bw, wanted_len;
  int64_t pos;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);

  fh = file->handle;
  io = file->io;
  fint = (rmff_file_internal_t *)file->internal;

  fint->num_index_chunks = 0;
  for (i = 0; i < file->num_tracks; i++)
    if (file->tracks[i]->num_index_entries > 0)
      fint->num_index_chunks++;

  if (fint->num_index_chunks == 0)
    return clear_error();

  io->seek(fh, 0, SEEK_END);

  for (i = 0; i < file->num_tracks; i++) {
    track = file->tracks[i];
    tint = (rmff_track_internal_t *)track->internal;
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
      if (bw != wanted_len)
        return set_error(RMFF_ERR_IO, "Could not write the INDX chunk",
                         RMFF_ERR_IO);
    }
  }

  return clear_error();
}
