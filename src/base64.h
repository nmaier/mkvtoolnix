/*
  mkvtoolnix - programs for manipulating Matroska files

  base64.h

  Written by Moritz Bunkus <moritz@bunkus.org>
*/

/*!
    \file
    \version $Id$
    \brief base64 encoding and decoding functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/


#ifndef __BASE64_H
#define __BASE64_H

#include <string>

using namespace std;

string base64_encode(const unsigned char *src, int src_len,
                     bool line_breaks = false, int max_line_len = 72);
int base64_decode(const string &src, unsigned char *dst);

#endif // __BASE64_H
