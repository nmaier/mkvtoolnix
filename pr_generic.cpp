/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: pr_generic.cpp,v 1.2 2003/02/24 12:31:17 mosu Exp $
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <malloc.h>

#include "pr_generic.h"

generic_packetizer_c::generic_packetizer_c() {
  serialno = -1;
  track_entry = NULL;
  private_data = NULL;
  private_data_size = 0;
}

generic_packetizer_c::~generic_packetizer_c() {
  if (private_data != NULL)
    free(private_data);
}

void generic_packetizer_c::set_private_data(void *data, int size) {
  if (private_data != NULL)
    free(private_data);
  private_data = malloc(size);
  if (private_data == NULL)
    die("malloc");
  memcpy(private_data, data, size);
  private_data_size = size;
}

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c() {
}

generic_reader_c::~generic_reader_c() {
}
