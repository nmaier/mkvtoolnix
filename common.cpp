/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: common.cpp,v 1.13 2003/05/02 20:11:34 mosu Exp $
    \brief helper functions, common variables
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int verbose = 1;

void _die(const char *s, const char *file, int line) {
  fprintf(stderr, "die @ %s/%d : %s\n", file, line, s);
  exit(1);
}

void _trace(const char *func, const char *file, int line) {
  fprintf(stdout, "trace: %s:%s (%d)\n", file, func, line);
}

/*
 * Endianess stuff
 */

uint16_t get_uint16(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t get_uint32(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

/*
 * Character map conversion stuff
 */

static iconv_t ict_to_utf8, ict_from_utf8;
static int iconv_initialized = -1;

void utf8_init() {
  char *lc_ctype;

  lc_ctype = getenv("LC_CTYPE");
  if (lc_ctype == NULL)
    lc_ctype = "ISO8859-15";

  iconv_initialized = 0;

  ict_to_utf8 = iconv_open("UTF8", lc_ctype);
  if (ict_to_utf8 == (iconv_t)(-1))
    fprintf(stdout, "Warning: Could not initialize the iconv library for "
            "the conversion from %s to UFT8. "
            "Strings will not be converted to UTF-8 and the resulting "
            "Matroska file might not comply with the Matroska specs ("
            "error: %d, %s).\n", lc_ctype, errno, strerror(errno));
  else
    iconv_initialized = 1;

  ict_from_utf8 = iconv_open(lc_ctype, "UTF8");
  if (ict_from_utf8 == (iconv_t)(-1))
    fprintf(stdout, "Warning: Could not initialize the iconv library for "
            "the conversion from UFT8 to %s. "
            "Strings cannot be converted from UTF-8 and might be displayed "
            "incorrectly (error: %d, %s).\n", lc_ctype, errno,
            strerror(errno));
  else
    iconv_initialized |= 2;
}

void utf8_done() {
  if (iconv_initialized < 0)
    return;

  if (iconv_initialized & 1)
    iconv_close(ict_to_utf8);
  if (iconv_initialized & 2)
    iconv_close(ict_from_utf8);
}

char *to_utf8(char *local) {
  char *utf8, *slocal, *sutf8;
  size_t llocal, lutf8;
  int len;

  if (iconv_initialized == -1)
    utf8_init();

  if (ict_to_utf8 == (iconv_t)(-1)) {
    utf8 = strdup(local);
    if (utf8 == NULL)
      die("strdup");
    return utf8;
  }

  len = strlen(local) * 4;
  utf8 = (char *)malloc(len + 1);
  if (utf8 == NULL)
    die("malloc");
  memset(utf8, 0, len + 1);

  iconv(ict_to_utf8, NULL, 0, NULL, 0);      // Reset the iconv state.
  llocal = len / 4;
  lutf8 = len;
  slocal = local;
  sutf8 = utf8;
  iconv(ict_to_utf8, &slocal, &llocal, &sutf8, &lutf8);

  return utf8;
}

char *from_utf8(char *utf8) {
  char *local, *slocal, *sutf8;
  int len;
  size_t llocal, lutf8;

  if (iconv_initialized == -1)
    utf8_init();

  if (ict_from_utf8 == (iconv_t)(-1)) {
    local = strdup(utf8);
    if (local == NULL)
      die("strdup");
    return local;
  }

  len = strlen(utf8) * 4;
  local = (char *)malloc(len + 1);
  if (local == NULL)
    die("malloc");
  memset(local, 0, len + 1);

  iconv(ict_from_utf8, NULL, 0, NULL, 0);      // Reset the iconv state.
  llocal = len;
  lutf8 = len / 4;
  slocal = local;
  sutf8 = utf8;
  iconv(ict_from_utf8, &sutf8, &lutf8, &slocal, &llocal);

  return local;
}

/*
 * Random unique uint32_t numbers
 */

static uint32_t *ru_numbers = NULL;
static int num_ru_numbers = 0;

int is_unique_uint32(uint32_t number) {
  int i;

  for (i = 0; i < num_ru_numbers; i++)
    if (ru_numbers[i] == number)
      return 0;

  return 1;
}

void add_unique_uint32(uint32_t number) {
  ru_numbers = (uint32_t *)realloc(ru_numbers, (num_ru_numbers + 1) *
                                   sizeof(uint32_t));
  if (ru_numbers == NULL)
    die("realloc");

  ru_numbers[num_ru_numbers] = number;
  num_ru_numbers++;
}

uint32_t create_unique_uint32() {
  uint32_t rnumber, half;

  do {
    half = (uint32_t)(65535.0 * rand() / RAND_MAX);
    rnumber = half;
    half = (uint32_t)(65535.0 * rand() / RAND_MAX);
    rnumber |= (half << 16);
  } while ((rnumber == 0) || !is_unique_uint32(rnumber));
  add_unique_uint32(rnumber);

  return rnumber;
}
