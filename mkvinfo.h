/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mkvinfo.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: mkvinfo.h,v 1.1 2003/05/28 08:08:00 mosu Exp $
    \brief definition of global variables and functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/


#ifndef __MKVINFO_H
#define __MKVINFO_H

#define NAME "MKVInfo"

void parse_args(int argc, char **argv, char *&file_name, bool &use_gui);
int console_main(int argc, char **argv);
bool process_file(const char *file_name);

#endif // __MKVINFO_H
