/*
 *  rmff.c
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
  uint32_t index_offset;
} rmff_file_internal_t;

typedef struct rmff_track_internal_t {
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t highest_timecode;
  uint32_t num_packets;
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
  for (i = 0; i < file->num_tracks; i++)
    rmff_free_track_data(&file->tracks[i]);
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

int
rmff_read_headers(rmff_file_t *file) {
  mb_file_io_t *io;
  void *fh;
  uint32_t object_id, object_size, size;
  uint16_t object_version;
  rmff_prop_t *prop;
  rmff_cont_t *cont;
  rmff_mdpr_t *mdpr;
  rmff_track_t track;
  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;
  int prop_header_found;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);
  if (file->headers_read)
    return 0;

  io = file->io;
  fh = file->handle;
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
      memset(&track, 0, sizeof(rmff_track_t));
      track.file = (struct rmff_file_t *)file;
      mdpr = &track.mdpr_header;
      read_uint16_be_to(&mdpr->id);
      track.id = rmff_get_uint16_be(&mdpr->id);
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
        track.type = RMFF_TRACK_TYPE_VIDEO;
      else if ((size >= sizeof(real_audio_v4_props_t)) &&
               (get_fourcc(&ra4p->fourcc1) ==
                rmffFOURCC('.', 'r', 'a', 0xfd))) {
        track.type = RMFF_TRACK_TYPE_AUDIO;
        if ((rmff_get_uint16_be(&ra4p->version1) == 5) &&
            (size < sizeof(real_audio_v5_props_t)))
          return set_error(RMFF_ERR_DATA, "RealAudio v5 data indicated but "
                           "data too small", RMFF_ERR_DATA);
      }

      track.internal = safecalloc(sizeof(rmff_track_internal_t));
      file->tracks =
        (rmff_track_t *)saferealloc(file->tracks, (file->num_tracks + 1) *
                                    sizeof(rmff_track_t));
      file->tracks[file->num_tracks] = track;
      file->num_tracks++;

    } else if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
      file->num_packets_in_chunk = read_uint32_be();
      file->next_data_header_offset = read_uint32_be();
      file->first_data_header_offset = io->tell(fh) - (4 + 4 + 2);
      break;

    } else {
      /* Unknown header type */
      return set_error(RMFF_ERR_DATA, NULL, RMFF_ERR_DATA);
    }
  }

  if (prop_header_found && (file->first_data_header_offset > 0)) {
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
  uint16_t object_version, length;
  uint32_t object_id;
  mb_file_io_t *io;
  void *fh;

  if ((file == NULL) || (!file->headers_read) || (file->io == NULL) ||
      (file->handle == NULL) || (file->open_mode != RMFF_OPEN_MODE_READING))
    return (rmff_frame_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);
  io = file->io;
  fh = file->handle;
  if ((file->size - io->tell(fh)) < 12)
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);

  object_version = read_uint16_be();
  length = read_uint16_be();
  object_id = ((uint32_t)object_version) << 16 | length;
  if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
    file->num_packets_in_chunk = read_uint32_be();
    file->next_data_header_offset = read_uint32_be();
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
rmff_set_track_specific_data(rmff_track_t *track,
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
rmff_add_track(rmff_file_t *file) {
  rmff_track_t track;
  int i, id, found;

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING))
    return (rmff_track_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);

  id = 0;
  do {
    found = 0;
    for (i = 0; i < file->num_tracks; i++)
      if (file->tracks[i].id == id) {
        found = 1;
        id++;
        break;
      }
  } while (found);

  memset(&track, 0, sizeof(rmff_track_t));
  track.id = id;
  track.file = file;

  file->tracks =
    (rmff_track_t *)saferealloc(file->tracks, (file->num_tracks + 1) *
                                sizeof(rmff_track_t));
  file->tracks[file->num_tracks] = track;
  file->num_tracks++;

  return &file->tracks[file->num_tracks - 1];
}

void
rmff_set_std_audio_v4_values(real_audio_v4_props_t *props) {
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
  return RMFF_ERR_OK;
}

int
rmff_write_headers(rmff_file_t *file) {
  void *fh;
  mb_file_io_t *io;
  int i, bw;
  rmff_file_internal_t *fint;
  const char *signature = ".RMF";

  if ((file == NULL) || (file->open_mode != RMFF_OPEN_MODE_WRITING))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);

  io = file->io;
  fh = file->handle;
  io->seek(fh, 0, SEEK_SET);

  /* Write the file header. */
  bw = io->write(fh, signature, 4);
  bw += write_uint32_be(0x12);  /* header_size */
  bw += write_uint16_be(1);     /* object_version */
  bw += write_uint32_be(0);     /* file_version */
  bw += write_uint32_be(0);     /* temporary: num_headers */

  if (bw != 18)
    return set_error(RMFF_ERR_IO, "Could not write the file header",
                     RMFF_ERR_IO);

  rmff_put_uint16_be(&file->prop_header.num_streams, file->num_tracks);
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
    bw = write_mdpr_header(&file->tracks[i]);
    if (bw < RMFF_ERR_OK)
      return bw;
  }

  fint = (rmff_file_internal_t *)file->internal;
  fint->data_offset = io->tell(fh);

  return RMFF_ERR_OK;
}

int
rmff_fix_headers(rmff_file_t *file) {
  return -1;
}

void
rmff_copy_track_headers(rmff_track_t *dst,
                        rmff_track_t *src) {
  if ((dst == NULL) || (src == NULL))
    return;

  rmff_free_track_data(dst);
  memcpy(&dst->mdpr_header, &src->mdpr_header, sizeof(rmff_mdpr_t));
  dst->mdpr_header.name = safestrdup(src->mdpr_header.name);
  dst->mdpr_header.mime_type = safestrdup(src->mdpr_header.mime_type);
  dst->mdpr_header.type_specific_data = (unsigned char *)
    safememdup(src->mdpr_header.type_specific_data,
               rmff_get_uint32_be(&src->mdpr_header.type_specific_size));
  dst->internal = (unsigned char *)safememdup(src->internal,
                                              sizeof(rmff_track_internal_t));
  dst->type = src->type;
}
