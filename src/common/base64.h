/*
 * mkvtoolnix - programs for manipulating Matroska files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * base64 encoding and decoding functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */


#ifndef __BASE64_H
#define __BASE64_H

#include "os.h"

#include <string>

using namespace std;

string MTX_DLL_API base64_encode(const unsigned char *src, int src_len,
                                 bool line_breaks = false,
                                 int max_line_len = 72);
int MTX_DLL_API base64_decode(const string &src, unsigned char *dst);

#endif // __BASE64_H
